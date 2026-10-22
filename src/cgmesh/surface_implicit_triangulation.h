#pragma once

#include <cstddef>
#include <vector>

// Tampons de sortie de get_triangulation, que l'appelant libere par free. Ils sont pris par
// AllocateImplicitOutput et rendus, sur un echec, par ReleaseImplicitOutput : std::malloc
// et std::free par defaut. SetImplicitOutputAllocator les remplace (nullptr : defaut) pour
// qu'un test compte ces blocs et fasse echouer une allocation ; le remplacant doit rendre
// des blocs liberables par free. A poser hors de tout calcul en cours.
struct ImplicitOutputAllocator
{
	void *(*allocate) (std::size_t bytes) = nullptr;
	void  (*release) (void *block) = nullptr;
};
ImplicitOutputAllocator SetImplicitOutputAllocator (ImplicitOutputAllocator allocator);   // rend le precedent
void *AllocateImplicitOutput (std::size_t bytes);   // nul si l'allocation echoue
void  ReleaseImplicitOutput (void *block);

// Accumulateur de ImplicitSurface::get_triangulation : une seule definition, partagee par
// ImplicitSurface et ses sous-classes (ImplicitSurfaceTandem le derive). Pendant compute,
// le pointeur `data` d'ImplicitSurface designe cet objet (sous-objet de base d'une classe
// derivee, le cas echeant). Toute croissance passe par std::vector : une allocation ratee
// leve std::bad_alloc, que get_triangulation rattrape.
struct ImplicitTriangulation
{
	std::vector<float>        vertices;   // x, y, z de chaque sommet, rang = indice
	std::vector<unsigned int> faces;      // 3 indices par triangle
	int nvertices = 0;                    // sommets ecrits (plus grand indice + 1)
	int nfaces = 0;                       // triangles complets

	ImplicitTriangulation () = default;
	ImplicitTriangulation (const ImplicitTriangulation &) = delete;
	ImplicitTriangulation &operator= (const ImplicitTriangulation &) = delete;
	virtual ~ImplicitTriangulation () = default;

	// Rappel de sommet de compute (mc_vertex_func_t), `self` designe l'accumulateur : range
	// le coin au rang `index` (>= 0 : indice d'arete du cache ; -1 : nouveau sommet en fin)
	// et l'ajoute au triangle en cours. Vrai quand ce triangle est complet.
	static bool AddCorner (float x, float y, float z, int index, void *self);

	// Copie dans deux tampons malloc, liberes par l'appelant avec free ; un tampon vide est
	// nul. Faux, sorties a 0 et nullptr, si un malloc echoue.
	bool CopyOut (int *nvertices, float **vertices, int *nfaces, unsigned int **faces) const;

	static ImplicitTriangulation *FromData (void *data) { return static_cast<ImplicitTriangulation *> (data); }
};
