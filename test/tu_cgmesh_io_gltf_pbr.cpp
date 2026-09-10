// ===========================================================================
//  Import glTF : le materiau de la source devient un MaterialPbr
// ===========================================================================
//
// Trois niveaux, qui ne mesurent pas la meme chose.
//
// 1. CE QUE L'IMPORTEUR PORTE MAINTENANT. VMeshesIO::load fabriquait un
//    MaterialColorExt dont le speculaire et la brillance etaient des CONSTANTES
//    ECRITES EN DUR : un metal rugueux et un plastique lisse en ressortaient
//    identiques. Il fabrique desormais un MaterialPbr, et l'oracle est que les
//    facteurs du JSON -- metallicFactor, roughnessFactor -- ARRIVENT jusqu'au
//    maillage, et qu'ils y sont DIFFERENTS d'un fichier a l'autre.
//
//    /!\ Ce bloc a remplace le detecteur qui enoncait la constance. Sa version
//    precedente assertait shininess == 0,25 et specular == (0,5 0,5 0,5) ; elle
//    est passee au rouge au branchement de cgpbr::materialFromGltf, ce qui etait
//    sa raison d'etre. Elle n'a pas ete supprimee : elle a ete retournee en
//    oracle de la nouvelle verite.
//
// 2. CE QUE LA PROJECTION RESTITUE. Les consommateurs Phong -- cgre, l'ecrivain
//    MTL, maker, sulina -- passent par cgpbr::toPhong. Deux proprietes s'y
//    jouent : la carte de couleur de base SURVIT (sinon Duck.glb perdrait sa
//    texture), et la conversion LINEAIRE -> sRGB a lieu (sinon un aller-retour
//    GLB assombrit les couleurs).
//
// 3. CE QUE LA LECTURE PBR SAIT FAIRE, unite par unite : cgpbr::materialFromGltf
//    sur un tinygltf::Model.
//
// PERIMETRE DES DONNEES : uniquement des GLB du coeur de la specification 2.0.
// Les fichiers portant KHR_texture_transform, KHR_materials_specular,
// KHR_materials_clearcoat ou toute autre extension ne sont PAS couverts par
// materialFromGltf et n'ont donc rien a faire ici.
//
#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "../src/cgimg/image.h"
#include "../src/cgmesh/material.h"
#include "../src/cgmesh/material_convert.h"
#include "../src/cgmesh/material_pbr.h"
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_io.h"
#include "../src/cgmesh/mesh_io_gltf_pbr.h"
#include "../src/cgmesh/vmeshes.h"
#include "../src/cgmesh/vmeshes_io.h"

#include <nlohmann/json.hpp>
#include <tinygltf/tiny_gltf.h>

namespace {

// pbr_sphere.glb, inventaire de son JSON : un materiau « PBR_Material »,
// baseColorFactor (0,85 / 0,35 / 0,20 / 1), metallicFactor 0,9,
// roughnessFactor 0,35, emissiveFactor nul, OPAQUE, simple face. AUCUNE image,
// AUCUNE texture, AUCUNE extension : c'est un materiau de FACTEURS seuls.
const char* kPbrSphere = "./test/data/pbr_sphere.glb";

// Duck.glb (Khronos) : une baseColorTexture, metallicFactor 0, AUCUN
// roughnessFactor -- donc la valeur par defaut du format, 1. Aucune extension.
const char* kDuck = "./test/data/Duck.glb";

// Fox.glb : une baseColorTexture, metallicFactor 0, roughnessFactor 0,58.
// Aucune extension. C'est la source de l'oracle « le facteur lu est celui du
// JSON », que Duck ne peut pas porter puisqu'il omet le champ.
const char* kFox = "./test/data/Fox.glb";

// Rappel d'image du lecteur. cgmesh compile tinygltf avec TINYGLTF_NO_STB_IMAGE :
// sans rappel pose, ParseImage ECHOUE et le fichier entier est refuse. On decode
// donc avec cgimg, qui rend du RGBA 8 bits -- la disposition qu'attend la lecture
// des materiaux.
bool LoadImageWithCgimg (tinygltf::Image* image, const int, std::string* err, std::string*,
                         int, int, const unsigned char* bytes, int size, void*)
{
	Img decoded;
	if (!bytes || size <= 0 || decoded.load_from_memory (bytes, (size_t) size) != 0)
	{
		if (err) *err += "cgimg n'a pas su decoder une image du glTF\n";
		return false;
	}

	image->width      = (int) decoded.width();
	image->height     = (int) decoded.height();
	image->component  = 4;
	image->bits       = 8;
	image->pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
	const size_t n = 4u * (size_t) decoded.width() * (size_t) decoded.height();
	image->image.assign (decoded.data(), decoded.data() + n);
	return true;
}

bool LoadGlb (tinygltf::Model& model, const char* filename)
{
	tinygltf::TinyGLTF loader;
	loader.SetImageLoader (LoadImageWithCgimg, nullptr);
	std::string err, warn;
	return loader.LoadBinaryFromFile (&model, &err, &warn, filename);
}

// Le materiau du premier maillage d'un fichier, tel que VMeshesIO::load le pose.
const MaterialPbr* ImportedPbrMaterial (VMeshes& vm, const char* filename)
{
	if (!VMeshesIO::load (vm, filename) || vm.GetMeshes().empty())
		return nullptr;
	const Mesh* pMesh = vm.GetMeshes()[0];
	if (!pMesh)
		return nullptr;
	return dynamic_cast<const MaterialPbr*> (pMesh->GetMaterial (0));
}

// La courbe lineaire -> sRGB de reference, RE-ECRITE ICI et non appelee depuis
// material_convert : un oracle qui partagerait la fonction testee ne testerait
// que lui-meme.
double linearToSrgbRef (double c)
{
	if (c <= 0.0) return 0.0;
	if (c >= 1.0) return 1.0;
	return (c <= 0.04045 / 12.92) ? (c * 12.92)
	                              : (1.055 * std::pow (c, 1.0 / 2.4) - 0.055);
}

} // namespace

