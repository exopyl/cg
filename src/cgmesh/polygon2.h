#pragma once
#include <vector>
#include <cgmath/TVector2.h>

#define SIGNATURE_DEVIATION 0
#define SIGNATURE_CURVATURE 1
#define SIGNATURE_BOUTIN    2

#define INTERPOLATION_LINEAR 0
#define INTERPOLATION_COSINE 1

// Regle d'enroulement de la tessellation (GLU_TESS_WINDING_*) : un point est
// dans la region si la somme des enroulements des contours autour de lui est
// impaire (Odd), non nulle (NonZero), strictement positive (Positive),
// strictement negative (Negative), ou de valeur absolue au moins 2 (AbsGeqTwo).
// Positive retire d'une region les contours inverses (sens horaire) sans les
// ajouter la ou ils debordent : c'est la difference A - B de deux contours
// directs A et B quand B est donne a l'envers.
enum class WindingRule { Odd, NonZero, Positive, Negative, AbsGeqTwo };

//
// Contour 2D
//
class Polygon2
{
public:
	Polygon2 () = default;
	// Copy-ctor, copy-assignment and destructor are the compiler-generated
	// ones: std::vector handles deep copy and release (rule of zero).

	// input
	void input (float *pPoints, int n);
	void input (float *x, float *y, int n);
	void input (Polygon2 *contour, int interpolation_type, int nn); // parameterize an existing contour
	void input (char *filename);
private:
	void input_from_svg_path (char *path);

public:
	// edit
	float* add_contour (unsigned int index, unsigned int nPoints, float *pPoints);
	int add_polygon2d (Polygon2 *pol);

	// output
	void dump ();
	void output (char *filename);

	int set_point (unsigned int iContour, unsigned int iPoint, float x, float y);
	int get_point (unsigned int iContour, unsigned int iPoint, float *x, float *y);

	// Surface de LECTURE, const. Ces quatre-la ne modifient rien -- la lecture
	// de leurs corps le montre, tesselate comprise : elle remplit des tampons
	// que l'appelant possede et ne touche pas a m_contours. La non-constance
	// etait donc un defaut de DECLARATION, pas une contrainte de corps, et elle
	// interdisait a un consommateur tenant un polygone immuable de les appeler.
	//
	// get_points garde sa surcharge non const : elle donne un acces en ECRITURE
	// sur les points, dont plusieurs algorithmes de ce fichier se servent.
	int    get_n_contours (void) const { return (int)m_contours.size(); };
	const float* get_points (unsigned int iContour) const { return (const float*)m_contours[iContour].data(); };
	float* get_points (unsigned int iContour) { return (float*)m_contours[iContour].data(); };
	int    get_n_points (unsigned int iContour) const { return (int)m_contours[iContour].size(); };
	int    get_n_points (void) const {
		int nPoints = 0;
		for (unsigned int i=0; i<m_contours.size(); i++)
			nPoints += get_n_points (i);
		return nPoints; };
	
	void  get_bbox (float *xmin, float *xmax, float *ymin, float *ymax);
	float length (int interpolation_type);
	float area   (void);
	float area   (int i1, int i2, int i3);
	
	void smooth (void);
	void flip_x (void);
	void apply_PCA (void);
	void translate (float tx, float ty);
	void rotate (float angle);
	void centerize (void);
	void center (float *xc, float *yc);
	
	int is_point_inside (float x, float y);

	// order of the points
	void inverse_order (void);
	int  is_trigonometric_order (void);
	
	int  generalized_barycentric_coordinates (float pt[2], float *coords);

	// moments
	void moment_0 (float m[1]);
	void moment_1 (float m[2]);
	void moment_2 (float m[3], bool centralized=false);
	void moment_3 (float m[4]);
	
	// symmetry
	void search_symmetry_zabrodsky (float *xc, float *yc, float *angle);
	void search_symmetry_signature (int signature_type, int interpolation_type, int nbins);
	void search_symmetry_pridmore (void);
	void search_symmetry_pridmore_bis (void);
	
