#pragma once

#include <map>
using namespace std;

#include "../cgmesh/cgmesh.h"

//
// ref : http://antongerdelan.net/opengl/vertexbuffers.html
//

typedef struct vbInfo
{
	GLuint id;
	unsigned int size;
} vbInfo;

class VertexBufferManager
{
public:
	VertexBufferManager ();
	~VertexBufferManager ();

	int addMesh (Mesh *mesh);
	void Draw (int id);

private:
	int m_idCurrent;
	std::map<unsigned int,vbInfo> m_mapVertexBuffer;
};

//
// vertex array
//
class VertexArrayManager
{
public:
	VertexArrayManager ();
	~VertexArrayManager ();

	int addMesh (Mesh *mesh);
	void Draw (int id);

private:
	int m_idCurrent;
	std::map<unsigned int,Mesh*> m_mapVertexArray;
};

//
//
//
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
	void Draw (int id);

	// Draw the mesh grouped by material: one glDrawElements per material run,
	// activating the matching renderer material in between. rendererIds maps a
	// mesh material index to a MaterialRenderer id (see
	// MeshRenderer::GetMaterialRendererIds).
	void DrawMaterialGroups (int id, const std::vector<int>& rendererIds, bool flat = false);

private:
	void uploadMesh(Mesh* mesh, vboInfo& info, bool flat);
	void releaseBuffers(vboInfo& info);

	int m_idCurrent;
	std::map<unsigned int,vboInfo> m_mapVBO;
};
