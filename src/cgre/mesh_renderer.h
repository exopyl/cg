#pragma once

#include "../cgmesh/cgmesh.h"
#include "vertex_buffer_manager.h"


#define FLAG_POINTS      1
#define FLAG_WIREFRAME   2
#define FLAG_FILL        4
#define FLAG_NORMALIZED  8

// COMMENT la geometrie est envoyee au GPU.
//
// Trois valeurs ont ete retirees -- DISPLAY_LIST, VERTEX_ARRAY, VERTEX_BUFFER.
// Aucune n'etait jamais demandee (sinaia demande VBO, cf. wxOpenGLCanvas.cpp et
// CuttingMat.cpp), et chacune portait un defaut avere : DisplayListManager
// ecrivait hors de son tableau de 8 des le 9e maillage, sur le tas ;
// VertexArrayManager appelait glDrawElements avec 3*GetNFaces() alors que la
// source est triangulee, donc geometrie tronquee sur des quads ou lecture hors
// bornes ; VertexBufferManager fuyait un malloc par maillage et ne supprimait
// aucun objet GL -- et son VAO d'attributs generiques ne liait aucun programme,
// donc ne pouvait rien afficher.
enum CG_rendering_method {	CG_RENDERING_DEFAULT = 0,
							CG_RENDERING_VBO	};

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
	// Base de coupe : le tapis quadrille de reference (cf. sinaia/CuttingMat.h).
	// Comme display_repere et display_grid, c'est un etat de VUE parque ici, que
	// le moteur ne lit pas -- le canvas s'en sert pour decider de dessiner ou non
	// son widget, et non pour changer le rendu d'un maillage.
	int display_cutting_mat;
	int normalized;
	float pointsize;
	float linesize;
	float line_color[3];    // colour of the mesh's line ('l') primitives
	float point_color[3];   // colour of the mesh's point ('p') primitives
	int clipping_plane_active;
	float clipping_plane_z;

	// NE RIEN METTRE ICI QUI ALLOUE. Cette structure est copiee par VALEUR a
	// chaque maillage et a chaque image (MeshRenderer::SetProperties, puis le
	// chemin VBO). Elle a longtemps porte deux
	//     map<Mesh*, vector<unsigned int>>
	// -- les aretes non-manifold et les bords de TOUTE la scene -- que
	// UpdateTopologicIssues remplissait d'une entree par maillage. Le cout par
	// image etait donc QUADRATIQUE en nombre de maillages : ~8*M^2 allocations,
	// plus la recopie des indices, std::map::operator= ne recyclant pas ses
	// noeuds. Deux fichiers ordinaires ouverts ensemble -- un maillage lourd et
	// un fichier a 223 objets -- suffisaient a rendre l'application inutilisable.
	//
	// Ces donnees appartiennent a la SCENE, pas aux proprietes de rendu d'un
	// maillage. mesh_draw les lit desormais directement dans
	// MeshDataManager::GetTopologicIssues(mesh), par reference et pour le seul
	// maillage qu'il dessine, avec le cache par revision qui existait deja.

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
// `surfaceAlreadyDrawn` : la surface a deja ete emise par un chemin rapide, on
// ne dessine que les surcouches. Remplace la copie complete de `prop` que le
// chemin VBO faisait pour y mettre display_fill = 0 -- copie devenue le second
// facteur quadratique du dessin.
extern void  mesh_draw (Mesh *mesh, rendering_properties_s &prop,
                        const vector<int>& materialIds = vector<int>(),
                        bool surfaceAlreadyDrawn = false);


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

	VBOManager *m_vboManager;

	std::vector<rendering_element_s> m_meshes;
	map<Mesh*, int> m_meshToId; // New mapping
};