	// matching
	float matching_arkin (Polygon2 *contour, int nn);
	
	// clean a polygon
	int clean (Polygon2* polygon);

	// tesselation
	// Sommets : les points des contours dans l'ordre, puis ceux que glutess cree
	// (croisements, et une copie par fusion de deux points confondus) ; z = 0.
	// Triangles en sens trigonometrique. Tampons alloues par malloc, a liberer
	// par l'appelant. La regle par defaut est NonZero.
	int tesselate (float **pVertices, unsigned int *nVertices,
		       unsigned int **pFaces, unsigned int *nFaces,
		       WindingRule rule = WindingRule::NonZero) const;

	// Bord de la region que `rule` retient (GLU_TESS_BOUNDARY_ONLY) : un contour
	// par boucle du bord, region a gauche (enveloppes en sens trigonometrique,
	// trous en sens horaire). Les sommets sont ceux des contours d'entree qui
	// restent sur le bord et les croisements que glutess cree. `out` est
	// remplace. Rend le nombre de contours, ou -1 si glutess signale une
	// erreur (`out` reste vide).
	int boundary (WindingRule rule, Polygon2 &out) const;

	void thicken (Polygon2* polygon, float ithickness, float othickness, int bOpen=1);
	void dilate (Polygon2* polygon, float d);

	void extrude (float fHeight, char *filename);
	
//private:
	int alloc_contours (int nContours);

	//
	// Storage: one std::vector<Vector2f> per contour. Vector2f is standard-layout
	// (two contiguous floats), so (float*)m_contours[c].data() aliases the flat
	// interleaved x,y array the public float* API exposes.
	std::vector<std::vector<Vector2f>> m_contours;
};

// Triangles plats d'une tessellation (sommets en x, y, z, triangles en indices) :
// un triangle A-B-C dont B est aligne entre A et C est remplace, avec son voisin
// A-C-D le long de AC, par A-B-D et B-C-D, quand ces deux triangles sont non plats
// et du sens de A-C-D. Seul le tableau de faces change. Rend le nombre de
// bascules. Polygon2::tesselate l'applique a sa sortie.
unsigned int flipFlatTriangles (const float *vertices, unsigned int *faces, unsigned int nFaces);

// Bascules de Lawson vers la triangulation de Delaunay contrainte (x, y) : une
// arete interieure (exactement deux demi-aretes de sens opposes, portees par
// deux triangles de meme sens) est basculee quand le sommet oppose de l'un est
// strictement dans le cercle circonscrit de l'autre, au-dela d'un seuil
// relatif, que les deux anciens triangles sont non plats et de meme sens, et
// que les deux nouveaux le sont aussi, du meme sens. Les aretes de bord (une
// seule demi-arete) et les aretes portees par plus de deux triangles ne bougent
// jamais. Seul le tableau de faces change ; chaque triangle garde son sens.
//
// Le seuil relatif rend les configurations cocirculaires (rectangles, grilles)
// stables : sans lui, l'arrondi, qui depend du compilateur (contraction FMA),
// deciderait de leurs bascules, et le maillage changerait d'une plateforme a
// l'autre.
//
// Ordre attendu : Polygon2::tesselate (qui applique deja flipFlatTriangles),
// puis delaunayFlips. Un triangle plat n'est jamais bascule.
//
// Cout : Lawson part de la tessellation et peut demander un nombre quadratique
// de bascules (anneau mince a milliers de sommets). Le travail est borne a 64
// visites de pile par demi-arete ; la borne atteinte, la triangulation rendue
// est valide mais pas forcement de Delaunay, et *complete (s'il est fourni)
// vaut false. Rend le nombre de bascules.
unsigned int delaunayFlips (const float *vertices, unsigned int *faces, unsigned int nFaces,
                            bool *complete = nullptr);
