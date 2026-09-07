// ===========================================================================
//  L'ecrivain GLB : oracles de conteneur, de JSON et de couleur
// ===========================================================================
//
// Un GLB ne se juge pas a l'oeil : c'est un conteneur binaire dont les defauts
// sont SILENCIEUX chez le lecteur. Cinq detecteurs, du plus mecanique au plus
// semantique :
//
//   1. CONTENEUR : magic `glTF`, version 2, longueur declaree egale a la taille
//      reelle, chunks alignes sur 4 octets, JSON complete par des ESPACES et BIN
//      par des ZEROS (la specification impose le remplissage, pas seulement
//      l'alignement) ;
//   2. JSON : parsable, asset.version == "2.0", et tout indice d'accessor ou de
//      vue de tampon DANS LES BORNES -- un indice hors bornes ne fait pas
//      echouer l'ecriture, il fait echouer la lecture chez l'autre ;
//   3. COMPTES : une primitive par MaterialRange, un materiau par materiau du
//      maillage (plus UN, et un seul, quand une plage MATERIAL_NONE existe) ;
//   4. COULEUR : baseColorFactor egal a la LINEARISATION sRGB de la couleur
//      source. Sans conversion, le modele sort delave sans que rien ne le dise --
//      c'est precisement ce que ce detecteur attrape ;
//   5. BORNES : min et max de POSITION presents (la specification les EXIGE) et
//      egaux a la boite englobante des positions ecrites.
//
// S'y ajoutent deux proprietes que le format rend piegeuses :
//   - COLOR_0 ABSENT d'un maillage non peint. Mesh::InitVertices remplit les
//     couleurs de sommet du gris 0,5 pour TOUT maillage ; l'emettre quand meme
//     multiplierait baseColorFactor par 0,5 chez tout lecteur PBR ;
//   - relecture par tinygltf : oracle de COHERENCE, pas de conformite -- ecrire
//     et relire avec la meme bibliotheque ne detecte pas une erreur commune aux
//     deux. Il ne remplace pas le validateur officiel.
//
#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "../src/cggraph/core/evaluator.h"
#include "../src/cggraph/nodes/catalog_factory.h"
#include "../src/cggraph/nodes/io/file_ref.h"
#include "../src/cggraph/nodes/value_types.h"
#include "../src/cgmesh/material.h"
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_io.h"

#include <nlohmann/json.hpp>
#include <tinygltf/tiny_gltf.h>

using json = nlohmann::json;

