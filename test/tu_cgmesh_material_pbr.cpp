// ===========================================================================
//  MaterialPbr et la projection vers Phong
// ===========================================================================
//
// Cinq proprietes que rien d'autre ne rattraperait :
//
//   1. la COPIE POLYMORPHE. MaterialPtr se copie en clonant ; un clone() oublie
//      ne produit ni erreur de compilation ni avertissement, seulement un
//      materiau tranche en Material de base a la premiere copie de Mesh ;
//   2. la PROJECTION PBR -> Phong, et en particulier le fait que sa brillance
//      est une FRACTION dans [0,1] -- le rendu la multiplie par 128 ;
//   3. le TYPE RENDU par la projection : MaterialTexture quand la carte de
//      couleur de base est la, MaterialColorExt sinon. Rendre inconditionnellement
//      un MaterialColorExt perdrait l'image, sans un mot ;
//   4. l'ESPACE COLORIMETRIQUE. MaterialPbr est lineaire (glTF), les materiaux
//      Phong du depot sont en sRGB : la projection franchit la frontiere ;
//   5. l'INVERSIBILITE des courbes sRGB, y compris AU GENOU, ou les deux seuils
//      usuels de la litterature ne sont pas images l'un de l'autre.
//
#include <gtest/gtest.h>

#include <memory>
#include <type_traits>

#include "../src/cgmesh/material.h"
#include "../src/cgmesh/material_convert.h"
#include "../src/cgmesh/material_pbr.h"

namespace {

MaterialPbr makePbr (float r, float g, float b, float metallic, float roughness)
{
	MaterialPbr m;
	m.EditFactors().baseColor[0] = r;
	m.EditFactors().baseColor[1] = g;
	m.EditFactors().baseColor[2] = b;
	m.EditFactors().baseColor[3] = 1.f;
	m.EditFactors().metallic     = metallic;
	m.EditFactors().roughness    = roughness;
	return m;
}

// La valeur sRGB attendue pour un canal calcule en LINEAIRE. Ecrite via la
// courbe de production, elle-meme eprouvee plus bas sur son inversibilite et
// sur son sens : ce que ces tests verifient ici, c'est la FORMULE de la
// projection et le fait que la conversion a bien lieu, pas la courbe.
float srgbOf (double linear)
{
	return (float) cgpbr::linearToSrgb (linear);
}

// La projection, sous la forme concrete attendue. Rend nullptr si le type rendu
// n'est pas celui-la -- ce que les ASSERT appelants transforment en echec.
const MaterialColorExt* asColorExt (const std::unique_ptr<Material>& m)
{
	return dynamic_cast<const MaterialColorExt*> (m.get());
}

} // namespace

// ---------------------------------------------------------------------------
//  1. Copie polymorphe
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_material_pbr, copying_a_material_ptr_keeps_the_concrete_type)
{
	MaterialPtr p { new MaterialPbr };
	p->SetName ("pbr");

	MaterialPtr q = p;

	ASSERT_TRUE((bool) q);
	EXPECT_EQ(q->GetType(), MATERIAL_PBR) << "clone() manquant : le sous-objet a ete tranche";
	EXPECT_NE(q.get(), p.get()) << "la copie doit etre profonde, pas un partage";
	EXPECT_EQ(q->GetName(), "pbr");
}