// ---------------------------------------------------------------------------
//  1. L'importeur porte les facteurs du fichier
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_io_gltf_pbr, the_imported_material_carries_the_files_factors)
{
	VMeshes vm;
	const MaterialPbr* mat = ImportedPbrMaterial (vm, kPbrSphere);
	ASSERT_NE(mat, nullptr) << kPbrSphere << " doit produire un MATERIAL_PBR";

	EXPECT_EQ(mat->GetType(), MATERIAL_PBR);
	EXPECT_EQ(mat->GetName(), "PBR_Material");

	// Les deux facteurs que l'importeur jetait. Ce sont EUX qui font passer le
	// detecteur precedent au rouge : ils n'atteignaient pas le maillage.
	EXPECT_NEAR(mat->GetFactors().metallic,  0.90f, 1e-6f);
	EXPECT_NEAR(mat->GetFactors().roughness, 0.35f, 1e-6f);

	// baseColorFactor reste LINEAIRE : MaterialPbr est le conteneur du format,
	// il ne convertit pas. C'est la projection qui franchit la frontiere.
	EXPECT_NEAR(mat->GetFactors().baseColor[0], 0.85f, 1e-6f);
	EXPECT_NEAR(mat->GetFactors().baseColor[1], 0.35f, 1e-6f);
	EXPECT_NEAR(mat->GetFactors().baseColor[2], 0.20f, 1e-6f);
	EXPECT_NEAR(mat->GetFactors().baseColor[3], 1.00f, 1e-6f);
}

// LA PROJECTION DEPEND DU CONTENU DU FICHIER, et c'est tout l'enjeu de l'etape.
//
// L'importeur precedent ecrivait specular = (0,5 0,5 0,5) et shininess = 0,25
// pour TOUT glTF sans texture : deux fichiers de rugosites differentes en
// sortaient identiques. La forme positive de cette propriete est ici : deux
// fichiers de facteurs differents doivent donner des projections DIFFERENTES.
TEST(TEST_cgmesh_io_gltf_pbr, the_projection_follows_the_file_and_not_a_constant)
{
	VMeshes vmSphere, vmFox;
	const MaterialPbr* sphere = ImportedPbrMaterial (vmSphere, kPbrSphere);
	const MaterialPbr* fox    = ImportedPbrMaterial (vmFox, kFox);
	ASSERT_NE(sphere, nullptr);
	ASSERT_NE(fox, nullptr);

	// roughnessFactor : 0,35 pour la sphere, 0,58 pour le renard.
	EXPECT_NEAR(sphere->GetFactors().roughness, 0.35f, 1e-6f);
	EXPECT_NEAR(fox->GetFactors().roughness,    0.58f, 1e-6f);

	const std::unique_ptr<Material> pSphere = cgpbr::toPhong (*sphere);
	const std::unique_ptr<Material> pFox    = cgpbr::toPhong (*fox);
	ASSERT_NE(pSphere, nullptr);
	ASSERT_NE(pFox, nullptr);

	// shininess = (1 - roughness)^2 : 0,4225 contre 0,1764. Sous l'ancien
	// importeur les deux valaient 0,25.
	const MaterialColorExt* extSphere = dynamic_cast<const MaterialColorExt*> (pSphere.get());
	const MaterialTexture*  texFox    = dynamic_cast<const MaterialTexture*>  (pFox.get());
	ASSERT_NE(extSphere, nullptr) << "pbr_sphere n'a aucune carte : un MaterialColorExt";
	ASSERT_NE(texFox, nullptr)    << "Fox porte une carte de base : un MaterialTexture";

	EXPECT_NEAR(extSphere->GetShininess(), 0.4225f, 1e-5f);
	EXPECT_NEAR(texFox->GetShininess(),    0.1764f, 1e-5f);
	EXPECT_NE(extSphere->GetShininess(), texFox->GetShininess());
}