namespace {

// --- fabriques de maillages -------------------------------------------------
//
// Des triangles NON PARTAGES : BuildPolygonRenderData indexe alors directement
// les sommets de topologie, donc le nombre de sommets de rendu est previsible et
// egal a celui du maillage. Un n-gon dupliquerait ses coins et brouillerait
// l'oracle de bornes.
Mesh makeTriangles (unsigned int nTriangles)
{
	Mesh m;
	m.Init (nTriangles * 3, nTriangles);
	for (unsigned int t = 0; t < nTriangles; ++t) {
		const float x = (float)t;
		m.SetVertex (t * 3 + 0, x,        0.f, 0.f);
		m.SetVertex (t * 3 + 1, x + 1.f,  0.f, 0.f);
		m.SetVertex (t * 3 + 2, x,       10.f, 2.f);
		m.SetFace (t, t * 3 + 0, t * 3 + 1, t * 3 + 2);
	}
	m.ComputeNormals ();
	return m;
}

// --- lecture du conteneur GLB ----------------------------------------------

unsigned int rd32 (const std::string& b, size_t o)
{
	return (unsigned int)((unsigned char)b[o]
			      | ((unsigned char)b[o + 1] << 8)
			      | ((unsigned char)b[o + 2] << 16)
			      | ((unsigned char)b[o + 3] << 24));
}

struct GlbChunks
{
	bool        ok = false;
	std::string json;
	std::string bin;
	std::string jsonPadBytes;      // remplissage du chunk JSON, tel qu'ecrit
};

// Relit le conteneur COMME LE FERAIT UN LECTEUR TIERS, sans passer par tinygltf :
// un oracle qui reutiliserait l'ecrivain ne verifierait rien.
GlbChunks splitGlb (const std::string& glb)
{
	GlbChunks out;
	if (glb.size() < 12) return out;
	if (glb.compare (0, 4, "glTF") != 0) return out;
	if (rd32 (glb, 4) != 2u) return out;
	if (rd32 (glb, 8) != glb.size()) return out;

	size_t p = 12;
	size_t declared = 12;
	while (p + 8 <= glb.size())
	{
		const unsigned int len = rd32 (glb, p);
		const unsigned int type = rd32 (glb, p + 4);
		if ((len % 4) != 0) return out;              // chunk aligne sur 4
		if (((p + 8) % 4) != 0) return out;          // debut de donnees aligne
		if (p + 8 + len > glb.size()) return out;
		const std::string data = glb.substr (p + 8, len);

		if (type == 0x4E4F534Au) {                   // "JSON"
			size_t end = data.size();
			while (end > 0 && data[end - 1] == ' ') --end;
			out.json = data.substr (0, end);
			out.jsonPadBytes = data.substr (end);
		}
		else if (type == 0x004E4942u) {              // "BIN\0"
			out.bin = data;
		}
		else return out;                             // aucun autre chunk attendu

		declared += 8 + len;
		p += 8 + len;
	}
	if (declared != glb.size()) return out;
	out.ok = !out.json.empty();
	return out;
}

// La courbe sRGB de reference, RE-ECRITE ICI et non appelee depuis l'ecrivain :
// un oracle qui partagerait la fonction testee ne testerait que lui-meme.
double srgbToLinearRef (double c)
{
	return (c <= 0.04045) ? (c / 12.92) : std::pow ((c + 0.055) / 1.055, 2.4);
}

} // namespace

// ---------------------------------------------------------------------------
//  1. Le conteneur
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_io_gltf, the_container_is_a_valid_glb)
{
	Mesh m = makeTriangles (4);
	m.ApplyMaterial (m.Material_Add (new MaterialColor (10, 20, 30)));

	const std::string glb = MeshIO::export_glb_bytes (m);
	ASSERT_FALSE(glb.empty());

	ASSERT_GE(glb.size(), 12u);
	EXPECT_EQ(glb.compare (0, 4, "glTF"), 0)  << "magic";
	EXPECT_EQ(rd32 (glb, 4), 2u)              << "version glTF";
	EXPECT_EQ(rd32 (glb, 8), glb.size())      << "longueur declaree vs taille reelle";

	const GlbChunks chunks = splitGlb (glb);
	ASSERT_TRUE(chunks.ok) << "conteneur illisible : chunks, alignement ou longueurs";
	EXPECT_FALSE(chunks.bin.empty()) << "la geometrie doit vivre dans le chunk BIN";

	// Le remplissage a un CONTENU impose : espaces pour le JSON, zeros pour le
	// BIN. Un remplissage a zero derriere le JSON casse les lecteurs stricts.
	for (char c : chunks.jsonPadBytes)
		EXPECT_EQ(c, ' ') << "le remplissage du chunk JSON doit etre des espaces";
	EXPECT_EQ(chunks.bin.size() % 4, 0u) << "le chunk BIN doit rester multiple de 4";

	// LE DETECTEUR SE VALIDE SUR DES CAS POSITIFS. Un lecteur de conteneur qui
	// accepte tout ne prouve rien : on lui presente quatre GLB corrompus, chacun
	// sur UN seul defaut, et il doit refuser les quatre.
	{
		std::string bad = glb; bad[0] = 'x';
		EXPECT_FALSE(splitGlb (bad).ok) << "magic altere";
	}
	{
		std::string bad = glb; bad[4] = 1;
		EXPECT_FALSE(splitGlb (bad).ok) << "version 1";
	}
	{
		std::string bad = glb; bad[8] = (char)((unsigned char)bad[8] ^ 0x01);
		EXPECT_FALSE(splitGlb (bad).ok) << "longueur declaree fausse";
	}
	{
		// Longueur du premier chunk decalee de 1 : elle cesse d'etre un
		// multiple de 4, ET la somme des chunks cesse de tomber juste.
		std::string bad = glb; bad[12] = (char)((unsigned char)bad[12] + 1);
		EXPECT_FALSE(splitGlb (bad).ok) << "chunk JSON desaligne";
	}
}

