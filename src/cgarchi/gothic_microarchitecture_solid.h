#pragma once

// Solide imprimable de la microarchitecture gothique (J7, decision M29) : la reunion
// des boules de rayon r centrees sur les courbes du reseau, coupee par le demi-espace
// z >= base, bordee par l'isosurface f = 0 de
//     f(p) = max(min_i d(p, courbe_i) - r, base - z)
// extraite par marching cubes (ImplicitSurface, cgmesh) sur une grille reguliere de
// pas h = r / q.
//
// Repere de calcul : h = 1, noeuds a coordonnees entieres exactes en float, origine
// en double : centre de la boite des polylignes en x et y ; en z, si la coupe est
// demandee, base - h/2 releve d'un nombre entier de pas jusque sous le bas du reseau
// (moins r + 3h), centre sinon. Le plan de coupe passe ainsi au milieu d'une couche de
// cellules : aucun noeud n'est sur lui, et aucun sommet n'est sous la base. Les
// coordonnees de grille sont bornees par l'etendue du reseau, pas par sa hauteur
// au-dessus de la base.
//
// Champ : distance en double aux segments de polylignes dont l'ecart a la courbe est
// mesure (subdivision tant que le milieu d'un pas s'ecarte de plus de 0,01 r de la
// corde). Ecrete a +2h : une arete coupee par l'isosurface a ses deux bouts a |f| <= h
// (f est 1-lipschitzienne), l'ecretage ne deplace donc aucun sommet.
//
// Bornes et etat de l'extraction :
// - un objet GridIsoExtractor par extraction, et Run refuse un second appel : son
//   accumulateur (sommets, cles, faces, plafond) ne vaut que pour une grille ;
// - au plus SolidLimits::max_nodes_per_axis noeuds par axe et max_grid_nodes au
//   total, controles avant toute allocation : ils bornent le temps et la memoire
//   (decision M29) ;
// - accumulateur propre (std::vector, plafond de triangles) ; un echec d'allocation
//   des caches d'ImplicitSurface::compute est rendu par son retour (CacheAllocation) ;
// - set_orientation(0) : avec « interieur = valeur <= iso », c'est le sens qui
//   oriente les normales vers l'exterieur.
// Le champ n'est evalue qu'aux noeuds : compute n'appelle pas get_normal (aucun
// rappel de normale ni de couleur n'est pose).
//
// Extraction par briques de 32^3 cellules, un GridIsoExtractor par brique ; une
// brique dont la maille de segments est vide est sautee (aucun noeud n'y est a moins
// de r d'un segment : la liste d'une maille couvre la portee r + 2h). Chaque sommet
// est identifie par son arete de grille (noeud bas entier, axe) et place a partir
// des valeurs des deux noeuds de l'arete, lues bas -> haut : sa position ne depend
// ni du cube qui l'a cree, ni de la brique. Les briques sont soudees par cette cle,
// et le resultat est identique, triangle pour triangle et au bit pres, a
// l'extraction en une seule grille.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "mesh.h"
#include "mesh_validation.h"
#include "print_validation.h"
#include "surface_implicit.h"
#include <cgarchi/gothic_microarchitecture.h>

