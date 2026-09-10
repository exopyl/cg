#include "material_renderer.h"

#include "diagnostics.h"

#include "../cgmesh/material_convert.h"
#include "../cgmesh/material_pbr.h"

MaterialRenderer *MaterialRenderer::m_pInstance = new MaterialRenderer;

// ---------------------------------------------------------------------------
//  Carte de reflexion : sphere mapping sur l'unite de texture 1
// ---------------------------------------------------------------------------
// Les coordonnees sont GENEREES par OpenGL depuis la normale en espace oeil
// (GL_SPHERE_MAP), pas lues du maillage : une carte de reflexion n'a donc besoin
// d'aucune UV, et elle suit le point de vue -- ce qui est le propre d'un reflet.
// Cela vaut pour tous les chemins de rendu, immediat comme VBO, puisque rien n'est
// ajoute au flux de sommets.
//
// L'unite 1 MELANGE le reflet et la couleur diffuse texturee, dans la proportion
// `amount` :
//
//     resultat = reflet * amount + diffuse * (1 - amount)
//
// c'est-a-dire un GL_INTERPOLATE sur une constante d'environnement. Ce n'est pas
// une addition : une carte d'environnement est claire par nature -- Ref.jpg est
// quasi blanche -- et l'AJOUTER a pleine intensite saturait toute la surface, ce
// qui donnait des pieds chromes uniformement blanchis. Le melange conserve
// l'energie : plus le materiau reflechit, moins on voit sa diffuse, ce qui est
// exactement le sens d'un « montant de reflexion ».
static void BindReflectionUnit (GLuint texId, float amount)
{
	glActiveTexture (GL_TEXTURE1);
	if (texId == 0 || amount <= 0.f)
	{
		// Toujours remettre l'unite 1 dans un etat neutre : ActivateMaterial est
		// le seul point de reglage, il n'existe pas de desactivation separee, donc
		// un materiau sans reflexion doit effacer celle du materiau precedent.
		glDisable (GL_TEXTURE_GEN_S);
		glDisable (GL_TEXTURE_GEN_T);
		glDisable (GL_TEXTURE_2D);
		glActiveTexture (GL_TEXTURE0);
		return;
	}

	glBindTexture (GL_TEXTURE_2D, texId);
	glEnable (GL_TEXTURE_2D);

	const GLfloat envColor[4] = { 0.f, 0.f, 0.f, amount };
	glTexEnvi  (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	glTexEnvi  (GL_TEXTURE_ENV, GL_COMBINE_RGB,      GL_INTERPOLATE);
	glTexEnvi  (GL_TEXTURE_ENV, GL_SRC0_RGB,         GL_TEXTURE);   // le reflet
	glTexEnvi  (GL_TEXTURE_ENV, GL_OPERAND0_RGB,     GL_SRC_COLOR);
	glTexEnvi  (GL_TEXTURE_ENV, GL_SRC1_RGB,         GL_PREVIOUS);  // l'unite 0
	glTexEnvi  (GL_TEXTURE_ENV, GL_OPERAND1_RGB,     GL_SRC_COLOR);
	glTexEnvi  (GL_TEXTURE_ENV, GL_SRC2_RGB,         GL_CONSTANT);  // le montant
	glTexEnvi  (GL_TEXTURE_ENV, GL_OPERAND2_RGB,     GL_SRC_ALPHA);
	glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, envColor);
	// L'alpha traverse sans etre touche : le melange ne concerne que la couleur.
	glTexEnvi  (GL_TEXTURE_ENV, GL_COMBINE_ALPHA,    GL_REPLACE);
	glTexEnvi  (GL_TEXTURE_ENV, GL_SRC0_ALPHA,       GL_PREVIOUS);
	glTexEnvi  (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA,   GL_SRC_ALPHA);

	// Coordonnees GENEREES depuis la normale en espace oeil : une carte de
	// reflexion n'a besoin d'aucune UV et suit le point de vue.
	glTexGeni (GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
	glTexGeni (GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
	glEnable (GL_TEXTURE_GEN_S);
	glEnable (GL_TEXTURE_GEN_T);
	glActiveTexture (GL_TEXTURE0);   // laisser l'unite 0 courante pour la suite
}

// ---------------------------------------------------------------------------
//  Exposant speculaire : la conversion, et sa borne
// ---------------------------------------------------------------------------
// Material::GetShininess() est une FRACTION dans [0,1] ; OpenGL veut un exposant
// dans [0,128]. D'ou le facteur 128.
//
// La borne n'est pas de la prudence gratuite : glMaterialf(GL_SHININESS, x) rend
// GL_INVALID_VALUE hors de [0,128] et l'appel est alors IGNORE -- l'exposant
// garde donc la valeur laissee par le materiau precedent, et l'apparence d'un
// objet depend de ce qui a ete dessine avant lui. Sans controle d'erreur GL, cela
// ne se voyait pas.
//
// Le cas qui l'a revele : l'importateur glTF ecrivait SetShininess(32.f), c'est-a-dire
// un exposant GL brut la ou les quatre autres importateurs ecrivent une fraction
// (OBJ Ns/128, 3DS Power/100, 3DM Shine/255, bibliotheque interne <= 0.25).
// 128 x 32 = 4096. La source est corrigee dans vmeshes_io.cpp, mais la borne reste :
// c'est ici qu'est la frontiere avec GL, et c'est ici que son contrat se tient,
// quelle que soit la valeur qu'un importateur presente demain.
static GLfloat GlShininess (float fraction)
{
	float exponent = 128.f * fraction;
	if (exponent < 0.f)   exponent = 0.f;
	if (exponent > 128.f) exponent = 128.f;
	return (GLfloat)exponent;
}

// ---------------------------------------------------------------------------
//  Niveaux de mipmap : la reponse a la MINIFICATION
// ---------------------------------------------------------------------------
// Sans eux, GL_LINEAR prend quatre texels de l'image PLEINE RESOLUTION quel que
// soit le facteur de reduction. Une texture porteuse de traits fins perd alors
// ses traits de facon irreguliere -- et, la vue bougeant d'un pixel, en perd
// d'autres a l'image suivante : elle SCINTILLE. Le cas type du depot est la base
// de coupe de sinaia, dont les graduations font 4 px dans une image de
// 4500 x 3000 : vue de loin, son quadrillage moire.
//
// Le mipmapping filtre l'image a chaque niveau au moment du televersement, si
// bien qu'un trait trop fin pour le pixel devient une teinte STABLE au lieu de
// clignoter. Le cout est une fois un tiers de memoire en plus, paye au
// chargement.
//
// glGenerateMipmap est un point d'entree GL 3.0, charge par glad. Sur un pilote
// qui ne l'expose pas, le pointeur est nul et l'on retombe sur GL_LINEAR seul --
// soit exactement le comportement d'avant : degrade, jamais casse. C'est ce que
// dit le booleen rendu, que l'appelant traduit en filtre de minification.
static bool GenerateMipmaps ()
{
	if (!glGenerateMipmap)
		return false;
	glGenerateMipmap (GL_TEXTURE_2D);
	return true;
}

// ---------------------------------------------------------------------------
//  Televersement d'une image de texture, borne a GL_MAX_TEXTURE_SIZE
// ---------------------------------------------------------------------------
// glTexImage2D REFUSE une dimension superieure au maximum du pilote (couramment
// 16384) : il rend GL_INVALID_VALUE et l'objet de texture reste INCOMPLET. Le
// pipeline fixe traite alors la texture comme si elle etait desactivee, et le
// fragment prend la couleur du materiau -- sans aucune erreur visible.
//
// Ce n'est pas theorique : « Star Wars Juggeren.3ds » embarque quatre PNG de
// 18116 x 35 (des bandes pour les pneus). Leur televersement echouait, et comme un
// materiau texture porte desormais un diffus BLANC, les roues sortaient blanches.
// Avant, le Kd du fichier (7/255, quasi noir) masquait l'echec par accident.
//
// On reduit donc l'image a la volee, sur une COPIE : le Img du materiau est
// partage entre plusieurs materiaux et sert aussi au panneau d'informations, il ne
// doit pas etre altere par un detail de rendu. L'echantillonnage reste correct
// puisque les UV sont normalisees -- une mise a l'echelle non uniforme ne deplace
// aucun texel.
static bool UploadTextureImage (Img *pImage)
{
	if (!pImage || !pImage->data()) return false;

	GLint maxSize = 0;
	glGetIntegerv (GL_MAX_TEXTURE_SIZE, &maxSize);
	if (maxSize <= 0) maxSize = 2048;      // pilote muet : borne prudente

	const unsigned int w = pImage->width(), h = pImage->height();
	if (w <= (unsigned int)maxSize && h <= (unsigned int)maxSize)
	{
		glTexImage2D (GL_TEXTURE_2D, 0, 4, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pImage->data());
		return GenerateMipmaps ();
	}

	Img scaled (*pImage);                  // copie : ne pas toucher a l'original
	const unsigned int nw = (w > (unsigned int)maxSize) ? (unsigned int)maxSize : w;
	const unsigned int nh = (h > (unsigned int)maxSize) ? (unsigned int)maxSize : h;
	if (scaled.resize (nw, nh, 1) != 0)    // mode 1 : bilineaire
	{
		cgre::Logf ("MaterialRenderer : texture %ux%u au-dela de GL_MAX_TEXTURE_SIZE=%d, "
		            "reduction impossible.", w, h, (int)maxSize);
		return false;
	}
	cgre::Logf ("MaterialRenderer : texture %ux%u reduite a %ux%u (GL_MAX_TEXTURE_SIZE=%d).",
	            w, h, nw, nh, (int)maxSize);
	glTexImage2D (GL_TEXTURE_2D, 0, 4, nw, nh, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled.data());
	CGRE_CHECK_GL ("MaterialRenderer::UploadTextureImage (reduite)");
	return GenerateMipmaps ();
}

MaterialRenderer::MaterialRenderer()
{
	// Rien a initialiser : Entry initialise ses membres a la declaration, et le
	// vecteur part vide. C'est ce qui corrige au passage m_pTexturesId, seul des
	// cinq anciens tableaux a n'avoir jamais ete remis a zero -- un
	// MATERIAL_TEXTURE dont GetImage() est nul saute le glGenTextures, et
	// ActivateMaterial faisait ensuite un glBindTexture sur une valeur
	// indeterminee.
}

MaterialRenderer::~MaterialRenderer()
{
	// Volontairement vide, et ce n'est pas un oubli : ce destructeur ne s'execute
	// jamais (le singleton est un `new` jamais rendu), et s'il s'executait ce
	// serait a la destruction des statiques, apres la disparition du contexte GL
	// -- ou glDeleteTextures n'a plus de sens. La liberation reelle a lieu dans
	// RemoveMaterial, au fil de l'eau ; le pilote reprend le reste a la sortie du
	// processus.
}

void MaterialRenderer::ReleaseEntry (Entry& entry)
{
	if (entry.textureId != 0)
		glDeleteTextures (1, &entry.textureId);
	if (entry.reflTextureId != 0)
		glDeleteTextures (1, &entry.reflTextureId);

	entry = Entry ();
}

MaterialRenderer::MaterialGlInfo MaterialRenderer::GetGlInfo (unsigned int id) const
{
	MaterialGlInfo info;
	if (id >= m_materials.size ())
		return info;                     // emplacement inconnu : rien a echantillonner

	const Entry& entry = m_materials[id];
	if (entry.pMaterial == nullptr)
		return info;                     // emplacement libere

	info.hasTexture    = (entry.textureId != 0);
	info.hasReflection = (entry.reflTextureId != 0 && entry.reflAmount > 0.f);
	info.reflAmount    = entry.reflAmount;
	return info;
}

void MaterialRenderer::RemoveMaterial (Material *pMaterial)
{
	if (!pMaterial) return;

	const auto it = m_indexByMaterial.find (pMaterial);
	if (it == m_indexByMaterial.end ())
		return;                          // jamais enregistre : rien a faire

	const int index = it->second;
	m_indexByMaterial.erase (it);

	if (index < 0 || index >= (int)m_materials.size ())
		return;

	ReleaseEntry (m_materials[index]);
	m_freeSlots.push_back (index);

	CGRE_CHECK_GL ("MaterialRenderer::RemoveMaterial");
}

int MaterialRenderer::AddMaterial (Material *pMaterial)
{
	if (!pMaterial) return -1;

	const MaterialType type = pMaterial->GetType ();
	const std::string  name = pMaterial->GetName ();

	// Deja enregistre ? Recherche en O(1). L'adresse est desormais une cle fiable
	// : RemoveMaterial retire l'entree quand le maillage proprietaire disparait,
	// donc le registre ne contient plus de pointeur pendant.
	const auto it = m_indexByMaterial.find (pMaterial);
	if (it != m_indexByMaterial.end ())
	{
		Entry& existing = m_materials[it->second];

		// Garde-fou. Si l'identite ne correspond pas, c'est qu'un chemin de
		// suppression a ete oublie et que l'allocateur a recycle l'adresse : le
		// dire, puis reconstruire l'entree plutot que de rendre l'ancienne
		// texture pour un materiau qui n'a plus rien a voir.
		if (existing.type != type || existing.name != name)
		{
			cgre::Logf ("MaterialRenderer : adresse de materiau recyclee (« %s » "
			            "remplace « %s ») -- un RemoveMaterial a ete manque.",
			            name.c_str (), existing.name.c_str ());
			ReleaseEntry (existing);
		}
		else
			return it->second;
	}

	// Emplacement : on reutilise ceux liberes par RemoveMaterial avant d'agrandir.
	// Plus aucun plafond : les 256 entrees en dur faisaient basculer en bleu par
	// defaut, sans message, tous les maillages ouverts au-dela.
	int index;
	if (!m_freeSlots.empty ())
	{
		index = m_freeSlots.back ();
		m_freeSlots.pop_back ();
	}
	else
	{
		index = (int)m_materials.size ();
		m_materials.emplace_back ();
	}

	Entry& entry = m_materials[index];
	entry.pMaterial = pMaterial;
	entry.type      = type;
	entry.name      = name;
	m_indexByMaterial[pMaterial] = index;

	// Un MATERIAL_PBR est enregistre PAR SA PROJECTION : c'est elle qui porte
	// les canaux que le pipeline fixe sait consommer, et sa carte de couleur de
	// base -- partagee, pas dupliquee -- qui doit etre televersee ci-dessous.
	if (type == MATERIAL_PBR)
	{
		if (const MaterialPbr *pPbr = dynamic_cast<const MaterialPbr*> (pMaterial))
			entry.projection = cgpbr::toPhong (*pPbr);
	}

	Material *pEffective = entry.projection ? entry.projection.get () : pMaterial;

	if (pEffective->GetType () == MATERIAL_TEXTURE)
	{
		MaterialTexture *pMaterialTexture = dynamic_cast<MaterialTexture*> (pEffective);
		if (pMaterialTexture && pMaterialTexture->GetImage())
		{
			glGenTextures(1, &entry.textureId);
			glBindTexture(GL_TEXTURE_2D, entry.textureId);

			const bool mipmapped = UploadTextureImage (pMaterialTexture->GetImage ());
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			                mipmapped ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
		}

		// Carte de reflexion : second objet de texture. GL_CLAMP_TO_EDGE et non
		// GL_REPEAT -- une sphere map couvre [0,1]x[0,1] une seule fois, et un
		// repliement ferait apparaitre une couture sur la silhouette.
		if (pMaterialTexture && pMaterialTexture->GetReflectionImage())
		{
			glGenTextures(1, &entry.reflTextureId);
			glBindTexture(GL_TEXTURE_2D, entry.reflTextureId);
			const bool reflMipmapped = UploadTextureImage (pMaterialTexture->GetReflectionImage ());
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			                reflMipmapped ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);

			// Montant du reflet. Ni le 3DS ni le MTL ne le fournissent pour la
			// map, on le derive donc du NIVEAU SPECULAIRE du materiau : une
			// surface tres speculaire reflechit beaucoup, une surface mate
			// presque pas. C'est la seule grandeur du fichier qui exprime cette
			// idee, et cela evite une constante arbitraire. Le metal du fauteuil
			// a Ks = 0.5, donc la moitie de reflet ; son cuir noir Ks = 0.02,
			// donc un reflet negligeable.
			const float *ks = pMaterialTexture->GetSpecular ();
			float amount = (ks[0] + ks[1] + ks[2]) / 3.f;
			if (amount < 0.f) amount = 0.f;
			if (amount > 1.f) amount = 1.f;
			entry.reflAmount = amount;
		}
	}

	CGRE_CHECK_GL ("MaterialRenderer::AddMaterial");
	return index;
}

void MaterialRenderer::ActivateMaterial (unsigned int id)
{
	// Un emplacement libere (RemoveMaterial) a un pMaterial nul : un identifiant
	// devenu perime n'active donc plus rien -- l'appelant retombe sur le materiau
	// par defaut -- au lieu de lier la texture d'un materiau disparu.
	if (id >= m_materials.size () || m_materials[id].pMaterial == nullptr)
		return;

	const Entry& entry = m_materials[id];
	// La PROJECTION quand il y en a une (MATERIAL_PBR) : la cascade ci-dessous
	// est sans repli, elle ne doit donc voir que des types qu'elle traite.
	Material *pMaterial = entry.projection ? entry.projection.get () : entry.pMaterial;
	if (pMaterial->GetType () == MATERIAL_TEXTURE)
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, entry.textureId);

		// The texture environment is GL_MODULATE: the sampled texel is
		// multiplied by the lit surface colour. Drive that colour from the
		// material's diffuse/ambient/specular so the texture is tinted as
		// authored (e.g. a light rubber tread darkened by a grey diffuse).
		MaterialTexture *pTex = dynamic_cast<MaterialTexture*>(pMaterial);
		if (pTex)
		{
			glDisable(GL_COLOR_MATERIAL);
			glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT,  pTex->GetAmbient());
			glMaterialfv (GL_FRONT_AND_BACK, GL_DIFFUSE,  pTex->GetDiffuse());
			glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, pTex->GetSpecular());
			glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS, GlShininess (pTex->GetShininess()));
			// Lighting-off path: texel modulated by the current colour.
			glColor4fv (pTex->GetDiffuse());
		}
		BindReflectionUnit (entry.reflTextureId, entry.reflAmount);
	}
	else if (pMaterial->GetType () == MATERIAL_COLOR_ADV)
	{
		BindReflectionUnit (0, 0.f);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_COLOR_MATERIAL);
		MaterialColorExt *pMatColExt = dynamic_cast<MaterialColorExt*>(pMaterial);
		if (pMatColExt)
		{
			// For when lighting is ON (both faces — two-sided lighting)
			glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT, pMatColExt->m_fAmbient);
			glMaterialfv (GL_FRONT_AND_BACK, GL_DIFFUSE, pMatColExt->m_fDiffuse);
			glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, pMatColExt->m_fSpecular);
			glMaterialf (GL_FRONT_AND_BACK, GL_SHININESS, GlShininess (pMatColExt->m_fShininess[0]));
			glMaterialfv (GL_FRONT_AND_BACK, GL_EMISSION, pMatColExt->m_fEmission);

			// For when lighting is OFF
			glColor4fv(pMatColExt->m_fDiffuse);
		}
	}
	else if (pMaterial->GetType() == MATERIAL_COLOR)
	{
		BindReflectionUnit (0, 0.f);
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_COLOR_MATERIAL);
		glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
		MaterialColor* pMatCol = dynamic_cast<MaterialColor*>(pMaterial);
		if (pMatCol)
		{
			glColor4f(pMatCol->GetFloatRed(), pMatCol->GetFloatGreen(), pMatCol->GetFloatBlue(), pMatCol->GetFloatAlpha());
		}
	}
}

