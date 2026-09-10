#pragma once

//
// Materiau METALLIC-ROUGHNESS, le modele de glTF 2.0.
//
// Cinq cartes, sept facteurs, un mode d'alpha : c'est la totalite de ce que le
// COEUR de la specification definit pour un materiau. Aucune extension n'est
// representee ici -- ni KHR_texture_transform, ni KHR_materials_specular, ni
// clearcoat : un fichier qui en porte perd ces attributs a la lecture.
//
// Ce type est un CONTENEUR de valeurs, pas un moteur de rendu. Sa projection
// vers le modele de Phong du depot vit dans material_convert.h, en exemplaire
// unique.
//

#include "material.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace cgpbr {

// L'espace colorimetrique d'une carte n'est pas une propriete de l'image, c'est
// une propriete de l'EMPLACEMENT ou elle est branchee : les cartes porteuses de
// couleur (base, emissive) sont en sRGB, les cartes porteuses de DONNEES
// (normale, metallique/rugosite, occlusion) sont lineaires. S'y tromper eclaire
// mal sans rien signaler.
enum class ColorSpace : std::uint8_t { srgb, linear };

enum class AlphaMode : std::uint8_t { opaque, mask, blend };

enum class MapSlot : std::uint8_t {
	base_color = 0, normal = 1, metallic_roughness = 2,
	occlusion = 3, emissive = 4, count = 5
};

struct TextureRef
{
	std::shared_ptr<Img> image;              // image NULLE = carte absente, seul test

	// Nom INDICATIF, destine a l'affichage et au diagnostic. Il n'est PAS un
	// identifiant : glTF n'impose l'unicite d'aucun `name`, et le repli de la
	// lecture est l'indice de l'image DANS SON MODELE -- « gltf_image_0 »
	// designe donc une image differente dans chaque fichier. Un backend qui
	// s'en servirait de cle de cache resservirait la texture du modele
	// precedent. L'identite d'une image, ici, c'est l'adresse pointee par
	// `image`.
	std::string          name;
	ColorSpace           colorSpace = ColorSpace::linear;

	// 0 ou 1 ; > 1 rabattu a 0 a la lecture.
	//
	// DESIGNE UN JEU DU MAILLAGE, PAS UN ATTRIBUT DU FICHIER :
	//   0 -> Mesh::GetTextureCoordinates ()
	//   1 -> Mesh::GetTextureCoordinates1 ()
	// et BuildPolygonRenderData emet les deux tableaux correspondants.
	//
	// La distinction n'est pas theorique : l'import glTF PERMUTE les deux jeux
	// quand la carte de couleur de base designe TEXCOORD_1, pour que le chemin
	// de rendu a fonction fixe -- qui ne lit que le jeu 0 -- texture le modele
	// avec la bonne parametrisation. Il recrit alors l'uvSet de toutes les
	// cartes. Un `uvSet` lu ici ne vaut donc pas forcement l'indice ecrit dans
	// le .gltf.
	//
	// ⚠ CE N'EST PAS NON PLUS une garantie que la donnee s'y trouve : le champ
	// designe un jeu DU MAILLAGE, rien de plus. Deux cas le rendent arbitraire :
	//   - `texCoord >= 2` dans le fichier est RABATTU A 0 a la lecture
	//     (mesh_io_gltf_pbr.cpp) alors que le TEXCOORD correspondant n'est
	//     jamais charge : `uvSet = 0` designe alors une AUTRE parametrisation ;
	//   - un maillage sans aucun jeu d'UV laisse `uvSet` a 0, qui ne designe
	//     rien d'echantillonnable.
	// Un consommateur reste donc tenu de verifier que le jeu vise est non vide.
	//
	// ⚠ LE MOTEUR DE RENDU cgre IGNORE CE CHAMP. Il ne televerse que le jeu 0
	// (`VertexBufferManager`) : la permutation ci-dessus garantit que la
	// COULEUR DE BASE tombe juste, et RIEN D'AUTRE. Une carte d'occlusion ou
	// d'emission laissee sur le jeu 1 est echantillonnee avec les UV du jeu 0,
	// en silence. Portee, verification et remedes : `debt_cgre.md`, entree
	// « Second jeu d'UV et tangentes ignores par le pipeline ».
	std::uint8_t         uvSet      = 0;
};

// VALEURS PAR DEFAUT DU FORMAT glTF 2.0, et non valeurs de rendu.
//
// La distinction est essentielle : un materiau glTF qui omet metallicFactor est
// METALLIQUE (1) et parfaitement RUGUEUX (1) -- c'est ce que dit la
// specification, donc c'est ce que doit rendre un conteneur fidele au fichier.
// cgre2::makeDefaultMaterial() choisit au contraire un gris 0,7 non metallique :
// c'est un defaut de RENDU, destine a ce qu'une scene sans materiau reste
// visible. Aligner l'un sur l'autre ferait mentir la lecture d'un fichier.
struct Factors
{
	float baseColor[4]      { 1.f, 1.f, 1.f, 1.f };
	float emissive[3]       { 0.f, 0.f, 0.f };
	float metallic          { 1.f };
	float roughness         { 1.f };
	float normalScale       { 1.f };
	float occlusionStrength { 1.f };
	float alphaCutoff       { 0.5f };
};

} // namespace cgpbr

class MaterialPbr : public Material
{
public:
	MaterialPbr () = default;

	// OBLIGATOIRE. MaterialPtr se COPIE en clonant (material.h) : sans
	// surcharge, la copie d'un Mesh porteur d'un MaterialPbr appellerait
	// Material::clone et TRANCHERAIT le sous-objet -- silencieusement, a la
	// premiere copie, et sans que la compilation dise quoi que ce soit.
	std::unique_ptr<Material> clone (void) const override
		{ return std::make_unique<MaterialPbr> (*this); }

	MaterialType GetType (void) const override;
	void         Dump    (void) override;

	const cgpbr::Factors& GetFactors  (void) const { return m_factors; }
	cgpbr::Factors&       EditFactors (void)       { return m_factors; }

	// Un emplacement hors bornes rend la reference VIDE partagee : la lecture
	// d'un slot invalide ne doit pas etre un comportement indefini.
	const cgpbr::TextureRef& GetMap (cgpbr::MapSlot s) const;
	void SetMap (cgpbr::MapSlot s, cgpbr::TextureRef ref);
	bool HasMap (cgpbr::MapSlot s) const;

	cgpbr::AlphaMode GetAlphaMode (void) const { return m_alphaMode; }
	void SetAlphaMode (cgpbr::AlphaMode m) { m_alphaMode = m; }
	bool IsDoubleSided (void) const { return m_doubleSided; }
	void SetDoubleSided (bool b) { m_doubleSided = b; }

private:
	cgpbr::Factors    m_factors {};
	cgpbr::TextureRef m_maps[static_cast<std::size_t> (cgpbr::MapSlot::count)] {};
	cgpbr::AlphaMode  m_alphaMode   { cgpbr::AlphaMode::opaque };
	bool              m_doubleSided { false };
};
