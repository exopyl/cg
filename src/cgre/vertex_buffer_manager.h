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
	GLuint buffers[3];
	int count;
} vboInfo;

class VBOManager
{
public:
	VBOManager ();
	~VBOManager ();

	int addMesh (Mesh *mesh);
	void Draw (int id);

private:
	int m_idCurrent;
	std::map<unsigned int,vboInfo> m_mapVBO;
};