// LES CANAUX DE LA PROJECTION, valeur par valeur.
//
// La formule est calculee en LINEAIRE puis convertie en sRGB :
//   specular = base * metallic + 0,04 * (1 - metallic)
//            = (0,769  0,319  0,184)  en lineaire.
// L'oracle applique la courbe de reference locale a ces trois nombres.
TEST(TEST_cgmesh_io_gltf_pbr, the_projected_channels_are_the_gltf_formula_in_srgb)
{
	VMeshes vm;
	const MaterialPbr* mat = ImportedPbrMaterial (vm, kPbrSphere);
	ASSERT_NE(mat, nullptr);

	const std::unique_ptr<Material> phong = cgpbr::toPhong (*mat);
	const MaterialColorExt* out = dynamic_cast<const MaterialColorExt*> (phong.get());
	ASSERT_NE(out, nullptr);

	// specular, en lineaire : 0,85 x 0,9 + 0,04 x 0,1 = 0,769 ; etc.
	EXPECT_NEAR(out->GetSpecular()[0], (float) linearToSrgbRef (0.769), 1e-4f);
	EXPECT_NEAR(out->GetSpecular()[1], (float) linearToSrgbRef (0.319), 1e-4f);
	EXPECT_NEAR(out->GetSpecular()[2], (float) linearToSrgbRef (0.184), 1e-4f);

	// diffuse = base x (1 - metallic) = (0,085  0,035  0,020) en lineaire.
	EXPECT_NEAR(out->GetDiffuse()[0], (float) linearToSrgbRef (0.085), 1e-4f);
	EXPECT_NEAR(out->GetDiffuse()[1], (float) linearToSrgbRef (0.035), 1e-4f);
	EXPECT_NEAR(out->GetDiffuse()[2], (float) linearToSrgbRef (0.020), 1e-4f);
	EXPECT_NEAR(out->GetDiffuse()[3], 1.f, 1e-6f);

	// FRACTION dans [0,1] : MaterialRenderer::GlShininess multiplie par 128.
	EXPECT_NEAR(out->GetShininess(), 0.4225f, 1e-5f);

	// Ce que l'ancien importeur ecrivait, et qui ne doit plus sortir.
	EXPECT_NE(out->GetShininess(), 0.25f);
	EXPECT_NE(out->GetSpecular()[0], 0.5f);
}

// ---------------------------------------------------------------------------
//  2. La texture de base survit a l'import (non-regression Duck / Fox)
// ---------------------------------------------------------------------------
// Avant l'etape 3, ces deux fichiers produisaient directement un
// MaterialTexture. Faire porter un MaterialPbr au maillage ne doit RIEN leur
// coûter : la projection doit rendre un MaterialTexture PORTANT L'IMAGE, et
// l'image doit etre PARTAGEE avec le materiau source, pas recopiee.

TEST(TEST_cgmesh_io_gltf_pbr, a_textured_file_still_projects_to_a_texture_material)
{
	for (const char* filename : { kDuck, kFox })
	{
		VMeshes vm;
		const MaterialPbr* mat = ImportedPbrMaterial (vm, filename);
		ASSERT_NE(mat, nullptr) << filename;
		ASSERT_TRUE(mat->HasMap (cgpbr::MapSlot::base_color)) << filename;

		const cgpbr::TextureRef& base = mat->GetMap (cgpbr::MapSlot::base_color);
		EXPECT_EQ(base.colorSpace, cgpbr::ColorSpace::srgb) << filename;

		const std::unique_ptr<Material> phong = cgpbr::toPhong (*mat);
		const MaterialTexture* tex = dynamic_cast<const MaterialTexture*> (phong.get());
		ASSERT_NE(tex, nullptr) << filename << " : la texture a ete perdue";

		ASSERT_NE(tex->GetImage(), nullptr) << filename;
		EXPECT_GT(tex->GetImage()->width(),  0u) << filename;
		EXPECT_GT(tex->GetImage()->height(), 0u) << filename;
		// PARTAGEE, pas dupliquee : meme objet Img que celui du MaterialPbr.
		EXPECT_EQ(tex->GetImage(), base.image.get()) << filename;
	}
}