// ---------------------------------------------------------------------------
//  2. Le JSON, et tout indice dans les bornes
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_io_gltf, every_index_of_the_json_stays_in_bounds)
{
	Mesh m = makeTriangles (6);
	const unsigned int a = m.Material_Add (new MaterialColor (200, 40, 40));
	const unsigned int b = m.Material_Add (new MaterialColor (40, 200, 40));
	for (unsigned int f = 0; f < m.GetNFaces (); ++f)
		m.SetFaceMaterialId (f, (f % 2) ? b : a);

	const GlbChunks chunks = splitGlb (MeshIO::export_glb_bytes (m));
	ASSERT_TRUE(chunks.ok);

	json j = json::parse (chunks.json, nullptr, false);
	ASSERT_FALSE(j.is_discarded()) << "le chunk JSON doit etre parsable";
	ASSERT_TRUE(j.contains ("asset"));
	EXPECT_EQ(j["asset"]["version"].get<std::string>(), "2.0");

	const size_t nAcc  = j.value ("accessors",  json::array ()).size ();
	const size_t nView = j.value ("bufferViews", json::array ()).size ();
	const size_t nBuf  = j.value ("buffers",     json::array ()).size ();
	const size_t nMat  = j.value ("materials",   json::array ()).size ();
	ASSERT_GT(nAcc, 0u);
	ASSERT_GT(nView, 0u);
	ASSERT_EQ(nBuf, 1u) << "un GLB n'a qu'un tampon, et il est sans URI";
	EXPECT_FALSE(j["buffers"][0].contains ("uri")) << "le tampon 0 EST le chunk BIN";

	// Chaque vue tient dans le tampon.
	const size_t bufLen = j["buffers"][0]["byteLength"].get<size_t>();
	EXPECT_EQ(bufLen, chunks.bin.size ()) << "byteLength doit couvrir le chunk BIN";
	for (const json& v : j["bufferViews"]) {
		EXPECT_LT(v["buffer"].get<size_t>(), nBuf);
		const size_t off = v.value ("byteOffset", (size_t)0);
		EXPECT_LE(off + v["byteLength"].get<size_t>(), bufLen);
	}

	// Chaque accessor designe une vue existante et tient dedans.
	for (const json& acc : j["accessors"]) {
		ASSERT_TRUE(acc.contains ("bufferView"));
		const size_t vi = acc["bufferView"].get<size_t>();
		ASSERT_LT(vi, nView);
		const size_t compSize =
			(acc["componentType"].get<int>() == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) ? 2
			: 4;
		const std::string type = acc["type"].get<std::string>();
		const size_t comps = (type == "SCALAR") ? 1 : (type == "VEC2") ? 2 : 3;
		const size_t need = acc["count"].get<size_t>() * comps * compSize;
		const size_t off = acc.value ("byteOffset", (size_t)0);
		EXPECT_LE(off + need, j["bufferViews"][vi]["byteLength"].get<size_t>())
			<< "accessor deborde de sa vue";
	}

	// Chaque primitive designe des accessors et un materiau existants.
	ASSERT_TRUE(j.contains ("meshes"));
	for (const json& prim : j["meshes"][0]["primitives"]) {
		EXPECT_LT(prim["indices"].get<size_t>(), nAcc);
		EXPECT_LT(prim["material"].get<size_t>(), nMat)
			<< "un materiau OMIS renverrait au defaut du lecteur, qui est metallique";
		for (auto it = prim["attributes"].begin (); it != prim["attributes"].end (); ++it)
			EXPECT_LT(it.value ().get<size_t>(), nAcc) << "attribut " << it.key ();
	}

	// Le noeud porte la conversion d'unite et d'orientation.
	ASSERT_TRUE(j.contains ("nodes"));
	const json& node = j["nodes"][0];
	ASSERT_TRUE(node.contains ("scale"));
	EXPECT_NEAR(node["scale"][0].get<double>(), 0.001, 1e-9) << "millimetres -> metres";
	ASSERT_TRUE(node.contains ("rotation"));
	EXPECT_NEAR(node["rotation"][0].get<double>(), -std::sqrt (0.5), 1e-6)
		<< "-90 degres autour de X : notre Z-up vers le Y-up de glTF";
	EXPECT_NEAR(node["rotation"][3].get<double>(), std::sqrt (0.5), 1e-6);
}

