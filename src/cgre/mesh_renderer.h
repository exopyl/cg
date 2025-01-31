#pragma once

#include "../cgmesh/cgmesh.h"
#include "vertex_buffer_manager.h"
#include "display_list_manager.h"


#define FLAG_POINTS      1
#define FLAG_WIREFRAME   2
#define FLAG_FILL        4
#define FLAG_NORMALIZED  8

enum CG_rendering_method {	CG_RENDERING_DEFAULT = 0,
							CG_RENDERING_DISPLAY_LIST,
							CG_RENDERING_VERTEX_ARRAY,
							CG_RENDERING_VBO,
							CG_RENDERING_VERTEX_BUFFER	};

typedef struct rendering_properties
{
	int light;
	int display_points;
	int display_vertex_normals;
	int display_face_normals;
	int display_fill;
	int display_warning;
	int normalized;
	float pointsize;
	float linesize;

	int display_wireframe;
	map<Mesh*, vector<unsigned int>> nonManifoldEdges;
	map<Mesh*, vector<unsigned int>> borders;

} rendering_properties_s;

typedef struct rendering_element
{
	Mesh *pMesh;
	CG_rendering_method method;
	int id;
	rendering_properties properties;
} rendering_element_s;

void rendering_properties_init (rendering_properties_s &prop);

//
// direct drawing
//
extern void  mesh_draw (Mesh *mesh, rendering_properties_s &prop);


//
//
//
class MeshRenderer
{
private:
	MeshRenderer();
	~MeshRenderer ();
public:
	static MeshRenderer* getInstance (void) { return m_pInstance; };

	int AddMesh (Mesh *pMesh, CG_rendering_method method);
	void Draw (int id);

private:
	static MeshRenderer *m_pInstance;
	
	DisplayListManager	*m_displayListManager;
	VertexArrayManager *m_vertexArrayManager;
	VBOManager *m_vboManager;
	VertexBufferManager *m_vertexBufferManager;

	unsigned int m_nMeshes;
	rendering_element_s m_meshes[8];
};
