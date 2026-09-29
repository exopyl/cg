#pragma once

// ============================================================================
//  Slicing multi-maillages par plans horizontaux
// ============================================================================
//
// Coupe une pile de plans z = cste a travers un ou plusieurs maillages et rend,
// pour chaque plan, des REGIONS (une enveloppe et ses trous) directement
// consommables par ExtrudeContour / contour_ops. Conception, etiquettes des
// limites D1-D8 et plan de tests : docs/mesh_slicing.md.
//
// Chaine de traitement, pour un maillage et un plan :
//   1. repere plateau : sommets transformes UNE fois par maillage (M, en
//      O(V)), plan z = cste de normale +Z (buildPlateMesh) ;
//   2. intersection triangle / plan, classement a trois etats (dessus /
//      dessous / sur, a +-FLT_EPSILON), decision par arete pour les aretes
//      contenues dans le plan (intersectTriangles) ;
//   3. chainage des segments par cle d'arete, reparations sur une couche non
//      variete (SliceLinker), puis chemins (traceChains) ;
//   4. ancrage canonique des boucles fermees, nettoyage (points proches, angles
//      plats), decoupe aux points doubles, retrait des contours degeneres ;
//   5. imbrication, par contenance ou par enroulement (Nesting).
// Aucune dependance hors STL, cgmath et Clipper2 (deja vendorise). Le slicer a
// demi-aretes de slicer.h est independant et reste en place.
//
// REPERE PLATEAU. Le plan reste z = cste de normale +Z quelle que soit la
// matrice : sa normale ne peut pas s'inverser sous une symetrie (D4). Pour que
// l'orientation survive REELLEMENT a une symetrie, l'ordre des sommets de
// chaque triangle est en plus retourne quand det(M) < 0 : sans cela les
// normales de face, calculees sur les sommets transformes, pointeraient vers
// l'interieur (voir buildPlateMesh).
//
// ACCELERATION. Les triangles sont tries sur [zmin, zmax] et les plans
// balayes par z croissant : l'ensemble des triangles candidats d'un plan -- et
// l'ordre croissant dans lequel on les visite -- ne depend que de ce plan. La
// sortie est donc independante de l'ordre des plans et du nombre de threads.
//
// LIMITES CONNUES :
//   - D1 : classement a +-FLT_EPSILON en ABSOLU (dimensionnellement faux loin
//     de l'origine) ; un seul defaut local envoie la couche entiere dans le
//     chemin de reparation du linker.
//   - D2 residuel : une couche non variete (un seul defaut local suffit) passe
//     par le retrait des segments de longueur nulle du linker (seuil absolu
//     3,45e-4) ; un contour tres facette peut s'y effondrer, et ce retrait y
//     reste en O(k^2). Voir SliceLinker::execute.
//   - D5 : un plan presque tangent rend une bande fine, geometriquement
//     correcte ; seul le plan tangent EXACT ne rend rien.
//   - D7 : en mode Containment, un solide imbrique dans un autre devient un
//     trou (le mode Winding le garde plein).
//   - D8 : chaque boucle fermee demarre sur son point lexicographiquement
//     minimal (x puis y, valeurs exactes ; a egalite, les points suivants
//     departagent), chaque jonction garde la copie calculee par le segment qui
//     y arrive, les boucles sont triees avant l'imbrication ou l'union, et
//     faceIds / hollowFaceIds sont tries. Points, contours et regions ne
//     dependent donc pas de l'ordre des faces sur une couche variete -- l'ordre
//     des faces s'entend sommets de chaque face inchanges : une rotation des
//     sommets d'un triangle change le sens d'interpolation sur ses aretes,
//     donc les points au dernier bit. Pour une arete dans le plan a deux
//     entrees reelles, le segment (donc le faceId) garde est celui de la face
//     "dessous", invariant lui aussi. RESTENT dependants de l'ordre : la
//     decision d'une arete dans le plan a une ou plus de deux entrees, les
//     heuristiques de reparation du linker, les chemins OUVERTS (non ancres),
//     et le SENS de parcours d'un anneau aux orientations melees referme par
//     linkToFillHoles (le sens suit alors le segment de depart) ; une boucle
//     marquee fermee dont les extremites ne coincident pas a kClosureTol pres
//     n'est pas ancree.
//   - Convention solide ferme : suppose un maillage soude (voir
//     intersectTriangles).
//   - Contours degeneres : ceux de moins de trois points sont ecartes. Une
//     section plus petite que 0,01 SUR LES DEUX AXES -- le nettoyage teste axe
//     par axe -- disparait donc, par exemple juste sous une pointe ; un
//     contour d'au moins trois points alignes, d'aire nulle, est GARDE --
//     aucun test du depot ne l'exhibe a ce jour.
//
// SORTIE (convention O4) : celle d'ExtrudeContour / contour_ops -- enveloppe
// d'aire signee POSITIVE, trous NEGATIFS. Les boucles brutes (sliceLoopsAtZ)
// sont, elles, dans le sens horaire pour une enveloppe : la conversion a lieu
// dans buildRegions.
//
// ============================================================================