// ---------------------------------------------------------------------------
//  3. L'aller-retour COULEUR par le GLB
// ---------------------------------------------------------------------------
// L'ecrivain applique srgbToLinear a la couleur du materiau (baseColorFactor est
// LINEAIRE) ; le lecteur recopiait le facteur brut dans la diffuse, sans jamais
// appliquer l'inverse. Un aller-retour changeait donc la couleur : 0,8 partait
// et 0,604 revenait. C'est la raison d'etre de linearToSrgb, qui n'avait aucun
// appelant en production.

TEST(TEST_cgmesh_io_gltf_pbr, a_colour_survives_a_glb_round_trip)
{
	// 204 / 255 = 0,8 et 51 / 255 = 0,2, en sRGB.
	Mesh m;
	m.Init (3, 1);
	m.SetVertex (0, 0.f,  0.f, 0.f);
	m.SetVertex (1, 10.f, 0.f, 0.f);
	m.SetVertex (2, 0.f, 10.f, 0.f);
	m.SetFace (0, 0, 1, 2);
	m.ComputeNormals ();
	m.ApplyMaterial (m.Material_Add (new MaterialColor (204, 51, 51)));

	ASSERT_EQ(MeshIO::export_glb (m, "./tu_pbr_colour_roundtrip.glb"), 0);

	VMeshes vm;
	const MaterialPbr* mat = ImportedPbrMaterial (vm, "./tu_pbr_colour_roundtrip.glb");
	ASSERT_NE(mat, nullptr);

	// Dans le fichier, le facteur est LINEAIRE : environ 0,604 et 0,033.
	EXPECT_LT(mat->GetFactors().baseColor[0], 0.7f)
		<< "baseColorFactor doit etre lineaire dans le conteneur";

	const std::unique_ptr<Material> phong = cgpbr::toPhong (*mat);
	const MaterialColorExt* out = dynamic_cast<const MaterialColorExt*> (phong.get());
	ASSERT_NE(out, nullptr);

	// metallicFactor vaut 0 a l'ecriture : la diffuse recoit toute la base.
	EXPECT_NEAR(out->GetDiffuse()[0], 0.8f, 0.01f);
	EXPECT_NEAR(out->GetDiffuse()[1], 0.2f, 0.01f);
	EXPECT_NEAR(out->GetDiffuse()[2], 0.2f, 0.01f);
	EXPECT_NEAR(out->GetDiffuse()[3], 1.0f, 0.01f);
}

// ---------------------------------------------------------------------------
//  4. byteStride : un GLB a tampon ENTRELACE
// ---------------------------------------------------------------------------
// POSITION et NORMAL dans UNE SEULE vue de tampon, separees par une foulee de
// 24 octets. La lecture par pointeur brut qu'utilisait l'importeur ignorait la
// foulee : elle prenait la normale du sommet i pour la position du sommet i+1.
// Le fichier est FABRIQUE ici -- aucun GLB entrelace n'existe dans test/data --
// et sa foulee est verifiee avant de servir d'oracle.

namespace {

const char* kInterleaved = "./tu_pbr_interleaved.glb";

// Trois sommets, positions et normales entrelacees, foulee 24 octets.
bool WriteInterleavedGlb (const char* path)
{
	const float interleaved[18] = {
		0.f, 0.f, 0.f,   0.f, 0.f, 1.f,     // sommet 0 : position, normale
		1.f, 0.f, 0.f,   0.f, 0.f, 1.f,     // sommet 1
		0.f, 1.f, 0.f,   0.f, 0.f, 1.f      // sommet 2
	};

	tinygltf::Model model;
	model.asset.version = "2.0";
	model.asset.generator = "TU interleaved fixture";

	model.buffers.resize (1);
	model.buffers[0].data.resize (sizeof (interleaved));
	std::memcpy (model.buffers[0].data.data(), interleaved, sizeof (interleaved));

	tinygltf::BufferView view;
	view.buffer     = 0;
	view.byteOffset = 0;
	view.byteLength = sizeof (interleaved);
	view.byteStride = 24;                    // 6 flottants par sommet
	view.target     = TINYGLTF_TARGET_ARRAY_BUFFER;
	model.bufferViews.push_back (view);

	tinygltf::Accessor pos;
	pos.bufferView    = 0;
	pos.byteOffset    = 0;
	pos.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
	pos.count         = 3;
	pos.type          = TINYGLTF_TYPE_VEC3;
	pos.minValues     = { 0.0, 0.0, 0.0 };
	pos.maxValues     = { 1.0, 1.0, 0.0 };
	model.accessors.push_back (pos);

	tinygltf::Accessor nrm = pos;
	nrm.byteOffset = 12;                     // decalage DANS l'element entrelace
	nrm.minValues.clear();
	nrm.maxValues.clear();
	model.accessors.push_back (nrm);

	tinygltf::Primitive prim;
	prim.mode = TINYGLTF_MODE_TRIANGLES;
	prim.attributes["POSITION"] = 0;
	prim.attributes["NORMAL"]   = 1;
	prim.indices = -1;                       // sans indices : 3 sommets, 1 face

	tinygltf::Mesh gltfMesh;
	gltfMesh.name = "interleaved";
	gltfMesh.primitives.push_back (prim);
	model.meshes.push_back (gltfMesh);

	tinygltf::Node node;
	node.mesh = 0;
	model.nodes.push_back (node);

	tinygltf::Scene scene;
	scene.nodes.push_back (0);
	model.scenes.push_back (scene);
	model.defaultScene = 0;

	tinygltf::TinyGLTF writer;
	return writer.WriteGltfSceneToFile (&model, path,
	                                    /*embedImages=*/true, /*embedBuffers=*/true,
	                                    /*prettyPrint=*/false, /*writeBinary=*/true);
}

} // namespace

