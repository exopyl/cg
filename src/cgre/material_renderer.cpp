#include "material_renderer.h"

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
static void UploadTextureImage (Img *pImage)
{
	if (!pImage || !pImage->data()) return;

	GLint maxSize = 0;
	glGetIntegerv (GL_MAX_TEXTURE_SIZE, &maxSize);
	if (maxSize <= 0) maxSize = 2048;      // pilote muet : borne prudente

	const unsigned int w = pImage->width(), h = pImage->height();
	if (w <= (unsigned int)maxSize && h <= (unsigned int)maxSize)
	{
		glTexImage2D (GL_TEXTURE_2D, 0, 4, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pImage->data());
		return;
	}

	Img scaled (*pImage);                  // copie : ne pas toucher a l'original
	const unsigned int nw = (w > (unsigned int)maxSize) ? (unsigned int)maxSize : w;
	const unsigned int nh = (h > (unsigned int)maxSize) ? (unsigned int)maxSize : h;
	if (scaled.resize (nw, nh, 1) != 0)    // mode 1 : bilineaire
	{
		printf ("MaterialRenderer: texture %ux%u au-dela de GL_MAX_TEXTURE_SIZE=%d, "
		        "reduction impossible\n", w, h, (int)maxSize);
		return;
	}
	printf ("MaterialRenderer: texture %ux%u reduite a %ux%u (GL_MAX_TEXTURE_SIZE=%d)\n",
	        w, h, nw, nh, (int)maxSize);
	glTexImage2D (GL_TEXTURE_2D, 0, 4, nw, nh, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled.data());
}

MaterialRenderer::MaterialRenderer()
{
	m_nMaterials = 0;
	for (unsigned int i = 0; i < 256; i++)
	{
		m_pReflTexturesId[i] = 0;
		m_pReflAmount[i]     = 0.f;
	}
}

MaterialRenderer::~MaterialRenderer()
{
}

int MaterialRenderer::AddMaterial (Material *pMaterial)
{
	if (!pMaterial) return -1;

	// Search if already added
	for (unsigned int i = 0; i < m_nMaterials; i++)
	{
		if (m_pMaterials[i] == pMaterial)
			return (int)i;
	}

	if (m_nMaterials >= 256)
		return - 1;

	m_pMaterials[m_nMaterials] = pMaterial;

	if (pMaterial->GetType () == MATERIAL_TEXTURE)
	{
		MaterialTexture *pMaterialTexture = dynamic_cast<MaterialTexture*> (pMaterial);
		if (pMaterialTexture && pMaterialTexture->GetImage())
		{
			glGenTextures(1, &m_pTexturesId[m_nMaterials]);
			glBindTexture(GL_TEXTURE_2D, m_pTexturesId[m_nMaterials]);

			UploadTextureImage (pMaterialTexture->GetImage ());
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
		}

		// Carte de reflexion : second objet de texture. GL_CLAMP_TO_EDGE et non
		// GL_REPEAT -- une sphere map couvre [0,1]x[0,1] une seule fois, et un
		// repliement ferait apparaitre une couture sur la silhouette.
		if (pMaterialTexture && pMaterialTexture->GetReflectionImage())
		{
			glGenTextures(1, &m_pReflTexturesId[m_nMaterials]);
			glBindTexture(GL_TEXTURE_2D, m_pReflTexturesId[m_nMaterials]);
			UploadTextureImage (pMaterialTexture->GetReflectionImage ());
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
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
			m_pReflAmount[m_nMaterials] = amount;
		}
	}
	m_nMaterials++;
	return m_nMaterials-1;
}

void MaterialRenderer::ActivateMaterial (unsigned int id)
{
	if (id >= m_nMaterials || m_pMaterials[id] == nullptr)
		return;

	Material *pMaterial = m_pMaterials[id];
	if (pMaterial->GetType () == MATERIAL_TEXTURE)
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, m_pTexturesId[id]);

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
			glMaterialf  (GL_FRONT_AND_BACK, GL_SHININESS, 128.f * pTex->GetShininess());
			// Lighting-off path: texel modulated by the current colour.
			glColor4fv (pTex->GetDiffuse());
		}
		BindReflectionUnit (m_pReflTexturesId[id], m_pReflAmount[id]);
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
			glMaterialf (GL_FRONT_AND_BACK, GL_SHININESS, 128. * pMatColExt->m_fShininess[0]);
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

void MaterialRenderer::SetMaterial (MaterialColorExt::MaterialColorExtType eType)
{
	MaterialColorExt *pMaterial = new MaterialColorExt ();
	pMaterial->Init_From_Library (eType);

	MaterialColorExt *pMatColExt = dynamic_cast<MaterialColorExt*>(pMaterial);
 
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
	glMaterialfv (GL_FRONT, GL_AMBIENT, pMatColExt->m_fAmbient);
  
	// diffuse
	/*
	mat[0] = material_parameters[10*index+3];
	mat[1] = material_parameters[10*index+4];
	mat[2] = material_parameters[10*index+5];
	*/
	glMaterialfv (GL_FRONT, GL_DIFFUSE, pMatColExt->m_fDiffuse);
  
	// specular
	/*
	mat[0] = material_parameters[10*index+6];
	mat[1] = material_parameters[10*index+7];
	mat[2] = material_parameters[10*index+8];
	*/
	glMaterialfv (GL_FRONT, GL_SPECULAR, pMatColExt->m_fSpecular);
  
	// shininess
	//mat[0] = 128 * material_parameters[10*index+9];
	glMaterialf (GL_FRONT, GL_SHININESS, 128. * pMatColExt->m_fShininess[0]);
  
	glMaterialfv (GL_FRONT, GL_EMISSION, pMatColExt->m_fEmission);

	delete pMaterial;
}



#ifndef WIN32
#include "cgmesh/cgmesh.h"
#include "cgimg/cgimg.h"
#include "cgimg/image_tga.h"

// Load a TGA texture
GLuint LoadTexture(char *TexName)
{
	TGAImg Img;
	GLuint Texture;
	
	// Load our Texture
	if(Img.Load(TexName)!=IMG_OK)
		return -1;
	
	glGenTextures(1,&Texture);
	glBindTexture(GL_TEXTURE_2D,Texture);
	
	// Create the texture
	if(Img.GetBPP()==24)
		glTexImage2D(GL_TEXTURE_2D,0,3,
			     Img.GetWidth(),Img.GetHeight(),
			     0,GL_RGB,GL_UNSIGNED_BYTE,
			     Img.GetImg());
	else if(Img.GetBPP()==32)
		glTexImage2D(GL_TEXTURE_2D,0,4,
			     Img.GetWidth(),Img.GetHeight(),
			     0,GL_RGBA,GL_UNSIGNED_BYTE,
			     Img.GetImg());
	else
		return -1;
	
	// Specify filtering and edge actions
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
	
	return Texture;
}

#endif