TEST(TEST_cgmesh_material_pbr, cloning_carries_factors_and_maps)
{
	MaterialPtr p { new MaterialPbr };
	MaterialPbr* src = dynamic_cast<MaterialPbr*> (p.get());
	ASSERT_NE(src, nullptr);
	src->EditFactors().metallic  = 0.25f;
	src->EditFactors().roughness = 0.75f;
	src->SetAlphaMode (cgpbr::AlphaMode::mask);
	src->SetDoubleSided (true);

	cgpbr::TextureRef ref;
	ref.image      = std::make_shared<Img> (2u, 2u, false);
	ref.name       = "albedo";
	ref.colorSpace = cgpbr::ColorSpace::srgb;
	src->SetMap (cgpbr::MapSlot::base_color, ref);

	MaterialPtr q = p;
	const MaterialPbr* dst = dynamic_cast<const MaterialPbr*> (q.get());
	ASSERT_NE(dst, nullptr);

	EXPECT_FLOAT_EQ(dst->GetFactors().metallic,  0.25f);
	EXPECT_FLOAT_EQ(dst->GetFactors().roughness, 0.75f);
	EXPECT_EQ(dst->GetAlphaMode(), cgpbr::AlphaMode::mask);
	EXPECT_TRUE(dst->IsDoubleSided());
	ASSERT_TRUE(dst->HasMap (cgpbr::MapSlot::base_color));
	EXPECT_EQ(dst->GetMap (cgpbr::MapSlot::base_color).colorSpace, cgpbr::ColorSpace::srgb);
	// L'image est PARTAGEE et non dupliquee, comme celle d'un MaterialTexture.
	EXPECT_EQ(dst->GetMap (cgpbr::MapSlot::base_color).image.get(), ref.image.get());
	EXPECT_FALSE(dst->HasMap (cgpbr::MapSlot::normal));
}

TEST(TEST_cgmesh_material_pbr, defaults_are_those_of_the_gltf_format)
{
	// Valeurs du FORMAT, pas valeurs de rendu : un materiau glTF qui omet
	// metallicFactor est metallique et parfaitement rugueux.
	const MaterialPbr m;
	EXPECT_FLOAT_EQ(m.GetFactors().baseColor[0], 1.f);
	EXPECT_FLOAT_EQ(m.GetFactors().baseColor[3], 1.f);
	EXPECT_FLOAT_EQ(m.GetFactors().metallic,     1.f);
	EXPECT_FLOAT_EQ(m.GetFactors().roughness,    1.f);
	EXPECT_FLOAT_EQ(m.GetFactors().alphaCutoff,  0.5f);
	EXPECT_EQ(m.GetAlphaMode(), cgpbr::AlphaMode::opaque);
	EXPECT_FALSE(m.IsDoubleSided());
}

// ---------------------------------------------------------------------------
//  1 bis. Les emplacements de carte HORS BORNES
// ---------------------------------------------------------------------------
// MapSlot::count et toute valeur fabriquee par un cast sont des indices que le
// tableau ne porte pas. Le garde de MaterialPbr::GetMap / SetMap est le SEUL
// controle de bornes ecrit a la main du lot : sans test, sa suppression ne
// produirait qu'une lecture hors tableau, silencieuse en Release.

TEST(TEST_cgmesh_material_pbr, an_out_of_range_slot_yields_the_shared_empty_reference)
{
	MaterialPbr m;

	cgpbr::TextureRef ref;
	ref.image = std::make_shared<Img> (2u, 2u, false);
	ref.name  = "albedo";
	m.SetMap (cgpbr::MapSlot::base_color, ref);

	const cgpbr::MapSlot outOfRange[] = {
		cgpbr::MapSlot::count,
		static_cast<cgpbr::MapSlot> (200)
	};

	for (const cgpbr::MapSlot slot : outOfRange)
	{
		const cgpbr::TextureRef& got = m.GetMap (slot);
		EXPECT_EQ(got.image, nullptr) << "un emplacement hors bornes doit rendre le vide";
		EXPECT_TRUE(got.name.empty());
		EXPECT_FALSE(m.HasMap (slot));

		// Meme reference pour tous : c'est l'exemplaire statique partage.
		EXPECT_EQ(&got, &m.GetMap (cgpbr::MapSlot::count));
	}

	// SetMap hors bornes est SANS EFFET, et surtout n'ecrase rien.
	cgpbr::TextureRef other;
	other.image = std::make_shared<Img> (4u, 4u, false);
	m.SetMap (cgpbr::MapSlot::count, other);
	m.SetMap (static_cast<cgpbr::MapSlot> (200), other);

	ASSERT_TRUE(m.HasMap (cgpbr::MapSlot::base_color));
	EXPECT_EQ(m.GetMap (cgpbr::MapSlot::base_color).image.get(), ref.image.get());
	EXPECT_FALSE(m.HasMap (cgpbr::MapSlot::count));
}