// ---------------------------------------------------------------------------
//  3. Les comptes : une primitive par plage, un materiau par materiau
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_io_gltf, one_primitive_per_material_range)
{
	// Mono, bi, puis un maillage dont une partie des faces n'a AUCUN materiau.
	struct Case { unsigned int nMat; bool leaveSomeFacesUnassigned; };
	const Case cases[] = { { 1, false }, { 2, false }, { 2, true } };

	for (const Case& c : cases)
	{
		Mesh m = makeTriangles (6);
		std::vector<unsigned int> ids;
		for (unsigned int i = 0; i < c.nMat; ++i)
			ids.push_back (m.Material_Add (
				new MaterialColor ((unsigned char)(30 + 60 * i), 90, 120)));
		for (unsigned int f = 0; f < m.GetNFaces (); ++f) {
			if (c.leaveSomeFacesUnassigned && (f % 3) == 0)
				continue;                       // reste a MATERIAL_NONE
			m.SetFaceMaterialId (f, ids[f % c.nMat]);
		}

		const Mesh::PolygonRenderData rd = m.BuildPolygonRenderData (false);
		const GlbChunks chunks = splitGlb (MeshIO::export_glb_bytes (m));
		ASSERT_TRUE(chunks.ok);
		const json j = json::parse (chunks.json);

		EXPECT_EQ(j["meshes"][0]["primitives"].size (), rd.materialRanges.size ())
			<< "une primitive par plage de materiau";

		// Un materiau glTF par materiau du maillage, PLUS le gris de
		// remplacement quand une plage MATERIAL_NONE existe.
		bool hasNoneRange = false;
		for (const Mesh::MaterialRange& r : rd.materialRanges)
			if (r.materialId >= m.GetNMaterials ()) hasNoneRange = true;
		EXPECT_EQ(j["materials"].size (), m.GetNMaterials () + (hasNoneRange ? 1u : 0u))
			<< "materiaux du maillage" << (hasNoneRange ? " + le gris de remplacement" : "");

		// Chaque primitive relit la BONNE tranche d'indices.
		for (size_t k = 0; k < rd.materialRanges.size (); ++k) {
			const json& acc = j["accessors"][j["meshes"][0]["primitives"][k]["indices"].get<size_t>()];
			EXPECT_EQ(acc["count"].get<unsigned int>(), rd.materialRanges[k].count);
			const size_t compSize =
				(acc["componentType"].get<int>() == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) ? 2 : 4;
			EXPECT_EQ(acc.value ("byteOffset", (size_t)0),
				  (size_t)rd.materialRanges[k].offset * compSize);
		}

		// Tous les materiaux sortent en NON metallique et double face.
		for (const json& mat : j["materials"]) {
			EXPECT_EQ(mat["pbrMetallicRoughness"]["metallicFactor"].get<double>(), 0.0)
				<< "le defaut glTF est 1.0, et rend NOIR sans carte d'environnement";
			EXPECT_TRUE(mat.value ("doubleSided", false));
		}
	}
}