namespace cgarchi::microarch
{

struct SolidParams
{
	double radius_ratio     = 0.005;   // r = radius_ratio * D, dans (0, SolidLimits::max_radius_ratio] (M21)
	int    cells_per_radius = 3;       // q = r / h, dans [min_cells_per_radius, max_cells_per_radius] ; jamais ramene
	bool   cut_at_base      = true;
};

struct SolidLimits
{
	std::size_t max_triangles  = std::size_t(1) << 22;   // M29
	std::size_t max_grid_nodes = std::size_t(1) << 28;   // noeuds de la grille, controles avant allocation
	static constexpr int    max_nodes_per_axis   = 2048;
	static constexpr double max_radius_ratio     = 0.1;
	static constexpr int    min_cells_per_radius = 2;
	static constexpr int    max_cells_per_radius = 8;
	// Extraction en une seule grille au lieu des briques (tests d'equivalence). Le
	// plafond de noeuds porte alors sur toute la grille ; sinon, sur les briques actives.
	bool full_grid = false;
};

struct SolidMesh
{
	std::unique_ptr<Mesh> mesh;            // nullptr si erreur ; normales calculees
	// Origine du maillage, en double : point du dessin = sommet + origin (M26).
	Vector3d              origin;
	double                radius = 0.0;    // r, unites du dessin
	double                cell   = 0.0;    // h, unites du dessin
	std::vector<long>     euler;           // caracteristique d'Euler de chaque composante (indices du maillage)
	std::size_t           bricks = 0, active_bricks = 0;   // 1 et 1 en grille complete
	std::size_t           evaluated_nodes = 0;             // evaluations du champ aux noeuds
};

// Extraction d'isosurface {f <= 0} sur la grille des noeuds entiers [lo, hi]^3, pas 1.
// Un objet par extraction : Run refuse un second appel (AlreadyUsed). Une valeur de
// champ |f| < kMargin devient +-kMargin, du signe de f (0 compte comme interieur) :
// aucun noeud n'est sur l'isosurface, et chaque sommet est a plus de ~kMargin / (1 +
// |pente|) d'un noeud, au-dessus de la soudure de 1e-3 du validateur.
class GridIsoExtractor : public ImplicitSurface
{
public:
	static constexpr float kMargin = 0.01f;
	using Field           = float (*) (float x, float y, float z, void *context);
	enum class Status { Ok, AlreadyUsed, NodesPerAxis, Nodes, CacheAllocation, Triangles, BadIndex };

	GridIsoExtractor (Field field, void *context, std::size_t maxTriangles, std::size_t maxNodes);

	Status Run (const int lo[3], const int hi[3]);

	// Sommets (x, y, z) dans le repere de la grille, places sur leur arete a partir des
	// valeurs des deux noeuds lues bas -> haut ; triangles (indices), orientes vers
	// l'exterieur de {f <= 0} ; cle d'arete de chaque sommet (EdgeKey).
	const std::vector<float>        &Vertices () const { return m_vertices; }
	const std::vector<std::uint64_t> &Keys () const { return m_keys; }
	const std::vector<unsigned int> &Faces () const { return m_faces; }
	std::size_t                      Evaluations () const { return m_evaluations; }

	// Cle d'une arete de grille : noeud bas (relatif a keyOrigin, 20 bits par axe) et axe.
	static std::uint64_t EdgeKey (const int node[3], int axis, const int keyOrigin[3]);
	// Origine des cles (defaut : 0, 0, 0).
	void SetKeyOrigin (const int origin[3]) { for (int k = 0; k < 3; ++k) m_keyOrigin[k] = origin[k]; }

private:
	static float evalNode (float x, float y, float z, void *self);
	static bool  addCorner (float x, float y, float z, int index, void *self);