// ---------------------------------------------------------------------------
//  2. Projection PBR -> Phong
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_material_pbr, a_polished_metal_projects_its_colour_onto_the_specular)
{
	const MaterialPbr src = makePbr (0.8f, 0.1f, 0.1f, 1.f, 0.f);
	const std::unique_ptr<Material> phong = cgpbr::toPhong (src);
	const MaterialColorExt* out = asColorExt (phong);
	ASSERT_NE(out, nullptr) << "sans carte de couleur de base : un MaterialColorExt";

	// specular = base (metal pur), calcule en lineaire puis rendu en sRGB.
	EXPECT_NEAR(out->GetSpecular()[0], srgbOf (0.8), 1e-5f);
	EXPECT_NEAR(out->GetSpecular()[1], srgbOf (0.1), 1e-5f);
	EXPECT_NEAR(out->GetSpecular()[2], srgbOf (0.1), 1e-5f);

	// Un metal n'a pas de diffuse -- et zero lineaire vaut zero en sRGB.
	EXPECT_NEAR(out->GetDiffuse()[0], 0.f, 1e-5f);
	EXPECT_NEAR(out->GetDiffuse()[1], 0.f, 1e-5f);
	EXPECT_NEAR(out->GetDiffuse()[2], 0.f, 1e-5f);

	// FRACTION dans [0,1] : MaterialRenderer::GlShininess multiplie par 128.
	// La rugosite n'est pas un signal lumineux : aucune conversion.
	EXPECT_NEAR(out->GetShininess(), 1.0f, 1e-5f);
}

TEST(TEST_cgmesh_material_pbr, a_rough_dielectric_keeps_its_colour_on_the_diffuse)
{
	const MaterialPbr src = makePbr (0.8f, 0.1f, 0.1f, 0.f, 1.f);
	const std::unique_ptr<Material> phong = cgpbr::toPhong (src);
	const MaterialColorExt* out = asColorExt (phong);
	ASSERT_NE(out, nullptr);

	EXPECT_NEAR(out->GetDiffuse()[0], srgbOf (0.8), 1e-5f);
	EXPECT_NEAR(out->GetDiffuse()[1], srgbOf (0.1), 1e-5f);
	EXPECT_NEAR(out->GetDiffuse()[2], srgbOf (0.1), 1e-5f);

	// F0 de Schlick : 4 % LINEAIRES sur les trois canaux, quelle que soit la
	// couleur. En sRGB cela ne fait plus 0,04 -- et c'est le point : une
	// projection qui rendrait 0,04 tel quel n'aurait pas converti.
	EXPECT_NEAR(out->GetSpecular()[0], srgbOf (0.04), 1e-5f);
	EXPECT_NEAR(out->GetSpecular()[1], srgbOf (0.04), 1e-5f);
	EXPECT_NEAR(out->GetSpecular()[2], srgbOf (0.04), 1e-5f);
	EXPECT_GT(out->GetSpecular()[0], 0.04f) << "F0 non converti : la frontiere sRGB n'est pas franchie";

	EXPECT_NEAR(out->GetShininess(), 0.0f, 1e-5f);

	// ambient = 0.2 * base, en lineaire.
	EXPECT_NEAR(out->GetAmbient()[0], srgbOf (0.16), 1e-5f);
}