TEST(TEST_cgmesh_io_gltf_pbr, an_interleaved_buffer_is_read_through_its_stride)
{
	ASSERT_TRUE(WriteInterleavedGlb (kInterleaved));

	// VALIDATION DE LA MONTURE avant de s'en servir d'oracle : si l'ecrivain
	// n'avait pas conserve la foulee, le test passerait pour la mauvaise raison.
	{
		tinygltf::Model check;
		ASSERT_TRUE(LoadGlb (check, kInterleaved));
		ASSERT_EQ(check.bufferViews.size(), 1u) << "une seule vue : c'est l'entrelacement";
		EXPECT_EQ(check.bufferViews[0].byteStride, 24u);
		ASSERT_EQ(check.accessors.size(), 2u);
		EXPECT_EQ(check.accessors[1].byteOffset, 12u);
	}

	VMeshes vm;
	ASSERT_TRUE(VMeshesIO::load (vm, kInterleaved));
	ASSERT_EQ(vm.GetNMeshes(), 1u);
	const Mesh* back = vm.GetMeshes()[0];
	ASSERT_NE(back, nullptr);
	ASSERT_EQ(back->GetNVertices(), 3u);

	// glTF -> depot : echelle 1000 (metre -> millimetre) et Rx(+90), soit
	// (x,y,z) -> (1000x, -1000z, 1000y).
	//
	// Foulee ignoree, la lecture contigue rendrait (0,0,0), (0,0,1), (1,0,0) :
	// le sommet 1 sortirait a (0,-1000,0) au lieu de (1000,0,0).
	const float expected[9] = {
		   0.f, 0.f,    0.f,
		1000.f, 0.f,    0.f,
		   0.f, 0.f, 1000.f
	};
	const std::vector<float>& v = back->GetVertices();
	ASSERT_EQ(v.size(), 9u);
	for (int i = 0; i < 9; ++i)
		EXPECT_NEAR(v[i], expected[i], 1e-3f) << "composante " << i;
}

// ---------------------------------------------------------------------------
//  5. La lecture PBR : cgpbr::materialFromGltf
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_io_gltf_pbr, an_out_of_range_index_yields_nullptr)
{
	tinygltf::Model model;
	ASSERT_TRUE(LoadGlb (model, kPbrSphere));
	ASSERT_EQ(model.materials.size(), 1u);

	EXPECT_EQ(cgpbr::materialFromGltf (model, -1), nullptr);
	EXPECT_EQ(cgpbr::materialFromGltf (model, 1),  nullptr);
	EXPECT_NE(cgpbr::materialFromGltf (model, 0),  nullptr);
}

TEST(TEST_cgmesh_io_gltf_pbr, factors_only_material_carries_every_factor)
{
	tinygltf::Model model;
	ASSERT_TRUE(LoadGlb (model, kPbrSphere));

	const std::unique_ptr<MaterialPbr> mat = cgpbr::materialFromGltf (model, 0);
	ASSERT_NE(mat, nullptr);

	EXPECT_EQ(mat->GetType(), MATERIAL_PBR);
	EXPECT_EQ(mat->GetName(), "PBR_Material");

	EXPECT_NEAR(mat->GetFactors().metallic,  0.90f, 1e-6f);
	EXPECT_NEAR(mat->GetFactors().roughness, 0.35f, 1e-6f);

	EXPECT_NEAR(mat->GetFactors().baseColor[0], 0.85f, 1e-6f);
	EXPECT_NEAR(mat->GetFactors().baseColor[1], 0.35f, 1e-6f);
	EXPECT_NEAR(mat->GetFactors().baseColor[2], 0.20f, 1e-6f);
	EXPECT_NEAR(mat->GetFactors().baseColor[3], 1.00f, 1e-6f);

	EXPECT_EQ(mat->GetAlphaMode(), cgpbr::AlphaMode::opaque);
	EXPECT_FALSE(mat->IsDoubleSided());

	// Un materiau de facteurs seuls n'a AUCUNE carte.
	EXPECT_FALSE(mat->HasMap (cgpbr::MapSlot::base_color));
	EXPECT_FALSE(mat->HasMap (cgpbr::MapSlot::normal));
	EXPECT_FALSE(mat->HasMap (cgpbr::MapSlot::metallic_roughness));
	EXPECT_FALSE(mat->HasMap (cgpbr::MapSlot::occlusion));
	EXPECT_FALSE(mat->HasMap (cgpbr::MapSlot::emissive));
}

