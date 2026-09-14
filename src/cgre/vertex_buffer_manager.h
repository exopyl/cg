#pragma once

#include <map>
using namespace std;

#include "../cgmesh/cgmesh.h"

typedef struct vboInfo
{
	Mesh*     pMesh;        // kept for revision-based re-upload
	uint64_t  revision;     // last uploaded mesh revision
	bool      flat;         // last uploaded shading mode (flat vs smooth normals)

	GLuint    vboPositions; // GL_ARRAY_BUFFER — vec3 per vertex
	GLuint    vboNormals;   // 0 if absent
	GLuint    vboColors;    // 0 if absent (vec3)
	GLuint    vboTexCoords; // 0 if absent (vec2)
	GLuint    iboIndices;   // GL_ELEMENT_ARRAY_BUFFER

	int       count;        // 3 * m_nFaces — index count for glDrawElements
	bool      hasNormals;
	bool      hasColors;
	bool      hasTexCoords;

	// One run of the index buffer per material (see Mesh::MaterialRange).
	// Empty meshes / single-material meshes still draw via 'count'.
	std::vector<Mesh::MaterialRange> materialRanges;
} vboInfo;

class VBOManager
{
public:
	VBOManager ();
	~VBOManager ();

	int addMesh (Mesh *mesh);

	// RETRAIT D'UN MAILLAGE : libere ses cinq objets GL (positions, normales,
	// couleurs, UV, indices) et rend l'entree.
	//
	// Il n'existait pas. releaseBuffers etait PRIVEE et n'etait appelee que par
	// ~VBOManager -- lequel appartient au singleton MeshRenderer, cree par un
	// `new` jamais rendu, donc jamais execute. Consequence : MeshRenderer::RemoveMesh
	// se contentait de marquer l'emplacement vacant et tous les tampons restaient
	// en VRAM pour la duree du processus. Sur un maillage de 2 M de triangles,
	// c'est de l'ordre de 72 Mo abandonnes a chaque cycle recharger / fermer, sans
	// aucun message -- le module n'ayant aucun controle d'erreur GL, un
	// glBufferData finissant par manquer de memoire se serait traduit par une
	// geometrie qui disparait.
	//
	// PRECONDITION : un contexte GL du groupe de partage doit etre courant, sinon
	// glDeleteBuffers ne fait rien. C'est pourquoi ~MyGLCanvas pose le sien avant
	// sa boucle de retrait.
	//
	// Les identifiants ne sont JAMAIS reutilises (m_idCurrent ne fait
	// qu'augmenter) : un identifiant perime ne peut donc pas designer les tampons
	// d'un autre maillage, la recherche echoue simplement.
	void removeMesh (int id);

	void Draw (int id);

	// Draw the mesh grouped by material: one glDrawElements per material run,
	// activating the matching renderer material in between. rendererIds maps a
	// mesh material index to a MaterialRenderer id (see
	// MeshRenderer::GetMaterialRendererIds).
	// `useMeshMaterials` a FAUX : le materiau neutre est lie une fois, et les
	// materiaux du maillage sont ignores. Sans ce parametre, ce chemin -- celui
	// du remplissage par defaut -- continuait d'activer les textures quel que
	// soit le mode d'ombrage demande, et le mode « neutre » ne changeait rien a
	// l'ecran.
	// `useVertexColors` : mode d'ombrage « couleurs par sommet ». Le pipeline fixe
	// l'obtenait par glEnable(GL_COLOR_MATERIAL), qui n'a plus d'effet sous
	// programme lie -- le shader doit donc se le faire dire.
	// `lighting` : meme raison. glEnable/glDisable(GL_LIGHTING), pose par l'hote
	// a chaque image, est ignore des qu'un programme est lie ; sans ce parametre
	// la bascule « lighting » ne touche plus les surfaces.
	void DrawMaterialGroups (int id, const std::vector<int>& rendererIds, bool flat = false,
	                         bool useMeshMaterials = true, bool useVertexColors = false,
	                         bool lighting = true);

private:
	void uploadMesh(Mesh* mesh, vboInfo& info, bool flat);
	void releaseBuffers(vboInfo& info);

	int m_idCurrent;
	std::map<unsigned int,vboInfo> m_mapVBO;
};
