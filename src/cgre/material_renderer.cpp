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
//  Le noir, et pourquoi il doit etre POSE
// ---------------------------------------------------------------------------
// Les parametres de materiau sont un ETAT GLOBAL du contexte, pas une propriete
// de l'objet dessine : un canal qu'une branche d'ActivateMaterial ne pose pas
// garde la valeur du materiau precedent, et l'apparence depend alors de l'ordre
// de dessin. Une branche doit donc poser TOUS les canaux qu'elle utilise et
// remettre a neutre ceux que sa classe de materiau ne porte pas.
//
// Poser le noir n'est pas la meme chose qu'une valeur deliberement nulle : c'est
// dire que la classe n'a pas ce canal. MaterialTexture n'a pas d'emission,
// MaterialColor n'a ni speculaire, ni exposant, ni emission.
//
// Un materiau qui CHOISIT une ambiante, un speculaire ou une emission nuls ne
// passe donc pas par cette constante, meme si les bits sont les memes :
// ActivateDefaultMaterial garde son propre `none`, qui exprime un choix et non
// une absence. Fusionner les deux effacerait la distinction.
static constexpr GLfloat kNoChannel[4] = { 0.f, 0.f, 0.f, 1.f };

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
// TROIS ISSUES, et non deux : le televersement peut echouer, reussir sans
// mipmaps, ou reussir avec. Un booleen ne distingue pas les deux premieres --
// un appelant qui le lirait comme « reussi » garderait un objet de texture SANS
// NIVEAU 0, donc incomplet, et l'echantillonnerait en noir sans rien signaler.
//
// LA TAILLE EFFECTIVE VOYAGE AVEC L'ISSUE, et c'est pourquoi c'est une structure
// et non une enumeration : au-dela de GL_MAX_TEXTURE_SIZE, le televersement
// porte sur une COPIE REDUITE et l'image d'origine n'en garde aucune trace. Un
// appelant qui mesurerait la VRAM sur les dimensions de l'image rapporterait
// alors quatre fois trop par facteur deux de reduction.
struct UploadResult
{
	enum class Status { failed, uploaded, mipmapped };
	Status       status = Status::failed;
	unsigned int width  = 0;
	unsigned int height = 0;

	bool Failed    () const { return status == Status::failed; }
	bool Mipmapped () const { return status == Status::mipmapped; }
};

// La forme heritee de GL 1.0 : le nombre de composantes tient lieu de format
// interne, et le pilote choisit. C'est ce que passaient les appelants d'origine,
// et le defaut le reproduit a l'octet pres. Les cartes PBR passent un format
// EXPLICITE, parce que le decodage sRGB en depend.
static constexpr GLint kLegacyRgbaComponents = 4;

// POLITIQUE DU MODULE sur la disponibilite des points d'entree et des formats :
// on se garde de ce qui peut etre ABSENT a l'execution sans erreur -- un
// glGenerateMipmap nul sur un pilote qui ne l'expose pas -- et non de ce dont
// l'absence emporterait le module entier. GL_SRGB8_ALPHA8 est du second groupe :
// un pilote assez ancien pour le refuser n'aurait pas non plus glActiveTexture,
// et cgre serait tombe bien avant d'arriver ici.
static UploadResult UploadTextureImage (const Img *pImage,
                                        GLint internalFormat = kLegacyRgbaComponents)
{
	UploadResult out;
	if (!pImage || !pImage->data()) return out;

	GLint maxSize = 0;
	glGetIntegerv (GL_MAX_TEXTURE_SIZE, &maxSize);
	if (maxSize <= 0) maxSize = 2048;      // pilote muet : borne prudente

	const unsigned int w = pImage->width(), h = pImage->height();

	auto finish = [&out](unsigned int uw, unsigned int uh) -> UploadResult {
		out.width  = uw;
		out.height = uh;
		out.status = GenerateMipmaps () ? UploadResult::Status::mipmapped
		                                : UploadResult::Status::uploaded;
		return out;
	};

	if (w <= (unsigned int)maxSize && h <= (unsigned int)maxSize)
	{
		glTexImage2D (GL_TEXTURE_2D, 0, internalFormat, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pImage->data());
		return finish (w, h);
	}

	Img scaled (*pImage);                  // copie : ne pas toucher a l'original
	const unsigned int nw = (w > (unsigned int)maxSize) ? (unsigned int)maxSize : w;
	const unsigned int nh = (h > (unsigned int)maxSize) ? (unsigned int)maxSize : h;
	if (scaled.resize (nw, nh, 1) != 0)    // mode 1 : bilineaire
	{
		cgre::Logf ("MaterialRenderer : texture %ux%u au-dela de GL_MAX_TEXTURE_SIZE=%d, "
		            "reduction impossible.", w, h, (int)maxSize);
		return out;                        // out.status vaut encore `failed`
	}
	cgre::Logf ("MaterialRenderer : texture %ux%u reduite a %ux%u (GL_MAX_TEXTURE_SIZE=%d).",
	            w, h, nw, nh, (int)maxSize);
	glTexImage2D (GL_TEXTURE_2D, 0, internalFormat, nw, nh, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled.data());
	CGRE_CHECK_GL ("MaterialRenderer::UploadTextureImage (reduite)");
	// LES DIMENSIONS RAPPORTEES SONT CELLES DE LA COPIE, pas celles de l'image.
	return finish (nw, nh);
}