TEST(TEST_cgmesh_io_gltf_pbr, a_base_colour_map_lands_in_its_slot_in_srgb)
{
	tinygltf::Model model;
	ASSERT_TRUE(LoadGlb (model, kDuck));
	ASSERT_EQ(model.materials.size(), 1u);
	ASSERT_EQ(model.images.size(), 1u);

	const std::unique_ptr<MaterialPbr> mat = cgpbr::materialFromGltf (model, 0);
	ASSERT_NE(mat, nullptr);

	ASSERT_TRUE(mat->HasMap (cgpbr::MapSlot::base_color));
	const cgpbr::TextureRef& base = mat->GetMap (cgpbr::MapSlot::base_color);
	EXPECT_EQ(base.colorSpace, cgpbr::ColorSpace::srgb)
		<< "une carte porteuse de COULEUR est en sRGB";
	EXPECT_EQ(base.uvSet, 0u);
	// `name` est INDICATIF et non unique (cf. TextureRef) : on verifie qu'il est
	// renseigne, pas qu'il identifie.
	EXPECT_FALSE(base.name.empty());
	ASSERT_NE(base.image, nullptr);
	EXPECT_EQ(base.image->width(),  (unsigned) model.images[0].width);
	EXPECT_EQ(base.image->height(), (unsigned) model.images[0].height);

	// Les quatre autres emplacements restent vides : Duck n'a que sa diffuse.
	EXPECT_FALSE(mat->HasMap (cgpbr::MapSlot::normal));
	EXPECT_FALSE(mat->HasMap (cgpbr::MapSlot::metallic_roughness));
	EXPECT_FALSE(mat->HasMap (cgpbr::MapSlot::occlusion));
	EXPECT_FALSE(mat->HasMap (cgpbr::MapSlot::emissive));

	// roughnessFactor est ABSENT du JSON de Duck : c'est le defaut du FORMAT,
	// 1, qui doit sortir -- et non un defaut de rendu quelconque.
	EXPECT_NEAR(mat->GetFactors().metallic,  0.0f, 1e-6f);
	EXPECT_NEAR(mat->GetFactors().roughness, 1.0f, 1e-6f);
}

// LE NOM D'UNE CARTE N'IDENTIFIE PAS SON IMAGE.
//
// Le contrat de TextureRef le dit ; ce test le montre. Duck et Fox portent
// chacun une image sans nom, donc toutes deux repliees sur l'indice LOCAL AU
// MODELE : les deux cles sont EGALES alors que les images different. Un backend
// qui prendrait ce champ pour une cle de cache resservirait la mauvaise texture.
TEST(TEST_cgmesh_io_gltf_pbr, the_map_name_is_not_a_cache_key)
{
	tinygltf::Model duckModel, foxModel;
	ASSERT_TRUE(LoadGlb (duckModel, kDuck));
	ASSERT_TRUE(LoadGlb (foxModel, kFox));

	const std::unique_ptr<MaterialPbr> duck = cgpbr::materialFromGltf (duckModel, 0);
	const std::unique_ptr<MaterialPbr> fox  = cgpbr::materialFromGltf (foxModel, 0);
	ASSERT_NE(duck, nullptr);
	ASSERT_NE(fox, nullptr);
	ASSERT_TRUE(duck->HasMap (cgpbr::MapSlot::base_color));
	ASSERT_TRUE(fox->HasMap (cgpbr::MapSlot::base_color));

	const cgpbr::TextureRef& a = duck->GetMap (cgpbr::MapSlot::base_color);
	const cgpbr::TextureRef& b = fox->GetMap (cgpbr::MapSlot::base_color);

	// Deux images DIFFERENTES -- par leurs dimensions, ou a defaut par leurs
	// pixels : c'est ce qui rend la collision de nom nuisible.
	EXPECT_NE(a.image.get(), b.image.get());
	bool different = (a.image->width()  != b.image->width()) ||
	                 (a.image->height() != b.image->height());
	if (!different)
	{
		const size_t n = 4u * (size_t) a.image->width() * (size_t) a.image->height();
		different = (std::memcmp (a.image->data(), b.image->data(), n) != 0);
	}
	EXPECT_TRUE(different) << "les deux fichiers doivent porter des images distinctes";

	// ... et pourtant le meme nom si aucune ne se nomme. C'est le contrat, pas
	// un defaut a corriger dans le code : c'est l'ADRESSE qui identifie.
	if (a.name.rfind ("gltf_image_", 0) == 0 && b.name.rfind ("gltf_image_", 0) == 0)
		EXPECT_EQ(a.name, b.name) << "le repli sur l'indice est local au modele";
}

