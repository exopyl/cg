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

	GLuint    vboPositions; // GL_ARRAY_BUFFER — vec3 per vertex
	GLuint    vboNormals;   // 0 if absent
	GLuint    vboColors;    // 0 if absent (vec3)
	GLuint    vboTexCoords; // 0 if absent (vec2)
	GLuint    iboIndices;   // GL_ELEMENT_ARRAY_BUFFER

	int       count;        // 3 * m_nFaces — index count for glDrawElements
	bool      hasNormals;
	bool      hasColors;
	bool      hasTexCoords;
} vboInfo;

class VBOManager
{
public:
	VBOManager ();
	~VBOManager ();

	int addMesh (Mesh *mesh);
	void Draw (int id);

private:
	void uploadMesh(Mesh* mesh, vboInfo& info);
	void releaseBuffers(vboInfo& info);

	int m_idCurrent;
	std::map<unsigned int,vboInfo> m_mapVBO;
};
