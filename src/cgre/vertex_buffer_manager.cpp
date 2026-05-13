#include "gl_wrapper.h"

#include "vertex_buffer_manager.h"
#include "../cgmesh/mesh_data_manager.h"

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
	float* pVertices = mesh->m_pVertices.data();
	float* pVertexNormals = mesh->m_pVertexNormals.data();

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

	const std::vector<unsigned int>& triangles = MeshDataManager::GetInstance().GetTriangles(mesh);
	if (triangles.empty())
		return;

	float* pVertices = mesh->m_pVertices.data();

	const bool bHasNormals   = !mesh->m_pVertexNormals.empty();
	const bool bHasColors    = !mesh->m_pVertexColors.empty();
	const bool bHasTexCoords = !mesh->m_pTextureCoordinates.empty();

	glEnableClientState(GL_VERTEX_ARRAY);
	if (bHasNormals)
		glEnableClientState(GL_NORMAL_ARRAY);
	if (bHasColors)
		glEnableClientState(GL_COLOR_ARRAY);
	if (bHasTexCoords)
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(3, GL_FLOAT, 0, mesh->m_pVertices.data());
	if (bHasNormals)
		glNormalPointer (GL_FLOAT, 0, mesh->m_pVertexNormals.data());
	if (bHasColors)
		glColorPointer(3, GL_FLOAT, 0, mesh->m_pVertexColors.data());
	if (bHasTexCoords)
		glTexCoordPointer(2, GL_FLOAT, 0, mesh->m_pTextureCoordinates.data());

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1.0, 1.0);

	glDrawElements(GL_TRIANGLES, 3*mesh->m_nFaces, GL_UNSIGNED_INT, triangles.data());

	glDisable(GL_POLYGON_OFFSET_FILL);

	if (bHasTexCoords)
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	if (bHasColors)
		glDisableClientState(GL_COLOR_ARRAY);
	if (bHasNormals)
		glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
}

//
// VBO
//
// Server-side buffers (positions, normals, optional UVs, optional colors,
// triangle indices) driving glDrawElements via the fixed-function client-
// state API. Buffers are rebuilt on demand whenever the underlying Mesh's
// revision counter changes — keeps the visual in sync with in-place edits
// from the remote console (e.g. the `flip` command).
//
VBOManager::VBOManager()
{
	m_idCurrent = 0;
}

VBOManager::~VBOManager()
{
	for (auto& kv : m_mapVBO)
		releaseBuffers(kv.second);
	m_mapVBO.clear();
	m_idCurrent = 0;
}

void VBOManager::releaseBuffers(vboInfo& info)
{
	GLuint ids[] = { info.vboPositions, info.vboNormals,
	                 info.vboColors,    info.vboTexCoords,
	                 info.iboIndices };
	for (GLuint& id : ids)
	{
		if (id != 0)
		{
			glDeleteBuffers(1, &id);
			id = 0;
		}
	}
	info.vboPositions = 0;
	info.vboNormals   = 0;
	info.vboColors    = 0;
	info.vboTexCoords = 0;
	info.iboIndices   = 0;
}

