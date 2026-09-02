#pragma once
// ===========================================================================
//  Charge utile « maillage -> JavaScript », PARTAGEE par les deux facades
// ===========================================================================
//
// maker a deux vues 3D, et elles lisaient le meme maillage de deux facons
// differentes :
//
//   - les PAGES (wasm_api.cpp / graphMeshData) le donnent a three.js via o3dv ;
//   - l'EDITEUR NODAL (graph_api.cpp / graphMeshView) le donne au petit
//     renderer WebGL du worker.
//
// La seconde ne transportait que positions et indices : dans l'editeur, un
// relief d'image ou des blocs pixelises sortaient donc en bloc uniforme, sans
// palette ni texture, alors que la page les montrait correctement. Ce fichier
// existe pour que la question « qu'est-ce qu'un maillage envoie au JS » n'ait
// plus qu'UNE reponse.
//
// Tout vient de Mesh::BuildPolygonRenderData, qui produit deja positions,
// normales, UV, couleurs et des indices GROUPES PAR MATERIAU avec la table de
// plages -- ecrite pour le chemin VBO d'OpenGL, un glDrawElements par materiau,
// et qui se trouve etre exactement ce dont les deux renderers ont besoin.
//
// ⚠ VUES SUR LE TAS WASM. Les tableaux sont exposes par typed_memory_view sur
// les tampons de `bufs` : ils restent valides jusqu'au prochain appel qui
// reutilise ces memes tampons. L'appelant JS doit les copier ou les televerser
// tout de suite. Seuls les pixels d'une texture sont COPIES cote JS, car une
// texture survit a l'appel qui l'a produite.
//
#ifdef __EMSCRIPTEN__

#include <emscripten/val.h>

#include <vector>

class Mesh;

namespace maker
{

// Tampons de sortie, propriete de l'appelant : c'est lui qui decide de leur
// duree de vie, et donc de la validite des vues typees.
struct MeshPayloadBuffers
{
	std::vector<float>        positions;
	std::vector<float>        normals;
	std::vector<float>        uvs;
	std::vector<float>        colors;
	std::vector<unsigned int> indices;

	void Clear ();
};

// Objet JS { positions, normals, uvs, colors, indices, groups, materials, nv, nf }.
//
// `groups` : [{ start, count, material }] -- des plages d'indices, a passer telles
// quelles a geometry.addGroup (three) ou a drawElements (WebGL).
// `materials` : une entree PAR PLAGE, dans le meme ordre, de la forme
//   { kind: "texture", width, height, rgba } | { kind: "color", r, g, b } | { kind: "none" }
// `kind: "none"` designe une face sans materiau : au JS de lui donner sa couleur
// par defaut, celle du selecteur.
//
// `mesh` nul rend un objet aux tableaux vides plutot qu'un objet nul : les deux
// appelants poursuivent alors sans cas particulier.
emscripten::val BuildMeshPayload (const Mesh *mesh, MeshPayloadBuffers &bufs);

} // namespace maker

#endif // __EMSCRIPTEN__
