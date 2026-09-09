#pragma once

#include "gl_wrapper.h"

#include "../cgmesh/cgmesh.h"

#include <string>
#include <unordered_map>
#include <vector>

class MaterialRenderer
{
private:
	MaterialRenderer();
	~MaterialRenderer ();
public:
	static MaterialRenderer* getInstance (void) { return m_pInstance; };

	int AddMaterial (Material *pMaterial);

	// RETRAIT D'UN MATERIAU : libere ses objets de texture et rend son emplacement.
	//
	// Sans lui, le registre ne faisait que croitre : les entrees n'etaient jamais
	// retirees, donc (a) les textures fuyaient pour toute la duree du processus,
	// (b) le plafond de 256 finissait par etre atteint et tous les maillages
	// ouverts ensuite passaient en bleu par defaut sans un mot, et (c) les
	// Material* conserves devenaient pendants, si bien qu'un materiau neuf ne a
	// l'adresse recyclee d'un ancien se voyait rendre l'ANCIEN indice, donc
	// l'ancienne texture.
	//
	// Appele par MeshRenderer::RemoveMesh pour chaque materiau du maillage
	// retire. C'est correct sans comptage de references parce qu'un Material
	// appartient a UN maillage et a un seul : Mesh::m_materials est un
	// vector<unique_ptr<Material>>, il n'y a pas de partage entre maillages.
	void RemoveMaterial (Material *pMaterial);

	static void SetMaterial (MaterialColorExt::MaterialColorExtType eType);

	// LE MATERIAU NEUTRE, et il n'y en a qu'un dans tout le moteur.
	//
	// Il sert a DEUX endroits qu'il serait faux de laisser diverger : le mode
	// d'ombrage « Neutre », et le maillage qui ne porte AUCUN materiau. Ce
	// second cas ne liait rien du tout jusqu'ici -- l'etat GL courant, herite du
	// maillage precedent, s'appliquait -- donc l'apparence « par defaut » etait
	// un residu et non un choix. Elle en est un maintenant.
	static void ActivateNeutralMaterial (void);

	// LE MATERIAU PAR DEFAUT : celui d'un maillage qui ne porte AUCUN materiau.
	//
	// Il vivait en dur dans le reglage du contexte GL de l'hote, pose une seule
	// fois au demarrage. Cela tenait tant que rien ne le remplacait -- mais le
	// premier materiau active, texture ou non, l'ecrasait pour de bon, et la
	// couleur d'un maillage sans materiau dependait alors de ce qui avait ete
	// dessine avant lui.
	//
	// Il est ici pour n'exister qu'a UN endroit, et il est repose a chaque
	// maillage qui en a besoin plutot qu'une fois pour toutes.
	static void ActivateDefaultMaterial (void);
	void ActivateMaterial (unsigned int id);

	// CE QUE LE SHADER DOIT SAVOIR d'un materiau, et qu'il ne peut plus deduire
	// de l'etat GL.
	//
	// Sous programme lie, glEnable(GL_TEXTURE_2D) n'a plus d'effet et aucune
	// fonction GLSL ne dit si une unite de texture porte quelque chose : le
	// fragment doit se le faire dire par un uniforme. ActivateMaterial lie
	// toujours les objets de texture aux unites 0 et 1 -- c'est seulement la
	// DECISION d'echantillonner qui remonte ici.
	struct MaterialGlInfo
	{
		bool  hasTexture    = false;   // unite 0
		bool  hasReflection = false;   // unite 1
		float reflAmount    = 0.f;
	};
	MaterialGlInfo GetGlInfo (unsigned int id) const;

private:
	static MaterialRenderer *m_pInstance;

	// UN materiau enregistre. Remplace cinq tableaux paralleles de 256 entrees,
	// dont le plafond faisait basculer en bleu par defaut, sans message, tous les
	// maillages ouverts au-dela du 256e materiau de la session.
	struct Entry
	{
		Material *pMaterial = nullptr;      // nullptr : emplacement libre

		// Identite capturee a l'insertion, JAMAIS relue par le pointeur.
		// Sert de garde-fou : si RemoveMaterial venait a etre oublie sur un
		// chemin, un materiau neuf ne a une adresse recyclee serait detecte ici
		// au lieu de se voir rendre l'ancienne texture en silence.
		MaterialType type = MATERIAL_NONE;
		std::string  name;

		GLuint textureId     = 0;
		// Carte de reflexion, 0 si aucune. Objet GL distinct : un materiau peut
		// porter les deux cartes, liees a deux unites de texture.
		GLuint reflTextureId = 0;
		// Taux de reflexion, dans [0,1] : proportion du reflet dans la couleur
		// finale. Ni le 3DS ni le MTL ne fournissent de montant pour la map, on
		// le derive donc du niveau speculaire du materiau (cf. ActivateMaterial).
		float  reflAmount    = 0.f;
	};

	// Libere les objets GL d'une entree et la remet a l'etat libre.
	void ReleaseEntry (Entry& entry);

	std::vector<Entry> m_materials;

	// Deduplication en O(1). Les emplacements liberes sont reutilises : c'est sur
	// parce que le seul detenteur d'indices est rendering_element_s::materialCache,
	// recalcule a chaque changement de revision du maillage, et vide pour un
	// maillage retire (GetMaterialRendererIds rend un vecteur vide des que
	// pMesh est nul).
	std::unordered_map<const Material*, int> m_indexByMaterial;
	std::vector<int> m_freeSlots;
};

// LoadTexture(char*) a ete retiree avec le decodeur TGAImg qu'elle etait seule a
// utiliser : jamais appelee, et compilee sous #ifndef WIN32 alors que cgre n'est
// construit qu'avec ENABLE_SINAIA, donc sous Windows. Pour charger une texture,
// passer par Img::load(), qui dispatche sur l'extension (TGA compris).