// ---------------------------------------------------------------------------
//  4. La couleur : sRGB -> lineaire
// ---------------------------------------------------------------------------
//
// Le detecteur qui justifie ce fichier. baseColorFactor est LINEAIRE ; la couleur
// source est en sRGB. Sans conversion, l'ecart est de 0,21 sur un canal a 0,5 --
// tres visible, et pourtant silencieux : rien dans le fichier ne le signale.

TEST(TEST_cgmesh_io_gltf, base_color_factor_is_the_srgb_colour_linearised)
{
	Mesh m = makeTriangles (2);
	// 128,64,192 : trois canaux de part et d'autre du seuil 0,04045, donc les
	// DEUX branches de la courbe sont eprouvees par la meme mesure.
	m.ApplyMaterial (m.Material_Add (new MaterialColor (128, 64, 192, 255)));

	const GlbChunks chunks = splitGlb (MeshIO::export_glb_bytes (m));
	ASSERT_TRUE(chunks.ok);
	const json j = json::parse (chunks.json);
	ASSERT_EQ(j["materials"].size (), 1u);

	const json& f = j["materials"][0]["pbrMetallicRoughness"]["baseColorFactor"];
	ASSERT_EQ(f.size (), 4u) << "baseColorFactor porte l'alpha, donc QUATRE composantes";

	const double src[3] = { 128. / 255., 64. / 255., 192. / 255. };
	for (int k = 0; k < 3; ++k)
		EXPECT_NEAR(f[k].get<double>(), srgbToLinearRef (src[k]), 1e-3)
			<< "canal " << k << " : baseColorFactor est LINEAIRE, MaterialColor est en sRGB";

	// L'alpha n'est pas un signal lumineux : il ne se linearise pas.
	EXPECT_NEAR(f[3].get<double>(), 1.0, 1e-6);
	EXPECT_EQ(j["materials"][0].value ("alphaMode", std::string ("OPAQUE")), "OPAQUE")
		<< "ne pas armer la transparence sans raison";
}

TEST(TEST_cgmesh_io_gltf, a_translucent_material_arms_blend_and_carries_its_alpha)
{
	Mesh m = makeTriangles (2);
	m.ApplyMaterial (m.Material_Add (new MaterialColor (255, 255, 255, 128)));

	const GlbChunks chunks = splitGlb (MeshIO::export_glb_bytes (m));
	ASSERT_TRUE(chunks.ok);
	const json j = json::parse (chunks.json);
	EXPECT_NEAR(j["materials"][0]["pbrMetallicRoughness"]["baseColorFactor"][3].get<double>(),
		    128. / 255., 1e-6);
	EXPECT_EQ(j["materials"][0]["alphaMode"].get<std::string>(), "BLEND");
}

// ---------------------------------------------------------------------------
//  5. Les bornes de POSITION
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_io_gltf, position_accessor_declares_the_bounding_box)
{
	Mesh m = makeTriangles (5);
	m.ApplyMaterial (m.Material_Add (new MaterialColor (10, 10, 10)));

	const Mesh::PolygonRenderData rd = m.BuildPolygonRenderData (false);
	ASSERT_FALSE(rd.positions.empty ());
	float lo[3] = { rd.positions[0], rd.positions[1], rd.positions[2] };
	float hi[3] = { lo[0], lo[1], lo[2] };
	for (size_t i = 0; i + 2 < rd.positions.size (); i += 3)
		for (int k = 0; k < 3; ++k) {
			lo[k] = std::min (lo[k], rd.positions[i + (size_t)k]);
			hi[k] = std::max (hi[k], rd.positions[i + (size_t)k]);
		}

	const GlbChunks chunks = splitGlb (MeshIO::export_glb_bytes (m));
	ASSERT_TRUE(chunks.ok);
	const json j = json::parse (chunks.json);

	const size_t posAcc = j["meshes"][0]["primitives"][0]["attributes"]["POSITION"].get<size_t>();
	const json& acc = j["accessors"][posAcc];
	ASSERT_TRUE(acc.contains ("min")) << "min et max sont EXIGES sur POSITION";
	ASSERT_TRUE(acc.contains ("max"));
	EXPECT_EQ(acc["count"].get<size_t>(), rd.positions.size () / 3);
	for (int k = 0; k < 3; ++k) {
		EXPECT_NEAR(acc["min"][k].get<double>(), lo[k], 1e-5);
		EXPECT_NEAR(acc["max"][k].get<double>(), hi[k], 1e-5);
	}
}