TEST(TEST_cgmesh_material_pbr, a_base_colour_map_makes_the_projection_a_texture_material)
{
	MaterialPbr src = makePbr (1.f, 1.f, 1.f, 0.f, 0.5f);
	src.SetName ("duck");

	cgpbr::TextureRef ref;
	ref.image      = std::make_shared<Img> (8u, 4u, false);
	ref.name       = "duck_albedo";
	ref.colorSpace = cgpbr::ColorSpace::srgb;
	src.SetMap (cgpbr::MapSlot::base_color, ref);

	const std::unique_ptr<Material> phong = cgpbr::toPhong (src);
	const MaterialTexture* out = dynamic_cast<const MaterialTexture*> (phong.get());
	ASSERT_NE(out, nullptr) << "la carte de couleur de base doit survivre a la projection";

	// L'IMAGE est PARTAGEE, pas dupliquee : meme adresse que la source.
	ASSERT_NE(out->GetImage(), nullptr);
	EXPECT_EQ(out->GetImage(), ref.image.get());
	EXPECT_EQ(out->GetFilename(), "duck_albedo");
	EXPECT_EQ(out->GetName(), "duck");

	// La projection des facteurs traverse aussi : une base blanche laisse la
	// texture inchangee sous GL_MODULATE.
	EXPECT_NEAR(out->GetDiffuse()[0], 1.f, 1e-5f);
	EXPECT_NEAR(out->GetShininess(), 0.25f, 1e-5f);   // (1 - 0.5)^2

	// L'image ne doit pas etre liberee avec le materiau source.
	EXPECT_EQ(ref.image.use_count() >= 2, true);
}

// ---------------------------------------------------------------------------
//  3. Phong -> PBR
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_material_pbr, from_phong_recovers_a_dielectric_exactly)
{
	const MaterialPbr src = makePbr (0.8f, 0.1f, 0.1f, 0.f, 0.4f);
	const std::unique_ptr<Material> phong = cgpbr::toPhong (src);
	ASSERT_NE(phong, nullptr);

	const std::unique_ptr<MaterialPbr> back = cgpbr::fromPhong (*phong);
	ASSERT_NE(back, nullptr);

	EXPECT_NEAR(back->GetFactors().metallic,     0.0f, 1e-5f);
	EXPECT_NEAR(back->GetFactors().roughness,    0.4f, 1e-5f);
	EXPECT_NEAR(back->GetFactors().baseColor[0], 0.8f, 1e-5f);
	EXPECT_NEAR(back->GetFactors().baseColor[1], 0.1f, 1e-5f);
	EXPECT_NEAR(back->GetFactors().baseColor[2], 0.1f, 1e-5f);
}

TEST(TEST_cgmesh_material_pbr, from_phong_recovers_a_pure_metal_exactly)
{
	const MaterialPbr src = makePbr (0.8f, 0.1f, 0.1f, 1.f, 0.25f);
	const std::unique_ptr<Material> phong = cgpbr::toPhong (src);
	const std::unique_ptr<MaterialPbr> back = cgpbr::fromPhong (*phong);
	ASSERT_NE(back, nullptr);

	EXPECT_NEAR(back->GetFactors().metallic,     1.0f,  1e-5f);
	EXPECT_NEAR(back->GetFactors().roughness,    0.25f, 1e-5f);
	EXPECT_NEAR(back->GetFactors().baseColor[0], 0.8f,  1e-5f);
	EXPECT_NEAR(back->GetFactors().baseColor[1], 0.1f,  1e-5f);
	EXPECT_NEAR(back->GetFactors().baseColor[2], 0.1f,  1e-5f);
}

// LE METAL PLUS SOMBRE QUE F0.
//
// Sa luminance de base est inferieure a 0,04, donc Ld + Ls - F0 devient NEGATIF
// et la formule par quotient ne dit plus rien. Prise au mot elle rendait
// metallic = 0, puis base = diffuse = 0 : un dielectrique NOIR, definitivement.
TEST(TEST_cgmesh_material_pbr, from_phong_recovers_a_metal_darker_than_f0)
{
	const MaterialPbr src = makePbr (0.03f, 0.03f, 0.03f, 1.f, 0.5f);
	const std::unique_ptr<Material> phong = cgpbr::toPhong (src);
	const std::unique_ptr<MaterialPbr> back = cgpbr::fromPhong (*phong);
	ASSERT_NE(back, nullptr);

	EXPECT_NEAR(back->GetFactors().metallic,     1.0f,  1e-4f) << "metal sombre rendu dielectrique";
	EXPECT_NEAR(back->GetFactors().baseColor[0], 0.03f, 1e-4f) << "metal sombre noirci";
	EXPECT_NEAR(back->GetFactors().roughness,    0.5f,  1e-4f);
}