	Field                     m_field;
	void                     *m_context;
	std::size_t               m_maxTriangles, m_maxNodes;
	bool                      m_used = false, m_overflow = false, m_badIndex = false;
	std::size_t               m_evaluations = 0;
	std::vector<float>        m_vertices;
	std::vector<std::uint64_t> m_keys;
	std::vector<bool>         m_placed;
	int                       m_keyOrigin[3] = {0, 0, 0};
	std::vector<unsigned int> m_faces;
	unsigned int              m_corner[3] = {0, 0, 0};
	int                       m_cornerCount = 0;
};

// Nom stable d'un statut d'extraction (diagnostics, tests).
const char *ExtractStatusName (GridIsoExtractor::Status status);

// Solide de rayon `radius` autour de polylignes du dessin, pas de grille `cell`, coupe
// a z >= *base si `base` est fourni. Rayon ou pas non finis ou <= 0 :
// solid_invalid_parameter sur solid.radius ou solid.cell ; aucun segment (polylignes
// vides ou d'un seul point) ou coordonnee non finie : solid_invalid_parameter sur
// solid.polylines. Refus :
// solid_limit_exceeded sur solid.grid.nodes_per_axis, solid.grid.nodes,
// solid.grid.cache (allocation a l'essai ratee), solid.grid (sommet hors d'une arete
// de grille, BadIndex) ou solid.triangles (plafond atteint pendant l'extraction) ;
// mesh nul. Une estimation (EstimateSolidTriangles) au-dela du plafond n'est qu'une
// information, solid_triangle_estimate_high : elle majore F sans borne connue
// (recouvrements des tubes) et ne peut donc pas refuser.
SolidMesh MeshTubeUnion (const std::vector<std::vector<Vector3d>> &polylines, double radius, double cell,
                         std::optional<double> base, MicroarchReport &report, const SolidLimits &limits = SolidLimits{});

// Solide d'un reseau. Parametres hors bornes : solid_invalid_parameter sur
// solid.radius_ratio ou solid.cells_per_radius (tous releves). Polylignes : plus de
// SamplingLimits::max_network_points points, solid_limit_exceeded sur solid.polylines.
// Base : celle du niveau des courbes.
SolidMesh BuildSolidMesh (const Network &network, const SolidParams &params, MicroarchReport &report,
                          const SolidLimits &limits = SolidLimits{});

// Nombre de triangles estime avant extraction (M32 b) : 3 A / h^2, A = 2 pi r L, L la
// longueur 3D des polylignes du champ. Majorant de l'aire (recouvrements comptes, sans
// borne : 5,7 F mesure sur une etoile de 128 branches convergentes). 0 si les parametres
// sont invalides.
std::size_t EstimateSolidTriangles (const Network &network, const SolidParams &params);

// ---------------------------------------------------------------- verdict (J7-c)

struct SolidPrintOptions
{
	// Seuil de verification (M33, Maker) : au-dela de ce nombre de triangles, le verdict
	// n'est pas rendu (verified faux, solid_unverified) ; l'export reste possible, marque
	// « non verifie ». Absent : verdict toujours complet (natif).
	std::optional<std::size_t> max_verified_triangles;
};

// Verdict d'imprimabilite d'un solide de BuildSolidMesh (decisions M31, M32).
//
// Bloquants : solide non sain ou hors de sa tolerance de discretisation (defaut du
// generateur, solid_generator_defect, distinct d'un refus d'imprimabilite) ; composantes
// du graphe reunies par le solide (solid_unplanned_fusion) ; composante qui ne touche pas
// la base (solid_floating_component) ; echelle absente ; 2r < minWallMm
// (solid_wall_too_thin, condition exacte, independante de q) ; enveloppe depassee.
// Informations, sans effet sur le verdict : caracteristique d'Euler d'une composante
// differente de celle du graphe (solid_closed_cell : cellule fermee ou anse) ; col
// minimal des fourches (solid_fork_neck) ; cavite scellee (solid_sealed_cavity : piege a
// resine non polymerisee) ; controle de discretisation sans objet, aucun sommet hors des
// fourches (solid_discretization_unchecked). Bloquant aussi : plus de composantes que le
// graphe (solid_unplanned_split, une scission).
//
// Zone de fourche : sommet a moins de r + 2h d'un second morceau du reseau (morceau
// distinct, ou meme morceau a plus de 1,25 (r + 2h) le long de la courbe). Hors de cette zone, la
// paroi d'un sommet est la longueur du rayon interieur (-normale) dans le cylindre de rayon
// r autour du segment d'axe le plus proche ; elle doit tenir paroi >= 2r (1 - c_max), avec
// c_max = 0,6 / q^2 (le double du deficit mesure) :
// c'est un controle de discretisation du generateur (son echec est solid_generator_defect),
// pas un critere d'imprimabilite. Le disque de coupe (z < base + h) est exclu. Col d'une
// fourche : corde 2 sqrt(r^2 - e^2 / 4) de la lentille commune aux deux tubes, e l'ecart de
// leurs axes au sommet. Une composante de volume signe negatif dont un sommet a un nombre
// d'enroulement 1 par rapport aux autres composantes, orientations comprises (WindingNumber), est
// une cavite scellee : information, ni defaut du generateur, ni composante. Sinon (hors de toute
// matiere, ou ilot retourne dans une cavite : enroulement 0), elle reste un defaut.
struct SolidPrintVerdict
{
	bool printable     = false;
	bool verified      = true;    // faux au-dela de SolidPrintOptions::max_verified_triangles (M33)
	bool generator_ok  = false;   // P1-P5 et tolerance de discretisation
	bool scale_set     = false;
	MeshValidationReport validation;
	PrintabilityReport   envelope;
	std::size_t components = 0, expected_components = 0;
	std::vector<std::size_t> floating;          // composantes du solide qui ne touchent pas la base
	std::vector<std::size_t> closed_cells;      // composantes dont chi differe du graphe
	double wall_mm = 0.0;                       // 2r en mm (0 sans echelle)
	std::optional<double> off_fork_wall;        // paroi minimale hors des fourches, en fraction de 2r ; absente sans sommet hors fourche
	bool fusion = false, split = false;         // composantes reunies ; composantes en trop
	double c_max = 0.0;
	std::size_t cavities = 0;                   // cavites scellees (volume negatif, contenues dans une autre composante)
	std::size_t fork_vertices = 0, off_fork_vertices = 0;
	std::optional<double> fork_neck;            // col minimal, unites du dessin
	Vector3d              fork_neck_at;         // position (dessin)
};

SolidPrintVerdict CheckSolidPrintable (const SolidMesh &solid, const Network &network, const PrintConstraints *constraints,
                                       MicroarchReport &report, const SolidPrintOptions &options = SolidPrintOptions{});

// ---------------------------------------------------------------- export (J7-d)

// Base du niveau des courbes du reseau (celle de BuildSolidMesh) ; 0 sans courbe ou sans niveau.
double NetworkBase (const Network &network);

// STL binaire du solide, en mm (decision M29, point 5). Chaque sommet est ramene au dessin en
// double (sommet + origin), translate pour centrer la boite du solide en x et y et poser `base` a
// z = 0, multiplie par mmPerUnit, puis ecrit en float. 80 octets d'en-tete, le nombre de
// triangles, puis 50 octets par triangle (normale unitaire calculee en double). Taille exacte :
// 84 + 50 F. "" sans maillage, ou si mmPerUnit n'est pas fini et > 0. `extentMm`, s'il est
// fourni, recoit les cotes de la boite en mm.
std::string SolidToStl (const SolidMesh &solid, double mmPerUnit, double base, double *extentMm = nullptr);

// Directions fixes, unitaires et non alignees sur la grille, de WindingNumber.
const std::vector<Vector3d> &WindingDirections ();

// Nombre d'enroulement de `p` par rapport aux triangles retenus (counted[f] ; tous si `counted`
// est vide), sommets en (x, y, z) : somme, sur les traversees d'un rayon issu de p, de +1 quand le
// rayon sort dans le sens de la normale du triangle (e1 x e2) et de -1 quand il entre. Une
// direction est ecartee des que son rayon passe a moins de 1e-9 (barycentrique) d'une arete ou d'un
// sommet, part d'un triangle, ou rase son plan : aucune traversee n'est comptee deux fois ni
// oubliee. Le resultat est rendu quand deux directions retenues consecutives concordent ; sinon
// nullopt (surface ouverte, point sur la surface, ou directions epuisees).
std::optional<long> WindingNumber (const std::vector<float> &vertices, const std::vector<unsigned int> &triangles,
                                   const std::vector<bool> &counted, const Vector3d &p,
                                   const std::vector<Vector3d> &directions = WindingDirections());


} // namespace cgarchi::microarch