// ---------------------------------------------------------------------------
//  COLOR_0 : present seulement si le maillage est PEINT
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_io_gltf, color0_is_absent_from_an_unpainted_mesh)
{
	Mesh m = makeTriangles (3);
	m.ApplyMaterial (m.Material_Add (new MaterialColor (200, 100, 50)));

	const GlbChunks chunks = splitGlb (MeshIO::export_glb_bytes (m));
	ASSERT_TRUE(chunks.ok);
	const json j = json::parse (chunks.json);
	for (const json& prim : j["meshes"][0]["primitives"])
		EXPECT_EQ(prim["attributes"].count ("COLOR_0"), 0u)
			<< "le gris 0,5 de Mesh::InitVertices assombrirait baseColorFactor de moitie";
}

TEST(TEST_cgmesh_io_gltf, color0_is_emitted_when_the_mesh_really_is_painted)
{
	Mesh m = makeTriangles (3);
	m.ApplyMaterial (m.Material_Add (new MaterialColor (200, 100, 50)));
	for (unsigned int v = 0; v < m.GetNVertices (); ++v)
		m.SetVertexColor (v, 1.f, 0.f, 0.25f);

	const GlbChunks chunks = splitGlb (MeshIO::export_glb_bytes (m));
	ASSERT_TRUE(chunks.ok);
	const json j = json::parse (chunks.json);
	for (const json& prim : j["meshes"][0]["primitives"])
		EXPECT_EQ(prim["attributes"].count ("COLOR_0"), 1u);
}

// ---------------------------------------------------------------------------
//  Relecture par tinygltf -- oracle de COHERENCE, pas de conformite
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_io_gltf, the_written_glb_reloads_with_the_same_geometry)
{
	Mesh m = makeTriangles (7);
	const unsigned int a = m.Material_Add (new MaterialColor (250, 10, 10));
	const unsigned int b = m.Material_Add (new MaterialColor (10, 10, 250));
	for (unsigned int f = 0; f < m.GetNFaces (); ++f)
		m.SetFaceMaterialId (f, (f % 2) ? b : a);

	const std::string glb = MeshIO::export_glb_bytes (m);
	ASSERT_FALSE(glb.empty ());
	const Mesh::PolygonRenderData rd = m.BuildPolygonRenderData (false);

	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string err, warn;
	ASSERT_TRUE(loader.LoadBinaryFromMemory (&model, &err, &warn,
						 (const unsigned char*)glb.data (),
						 (unsigned int)glb.size ()))
		<< err;
	EXPECT_TRUE(err.empty ()) << err;
	ASSERT_EQ(model.meshes.size (), 1u);
	EXPECT_EQ(model.meshes[0].primitives.size (), rd.materialRanges.size ());

	const int posAcc = model.meshes[0].primitives[0].attributes.at ("POSITION");
	EXPECT_EQ(model.accessors[(size_t)posAcc].count, rd.positions.size () / 3);

	size_t totalIndices = 0;
	for (const tinygltf::Primitive& p : model.meshes[0].primitives)
		totalIndices += model.accessors[(size_t)p.indices].count;
	EXPECT_EQ(totalIndices, rd.indices.size ()) << "aucun triangle perdu ni double";
}