void VBOManager::uploadMesh(Mesh* mesh, vboInfo& info)
{
	info.pMesh = mesh;

	// Build the expanded per-polygon vertex layout: every face contributes
	// its own N corners, each carrying the face's polygon normal. Adjacent
	// faces do NOT share corners — this is what guarantees uniform shading
	// within each polygon and eliminates fan-diagonal kinks on non-planar
	// n-gons.
	const Mesh::PolygonRenderData rd = mesh->BuildPolygonRenderData();

	info.hasNormals   = !rd.normals.empty();
	info.hasColors    = !rd.colors.empty();
	info.hasTexCoords = !rd.texCoords.empty();

	const size_t nRenderVerts = rd.positions.size() / 3;

	// Positions
	if (info.vboPositions == 0) glGenBuffers(1, &info.vboPositions);
	glBindBuffer(GL_ARRAY_BUFFER, info.vboPositions);
	glBufferData(GL_ARRAY_BUFFER, rd.positions.size() * sizeof(float),
	             rd.positions.data(), GL_STATIC_DRAW);

	if (info.hasNormals)
	{
		if (info.vboNormals == 0) glGenBuffers(1, &info.vboNormals);
		glBindBuffer(GL_ARRAY_BUFFER, info.vboNormals);
		glBufferData(GL_ARRAY_BUFFER, rd.normals.size() * sizeof(float),
		             rd.normals.data(), GL_STATIC_DRAW);
	}
	else if (info.vboNormals != 0)
	{
		glDeleteBuffers(1, &info.vboNormals);
		info.vboNormals = 0;
	}

	if (info.hasColors)
	{
		if (info.vboColors == 0) glGenBuffers(1, &info.vboColors);
		glBindBuffer(GL_ARRAY_BUFFER, info.vboColors);
		glBufferData(GL_ARRAY_BUFFER, rd.colors.size() * sizeof(float),
		             rd.colors.data(), GL_STATIC_DRAW);
	}
	else if (info.vboColors != 0)
	{
		glDeleteBuffers(1, &info.vboColors);
		info.vboColors = 0;
	}

	if (info.hasTexCoords)
	{
		if (info.vboTexCoords == 0) glGenBuffers(1, &info.vboTexCoords);
		glBindBuffer(GL_ARRAY_BUFFER, info.vboTexCoords);
		glBufferData(GL_ARRAY_BUFFER, rd.texCoords.size() * sizeof(float),
		             rd.texCoords.data(), GL_STATIC_DRAW);
	}
	else if (info.vboTexCoords != 0)
	{
		glDeleteBuffers(1, &info.vboTexCoords);
		info.vboTexCoords = 0;
	}

	if (!rd.indices.empty())
	{
		if (info.iboIndices == 0) glGenBuffers(1, &info.iboIndices);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, info.iboIndices);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		             rd.indices.size() * sizeof(unsigned int),
		             rd.indices.data(), GL_STATIC_DRAW);
		info.count = (int)rd.indices.size();
	}
	else
	{
		info.count = 0;
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	info.revision = mesh->GetRevision();
	(void)nRenderVerts;
}

int VBOManager::addMesh (Mesh *mesh)
{
	if (!mesh) return -1;

	vboInfo info{};
	uploadMesh(mesh, info);
	if (info.iboIndices == 0)
		return -1;

	m_mapVBO[m_idCurrent++] = info;
	return m_idCurrent - 1;
}


void VBOManager::Draw (int id)
{
	auto it = m_mapVBO.find(id);
	if (it == m_mapVBO.end())
		return;
	vboInfo& info = it->second;

	// Pick up in-place mesh mutations (e.g. RemoteConsole `flip`).
	if (info.pMesh && info.pMesh->GetRevision() != info.revision)
		uploadMesh(info.pMesh, info);

	glEnableClientState(GL_VERTEX_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, info.vboPositions);
	glVertexPointer(3, GL_FLOAT, 0, nullptr);

	if (info.hasNormals)
	{
		glEnableClientState(GL_NORMAL_ARRAY);
		glBindBuffer(GL_ARRAY_BUFFER, info.vboNormals);
		glNormalPointer(GL_FLOAT, 0, nullptr);
	}

	if (info.hasColors)
	{
		glEnableClientState(GL_COLOR_ARRAY);
		glBindBuffer(GL_ARRAY_BUFFER, info.vboColors);
		glColorPointer(3, GL_FLOAT, 0, nullptr);
	}

	if (info.hasTexCoords)
	{
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glBindBuffer(GL_ARRAY_BUFFER, info.vboTexCoords);
		glTexCoordPointer(2, GL_FLOAT, 0, nullptr);
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, info.iboIndices);

	// Offset the surface so wireframe / vertex normals overlays don't z-fight
	// (parity with VertexArrayManager::Draw).
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1.0f, 1.0f);

	glDrawElements(GL_TRIANGLES, info.count, GL_UNSIGNED_INT, nullptr);

	glDisable(GL_POLYGON_OFFSET_FILL);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	if (info.hasTexCoords) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	if (info.hasColors)    glDisableClientState(GL_COLOR_ARRAY);
	if (info.hasNormals)   glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
}
