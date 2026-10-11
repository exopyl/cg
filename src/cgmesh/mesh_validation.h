#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

class Mesh;

//
// Validateur de maillage INDEPENDANT de tout generateur : il ne lit que les
// sommets et les faces d'un Mesh, et rend un rapport chiffre par categorie de
// defaut stable (MeshDefect), jamais un texte libre.
//
// Toute la topologie se lit APRES soudure : deux sommets references dont la
// distance de Chebyshev est <= weldTolerance sont soudes, relation fermee par
// transitivite (union-find). Le resultat ne depend pas de l'ordre des sommets ;
// la position d'une grappe soudee est la plus petite position de ses membres
// dans l'ordre lexicographique. Les sommets non finis ne sont jamais soudes.
//
// Faces : une face de moins de 3 sommets est NonTriangleFace et exclue de tout
// le reste ; une face de 3 sommets ou plus participe a la topologie (boucle
// d'aretes), au volume et a l'aire (eventail depuis son premier sommet), mais
// seule une face de 3 sommets participe au test d'auto-intersection. Une face
// citant un sommet hors bornes est InvalidIndex et exclue de tout le reste.
// Une face citant un sommet non fini n'entre dans aucune mesure geometrique.
//
// Auto-intersection (t = weldTolerance, faces degenerees exclues) :
//   - paire coplanaire (chaque sommet de l'un a au plus t du plan de l'autre,
//     dans les deux sens) : signalee si l'aire de leur recouvrement depasse
//     t fois son perimetre (recouvrement plus epais qu'environ 2t) ;
//   - paire non coplanaire : signalee si une arete de l'un a ses extremites de
//     part et d'autre du plan de l'autre a plus de t et le perce a plus de t de
//     ses cotes, OU si chacun a des sommets a plus de t des deux cotes du plan
//     de l'autre et que leurs segments d'intersection se recouvrent sur plus de
//     t le long de la droite commune aux deux plans. Une paire a arete commune
//     n'est jamais signalee ; une paire a sommet commun seulement si elle se
//     traverse au-dela de ce sommet. Un contact sans traversee n'est pas
//     signale ;
//   - zone grise : une paire presque coplanaire dont un sommet est entre t et
//     environ 2t du plan de l'autre n'est pas signalee.
// Le test est borne par maxSelfIntersectionPairs paires examinees ; au-dela il
// s'arrete et selfIntersectionTruncated est vrai.
//
// InvertedShell est evalue PAR composante fermee et orientee (chaque arete portee
// par exactement deux faces qui la parcourent en sens opposes). Une cavite
// interieure orientee vers l'interieur, valide pour un solide creux, est donc
// signalee.
//
enum class MeshDefect
{
	InvalidIndex,              // face citant un sommet hors bornes
	NonTriangleFace,           // face de taille differente de 3
	NonFiniteVertex,           // sommet a composante NaN ou infinie
	UnreferencedVertex,        // sommet cite par aucune face valide
	DegenerateFace,            // indice soude repete, ou aire < degenerateArea
	DuplicateFace,             // meme ensemble de sommets soudes qu'une face anterieure, quel que soit l'ordre
	BoundaryEdge,              // arete soudee portee par une seule face
	NonManifoldEdge,           // arete soudee portee par plus de deux faces
	NonManifoldVertex,         // faces incidentes formant plus d'un eventail
	InconsistentOrientation,   // arete parcourue dans le meme sens par ses deux faces
	InvertedShell,             // composante fermee et orientee de volume signe negatif
	UnexpectedComponentCount,  // nombre de composantes different de expectedComponents
	SelfIntersection,          // paire de triangles qui se traversent
};

constexpr std::size_t kMeshDefectCount = static_cast<std::size_t>(MeshDefect::SelfIntersection) + 1;

// Budget par defaut du test d'auto-intersection, proportionnel a la taille :
// max(kMinSelfIntersectionPairs, kSelfIntersectionPairsPerTriangle x triangles
// candidats). Un maillage sans eventail degenere examine moins de 25 paires par
// triangle ; le pire cas (eventail, toutes les boites se recouvrent) coute
// donc au plus ~0,8 s ou ~25 us par triangle (MSVC Release), au lieu d'etre
// quadratique. Mesures : gothic_implementation.md, M3.
constexpr std::size_t kMinSelfIntersectionPairs         = 2000000;
constexpr std::size_t kSelfIntersectionPairsPerTriangle = 64;

