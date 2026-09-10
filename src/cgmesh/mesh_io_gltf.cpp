// ===========================================================================
//  glTF binaire (.glb) -- ecriture
// ===========================================================================
//
// Un GLB embarque sa geometrie ET ses materiaux dans un seul fichier, la ou
// l'OBJ doit referencer un .mtl compagnon. C'est la sortie coloree pour Blender,
// un viseur three.js ou Sketchfab ; les slicers ne le lisent pas, le format
// porteur de couleur pour l'impression etant le 3MF.
//
// STRUCTURE, une primitive par MaterialRange
// ------------------------------------------
// Mesh::BuildPolygonRenderData rend deja des indices GROUPES par materiau, en
// plages contigues (materialRanges). C'est exactement la granularite d'une
// primitive glTF : la geometrie est ecrite UNE fois, et chaque plage devient une
// primitive qui la relit par un accessor d'indices decale.
//
//   buffer 0 (chunk BIN)
//     bufferView 0  POSITION    nv x 12 o
//     bufferView 1  NORMAL      nv x 12 o
//     bufferView 2  TEXCOORD_0  nv x  8 o   (si le maillage porte des UV)
//     bufferView 3  COLOR_0     nv x 12 o   (si le maillage est PEINT, cf. plus bas)
//     bufferView 4  indices     ni x 2 ou 4 o
//
// Disposition PAR ATTRIBUT et non entrelacee : BuildPolygonRenderData rend deja
// quatre vecteurs contigus separes, qui s'ecrivent tels quels.
//
// DEUX PIEGES QUE LE FORMAT REND SILENCIEUX
// -----------------------------------------
// 1. baseColorFactor est LINEAIRE, MaterialColor est en sRGB. Sans la conversion,
//    le modele sort delave -- sans erreur, sans avertissement, et la difference
//    passe pour un choix esthetique. D'ou srgbToLinear ci-dessous.
// 2. metallicFactor vaut 1 PAR DEFAUT en glTF. Un materiau metallique sans carte
//    d'environnement rend NOIR. On ecrit donc 0 explicitement.
//
// UNITES ET ORIENTATION : par transformation de NOEUD
// ---------------------------------------------------
// Les unites monde du depot sont des millimetres et le monde est Z-up (dessin
// dans XY, extrusion selon Z). glTF est Y-up et le metre y est l'unite de fait.
// La conversion -- echelle 0,001 et rotation de -90 degres autour de X -- est
// portee par le NOEUD et non cuite dans les sommets : les coordonnees ecrites
// restent celles du maillage, donc identiques a celles de l'export OBJ.
//
// HORS PERIMETRE : les images. Un GLB sait les embarquer, mais cgimg n'a
// d'encodeur ni PNG ni JPEG (ImgIO::export_png est un `return -1`), soit
// precisement les deux formats qu'un GLB accepte. Un MATERIAL_TEXTURE sort donc
// avec sa teinte diffuse et sans son image.
//
// ===========================================================================

#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "material.h"
#include "material_convert.h"
#include "material_pbr.h"
#include "mesh.h"
#include "mesh_io.h"

// TINYGLTF_NO_INCLUDE_JSON (pose par CMakeLists.txt) impose d'inclure nlohmann
// avant l'en-tete. L'IMPLEMENTATION de tinygltf vit dans tinygltf_impl.cpp : la
// definir ici la rendrait absente de toute cible qui n'inclut pas ce fichier.
#include <nlohmann/json.hpp>
#include <tinygltf/tiny_gltf.h>