void MaterialRenderer::ActivateDefaultMaterial (void)
{
	// Meme preambule que le neutre : defaire ce qu'un materiau texture a pose.
	BindReflectionUnit (0, 0.f);
	glDisable (GL_TEXTURE_2D);
	glDisable (GL_COLOR_MATERIAL);

	// Le bleu historique de l'hote. Valeurs reprises telles quelles, pour que le
	// deplacement ne change rien a ce qui s'affiche.
	const GLfloat none[4]      = { 0.f, 0.f, 0.f, 1.f };
	const GLfloat diffuse[4]   = { 0.1f, 0.5f, 0.8f, 1.f };
	const GLfloat shininess    = 20.f;

	glColor3f (0.2f, 0.5f, 0.8f);
	glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT,   none);
	glMaterialfv (GL_FRONT_AND_BACK, GL_DIFFUSE,   diffuse);
	glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,  none);
	glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS, shininess);
	glMaterialfv (GL_FRONT_AND_BACK, GL_EMISSION,  none);
}

void MaterialRenderer::ActivateNeutralMaterial (void)
{
	// ⚠ L'ETAT DE TEXTURE DOIT ETRE DEFAIT, et c'est le point : SetMaterial ne
	// pose que des glMaterialfv. Si un ActivateMaterial precedent a laisse
	// GL_TEXTURE_2D actif -- ce que fait toute branche MATERIAL_TEXTURE -- la
	// texture continue de moduler la surface, et le mode « neutre » ne change
	// rien a l'ecran. Meme raisonnement pour COLOR_MATERIAL, qui ferait suivre
	// le dernier glColor, et pour l'unite de reflexion.
	//
	// Ce sont exactement les trois gestes de la branche MATERIAL_COLOR_ADV
	// d'ActivateMaterial : un materiau de couleur doit defaire ce qu'un materiau
	// texture a pose.
	BindReflectionUnit (0, 0.f);
	glDisable (GL_TEXTURE_2D);
	glDisable (GL_COLOR_MATERIAL);

	// WHITE_PLASTIC : un blanc casse mat, sans teinte propre, qui laisse lire la
	// forme et son ombrage sans rien raconter sur la matiere. C'est le rendu
	// « terre cuite » habituel des visualiseurs, et le seul de la bibliotheque
	// qui ne colore pas ce qu'il montre.
	SetMaterial (MaterialColorExt::WHITE_PLASTIC);
}