// ---------------------------------------------------------------------------
//  Objets de texture partages
// ---------------------------------------------------------------------------
// UN OBJET PAR (IMAGE, FORMAT INTERNE, HABILLAGE), quel que soit le nombre de
// materiaux qui s'en servent. Les trois composantes de la cle sont un etat de
// l'OBJET : deux usages qui divergent sur l'une d'elles ont besoin de deux
// objets, et les confondre serait un defaut d'image, pas une economie.
//
// LE COMPTE DE REFERENCES EST LA RAISON D'ETRE DE LA TABLE. Un materiau retire
// rend sa reference ; seul le dernier detruit l'objet. Sans lui, retirer l'un
// des trois maillages de Lantern.glb -- qui naissent d'un seul materiau glTF et
// partagent donc leurs quatre cartes -- detruirait les textures des deux autres.
GLuint MaterialRenderer::AcquireTexture (std::shared_ptr<const Img> image,
                                         GLint internalFormat, GLint wrap,
                                         TextureKey& outKey)
{
	outKey = TextureKey ();
	if (!image || !image->data ())
		return 0;

	const TextureKey key { image.get (), internalFormat, wrap };

	const auto hit = m_sharedTextures.find (key);
	if (hit != m_sharedTextures.end ())
	{
		++hit->second.refs;
		outKey = key;
		return hit->second.id;
	}

	// NEUTRALITE DE LIAISON, ET C'EST ICI QU'ELLE APPARTIENT. Televerser exige de
	// lier ; or ce chemin ne lie QUE sur defaut de cache. Laisser la restitution
	// a l'appelant ferait donc dependre l'etat GL apres AddMaterial de la
	// question « cette image avait-elle deja ete televersee ? », qui n'a rien a
	// voir avec l'etat. Un seul endroit couvre les trois sites de televersement.
	//
	// L'unite est POSEE et non supposee, puis restituee : une remise a 0 serait
	// deja un changement d'etat pour un appelant qui avait active autre chose.
	GLint previousUnit = GL_TEXTURE0;
	glGetIntegerv (GL_ACTIVE_TEXTURE, &previousUnit);
	glActiveTexture (GL_TEXTURE0);

	GLint previousBinding = 0;
	glGetIntegerv (GL_TEXTURE_BINDING_2D, &previousBinding);

	auto restore = [previousUnit, previousBinding]() {
		glBindTexture (GL_TEXTURE_2D, (GLuint)previousBinding);
		glActiveTexture ((GLenum)previousUnit);
	};

	GLuint id = 0;
	glGenTextures (1, &id);
	// ZERO N'EST PAS UN NOM : c'est l'objet de texture PAR DEFAUT. L'inscrire
	// dans la table y ferait converger toute image dont la generation n'a rien
	// alloue, et plusieurs materiaux partageraient alors le meme objet par
	// accident. Le refus est une regle de la table.
	if (id == 0)
	{
		restore ();
		return 0;
	}

	glBindTexture (GL_TEXTURE_2D, id);
	const UploadResult up = UploadTextureImage (image.get (), internalFormat);

	// Un objet sans niveau 0 est INCOMPLET : il s'echantillonnerait en noir sans
	// rien signaler. On le detruit plutot que de l'inscrire dans la table, et
	// aucune reference n'est comptee.
	if (up.Failed ())
	{
		glDeleteTextures (1, &id);
		restore ();
		return 0;
	}

	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
	                 up.Mipmapped () ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

	SharedTexture& slot = m_sharedTextures[key];
	slot.id        = id;
	slot.refs      = 1;
	// L'IMAGE EST RETENUE, et c'est ce qui rend la cle non recyclable : tant
	// qu'un objet la reference, l'adresse ne peut pas etre reattribuee a une
	// autre Img. Sans cette prise, un RemoveMaterial manque -- cas que
	// AddMaterial admet et journalise -- laisserait une cle perimee, et une Img
	// reallouee au meme endroit obtiendrait la texture de l'ancien modele.
	slot.image     = std::move (image);
	// LES DIMENSIONS SONT CELLES DU TELEVERSEMENT, pas celles de l'image : au
	// dela de GL_MAX_TEXTURE_SIZE elles different, et c'est la VRAM qu'on mesure.
	slot.width     = up.width;
	slot.height    = up.height;
	slot.mipmapped = up.Mipmapped ();

	restore ();
	outKey = key;
	return id;
}