#include <functional>
#include <vector>

#include <cgmath/cgmath.h>

#include "extrude_contours.h"   // ExtrudeContour
#include "slice_linker.h"       // SliceSegment, SliceLinker, traceChains

class Mesh;

namespace cgmesh {
namespace slicing {

// --- maillage en repere plateau ------------------------------------------------

// Vue triangulaire d'un Mesh, transformee en repere plateau. Construite une
// fois par maillage et par matrice, puis partagee en LECTURE par les threads.
struct PlateMesh
{
	std::vector<float> vertices;          // 3 flottants par sommet, repere plateau
	std::vector<int>   triangles;         // 3 indices par triangle
	std::vector<int>   faceOfTriangle;    // triangle -> indice de face du Mesh
	std::vector<float> faceNormalZ;       // z de la normale (Newell, normee) de la FACE du triangle ;
	                                      // une entree par triangle (sinon : pas de vote nOn == 3)
	std::vector<float> zmin, zmax;        // emprise en z de chaque triangle
	std::vector<int>   byZmin;            // triangles tries par zmin (tri stable)
	bool mirrored = false;                // det(M) < 0 : triangles retournes

	int triangleCount () const { return (int)faceOfTriangle.size (); }
};

// Transforme les sommets par `matrix` (objet -> plateau, calcul en double) et
// triangule les faces avec le critere de Mesh : un triangle tel quel, une face
// CONVEXE a N > 3 sommets en eventail depuis son sommet 0, une face NON convexe
// par la triangulation de Mesh (glutess) -- obtenue face par face, Mesh
// n'exposant pas de triangulation qui rende la face de chaque triangle.
// faceNormalZ garde la normale de la face pour le vote des faces dans le plan.
// Les faces supprimees (RemoveFace), ainsi que celles qui designent un sommet
// hors bornes, sont ignorees. La matrice est supposee affine (sa ligne
// homogene n'est pas lue).
//
// Quand det(M) < 0, l'ordre de chaque triangle est retourne : l'orientation
// exterieure du maillage survit ainsi a la symetrie (D4).
PlateMesh buildPlateMesh (const Mesh& mesh, const Matrix4f& matrix);

// Segments du plan z pour une liste de triangles. Deux phases :
//   1. classement de chaque triangle, a trois etats (D1 : +-FLT_EPSILON en
//      absolu). nOn == 0 / 1 : un segment, definitif. nOn == 2 (une arete dans
//      le plan) : un segment CANDIDAT et une entree (arete, cote du troisieme
//      sommet). nOn == 3 (face dans le plan) : aucun segment, mais une
//      pseudo-entree par arete, de cote donne par la normale de la face --
//      "dessus" si elle monte, "dessous" si elle descend ; elle ne vote que si
//      |nz| depasse un seuil proche de 1 ;
//   2. decision PAR ARETE (table indexee par la cle d'arete) : exactement deux
//      entrees de cotes opposes -> l'arete est gardee une fois, par un segment
//      reel (s'il y en a deux, celui de la face "dessous", choix invariant par
//      permutation des faces) ; de meme cote -> retiree (arete posee sur le
//      plan, diagonale d'une face horizontale), et rien n'est emis. Une
//      entree, ou plus de deux : la premiere entree reelle est emise, INVALIDE
//      si une entree reelle posterieure est du meme cote (voir le .cpp) ; ce
//      cas depend de l'ordre des faces (D8).
//
// ORIENTATION (D6) : combinatoire, tiree de l'enroulement du triangle et du
// classement de ses sommets -- du point ou l'enroulement monte (dessous ->
// dessus) vers celui ou il descend. Sur tout triangle non degenere, c'est
// dir = + n x z : boucle brute horaire pour un maillage oriente vers
// l'exterieur. La regle reste definie pour un segment de longueur nulle ou
// une face degeneree.
//
// Convention SOLIDE FERME : un plan pose sur une face horizontale rend la
// section FERMEE, qu'elle soit le haut ou le bas du solide (cube a z = 0 comme
// a z = 1, palier d'une marche). Aucune face n'est rejetee sur sa normale
// (D3) : une face quasi horizontale qui traverse le plan rend son segment. La
// convention suppose un maillage SOUDE : la decision passe par des cles
// d'aretes (indices de sommets). Sur une soupe de triangles non soudee, chaque
// arete n'a qu'une entree et est gardee telle quelle -- MergeVertices est
// recommande en amont.
void intersectTriangles (const PlateMesh& mesh, const std::vector<int>& triangles, float z,
                         std::vector<SliceSegment>& out);

// Tous les segments du plan z (intersectTriangles sur les triangles candidats,
// par indice croissant).
void intersectAtZ (const PlateMesh& mesh, float z, std::vector<SliceSegment>& out);

// --- contours 2D et nettoyage (en double) --------------------------------------

using Contour2d = std::vector<Vector2d>;

// Aire signee (lacet), positive dans le sens trigonometrique.
double contourSignedAreaD (const Contour2d& c);

// Orientation par le sommet le plus bas, puis le plus a droite. Faux sous trois
// points.
bool isClockwise (const Contour2d& c);

// Retire les points a moins de `eps` (test PAR AXE, non euclidien) du dernier
// point garde, ainsi que la fermeture repetee en fin de contour.
void removeClosePoints (Contour2d& c, double eps = 0.001);

// Retire les sommets dont l'angle entre aretes entrante et sortante ne depasse
// pas `angle` (radians). Le resultat depend du point de depart -- que
// buildRegions fixe en ancrant chaque boucle fermee (D8).
void removeFlatAngle (Contour2d& c, double angle = 0.);

// removeClosePoints puis removeFlatAngle.
void cleanContour (Contour2d& c, double closeEps = 0.001, double angleEps = 0.);

// Decoupe un contour a ses points doubles (a `eps` pres, par axe),
// recursivement. Rend false -- et `out` vide -- quand il n'y a rien a decouper
// ou moins de trois points.
bool splitAtRepeatedPoints (const Contour2d& c, std::vector<Contour2d>& out,
                             double eps = 0.001);

// Point dans polygone par croisements, bord compte dedans.
// `minSquaredDistance` recoit le carre de la distance au SOMMET le plus proche.
bool isPointInside (const Contour2d& c, double x, double y, double& minSquaredDistance);

// --- imbrication ----------------------------------------------------------------

// Une composante issue de l'imbrication : indices dans la soupe d'entree.
struct NestedContour
{
	int outer = -1;
	std::vector<int> holes;
};

// Imbrication par contenance : tri STABLE par aire croissante, puis chaque
// contour devient trou du PREMIER contour plus grand qui le contient (tous ses
// points dedans, un seul admis dehors a moins de 0,0012 du sommet le plus
// proche). Deux niveaux : les trous d'un contour qui devient lui-meme trou sont
// rendus a la soupe et redeviennent des composantes. L'orientation est
// IGNOREE.
//
// (D7) un solide imbrique dans un autre devient un trou.
std::vector<NestedContour> nestContours (const std::vector<Contour2d>& soup);

enum class Nesting
{
	// Par defaut : contenance seule, orientation ignoree (nestContours). Porte
	// D7.
	Containment,
	// Union Clipper2 NonZero puis PolyTree (contourRegions de contour_ops) :
	// l'enroulement decide, ce qui corrige D7. Change la semantique sur les cas
	// sales -- les contours qui se recouvrent fondent -- et depend de
	// l'orientation des boucles. Les faces sources sont rattachees a chaque
	// region apres coup (boucle dont le premier point tombe dans l'enveloppe).
	Winding
};

// --- sortie ----------------------------------------------------------------------

// Une composante d'une couche : une enveloppe et ses trous.
//
// CONTRAT DES CONTOURS. Dans les DEUX modes, chaque contour a au moins trois
// points : les contours degeneres (moins de trois points apres nettoyage et
// decoupe -- bouts d'une chaine ouverte, contour vide sous une pointe) sont
// ecartes avant l'imbrication ou l'union. Une enveloppe degeneree ne donne
// donc pas de region, un trou degenere disparait seul, et leurs faces sources
// sont perdues avec eux. O4 (enveloppe d'aire > 0, trous < 0) est garanti en
// mode Winding ; en mode Containment il l'est pour tout contour d'aire
// NETTEMENT non nulle (l'orientation est decidee sur l'aire en double avant
// l'arrondi float : un contour d'aire de l'ordre de cet arrondi peut changer de
// signe), mais PAS pour un contour d'au moins trois points ALIGNES (aire
// nulle) : il n'y a pas de filtre d'aire, son orientation n'est pas definie.
//
// PROVENANCE DES FACES. faceIds ne contient QUE des faces du maillage
// principal (SliceInput::mesh), hollowFaceIds QUE des faces du maillage
// d'evidement (SliceInput::hollowing). Une face a N > 3 sommets etant decoupee
// en N - 2 triangles, son indice peut apparaitre plusieurs fois. Les deux
// listes sont TRIEES (D8) : ce sont des ensembles, sans ordre de parcours.
// Elles sont vides pour un contour issu d'une decoupe aux points doubles, et
// n'incluent pas les faces d'un contour degenere ecarte.
struct SliceRegion
{
	// [0] = enveloppe (isHole = false), puis les trous (isHole = true).
	std::vector<ExtrudeContour> contours;
	int sourceMesh = -1;              // indice dans les entrees de sliceMeshes
	std::vector<int> faceIds;         // faces du maillage principal, triees
	std::vector<int> hollowFaceIds;   // faces du maillage d'evidement, triees
	bool isSupport = false;
	bool isHole = false;
};
using SliceLayer = std::vector<SliceRegion>;

// Une boucle brute d'un maillage dans un plan, avant nettoyage.
struct SliceLoop
{
	Contour2d pts;
	std::vector<int> faceIds;
	bool fromHollowing = false;
	bool closed = false;   // chaine fermee (topologique) ; le dernier point repete alors le premier
};

// Segments, chainage et chemins pour UN maillage et UN plan. Les boucles sont
// brutes : enveloppe dans le sens horaire, fermeture repetee.
void sliceLoopsAtZ (const PlateMesh& mesh, float z, std::vector<SliceLoop>& loops,
                    bool fromHollowing = false);

// Ancrage, nettoyage, decoupe et imbrication d'un jeu de boucles, puis
// conversion vers la convention O4 (voir le contrat de SliceRegion). Les
// boucles sont consommees ; celles marquees fromHollowing alimentent
// hollowFaceIds.
SliceLayer buildRegions (std::vector<SliceLoop>& loops, int sourceMesh,
                         bool isSupport, bool isHole, Nesting nesting);

// --- API haut niveau -------------------------------------------------------------

struct SliceInput
{
	const Mesh* mesh = nullptr;     // non possede ; doit survivre a l'appel
	Matrix4f matrix;                // objet -> plateau (identite par defaut)
	bool isSupport = false;
	bool isHole = false;
	// Maillage d'evidement optionnel, tranche avec la matrice du principal.
	// Ses boucles rejoignent celles du principal AVANT l'imbrication. En mode
	// Containment, c'est la contenance qui en fait des trous : il est donc
	// attendu de MEME orientation que le principal. En mode Winding, ses
	// boucles sont retournees pour retirer de la matiere.
	// Difference avec une cavite VRAIE (surface interieure du principal,
	// normales tournees vers le vide), au plan limite pose sur un fond ou un
	// plafond plat : la convention solide ferme ferme la section d'un
	// evidement de meme orientation, qui y rend donc un trou ; celle d'une
	// cavite vraie n'y rend RIEN (le fond et les murs de la cavite sont du
	// meme cote), la tranche est alors pleine a ce plan exact.
	const Mesh* hollowing = nullptr;
};

// Plafond du nombre de threads de sliceMeshes, quelle que soit la demande.
constexpr unsigned int kMaxSliceThreads = 64;

struct SliceOptions
{
	Nesting nesting = Nesting::Containment;
	// 0 = std::thread::hardware_concurrency() (8 si inconnu). Borne en tout cas
	// par le nombre de plans et par kMaxSliceThreads. Ignore sous
	// __EMSCRIPTEN__, ou le calcul est TOUJOURS sequentiel (std::thread leve
	// sans -pthread).
	unsigned int threads = 0;
};

// Avancement en pourcents, non decroissant, 100 en fin de calcul. Appele depuis
// les threads de travail, serialise par un mutex.
using SliceProgress = std::function<void (int percent)>;

// Tranche chaque entree a chaque altitude de `zs`. out[k] rassemble les regions
// de toutes les entrees au plan zs[k], dans l'ordre des entrees ; les regions
// ne sont PAS fusionnees entre entrees (le regroupement se fait par maillage).
// Sortie identique quel que soit le nombre de threads. Rend un vecteur vide
// pour `zs` vide. Une altitude non finie (NaN, infini) rend une couche VIDE, et
// n'entre ni dans le tri ni dans le balayage des autres.
std::vector<SliceLayer> sliceMeshes (const std::vector<SliceInput>& inputs,
                                     const std::vector<float>& zs,
                                     const SliceOptions& options = SliceOptions (),
                                     const SliceProgress& progress = nullptr);

} // namespace slicing
} // namespace cgmesh