void MaterialRenderer::SetMaterial (MaterialColorExt::MaterialColorExtType eType)
{
	// PILE, et non `new` : la version d'origine allouait un MaterialColorExt a
	// chaque appel sans jamais le detruire. Appelee une fois par maillage et par
	// image, elle fuyait a la cadence du rendu.
	MaterialColorExt material;
	material.Init_From_Library (eType);
	MaterialColorExt *pMatColExt = &material;
 
	GLfloat mat[4];

	//glEnable (GL_COLOR_MATERIAL);
	glColorMaterial (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE | GL_SPECULAR | GL_SHININESS);
 
	// ambient
	/*
	mat[0] = material_parameters[10*index];
	mat[1] = material_parameters[10*index+1];
	mat[2] = material_parameters[10*index+2];
	mat[3] = 1.;
	*/
	glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT, pMatColExt->m_fAmbient);
  
	// diffuse
	/*
	mat[0] = material_parameters[10*index+3];
	mat[1] = material_parameters[10*index+4];
	mat[2] = material_parameters[10*index+5];
	*/
	glMaterialfv (GL_FRONT_AND_BACK, GL_DIFFUSE, pMatColExt->m_fDiffuse);
  
	// specular
	/*
	mat[0] = material_parameters[10*index+6];
	mat[1] = material_parameters[10*index+7];
	mat[2] = material_parameters[10*index+8];
	*/
	glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, pMatColExt->m_fSpecular);
  
	// shininess
	//mat[0] = 128 * material_parameters[10*index+9];
	glMaterialf (GL_FRONT_AND_BACK, GL_SHININESS, GlShininess (pMatColExt->m_fShininess[0]));
  
	glMaterialfv (GL_FRONT_AND_BACK, GL_EMISSION, pMatColExt->m_fEmission);
}



// Le chargeur de texture TGA autonome (LoadTexture + TGAImg) a ete retire :
// jamais appele, et place sous #ifndef WIN32 alors que cgre n'est construit
// qu'avec ENABLE_SINAIA, donc sous Windows -- il n'etait donc meme pas
// compile. cgimg a deja un decodeur TGA, atteint par Img::load().