namespace {

// sRGB -> lineaire : la courbe vit dans material_convert.h, en exemplaire
// unique. baseColorFactor est defini dans l'espace LINEAIRE.
using cgpbr::srgbToLinear;

// Le gris par defaut de Mesh::InitVertices. Sert de reference au test « le
// maillage est-il peint ? » et de couleur au materiau de remplacement.
const float kDefaultGrey = 0.5f;

// Aligne la fin du tampon sur 4 octets. Toute vue de tampon commence donc a un
// multiple de 4, ce que la specification exige des accessors d'attributs et ce
// que le chunk BIN attend de toute facon.
void padTo4 (std::vector<unsigned char>& buf)
{
	while ((buf.size() % 4) != 0)
		buf.push_back (0);
}

// Ajoute une vue de tampon couvrant `bytes` octets a partir de la fin courante,
// et rend son indice.
int appendBufferView (tinygltf::Model& model, std::vector<unsigned char>& buf,
		      const void* data, size_t bytes, int target)
{
	padTo4 (buf);

	tinygltf::BufferView view;
	view.buffer = 0;
	view.byteOffset = buf.size();
	view.byteLength = bytes;
	view.target = target;

	const unsigned char* src = static_cast<const unsigned char*> (data);
	buf.insert (buf.end(), src, src + bytes);

	model.bufferViews.push_back (view);
	return (int)model.bufferViews.size() - 1;
}

// Accessor d'attribut flottant, sur toute la vue.
int appendFloatAccessor (tinygltf::Model& model, int bufferView, size_t count, int type)
{
	tinygltf::Accessor acc;
	acc.bufferView = bufferView;
	acc.byteOffset = 0;
	acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
	acc.count = count;
	acc.type = type;
	model.accessors.push_back (acc);
	return (int)model.accessors.size() - 1;
}

// Un materiau du maillage -> un materiau glTF.
//
// `mat` nul designe la plage MATERIAL_NONE : elle recoit un gris EXPLICITE et non
// l'absence de materiau, qui renverrait au defaut du lecteur -- lequel est
// metallique, donc noir.
// Nom de materiau tel qu'ECRIT dans le fichier : celui du materiau, ou son
// indice a defaut. Un materiau glTF sans nom est licite, mais illisible dans un
// editeur.
std::string gltfMaterialName (const Material* mat, unsigned int index)
{
	if (mat != nullptr && !mat->GetName().empty())
		return mat->GetName();
	char buf[32];
	std::snprintf (buf, sizeof(buf), "material_%u", index);
	return buf;
}

// MaterialPbr -> materiau glTF : ALLER SIMPLE, sans passer par Phong.
//
// Ce materiau EST le modele du format : ses facteurs s'ecrivent tels quels. Les
// projeter en Phong puis reconstruire perdrait alphaMode, alphaCutoff,
// doubleSided, normalScale et occlusionStrength, et arrondirait metallic et
// roughness au passage.
//
// Aucune conversion colorimetrique : baseColorFactor et emissiveFactor sont
// LINEAIRES des deux cotes. srgbToLinear ne s'applique qu'aux materiaux Phong
// du depot, qui sont en sRGB.
//
// HORS PERIMETRE : les CINQ CARTES. cgimg n'a d'encodeur ni PNG ni JPEG
// (ImgIO::export_png rend -1, src/cgimg/image_io_png.cpp), soit exactement les
// deux formats qu'un GLB accepte. Un MaterialPbr texture sort donc avec ses
// facteurs et sans ses images -- silencieusement, faute de canal pour le dire.
tinygltf::Material pbrToGltfMaterial (const MaterialPbr& src, unsigned int index)
{
	const cgpbr::Factors& f = src.GetFactors();

	tinygltf::Material out;
	out.name = gltfMaterialName (&src, index);

	out.pbrMetallicRoughness.baseColorFactor = {
		f.baseColor[0], f.baseColor[1], f.baseColor[2], f.baseColor[3]
	};
	out.pbrMetallicRoughness.metallicFactor  = f.metallic;
	out.pbrMetallicRoughness.roughnessFactor = f.roughness;
	out.emissiveFactor = { f.emissive[0], f.emissive[1], f.emissive[2] };

	// normalScale et occlusionStrength appartiennent, dans le format, a la
	// REFERENCE de texture qui les porte. Aucune carte n'etant ecrite, tinygltf
	// omet les deux objets et ces deux facteurs ne survivent pas au fichier.
	// Les poser reste juste : ils seront ecrits le jour ou les cartes le seront.
	out.normalTexture.scale       = f.normalScale;
	out.occlusionTexture.strength = f.occlusionStrength;
	out.alphaCutoff               = f.alphaCutoff;
	out.doubleSided               = src.IsDoubleSided();

	switch (src.GetAlphaMode())
	{
	case cgpbr::AlphaMode::mask:  out.alphaMode = "MASK";  break;
	case cgpbr::AlphaMode::blend: out.alphaMode = "BLEND"; break;
	case cgpbr::AlphaMode::opaque:
	default:                      out.alphaMode = "OPAQUE"; break;
	}

	return out;
}

tinygltf::Material toGltfMaterial (const Material* mat, unsigned int index)
{
	// Un materiau deja PBR ne se projette pas : il s'ecrit.
	if (const MaterialPbr* pbr = dynamic_cast<const MaterialPbr*> (mat))
		return pbrToGltfMaterial (*pbr, index);

	tinygltf::Material out;

	double r = kDefaultGrey, g = kDefaultGrey, b = kDefaultGrey, a = 1.0;
	if (const MaterialColor* col = dynamic_cast<const MaterialColor*> (mat)) {
		r = col->GetFloatRed();
		g = col->GetFloatGreen();
		b = col->GetFloatBlue();
		a = col->GetFloatAlpha();
	}
	else if (const MaterialColorExt* ext = dynamic_cast<const MaterialColorExt*> (mat)) {
		r = ext->GetDiffuse()[0];
		g = ext->GetDiffuse()[1];
		b = ext->GetDiffuse()[2];
		// MaterialColorExt nait a zero sur les quatre canaux : un alpha nul
		// n'y est pas une intention, il rendrait le modele invisible. Meme
		// garde que l'ecrivain MTL.
		a = (ext->GetDiffuse()[3] > 0.f) ? ext->GetDiffuse()[3] : 1.0;
	}
	else if (const MaterialTexture* tex = dynamic_cast<const MaterialTexture*> (mat)) {
		// L'IMAGE n'est pas embarquee (cf. l'en-tete) : seule sa teinte
		// diffuse traverse, blanche par defaut.
		r = tex->GetDiffuse()[0];
		g = tex->GetDiffuse()[1];
		b = tex->GetDiffuse()[2];
		a = tex->GetDiffuse()[3];
	}

	out.name = gltfMaterialName (mat, index);

	out.pbrMetallicRoughness.baseColorFactor = {
		srgbToLinear (r), srgbToLinear (g), srgbToLinear (b), a
	};
	// L'ALPHA n'est PAS un signal lumineux : il ne se linearise pas.

	// 0 : un materiau metallique sans carte d'environnement rend noir.
	out.pbrMetallicRoughness.metallicFactor = 0.0;
	// 0,9 : un plastique imprime credible, sans speculaire dur.
	out.pbrMetallicRoughness.roughnessFactor = 0.9;

	// ExtrudeAppendOptions::normalizeOrientation vaut false par defaut :
	// l'orientation des capots n'est pas garantie. En simple face, un capot
	// inverse serait INVISIBLE chez tout lecteur qui elimine les faces arriere.
	out.doubleSided = true;

	// BLEND seulement quand il y a vraiment de la transparence : l'armer sans
	// raison impose un tri par profondeur et degrade le rendu.
	out.alphaMode = (a < 1.0) ? "BLEND" : "OPAQUE";

	return out;
}

} // namespace