TEST(TEST_cgmesh_io_gltf_pbr, the_roughness_factor_read_is_the_one_in_the_json)
{
	tinygltf::Model model;
	ASSERT_TRUE(LoadGlb (model, kFox));

	const std::unique_ptr<MaterialPbr> mat = cgpbr::materialFromGltf (model, 0);
	ASSERT_NE(mat, nullptr);

	EXPECT_EQ(mat->GetName(), "fox_material");
	EXPECT_NEAR(mat->GetFactors().roughness, 0.58f, 1e-6f);
	EXPECT_NEAR(mat->GetFactors().metallic,  0.0f,  1e-6f);
	EXPECT_TRUE(mat->HasMap (cgpbr::MapSlot::base_color));
}

// L'image est detenue par le materiau, pas par le modele : le tinygltf::Model
// peut disparaitre sans emporter les pixels.
TEST(TEST_cgmesh_io_gltf_pbr, the_material_outlives_the_model)
{
	std::unique_ptr<MaterialPbr> mat;
	unsigned int w = 0, h = 0;
	{
		tinygltf::Model model;
		ASSERT_TRUE(LoadGlb (model, kDuck));
		mat = cgpbr::materialFromGltf (model, 0);
		ASSERT_NE(mat, nullptr);
		ASSERT_TRUE(mat->HasMap (cgpbr::MapSlot::base_color));
		w = mat->GetMap (cgpbr::MapSlot::base_color).image->width();
		h = mat->GetMap (cgpbr::MapSlot::base_color).image->height();
	}

	const cgpbr::TextureRef& base = mat->GetMap (cgpbr::MapSlot::base_color);
	ASSERT_NE(base.image, nullptr);
	EXPECT_EQ(base.image->width(),  w);
	EXPECT_EQ(base.image->height(), h);
	EXPECT_GT(w, 0u);
	EXPECT_GT(h, 0u);
}

// ---------------------------------------------------------------------------
//  6. Re-export : un MaterialPbr ressort en glTF avec ses VRAIS facteurs
// ---------------------------------------------------------------------------
// L'ecrivain GLB ne connaissait que les trois types Phong : un MaterialPbr y
// tombait dans le repli et sortait en gris par defaut. Il a desormais sa propre
// branche, et c'est un ALLER SIMPLE -- pas une projection Phong aller-retour.
//
// Les CARTES restent hors perimetre : cgimg n'a d'encodeur ni PNG ni JPEG, les
// deux seuls formats qu'un GLB accepte.