void MaterialRenderer::ReleaseTexture (TextureKey& key)
{
	if (key.Empty ())
		return;

	const auto hit = m_sharedTextures.find (key);
	key = TextureKey ();
	if (hit == m_sharedTextures.end ())
		return;

	if (hit->second.refs > 1)
	{
		--hit->second.refs;
		return;
	}

	// DERNIER USAGER.
	//
	// AUCUNE GARDE CONTRE L'ABSENCE DE CONTEXTE N'EST NECESSAIRE, et ce n'est pas
	// un oubli : une cle n'entre dans la table que par un AcquireTexture dont le
	// televersement a REUSSI, ce qui suppose un contexte courant. Le registre de
	// materiaux s'exerce sans contexte (tu_cgre_material_renderer.cpp) et n'y
	// inscrit donc rien -- il n'atteint jamais cette ligne.
	//
	// Le test sur le nom reste, parce que zero designe la texture par defaut et
	// non un objet a detruire.
	if (hit->second.id != 0)
		glDeleteTextures (1, &hit->second.id);
	m_sharedTextures.erase (hit);
}

MaterialRenderer::TextureStats MaterialRenderer::GetTextureStats () const
{
	TextureStats st;
	for (const auto& kv : m_sharedTextures)
	{
		const SharedTexture& t = kv.second;
		++st.objects;
		st.uses += t.refs;

		std::uint64_t bytes = 4ull * t.width * t.height;
		// La chaine complete de mipmaps ajoute un tiers du niveau 0 : la somme
		// de 1/4^k converge vers 4/3.
		if (t.mipmapped)
			bytes = (bytes * 4ull) / 3ull;
		st.bytes += bytes;
	}
	return st;
}

