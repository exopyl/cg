#include "gl_wrapper.h"

#include "vertex_buffer_manager.h"

VertexBufferManager::VertexBufferManager()
{
	m_idCurrent = 0;
}

VertexBufferManager::~VertexBufferManager()
{
	m_idCurrent = 0;
}

int VertexBufferManager::addMesh (Mesh *mesh)
{
	unsigned int* pIndices = mesh->GetTriangles();
	if (!pIndices)
		return -1;

	int nVertices = mesh->m_nVertices;
	int nTriangles = mesh->m_nFaces;
	float* pVertices = mesh->m_pVertices;
	float* pVertexNormals = mesh->m_pVertexNormals;

	GLfloat* data = (GLfloat*)malloc(6*nVertices*sizeof(GLfloat));
	for (int i=0; i<nVertices; i++)
	{
		data[6*i]   = pVertices[3*i];
		data[6*i+1] = pVertices[3*i+1];
		data[6*i+2] = pVertices[3*i+2];
		data[6*i+3] = pVertexNormals[3*i];
		data[6*i+4] = pVertexNormals[3*i+1];
		data[6*i+5] = pVertexNormals[3*i+2];
	}
	unsigned int* indices = mesh->GetTriangles();

	GLuint id;
	glGenVertexArrays(1, &id);
	glBindVertexArray(id);

	GLuint m_indicesBuf, m_bufHandle;
	glGenBuffers(1, &m_indicesBuf);
	glGenBuffers(1, &m_bufHandle);

	glBindBuffer(GL_ARRAY_BUFFER, m_bufHandle);
	glBufferData(GL_ARRAY_BUFFER, 6*nVertices*sizeof(GLfloat), data, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indicesBuf);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3*nTriangles*sizeof(unsigned int), indices, GL_STATIC_DRAW);

	glBindVertexArray(0);

	// store info
	vbInfo info = {id, 3*nTriangles};
	m_mapVertexBuffer[m_idCurrent++] = info;

	// clean
	free (pIndices);

	return m_idCurrent-1;
}


void VertexBufferManager::Draw (int id)
{
	vbInfo info = m_mapVertexBuffer[id];

	glBindVertexArray(info.id);
	glDrawElements(GL_TRIANGLES, info.size, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}


//
// Vertex Array
//
// http://raptor.developpez.com/tutorial/opengl/vbo/
//
VertexArrayManager::VertexArrayManager()
{
	m_idCurrent = 0;
}

VertexArrayManager::~VertexArrayManager()
{
	m_idCurrent = 0;
}

int VertexArrayManager::addMesh (Mesh *mesh)
{
	m_mapVertexArray[m_idCurrent++] = mesh;
	return m_idCurrent-1;
}


void VertexArrayManager::Draw (int id)
{
	Mesh* mesh = m_mapVertexArray[id];
	if (!mesh)
		return;

	unsigned int* pIndices = mesh->GetTriangles();
	float* pVertices = mesh->m_pVertices;
	assert (pIndices);

	bool bHasNormals = (mesh->m_pVertexNormals)? true : false;
	bool bHasColors = (mesh->m_pVertexColors)? true : false;

	glEnableClientState(GL_VERTEX_ARRAY);
	if (bHasNormals)
		glEnableClientState(GL_NORMAL_ARRAY);
	if (bHasColors)
		glEnableClientState(GL_COLOR_ARRAY);

	glVertexPointer(3, GL_FLOAT, 0, mesh->m_pVertices);
	if (bHasNormals)
		glNormalPointer (GL_FLOAT, 0, mesh->m_pVertexNormals);
	if (bHasColors)
		glColorPointer(3, GL_FLOAT, 0, mesh->m_pVertexColors);

	glDrawElements(GL_TRIANGLES, 3*mesh->m_nFaces, GL_UNSIGNED_INT, pIndices);

	if (bHasColors)
		glDisableClientState(GL_COLOR_ARRAY);
	if (bHasNormals)
		glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
}

//
// VBO
//
// http://raptor.developpez.com/tutorial/opengl/vbo/
//
VBOManager::VBOManager()
{
	m_idCurrent = 0;
}

VBOManager::~VBOManager()
{
	m_idCurrent = 0;
}

int VBOManager::addMesh (Mesh *mesh)
{
	vboInfo info;

	unsigned int* pIndices = mesh->GetTriangles();
	float* pVertices = mesh->m_pVertices;
	if (!pIndices)
		return -1;

	bool bHasNormals = (mesh->m_pVertexNormals)? true : false;
	bool bHasColors = (mesh->m_pVertexColors)? true : false;



	// Génération des buffers
	glGenBuffers(3, info.buffers);

	// Buffer d'informations de vertex
	glBindBuffer(GL_ARRAY_BUFFER, info.buffers[0]);
	glBufferData(GL_ARRAY_BUFFER, 3*mesh->m_nVertices*sizeof(float), mesh->m_pVertices, GL_STATIC_DRAW);

	// indices
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, info.buffers[1]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3*mesh->m_nFaces*sizeof(unsigned int), pIndices, GL_STATIC_DRAW);

	// normals
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, info.buffers[2]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3*mesh->m_nVertices*sizeof(float), mesh->m_pVertexNormals, GL_STATIC_DRAW);

	// colors
	if (0 && mesh->m_pVertexColors)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, info.buffers[2]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3*mesh->m_nVertices*sizeof(float), mesh->m_pVertexColors, GL_STATIC_DRAW);
	}

	
	info.count = 3*mesh->m_nFaces;

	m_mapVBO[m_idCurrent++] = info;

	// clean
	free(pIndices);

	return m_idCurrent-1;
}


void VBOManager::Draw (int id)
{
	vboInfo info = m_mapVBO[id];

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	//glEnableClientState(GL_COLOR_ARRAY);
	
	glBindBuffer(GL_ARRAY_BUFFER, info.buffers[0]);
	glVertexPointer(3, GL_FLOAT, 3 * sizeof(float), 0);

	glBindBuffer(GL_ARRAY_BUFFER, info.buffers[2]);
	glNormalPointer(GL_FLOAT, 0, 0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, info.buffers[1]);
	glDrawElements(GL_TRIANGLES, info.count, GL_UNSIGNED_INT, 0);

	// clean
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);

	//glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
}
