#include <stdio.h>
#include <stdlib.h>

#include "polygon2.h"
#ifdef USE_GLUTESS
#include "glutess/sgi-glu.h"
#endif

// Callbacks for "polygon_merge_contours"
struct glutess_state {
	Polygon2 *polygon;
	unsigned int n_vertices_per_contour;
	unsigned int ntmp;
	unsigned int current_contour;
	unsigned int current_vertex;
	double *tmp;
};

static void glutess_begin(int type, void *user_data)
{
	struct glutess_state *state = (struct glutess_state *) user_data;
	Polygon2 *polygon = state->polygon;

	printf ("begin contour %d\n", state->current_contour);

	if (state->current_contour >= polygon->m_contours.size()) {
		printf ("realloc polygon\n");
		polygon->m_contours.resize (polygon->m_contours.size() + 1);
	}

	polygon->m_contours[state->current_contour].assign (state->n_vertices_per_contour, {});

	state->current_vertex = 0;
	(void) type;
}

static void glutess_end(void *user_data)
{
	struct glutess_state *state = (struct glutess_state *) user_data;
	Polygon2 *polygon = state->polygon;

	polygon->m_contours[state->current_contour].resize (state->current_vertex);
	state->current_contour++;
	// optional but cleaner : resize correctly the size of polygon->pContours
}

static void glutess_vertex(void *vertex_data, void *user_data)
{
	struct glutess_state *state = (struct glutess_state *) user_data;
	Polygon2 *polygon = state->polygon;

	unsigned long ntmp = (unsigned long) vertex_data;
	double *coords = (double*)&state->tmp[3*ntmp];

	if (state->current_vertex >= polygon->m_contours[state->current_contour].size()) {
		polygon->m_contours[state->current_contour].resize (state->current_vertex + 1);
	}

	polygon->set_point (state->current_contour, state->current_vertex, coords[0], coords[1]);

	state->current_vertex++;
}

static void glutess_error(int errno, void *user_data)
{
	(void) user_data;

	printf ("Tessellation Error: %d\n", errno);
}

static void glutess_combine(double coords[3],
			    void *vertex_data[4],
			    float weight[4],
			    void **outData,
			    void *user_data)
{
	//dbg ("COMBINE : %f %f", coords[0], coords[1]);
	(void) vertex_data;
	(void) weight;

	struct glutess_state *state = (struct glutess_state *) user_data;

	state->tmp = (double*)realloc (state->tmp, (state->ntmp+1) * 2 * sizeof(double));
	state->tmp[2*state->ntmp+0] = coords[0];
	state->tmp[2*state->ntmp+1] = coords[1];
	//state->tmp[3*state->ntmp+2] = coords[2];

	*outData = (void *)(unsigned long) state->ntmp;
	state->ntmp++;
}