// LE CONTRE-EXEMPLE DU PRECEDENT, et la raison pour laquelle le critere n'est
// pas « Ld nul => metal ».
//
// Un dielectrique NOIR a lui aussi Ld = 0, et son speculaire vaut exactement F0.
// La regle qui distingue les deux est donc « Ls STRICTEMENT sous F0 » : sur ce
// materiau, elle doit laisser metallic a 0.
TEST(TEST_cgmesh_material_pbr, from_phong_keeps_a_black_dielectric_dielectric)
{
	const MaterialPbr src = makePbr (0.f, 0.f, 0.f, 0.f, 0.5f);
	const std::unique_ptr<Material> phong = cgpbr::toPhong (src);
	const std::unique_ptr<MaterialPbr> back = cgpbr::fromPhong (*phong);
	ASSERT_NE(back, nullptr);

	EXPECT_NEAR(back->GetFactors().metallic,     0.0f, 1e-4f) << "plastique noir rendu metallique";
	EXPECT_NEAR(back->GetFactors().baseColor[0], 0.0f, 1e-4f);
	EXPECT_NEAR(back->GetFactors().roughness,    0.5f, 1e-4f);
}

TEST(TEST_cgmesh_material_pbr, from_phong_is_idempotent_on_a_pbr_material)
{
	MaterialPbr src = makePbr (0.2f, 0.4f, 0.6f, 0.7f, 0.3f);
	src.SetName ("deja_pbr");
	src.SetAlphaMode (cgpbr::AlphaMode::mask);
	src.SetDoubleSided (true);
	src.EditFactors().alphaCutoff = 0.25f;

	const std::unique_ptr<MaterialPbr> back = cgpbr::fromPhong (src);
	ASSERT_NE(back, nullptr);

	// Sans la garde, ReadPhong ne reconnaitrait aucun des trois types Phong et
	// rendrait le NEUTRE : blanc, metallic 0, roughness 1.
	EXPECT_EQ(back->GetName(), "deja_pbr");
	EXPECT_NEAR(back->GetFactors().baseColor[0], 0.2f, 1e-6f);
	EXPECT_NEAR(back->GetFactors().baseColor[1], 0.4f, 1e-6f);
	EXPECT_NEAR(back->GetFactors().baseColor[2], 0.6f, 1e-6f);
	EXPECT_NEAR(back->GetFactors().metallic,     0.7f, 1e-6f);
	EXPECT_NEAR(back->GetFactors().roughness,    0.3f, 1e-6f);
	EXPECT_NEAR(back->GetFactors().alphaCutoff,  0.25f, 1e-6f);
	EXPECT_EQ(back->GetAlphaMode(), cgpbr::AlphaMode::mask);
	EXPECT_TRUE(back->IsDoubleSided());
}

TEST(TEST_cgmesh_material_pbr, from_phong_reads_a_plain_colour_material)
{
	// 204 / 255 = 0,8 et 51 / 255 = 0,2, en sRGB : la base PBR doit en etre la
	// version LINEAIRE.
	const MaterialColor src (204, 51, 51, 255);
	const std::unique_ptr<MaterialPbr> out = cgpbr::fromPhong (src);
	ASSERT_NE(out, nullptr);

	EXPECT_NEAR(out->GetFactors().baseColor[0], (float) cgpbr::srgbToLinear (0.8), 1e-5f);
	EXPECT_NEAR(out->GetFactors().baseColor[1], (float) cgpbr::srgbToLinear (0.2), 1e-5f);
	EXPECT_NEAR(out->GetFactors().baseColor[3], 1.f, 1e-5f);
	// MaterialColor ne porte NI speculaire NI brillance : dielectrique
	// parfaitement rugueux, soit les defauts que la lecture doit produire.
	EXPECT_NEAR(out->GetFactors().metallic,  0.f, 1e-5f);
	EXPECT_NEAR(out->GetFactors().roughness, 1.f, 1e-5f);
	EXPECT_EQ(out->GetAlphaMode(), cgpbr::AlphaMode::opaque);
}