TEST(TEST_cgmesh_io_gltf_pbr, a_pbr_material_is_re_exported_with_its_own_factors)
{
	Mesh m;
	m.Init (3, 1);
	m.SetVertex (0, 0.f,  0.f, 0.f);
	m.SetVertex (1, 10.f, 0.f, 0.f);
	m.SetVertex (2, 0.f, 10.f, 0.f);
	m.SetFace (0, 0, 1, 2);
	m.ComputeNormals ();

	auto* pbr = new MaterialPbr;
	pbr->SetName ("brass");
	pbr->EditFactors().baseColor[0] = 0.85f;
	pbr->EditFactors().baseColor[1] = 0.35f;
	pbr->EditFactors().baseColor[2] = 0.20f;
	pbr->EditFactors().baseColor[3] = 1.f;
	pbr->EditFactors().metallic     = 0.9f;
	pbr->EditFactors().roughness    = 0.35f;
	pbr->EditFactors().emissive[1]  = 0.5f;
	pbr->EditFactors().alphaCutoff  = 0.25f;
	pbr->SetAlphaMode (cgpbr::AlphaMode::mask);
	pbr->SetDoubleSided (false);
	m.ApplyMaterial (m.Material_Add (pbr));

	ASSERT_EQ(MeshIO::export_glb (m, "./tu_pbr_reexport.glb"), 0);

	tinygltf::Model model;
	ASSERT_TRUE(LoadGlb (model, "./tu_pbr_reexport.glb"));
	ASSERT_EQ(model.materials.size(), 1u);
	const tinygltf::Material& out = model.materials[0];

	EXPECT_EQ(out.name, "brass");
	// Les VRAIS facteurs, sans reconversion : 0,9 et 0,35, et non le 0 / 0,9 en
	// dur des materiaux Phong.
	EXPECT_NEAR(out.pbrMetallicRoughness.metallicFactor,  0.9,  1e-6);
	EXPECT_NEAR(out.pbrMetallicRoughness.roughnessFactor, 0.35, 1e-6);
	ASSERT_EQ(out.pbrMetallicRoughness.baseColorFactor.size(), 4u);
	// baseColorFactor est LINEAIRE des deux cotes : aucune conversion.
	EXPECT_NEAR(out.pbrMetallicRoughness.baseColorFactor[0], 0.85, 1e-6);
	EXPECT_NEAR(out.pbrMetallicRoughness.baseColorFactor[1], 0.35, 1e-6);
	EXPECT_NEAR(out.pbrMetallicRoughness.baseColorFactor[2], 0.20, 1e-6);

	ASSERT_EQ(out.emissiveFactor.size(), 3u);
	EXPECT_NEAR(out.emissiveFactor[1], 0.5, 1e-6);

	EXPECT_EQ(out.alphaMode, "MASK");
	EXPECT_NEAR(out.alphaCutoff, 0.25, 1e-6);
	EXPECT_FALSE(out.doubleSided);

	// Relu en MaterialPbr, le materiau doit etre celui de depart.
	const std::unique_ptr<MaterialPbr> back = cgpbr::materialFromGltf (model, 0);
	ASSERT_NE(back, nullptr);
	EXPECT_NEAR(back->GetFactors().metallic,  0.9f,  1e-6f);
	EXPECT_NEAR(back->GetFactors().roughness, 0.35f, 1e-6f);
	EXPECT_EQ(back->GetAlphaMode(), cgpbr::AlphaMode::mask);
}

// ---------------------------------------------------------------------------
//  7. Export MTL : un MaterialPbr ne laisse plus de `usemtl` pendant
// ---------------------------------------------------------------------------
// L'ecrivain OBJ ecrit `usemtl <nom>` depuis les faces, INCONDITIONNELLEMENT,
// mais le `.mtl` ne definissait que les trois types connus. Un MaterialPbr
// laissait donc un `usemtl` sans `newmtl` correspondant : modele blanc chez tout
// lecteur, sans un mot.

TEST(TEST_cgmesh_io_gltf_pbr, a_pbr_material_gets_its_newmtl_block)
{
	Mesh m;
	m.Init (3, 1);
	m.SetVertex (0, 0.f,  0.f, 0.f);
	m.SetVertex (1, 10.f, 0.f, 0.f);
	m.SetVertex (2, 0.f, 10.f, 0.f);
	m.SetFace (0, 0, 1, 2);
	m.ComputeNormals ();

	auto* pbr = new MaterialPbr;
	pbr->SetName ("pbr_mtl");
	pbr->EditFactors().baseColor[0] = 0.8f;
	pbr->EditFactors().baseColor[1] = 0.2f;
	pbr->EditFactors().baseColor[2] = 0.2f;
	pbr->EditFactors().metallic     = 0.f;
	pbr->EditFactors().roughness    = 0.5f;
	m.ApplyMaterial (m.Material_Add (pbr));

	ASSERT_EQ(MeshIO::export_obj (m, "./tu_pbr_material.obj"), 0);

	const auto readAll = [] (const char* path) {
		std::string out;
		FILE* fp = fopen (path, "rb");
		if (!fp) return out;
		char buf[4096];
		size_t n;
		while ((n = fread (buf, 1, sizeof (buf), fp)) > 0)
			out.append (buf, n);
		fclose (fp);
		return out;
	};

	const std::string obj = readAll ("./tu_pbr_material.obj");
	const std::string mtl = readAll ("./tu_pbr_material.mtl");
	ASSERT_FALSE(obj.empty());
	ASSERT_FALSE(mtl.empty()) << "aucun .mtl ecrit";

	EXPECT_NE(obj.find ("usemtl pbr_mtl"), std::string::npos);
	EXPECT_NE(mtl.find ("newmtl pbr_mtl"), std::string::npos)
		<< "usemtl pendant : le .mtl ne definit pas le materiau cite";
	// La projection d'un dielectrique de rugosite 0,5 : Ns = 0,25 x 128 = 32.
	EXPECT_NE(mtl.find ("Ns 32."), std::string::npos) << mtl;
}