// Nom stable en snake_case (« boundary_edge », ...), pour les diagnostics et
// les tests. Ne change pas d'une version a l'autre.
const char *MeshDefectName (MeshDefect kind);

// Un defaut localise. Indices ORIGINAUX du Mesh : faces fautives, et pour les
// sommets soudes le plus petit indice original de la grappe.
//   - arete (Boundary/NonManifold/Inconsistent) : 2 sommets, faces incidentes ;
//   - NonManifoldVertex : 1 sommet, ses faces incidentes ;
//   - SelfIntersection : les 2 faces ;
//   - DuplicateFace : la face anterieure puis le doublon ;
//   - InvertedShell : un sommet et une face de la composante ;
//   - UnexpectedComponentCount : aucun element.
struct MeshDefectRecord
{
	MeshDefect            kind = MeshDefect::InvalidIndex;
	std::vector<unsigned> faces;
	std::vector<unsigned> vertices;
};

struct MeshValidationOptions
{
	// Tolerance ABSOLUE de soudure, en unites du maillage. Elle sert aussi de
	// tolerance geometrique du test d'auto-intersection. Absente : voir
	// DefaultWeldTolerance. Fournie : doit etre finie et > 0.
	std::optional<double> weldTolerance;

	// Aire en dessous de laquelle une face est degeneree. Absente :
	// weldTolerance^2.
	std::optional<double> degenerateArea;

	// Nombre de composantes connexes attendu ; absent : non verifie.
	std::optional<unsigned> expectedComponents;

	bool checkSelfIntersection = true;

	// Budget de paires examinees par le test d'auto-intersection (paires dont
	// les boites se recouvrent). Une phase large par boites devient quadratique
	// quand toutes les boites se recouvrent -- eventail autour d'un sommet
	// commun -- : le budget borne ce pire cas. Absent : budget par defaut, voir
	// kSelfIntersectionPairsPerTriangle.
	std::optional<std::size_t> maxSelfIntersectionPairs;

	// Plafond des enregistrements PAR CATEGORIE. Les compteurs, eux, ne sont
	// jamais plafonnes.
	std::size_t maxRecordsPerKind = 32;
};

struct MeshValidationReport
{
	std::array<std::size_t, kMeshDefectCount> counts {};

	double                weldTolerance  = 0.0;   // tolerance effectivement employee
	double                degenerateArea = 0.0;
	unsigned              weldedVertices = 0;     // grappes de sommets references
	unsigned              components     = 0;     // composantes connexes par aretes
	double                signedVolume   = 0.0;   // somme sur les faces, positions soudees
	double                area           = 0.0;
	std::array<double, 3> bbMin {};               // sommets references et finis ; 0 si aucun
	std::array<double, 3> bbMax {};
	bool                  selfIntersectionChecked = false;
	bool                  selfIntersectionTruncated = false;   // budget de paires epuise
	std::size_t           selfIntersectionPairsTested = 0;
	std::size_t           selfIntersectionPairBudget  = 0;     // budget effectivement employe

	std::vector<MeshDefectRecord> records;        // au plus maxRecordsPerKind par categorie

	std::size_t Count (MeshDefect kind) const { return counts[static_cast<std::size_t>(kind)]; }

	// Vrai si et seulement si aucune categorie n'a de defaut ET que le test
	// d'auto-intersection, s'il a ete demande, est alle a son terme : un test
	// tronque n'etablit pas l'absence d'auto-intersection. Un test desactive
	// (checkSelfIntersection = false) n'intervient pas.
	bool IsValid () const;
};

// Tolerance par defaut : max(1e-6 * diagonale, 4 * FLT_EPSILON * max|coordonnee|)
// sur les sommets references et finis. Le terme relatif suit l'echelle du
// modele ; le terme en ulp couvre un modele eloigne de l'origine, ou deux
// sommets cense coincider different de quelques ulp float -- quantite que la
// diagonale ne voit pas. Repli a 1.0 si les deux termes sont nuls (sommets
// tous confondus a l'origine).
double DefaultWeldTolerance (const Mesh &mesh);

// Leve std::invalid_argument si une tolerance fournie n'est pas finie et > 0.
MeshValidationReport ValidateMesh (const Mesh &mesh, const MeshValidationOptions &options = {});