TEST(TEST_cgmesh_material_pbr, from_phong_reads_a_texture_material_factors_only)
{
	MaterialTexture src ("albedo.png", std::make_shared<Img> (2u, 2u, false));
	src.SetName ("tex");
	src.SetDiffuse  (0.5f, 0.5f, 0.5f, 1.f);
	src.SetSpecular (0.f, 0.f, 0.f, 1.f);
	src.SetShininess (0.25f);

	const std::unique_ptr<MaterialPbr> out = cgpbr::fromPhong (src);
	ASSERT_NE(out, nullptr);

	EXPECT_EQ(out->GetName(), "tex");
	EXPECT_NEAR(out->GetFactors().baseColor[0], (float) cgpbr::srgbToLinear (0.5), 1e-5f);
	// shininess = (1 - roughness)^2 -> roughness = 1 - sqrt(0,25) = 0,5.
	EXPECT_NEAR(out->GetFactors().roughness, 0.5f, 1e-5f);
	// L'IMAGE ne traverse PAS : le contrat le dit, et c'est ce qui est verifie.
	EXPECT_FALSE(out->HasMap (cgpbr::MapSlot::base_color));
}

TEST(TEST_cgmesh_material_pbr, from_phong_on_an_unknown_type_yields_the_neutral)
{
	const Material src;                   // MATERIAL_NONE : aucune branche
	const std::unique_ptr<MaterialPbr> out = cgpbr::fromPhong (src);
	ASSERT_NE(out, nullptr);

	EXPECT_NEAR(out->GetFactors().baseColor[0], 1.f, 1e-5f);
	EXPECT_NEAR(out->GetFactors().baseColor[1], 1.f, 1e-5f);
	EXPECT_NEAR(out->GetFactors().baseColor[2], 1.f, 1e-5f);
	EXPECT_NEAR(out->GetFactors().metallic,     0.f, 1e-5f);
	EXPECT_NEAR(out->GetFactors().roughness,    1.f, 1e-5f);
}

// ---------------------------------------------------------------------------
//  4. Courbes sRGB
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_material_pbr, srgb_curves_are_exact_inverses_including_at_the_knee)
{
	// 0,04045 est le GENOU de la courbe : c'est la que les deux seuils usuels
	// (0,04045 et 0,0031308 arrondi) cessent d'etre images l'un de l'autre.
	const double xs[] = { 0.0, 0.04045, 0.5, 1.0 };
	for (double x : xs)
	{
		EXPECT_NEAR(cgpbr::linearToSrgb (cgpbr::srgbToLinear (x)), x, 1e-6)
			<< "aller-retour sRGB rompu en x = " << x;
	}

	// Sens metier : le lineaire est plus SOMBRE que le sRGB au milieu de
	// l'echelle. Une conversion posee a l'envers passerait les tests
	// d'aller-retour sans etre correcte.
	EXPECT_LT(cgpbr::srgbToLinear (0.5), 0.5);
	EXPECT_GT(cgpbr::linearToSrgb (0.5), 0.5);
}