//
// merge contours defining a polygon to avoid overlapping contours
//
// ATTENTION : CETTE FONCTION N'EST PAS IMPLEMENTEE, et elle le DIT desormais.
//
// Elle rendait 0 -- succes -- sans rien faire, parce que la totalite de son
// corps vit sous `#ifdef USE_GLUTESS`, drapeau qui n'est defini nulle part dans
// le depot. Un appelant recevait donc un accuse de bonne fin sur une fusion de
// contours qui n'avait pas eu lieu. C'est le defaut que `debt_cgmesh.md` decrit
// au § 4 et qu'il resume ainsi : un talon qui rend « succes » est plus dangereux
// qu'une absence de code.
//
// POURQUOI LE CORPS N'EST PAS SIMPLEMENT REACTIVE, contrairement a ce que la
// fiche de dette proposait. Trois obstacles constates, et non supposes :
//
//   1. il inclut `glutess/sgi-glu.h`, qui N'EXISTE PAS dans extern/glutess. Le
//      fichier voisin qui se sert reellement de la bibliotheque
//      (polygon2_tesselation.cpp) inclut `glutess/glutess.h` ;
//   2. il contient `return nullptr;` dans une fonction qui rend un `int`. Ce
//      corps n'a donc JAMAIS ete compile, sous aucune configuration ;
//   3. il utilise l'API GLU historique (gluNewTess, gluTessCallback...) alors
//      que le glutess versionne du depot expose la sienne.
//
// Le reanimer n'est donc pas un retrait de garde : c'est un portage, sur 63
// lignes dont la correction n'a jamais ete verifiee par quoi que ce soit. Ce
// n'est pas ce qu'on fait a l'occasion d'un nettoyage d'honnetete.
//
// Le corps est CONSERVE, et non supprime : il porte l'intention algorithmique
// (fusion de contours par regle de remplissage non nulle, contours de sortie
// seulement), et le depot n'est pas sous gestion de version -- le supprimer le
// perdrait. Son unique appelant est le test qui constate ce talon.
//
int Polygon2::clean (Polygon2* polygon)
{
#ifdef USE_GLUTESS
	if (polygon == nullptr || polygon->m_contours.empty())
		return -1;

	// polygon merged
	m_contours.assign (5*polygon->m_contours.size(), {});

	// compute the number of vertices
	unsigned int nVertices = 0;
	for (unsigned int i=0; i<polygon->m_contours.size(); i++)
		nVertices += (unsigned int)polygon->m_contours[i].size();

	double *coords = (double*) malloc(2 * sizeof(double) * nVertices);
	if (coords == nullptr)
		return nullptr;

	struct glutess_state state;
	state.polygon = this;//polygon_merged;
	state.n_vertices_per_contour = 2*nVertices;
	state.ntmp = nVertices;
	state.tmp = coords;
	state.current_contour = 0;

	GLUtesselator *tess = gluNewTess();

	gluTessCallback(tess, GLU_TESS_BEGIN_DATA, (void(*)()) &glutess_begin);
	gluTessCallback(tess, GLU_TESS_VERTEX_DATA, (void(*)()) &glutess_vertex);
	gluTessCallback(tess, GLU_TESS_END_DATA, (void(*)()) &glutess_end);
	gluTessCallback(tess, GLU_TESS_ERROR_DATA, (void(*)()) &glutess_error);
	gluTessCallback(tess, GLU_TESS_COMBINE_DATA, (void(*)()) &glutess_combine);

	gluTessNormal(tess, 0, 0, 1);

	gluTessProperty(tess, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_NONZERO);
	gluTessProperty(tess, GLU_TESS_BOUNDARY_ONLY, 1);
	//gluTessProperty(tess, GLU_TESS_TOLERANCE, 0.0);
	
	gluTessBeginPolygon(tess, &state);
	unsigned int iVertex = 0;
	for (unsigned int j=0; j<polygon->m_contours.size(); j++)
	{
		gluTessBeginContour(tess);
		for (unsigned int i=0; i<polygon->m_contours[j].size(); i++)
		{
			state.tmp[2 * iVertex + 0] = (double) polygon->m_contours[j][i].x;
			state.tmp[2 * iVertex + 1] = (double) polygon->m_contours[j][i].y;
			//state.tmp[3 * iVertex + 2] = (double) polygon->m_contours[j][i].z;
			//dbg ("adding %d : %f %f %f", i, coords[3 * iVertex + 0], coords[3 * iVertex + 1], coords[3 * iVertex + 2]);

			gluTessVertex(tess, &state.tmp[2 * iVertex], (void *)(unsigned long) iVertex);
			iVertex++;
		}
		gluTessEndContour(tess);
	}
	gluTessEndPolygon(tess);

	gluDeleteTess(tess);

	// cleaning
	free(state.tmp);
	return 0;
#else
	(void) polygon;
	// -1, et non 0 : c'est la valeur d'echec que le corps ci-dessus emploie
	// lui-meme pour une entree invalide. Un appelant qui la teste apprend que
	// rien n'a ete fusionne, au lieu de le croire fait.
	fprintf (stderr, "Polygon2::clean : non implemente (USE_GLUTESS absent)\n");
	return -1;
#endif
}
