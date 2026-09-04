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

// MODE D'OMBRAGE : d'ou vient la couleur des faces.
//
// Un SELECTEUR et non un booleen « materiaux oui/non » : la question « comment
// je regarde » a deja plus de deux reponses utiles dans ce moteur, et un
// booleen qu'il faudrait transformer en liste plus tard se paierait deux fois.
// Ajouter un mode = une valeur ici et un cas dans mesh_draw.
//
// Materials est la valeur ZERO, donc le defaut de toute structure remise a
// zero, et le comportement d'avant l'introduction du mode.
enum class CG_shading_mode
{
	Materials = 0,   //!< les materiaux du maillage, textures comprises
	Neutral,         //!< un materiau unique, celui des maillages sans materiau
	VertexColors     //!< les couleurs par sommet, quand le maillage en porte
};

typedef struct rendering_properties
{
	CG_shading_mode shading;
	int light;
	int smooth;
	int display_points;
	int display_vertex_normals;
	int display_face_normals;
	int display_wireframe;
	int display_fill;
	int display_warning;
	int display_repere;
	int display_grid;
	int normalized;
	float pointsize;
	float linesize;
	float line_color[3];    // colour of the mesh's line ('l') primitives
	float point_color[3];   // colour of the mesh's point ('p') primitives
	int clipping_plane_active;
	float clipping_plane_z;

	map<Mesh*, vector<unsigned int>> nonManifoldEdges;
	map<Mesh*, vector<unsigned int>> borders;

} rendering_properties_s;

typedef struct rendering_element
{
	Mesh *pMesh;
	CG_rendering_method method;
	int id;
	rendering_properties properties;

	// Material cache
	struct {
		vector<int> rendererIds;
		uint64_t revision = (uint64_t)-1;
	} materialCache;
} rendering_element_s;

void rendering_properties_init (rendering_properties_s &prop);

// direct drawing
//
extern void  mesh_draw (Mesh *mesh, rendering_properties_s &prop, const vector<int>& materialIds = vector<int>());


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
	int GetMeshId (Mesh *pMesh, CG_rendering_method method);
	void RemoveMesh (Mesh *pMesh);
	void Draw (int id);

	void SetProperties(int id, const rendering_properties_s& prop);

	// Get or update material IDs for a specific rendering element
	const vector<int>& GetMaterialRendererIds(int elementId);

private:
	static MeshRenderer *m_pInstance;

	DisplayListManager	*m_displayListManager;
	VertexArrayManager *m_vertexArrayManager;
	VBOManager *m_vboManager;
	VertexBufferManager *m_vertexBufferManager;

	std::vector<rendering_element_s> m_meshes;
	map<Mesh*, int> m_meshToId; // New mapping
};