// ---------------------------------------------------------------------------
//  Televersement des cinq cartes d'un MaterialPbr
// ---------------------------------------------------------------------------
// CETTE FONCTION NE LIE RIEN POUR LE DESSIN : elle acquiert des objets, et c'est
// VertexBufferManager::BindPbrMaps qui les lie aux unites 2 a 6 au moment du
// rendu. Les emplacements et le masque de presence sont tout ce qu'elle laisse.
//
// LA NEUTRALITE D'ETAT EST CELLE D'AcquireTexture, qui releve et restitue l'unite
// active et la liaison de l'unite 0. Elle n'est donc plus a refaire ici, et la
// refaire masquerait le seul endroit qui en repond.
void MaterialRenderer::UploadPbrMaps (Entry& entry, const MaterialPbr& src)
{
	entry.pbrFactors = src.GetFactors ();
	// Ce que toPhong jette et que le chemin d'echantillonnage reclamera. Capture
	// ici pour la meme raison que les facteurs : le materiau d'origine peut
	// disparaitre, et son pointeur n'est jamais relu.
	entry.alphaMode   = src.GetAlphaMode ();
	entry.doubleSided = src.IsDoubleSided ();

	for (std::size_t i = 0; i < entry.mapTex.size (); ++i)
	{
		const cgpbr::TextureRef& ref = src.GetMap (static_cast<cgpbr::MapSlot> (i));
		entry.mapUvSet[i] = ref.uvSet;

		// Image nulle = carte absente : c'est le SEUL test, le nom n'est pas un
		// identifiant (cf. TextureRef).
		if (!ref.image)
			continue;

		// L'espace colorimetrique est une propriete de l'EMPLACEMENT, portee par
		// la donnee lue : on ne la rededuit pas de l'indice.
		const GLint internalFormat =
			(ref.colorSpace == cgpbr::ColorSpace::srgb) ? GL_SRGB8_ALPHA8 : GL_RGBA8;

		// GL_REPEAT : c'est le mode d'habillage par defaut du format glTF, et
		// cgre ne lit pas encore le `sampler` du fichier.
		const GLuint id = AcquireTexture (ref.image, internalFormat, GL_REPEAT,
		                                  entry.mapKey[i]);
		// Un nom nul couvre les trois refus d'AcquireTexture -- pas de contexte,
		// image sans pixels, televersement incomplet. Le masque ne le porte
		// pas : un echantillonneur teste une presence, il ne devine pas.
		if (id == 0)
			continue;

		entry.mapTex[i] = id;
		entry.mapMask  |= static_cast<std::uint8_t> (1u << i);
	}

	CGRE_CHECK_GL ("MaterialRenderer::UploadPbrMaps");
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
	// L'ENTREE NE POSSEDE AUCUN OBJET DE TEXTURE : elle en detient des
	// references. Chaque cle rendue decremente un compte, et seul le dernier
	// usager appelle glDeleteTextures. Detruire ici sans passer par la table
	// arracherait a ses voisins les cartes qu'un materiau partage avec eux.
	//
	// Les cartes PBR sont rendues au meme titre que les deux precedentes : les
	// omettre ferait fuir jusqu'a cinq textures par materiau retire.
	//
	// Une cle vide ne trouve rien et ne declenche aucun appel GL : c'est ce qui
	// permet au registre de s'exercer SANS CONTEXTE (tu_cgre_material_renderer),
	// ou aucune cle n'a jamais pu etre acquise.
	ReleaseTexture (entry.textureKey);
	ReleaseTexture (entry.reflKey);
	for (TextureKey& key : entry.mapKey)
		ReleaseTexture (key);

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

MaterialRenderer::MaterialPbrInfo MaterialRenderer::GetPbrInfo (unsigned int id) const
{
	MaterialPbrInfo info;
	if (id >= m_materials.size ())
		return info;                     // emplacement inconnu

	const Entry& entry = m_materials[id];
	if (entry.pMaterial == nullptr || entry.type != MATERIAL_PBR)
		return info;                     // emplacement libere, ou materiau non PBR

	info.isPbr     = true;
	info.factors   = entry.pbrFactors;
	info.alphaMode = entry.alphaMode;
	for (std::size_t i = 0; i < entry.mapTex.size (); ++i)
	{
		info.mapTex[i] = entry.mapTex[i];
		info.uvSet[i]  = entry.mapUvSet[i];
		// Le MASQUE fait foi, pas l'identifiant : un televersement echoue laisse
		// l'emplacement a zero, et un zero est le nom de la texture par defaut.
		info.hasMap[i] = (entry.mapMask & (1u << i)) != 0;
	}
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
		{
			entry.projection = cgpbr::toPhong (*pPbr);
			// L'ordre d'appel est indifferent : UploadPbrMaps releve et restitue
			// la liaison de l'unite 0, elle est neutre quoi qu'il suive.
			UploadPbrMaps (entry, *pPbr);
		}
	}

	Material *pEffective = entry.projection ? entry.projection.get () : pMaterial;

	if (pEffective->GetType () == MATERIAL_TEXTURE)
	{
		MaterialTexture *pMaterialTexture = dynamic_cast<MaterialTexture*> (pEffective);
		if (pMaterialTexture && pMaterialTexture->GetImage())
		{
			// FORMAT HERITE, et c'est ce qui le distingue de la carte de couleur
			// de base du chemin PBR : les deux portent la MEME image, le partage
			// s'arrete au format interne (cf. Entry::mapTex).
			entry.textureId = AcquireTexture (pMaterialTexture->ShareImage (),
			                                  kLegacyRgbaComponents, GL_REPEAT,
			                                  entry.textureKey);
		}

		// Carte de reflexion : second objet de texture. GL_CLAMP_TO_EDGE et non
		// GL_REPEAT -- une sphere map couvre [0,1]x[0,1] une seule fois, et un
		// repliement ferait apparaitre une couture sur la silhouette.
		if (pMaterialTexture && pMaterialTexture->GetReflectionImage())
		{
			entry.reflTextureId = AcquireTexture (pMaterialTexture->ShareReflectionImage (),
			                                      kLegacyRgbaComponents, GL_CLAMP_TO_EDGE,
			                                      entry.reflKey);

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
		// Preambule d'etat : il ne depend d'aucun dynamic_cast. Un etat que l'on
		// defait ne doit pas rester pose parce qu'une conversion a echoue.
		glDisable(GL_COLOR_MATERIAL);

		MaterialTexture *pTex = dynamic_cast<MaterialTexture*>(pMaterial);
		if (pTex)
		{
			glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT,  pTex->GetAmbient());
			glMaterialfv (GL_FRONT_AND_BACK, GL_DIFFUSE,  pTex->GetDiffuse());
			glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, pTex->GetSpecular());
			glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS, GlShininess (pTex->GetShininess()));
			// MaterialTexture ne porte pas d'emission : on pose le noir pour ne
			// pas heriter de celle du materiau precedent.
			glMaterialfv (GL_FRONT_AND_BACK, GL_EMISSION, kNoChannel);
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

		// MaterialColor ne porte qu'une couleur, et les trois canaux qu'il n'a
		// pas sont remis a neutre : sans quoi un materiau de couleur dessine
		// apres un materiau brillant ou emissif en heriterait.
		glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,  kNoChannel);
		glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS, 0.f);
		glMaterialfv (GL_FRONT_AND_BACK, GL_EMISSION,  kNoChannel);

		MaterialColor* pMatCol = dynamic_cast<MaterialColor*>(pMaterial);
		if (pMatCol)
		{
			const GLfloat color[4] = { pMatCol->GetFloatRed(),  pMatCol->GetFloatGreen(),
			                           pMatCol->GetFloatBlue(), pMatCol->GetFloatAlpha() };

			// LA COULEUR EST POSEE DEUX FOIS, et les deux sont necessaires.
			//
			// glColor avec GL_COLOR_MATERIAL suffit au pipeline fixe. Il ne
			// suffit pas sous programme lie : GL_COLOR_MATERIAL n'y existe plus
			// et le fragment lit gl_FrontMaterial.diffuse. Le programme de
			// surface etant le chemin par defaut, une branche qui ne poserait
			// que glColor rendrait toutes ses plages de la couleur laissee par
			// ActivateDefaultMaterial -- une seule teinte pour tout le maillage.
			glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT, color);
			glMaterialfv (GL_FRONT_AND_BACK, GL_DIFFUSE, color);
			glColor4fv (color);
		}
	}
	else
	{
		// LA CASCADE EST FERMEE PAR CONSTRUCTION, et non par un inventaire des
		// types atteignables : un type qu'aucune branche ne traite laisserait
		// fuir la grille entiere, texture liee comprise. Le materiau par defaut
		// pose ses cinq canaux et defait l'etat de texture.
		static bool reported = false;
		if (!reported)
		{
			reported = true;
			cgre::Log ("Type de materiau non traite par ActivateMaterial : "
			           "materiau par defaut applique.");
		}
		ActivateDefaultMaterial ();
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
 
	// Aucun glColorMaterial ici : le materiau neutre pose tous ses canaux par
	// glMaterialfv, et ses appelants desactivent GL_COLOR_MATERIAL. Le parametre
	// `mode` de glColorMaterial est une ENUMERATION, pas un masque : une
	// disjonction de plusieurs canaux ne designe aucun mode valide.
	glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT,   pMatColExt->m_fAmbient);
	glMaterialfv (GL_FRONT_AND_BACK, GL_DIFFUSE,   pMatColExt->m_fDiffuse);
	glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR,  pMatColExt->m_fSpecular);
	glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS, GlShininess (pMatColExt->m_fShininess[0]));
	glMaterialfv (GL_FRONT_AND_BACK, GL_EMISSION,  pMatColExt->m_fEmission);

	// Couleur courante, pour tout chemin qui lit la couleur plutot que le
	// materiau : c'est le geste que font les branches MATERIAL_COLOR_ADV et
	// MATERIAL_TEXTURE d'ActivateMaterial.
	//
	// CELA CHANGE LE RENDU, et c'est voulu. mesh_renderer reactive
	// GL_COLOR_MATERIAL juste apres ce point en mode couleurs par sommet. Ses
	// branches TRIANGLE reecrivent la couleur a chaque sommet, donc rien n'y
	// change ; ses branches QUAD et POLYGONE n'emettent aucun glColor et
	// prenaient la couleur laissee par le materiau precedent -- un resultat
	// dependant de l'ordre de dessin. Elles prennent desormais celle du materiau
	// neutre : ces primitives changent d'apparence, et deviennent deterministes.
	glColor4fv (pMatColExt->m_fDiffuse);
}



// Le chargeur de texture TGA autonome (LoadTexture + TGAImg) a ete retire :
// jamais appele, et place sous #ifndef WIN32 alors que cgre n'est construit
// qu'avec ENABLE_SINAIA, donc sous Windows -- il n'etait donc meme pas
// compile. cgimg a deja un decodeur TGA, atteint par Img::load().