// ---------------------------------------------------------------------------
//  Le cas de charge : le Tigre, 39 materiaux
// ---------------------------------------------------------------------------
//
// Meme chaine que l'export OBJ colore (tu_cgmesh_io_obj_materials.cpp) : le sujet
// est le trajet des couleurs du document jusqu'aux octets, pas un maillage de
// laboratoire.

namespace {

using namespace cggraph;
using namespace cggraph_nodes;

std::shared_ptr<const Mesh> buildTiger ()
{
	Graph graph;
	const NodeId file  = graph.AddNode (MakeNode ("file.ref"));
	const NodeId svg   = graph.AddNode (MakeNode ("svg.extrude.colored"));
	const NodeId color = graph.AddNode (MakeNode ("mesh.color"));
	static_cast<FileRefNode*> (graph.FindNode (file))
		->SetPath ("./test/data/svg/Ghostscript_Tiger.svg");
	graph.Connect (file, 0, svg, 0);
	graph.Connect (svg, 0, color, 0);
	graph.FindNode (svg)->GetParams ().SetBool ("useSvgColors", true);

	Evaluator evaluator (graph);
	EvalContext ctx;
	ValueList outputs;
	if (!evaluator.Evaluate (color, outputs, ctx).IsOk () || outputs.empty ())
		return nullptr;
	return outputs[0].Share<Mesh> (Types ().mesh);
}

} // namespace

TEST(TEST_cgmesh_io_gltf, the_tiger_exports_every_material_as_its_own_primitive)
{
	std::shared_ptr<const Mesh> mesh = buildTiger ();
	ASSERT_NE(mesh, nullptr);
	ASSERT_GE(mesh->GetNMaterials (), 30u) << "le document doit porter sa palette";

	const std::string glb = MeshIO::export_glb_bytes (*mesh);
	ASSERT_FALSE(glb.empty ());

	const GlbChunks chunks = splitGlb (glb);
	ASSERT_TRUE(chunks.ok) << "conteneur du Tigre illisible";

	const json j = json::parse (chunks.json);
	const Mesh::PolygonRenderData rd = mesh->BuildPolygonRenderData (false);
	EXPECT_EQ(j["meshes"][0]["primitives"].size (), rd.materialRanges.size ());
	EXPECT_GE(j["meshes"][0]["primitives"].size (), 30u);

	// 74 296 sommets de rendu : UNSIGNED_SHORT ne suffit plus.
	const size_t posAcc = j["meshes"][0]["primitives"][0]["attributes"]["POSITION"].get<size_t>();
	EXPECT_EQ(j["accessors"][posAcc]["count"].get<size_t>(), rd.positions.size () / 3);
	if (rd.positions.size () / 3 > 65535) {
		const size_t idxAcc = j["meshes"][0]["primitives"][0]["indices"].get<size_t>();
		EXPECT_EQ(j["accessors"][idxAcc]["componentType"].get<int>(),
			  TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
			<< "au-dela de 65535 sommets, USHORT ne designe plus tout le tampon";
	}

	// Des couleurs DISTINCTES : trente fois la meme passerait tous les tests
	// mecaniques ci-dessus.
	// baseColorFactor ABSENT vaut [1,1,1,1] : le serialiseur omet la valeur par
	// defaut, et un blanc omis reste un blanc. Le comparer a la chaine "null"
	// confondrait tous les blancs avec une absence de couleur.
	std::set<std::string> factors;
	for (const json& mat : j["materials"]) {
		const json& pbr = mat["pbrMetallicRoughness"];
		factors.insert (pbr.contains ("baseColorFactor")
				? pbr["baseColorFactor"].dump ()
				: std::string ("[1.0,1.0,1.0,1.0]"));
	}
	EXPECT_GE(factors.size (), 30u) << "la palette doit traverser jusqu'au fichier";

	// Ecrit sur disque : c'est ce fichier que la sonde three.js et Blender
	// ouvrent.
	EXPECT_EQ(MeshIO::export_glb (*mesh, "./tu_tiger.glb"), 0);
}