std::string MeshIO::export_glb_bytes (const Mesh& mesh)
{
	Mesh::PolygonRenderData rd = mesh.BuildPolygonRenderData (false);
	if (rd.positions.empty() || rd.indices.empty() || rd.materialRanges.empty())
		return std::string();

	const size_t nv = rd.positions.size() / 3;
	const size_t ni = rd.indices.size();

	tinygltf::Model model;
	model.asset.version = "2.0";
	model.asset.generator = "cgmesh MeshIO::export_glb";

	model.buffers.resize (1);
	std::vector<unsigned char>& bin = model.buffers[0].data;

	// --- attributs, partages par TOUTES les primitives ---------------------
	const int posView = appendBufferView (model, bin, rd.positions.data(),
					      rd.positions.size() * sizeof(float),
					      TINYGLTF_TARGET_ARRAY_BUFFER);
	const int posAcc = appendFloatAccessor (model, posView, nv, TINYGLTF_TYPE_VEC3);

	// min et max sont OBLIGATOIRES sur POSITION : le validateur officiel rejette
	// leur absence, et un lecteur qui recadre la camera s'en sert.
	{
		double lo[3] = { rd.positions[0], rd.positions[1], rd.positions[2] };
		double hi[3] = { lo[0], lo[1], lo[2] };
		for (size_t i = 0; i < nv; ++i)
			for (int k = 0; k < 3; ++k) {
				const double v = rd.positions[i * 3 + (size_t)k];
				if (v < lo[k]) lo[k] = v;
				if (v > hi[k]) hi[k] = v;
			}
		model.accessors[(size_t)posAcc].minValues = { lo[0], lo[1], lo[2] };
		model.accessors[(size_t)posAcc].maxValues = { hi[0], hi[1], hi[2] };
	}

	int normAcc = -1;
	if (rd.normals.size() == rd.positions.size()) {
		const int view = appendBufferView (model, bin, rd.normals.data(),
						   rd.normals.size() * sizeof(float),
						   TINYGLTF_TARGET_ARRAY_BUFFER);
		normAcc = appendFloatAccessor (model, view, nv, TINYGLTF_TYPE_VEC3);
	}

	int uvAcc = -1;
	if (rd.texCoords.size() == nv * 2) {
		const int view = appendBufferView (model, bin, rd.texCoords.data(),
						   rd.texCoords.size() * sizeof(float),
						   TINYGLTF_TARGET_ARRAY_BUFFER);
		uvAcc = appendFloatAccessor (model, view, nv, TINYGLTF_TYPE_VEC2);
	}

	// TEXCOORD_1 et TANGENT : ecrits SEULEMENT si le maillage les porte.
	// BuildPolygonRenderData laisse les tableaux VIDES sinon, et un attribut
	// synthetise ici mentirait sur le contenu du fichier -- un TANGENT
	// (1,0,0,1) uniforme est indiscernable d'une base tangente reelle pour le
	// lecteur, qui s'en servirait au lieu de la recalculer.
	int uv1Acc = -1;
	if (rd.texCoords1.size() == nv * 2) {
		const int view = appendBufferView (model, bin, rd.texCoords1.data(),
						   rd.texCoords1.size() * sizeof(float),
						   TINYGLTF_TARGET_ARRAY_BUFFER);
		uv1Acc = appendFloatAccessor (model, view, nv, TINYGLTF_TYPE_VEC2);
	}

	int tanAcc = -1;
	if (rd.tangents.size() == nv * 4) {
		const int view = appendBufferView (model, bin, rd.tangents.data(),
						   rd.tangents.size() * sizeof(float),
						   TINYGLTF_TARGET_ARRAY_BUFFER);
		tanAcc = appendFloatAccessor (model, view, nv, TINYGLTF_TYPE_VEC4);
	}

	// COLOR_0 SEULEMENT si le maillage est PEINT. Mesh::InitVertices remplit
	// m_vertexColors du gris 0,5 pour TOUT maillage : emettre l'attribut
	// inconditionnellement ferait MULTIPLIER baseColorFactor par 0,5 chez tout
	// lecteur PBR -- toutes les couleurs assombries de moitie, sans rien qui le
	// signale. Meme test que maker/mesh_payload.cpp.
	int colAcc = -1;
	if (rd.colors.size() == rd.positions.size()) {
		bool painted = false;
		for (size_t i = 0; i < rd.colors.size() && !painted; ++i)
			if (rd.colors[i] != kDefaultGrey) painted = true;
		if (painted) {
			const int view = appendBufferView (model, bin, rd.colors.data(),
							   rd.colors.size() * sizeof(float),
							   TINYGLTF_TARGET_ARRAY_BUFFER);
			colAcc = appendFloatAccessor (model, view, nv, TINYGLTF_TYPE_VEC3);
		}
	}

	// --- indices ------------------------------------------------------------
	// Meme regle que le viseur : au-dela de 65535 sommets, UNSIGNED_SHORT ne
	// designe plus tout le tampon.
	const bool wide = (nv > 65535);
	const int indexComponent = wide ? TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT
					: TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
	const size_t indexSize = wide ? 4u : 2u;

	std::vector<unsigned char> packed (ni * indexSize);
	if (wide) {
		std::memcpy (packed.data(), rd.indices.data(), ni * sizeof(unsigned int));
	}
	else {
		for (size_t i = 0; i < ni; ++i) {
			const unsigned short v = (unsigned short)rd.indices[i];
			std::memcpy (packed.data() + i * 2, &v, 2);
		}
	}
	const int idxView = appendBufferView (model, bin, packed.data(), packed.size(),
					      TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER);

	// --- materiaux : un par materiau du maillage ----------------------------
	// Meme table que l'ecrivain MTL, dans le meme ordre : l'indice d'un materiau
	// glTF est celui du maillage, donc une plage le designe directement.
	for (unsigned int i = 0; i < mesh.GetNMaterials(); ++i)
		model.materials.push_back (toGltfMaterial (mesh.GetMaterial (i), i));

	// La plage MATERIAL_NONE, quand elle existe, prend un materiau de remplacement
	// AJOUTE en fin de table -- omettre `material` renverrait au defaut du lecteur,
	// qui est metallique.
	int fallbackMaterial = -1;

	// --- primitives : une par plage ----------------------------------------
	tinygltf::Mesh gltfMesh;
	gltfMesh.name = "mesh";

	for (const Mesh::MaterialRange& range : rd.materialRanges)
	{
		tinygltf::Accessor acc;
		acc.bufferView = idxView;
		acc.byteOffset = (size_t)range.offset * indexSize;
		acc.componentType = indexComponent;
		acc.count = range.count;
		acc.type = TINYGLTF_TYPE_SCALAR;
		model.accessors.push_back (acc);

		tinygltf::Primitive prim;
		prim.mode = TINYGLTF_MODE_TRIANGLES;
		prim.indices = (int)model.accessors.size() - 1;
		prim.attributes["POSITION"] = posAcc;
		if (normAcc >= 0) prim.attributes["NORMAL"] = normAcc;
		if (uvAcc >= 0)   prim.attributes["TEXCOORD_0"] = uvAcc;
		if (uv1Acc >= 0)  prim.attributes["TEXCOORD_1"] = uv1Acc;
		if (tanAcc >= 0)  prim.attributes["TANGENT"] = tanAcc;
		if (colAcc >= 0)  prim.attributes["COLOR_0"] = colAcc;

		if (range.materialId < mesh.GetNMaterials())
			prim.material = (int)range.materialId;
		else {
			if (fallbackMaterial < 0) {
				model.materials.push_back (
					toGltfMaterial (nullptr, mesh.GetNMaterials()));
				fallbackMaterial = (int)model.materials.size() - 1;
			}
			prim.material = fallbackMaterial;
		}

		gltfMesh.primitives.push_back (prim);
	}
	model.meshes.push_back (gltfMesh);

	// --- noeud, scene -------------------------------------------------------
	tinygltf::Node node;
	node.mesh = 0;
	node.name = "model";
	// Millimetres -> metres.
	node.scale = { 0.001, 0.001, 0.001 };
	// -90 degres autour de X : (x,y,z) monde -> (x,z,-y) glTF, donc notre Z
	// d'epaisseur devient le Y-up de glTF.
	// Quaternion (x,y,z,w) = (sin(-45 deg), 0, 0, cos(-45 deg)).
	{
		const double s = -std::sqrt (0.5);
		const double c = std::sqrt (0.5);
		node.rotation = { s, 0.0, 0.0, c };
	}
	model.nodes.push_back (node);

	tinygltf::Scene scene;
	scene.nodes.push_back (0);
	model.scenes.push_back (scene);
	model.defaultScene = 0;

	std::ostringstream out;
	tinygltf::TinyGLTF writer;
	if (!writer.WriteGltfSceneToStream (&model, out, /*prettyPrint=*/false,
					    /*writeBinary=*/true))
		return std::string();

	return out.str();
}

int MeshIO::export_glb (const Mesh& mesh, const char *filename)
{
	if (!filename)
		return -1;

	const std::string bytes = export_glb_bytes (mesh);
	if (bytes.empty())
		return -1;

	FILE *fp = fopen (filename, "wb");
	if (!fp)
		return -1;
	const size_t written = fwrite (bytes.data(), 1, bytes.size(), fp);
	fclose (fp);
	return (written == bytes.size()) ? 0 : -1;
}