// L'ALLER-RETOUR DANS L'AUTRE SENS, autour du genou LINEAIRE.
//
// C'est la que la constante derivee (0,04045 / 12,92) s'ecarte du 0,0031308 de
// la litterature. Le test precedent part du sRGB et ne franchit ce seuil que
// par l'image de 0,04045 ; celui-ci l'encadre des deux cotes.
//
// TOLERANCE 1e-8, et non l'exactitude : la SPECIFICATION elle-meme n'est pas
// continue en ce point. Ses deux branches, evaluees au genou, ne rendent pas la
// meme valeur -- 12,92 x genou vaut 0,04045 exactement, la branche en puissance
// vaut 0,040449970..., soit un saut MESURE de 3,0e-8 en sRGB, ce qui fait
// 2,3e-9 en lineaire. La derivation du seuil supprime la marche de 2e-5 que
// laisserait la constante arrondie ; elle ne peut pas supprimer celle-la, qui
// est dans les constantes du standard. 2,3e-9 vaut 6e-7 niveau sur 255 : sans
// portee visuelle, mais a ne pas maquiller en zero.
TEST(TEST_cgmesh_material_pbr, srgb_curves_are_exact_inverses_around_the_linear_knee)
{
	const double knee = 0.04045 / 12.92;
	const double ys[] = { 0.0, knee * 0.5, knee - 1e-9, knee, knee + 1e-9,
	                      knee * 2.0, 0.0031308, 0.5, 1.0 };
	for (double y : ys)
	{
		EXPECT_NEAR(cgpbr::srgbToLinear (cgpbr::linearToSrgb (y)), y, 1e-8)
			<< "aller-retour lineaire rompu en y = " << y;
	}

	// SOUS le genou et a partir du DOUBLE du genou, l'aller-retour est exact :
	// l'ecart ci-dessus est confine a l'intervalle ou les deux branches se
	// chevauchent.
	EXPECT_NEAR(cgpbr::srgbToLinear (cgpbr::linearToSrgb (knee)), knee, 1e-15);
	EXPECT_NEAR(cgpbr::srgbToLinear (cgpbr::linearToSrgb (2.0 * knee)), 2.0 * knee, 1e-15);

	// La branche lineaire atteint le genou sRGB EXACTEMENT : c'est ce que la
	// derivation du seuil assure et qu'une constante arrondie romprait.
	EXPECT_NEAR(cgpbr::linearToSrgb (knee), 0.04045, 1e-15);
}

// ---------------------------------------------------------------------------
//  Contrat de type consomme par maker/mesh_payload.cpp
// ---------------------------------------------------------------------------
//
// maker n'est bati que sous Emscripten : ni cette machine ni la CI ne le
// compilent, et la branche MATERIAL_PBR de DescribeMaterial n'a donc AUCUN
// compilateur derriere elle. Ce qui suit ne compile pas mesh_payload.cpp -- il
// verifie, avec le compilateur qu'on a, les trois proprietes de TYPE dont cette
// branche depend. Une rupture de signature devient une erreur ici au lieu d'une
// erreur de build emsdk decouverte par quelqu'un d'autre.
//
// ⚠ CE QUE CE TEST NE COUVRE PAS : la branche elle-meme n'est ni compilee ni
// exercee. Il ne dit rien de sa logique, seulement que les types qu'elle
// suppose sont ceux qui existent.
TEST (TEST_cgmesh_material_pbr, the_types_used_by_the_maker_payload_hold)
{
	MaterialPbr pbr;

	// 1. toPhong rend un unique_ptr<Material> : c'est ce qui permet a
	//    mesh_payload de detenir la projection le temps de la decrire.
	static_assert (
		std::is_same_v<decltype (cgpbr::toPhong (pbr)), std::unique_ptr<Material>>,
		"toPhong doit rendre std::unique_ptr<Material>");

	// 2. Sans carte de couleur de base, la projection est un MaterialColorExt
	//    -- le type que la cascade de DescribeMaterial ne connait pas et que la
	//    branche traite elle-meme.
	std::unique_ptr<Material> projected = cgpbr::toPhong (pbr);
	ASSERT_NE (projected, nullptr);
	const MaterialColorExt* ext = dynamic_cast<const MaterialColorExt*> (projected.get ());
	ASSERT_NE (ext, nullptr)
		<< "sans carte de couleur de base, la projection doit etre un MaterialColorExt";

	// 3. GetDiffuse() est const et rend un pointeur indexable sur au moins
	//    trois composantes.
	static_assert (std::is_same_v<decltype (ext->GetDiffuse ()), const float*>,
	               "MaterialColorExt::GetDiffuse() const doit rendre const float*");
	const float* d = ext->GetDiffuse ();
	ASSERT_NE (d, nullptr);
	EXPECT_GE (d[0], 0.f); EXPECT_LE (d[0], 1.f);
	EXPECT_GE (d[1], 0.f); EXPECT_LE (d[1], 1.f);
	EXPECT_GE (d[2], 0.f); EXPECT_LE (d[2], 1.f);
}
