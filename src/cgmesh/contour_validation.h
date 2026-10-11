#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include "extrude_contours.h"   // ExtrudeContour

class Polygon2;

//
// Validateur de jeux de contours 2D FERMES, INDEPENDANT de tout generateur : il
// ne lit que des points et rend un rapport chiffre par categorie de defaut
// stable (ContourDefect), jamais un texte libre. Il controle les preconditions
// d'une extrusion (tessellation glutess NONZERO + parois) : contours simples,
// deux a deux disjoints, orientes selon leur niveau d'imbrication.
//
// Entrees : les deux representations que consomment reellement les
// extrusions du depot, en float, telles que le tessellateur les recoit :
//   - std::vector<ExtrudeContour> (texte, SVG, plaques ; ExtrudedMeshBuilder) :
//     le drapeau isHole y est confronte au niveau d'imbrication ;
//   - Polygon2 (fenetre gothique : buildBayStonePolygon -> extrudeToMesh) :
//     pas de drapeau, le role se deduit du seul niveau.
// Un contour est ferme implicitement (arete du dernier point au premier) ; un
// dernier point egal au premier est donc une arete de longueur nulle.
//
// Aretes : l'arete i d'un contour de n points va du point i au point
// (i + 1) mod n. Toutes les mesures sont en double, sur les coordonnees float
// converties exactement ; t designe la tolerance absolue (options).
//
// Regles (t = tolerance) :
//   - NonFinitePoint : un point a composante NaN ou infinie. Son contour est
//     exclu de toutes les mesures suivantes ;
//   - TooFewPoints : un contour de moins de 3 points, exclu lui aussi ;
//   - ShortEdge : une arete de longueur < t (doublon compris) ;
//   - SelfIntersection, par paire d'aretes d'un meme contour, aretes courtes
//     exclues. Les points relies par une chaine d'aretes courtes forment une
//     grappe ; deux aretes sont voisines si elles ont une grappe d'extremite
//     commune. Deux aretes non voisines sont signalees si leur distance est
//     < t (croisement, contact, pincement) ; deux aretes voisines si une
//     extremite de l'une hors grappe commune est a moins de t de l'autre
//     (repli, pointe plus fine que t). Une arete courte ne cree donc pas de
//     cascade : elle n'est comptee qu'une fois, en ShortEdge ;
//   - ContourContact, par paire d'aretes de deux contours DISTINCTS : signalee
//     si leur distance est < t -- croisement, sommet pose sur une arete, arete
//     ou sommet partage, ou simple proximite. C'est la precondition
//     d'extrusion de Havemann (these, p. 246) : deux anneaux d'un meme plan ne
//     doivent ni se toucher ni partager un sommet ;
//   - NegligibleArea : |aire signee| <= t x perimetre, soit une largeur
//     moyenne d'environ 2t au plus. Le sens d'un tel contour n'a pas de sens :
//     il est exclu du controle d'orientation ;
//   - niveau d'imbrication d'un contour = nombre d'autres contours qui
//     contiennent le milieu de sa plus longue arete sans ContourContact (a
//     defaut, de sa plus longue arete), regle pair-impair sur chaque contour,
//     rayon horizontal semi-ouvert (un sommet sur le rayon compte une fois).
//     Niveau pair : matiere (enveloppe ou ilot) ; impair : vide (trou). Le
//     point choisi est a au moins t de tout autre contour, donc le niveau est
//     exact, sauf pour un contour dont toutes les aretes sont en contact :
//     c'est alors une estimation ;
//   - WrongOrientation : sens contraire a orientation pour son niveau ;
//   - BadNesting : niveau > maxNestingLevel quand il est fourni, ou, pour
//     l'entree ExtrudeContour, isHole different de « niveau impair » (trou
//     hors de toute enveloppe, enveloppe posee dans une enveloppe, ...).
//
// Imbrication : par defaut la profondeur n'est PAS bornee. Sous NONZERO, des
// contours disjoints dont le sens alterne avec le niveau donnent un
// enroulement 0 ou 1 partout, donc une region bien definie quelle que soit la
// profondeur ; la fenetre gothique a recursion pose des ilots de pierre
// (niveau 2) perces de vides (niveau 3) dans ses ouvertures. Un consommateur
// limite a « enveloppe + trous » (contourRegions, contour_ops.h) fixe
// maxNestingLevel = 1. Un ilot est une piece separee tant qu'il ne rejoint pas
// la matiere qui l'entoure : ce validateur ne le signale pas, c'est le compte
// de composantes du maillage (mesh_validation.h) qui le mesure.
//
// Complexite : grille creuse sur les aretes (chacune inscrite dans les
// cellules que touche son voisinage de rayon t), pas = max(premier quartile
// des longueurs, t, longueur totale / 16n), sans dependance a l'emprise ; paires
// dedoublonnees. Le niveau se lit dans des bandes horizontales creuses, de
// hauteur choisie par la meme regle sur les |dy|. Le
// cout est un nombre de CANDIDATS visites : une arete rencontree dans une
// cellule partagee, ou une arete de la bande du point d'un contour.
//   - Paires : quelques candidats par arete, que les aretes soient
//     dispersees ou concentrees dans un coin d'une grande emprise.
//   - Imbrication : par contour, les aretes de la bande de son point, soit
//     environ racine(n) pour une disposition 2D uniforme (40 000 carres :
//     environ 100 candidats par arete).
//   - Borne reelle : de longues aretes paralleles distantes de s < pas (barres
//     d'une grille, hachures) ; une arete de longueur L visite environ 2L/s
//     candidats, quel que soit le pas. Pour l'imbrication, k contours dont
//     les aretes traversent toutes les memes bandes (barres verticales cote a
//     cote) visitent chacun les 2k aretes de sa bande. Superlineaire, jusqu'a
//     quadratique : 20 000 barres serrees (80 000 aretes) demandent 1,7e9
//     candidats.
// Le test s'arrete apres maxCandidates candidats ; truncated passe alors a
// vrai et IsValid() est faux. Une troncature pendant les paires saute
// l'imbrication (niveaux -1, orientation et imbrication non evaluees) ;
// pendant l'imbrication, seuls les contours deja situes sont evalues.
//
enum class ContourDefect
{
	NonFinitePoint,     // point a composante NaN ou infinie
	TooFewPoints,       // contour de moins de 3 points
	ShortEdge,          // arete de longueur < tolerance
	SelfIntersection,   // paire d'aretes d'un meme contour a distance < tolerance (hors voisinage)
	ContourContact,     // paire d'aretes de contours distincts a distance < tolerance
	WrongOrientation,   // sens contraire a la convention pour le niveau d'imbrication
	BadNesting,         // niveau > maxNestingLevel, ou isHole contraire au niveau
	NegligibleArea,     // |aire signee| <= tolerance x perimetre
};

constexpr std::size_t kContourDefectCount = static_cast<std::size_t>(ContourDefect::NegligibleArea) + 1;

// Budget par defaut : max(kMinContourCandidates, kContourCandidatesPerEdge x
// aretes). C'est une GARDE DE TEMPS : environ 1 a 2 ns par candidat (MSVC
// Release), soit un plancher d'environ 0,3 s (20 000 barres
// serrees tronquees au plancher : 0,36 s), puis environ 1 a 2 us par arete.
// Le plancher laisse aboutir les jeux sains mesures (au plus ~150 candidats par
// arete ; hachures de 1 500 barres au pas de 0,13 sur 200 : 18 ms) ; au-dela,
// le pire cas -- barres longues et serrees -- s'arrete tronque a ce temps-la.
// Mesures : gothic_implementation.md, M1a.
constexpr std::size_t kMinContourCandidates     = 250000000;
constexpr std::size_t kContourCandidatesPerEdge = 1024;

// Nom stable en snake_case (« contour_contact », ...), pour les diagnostics et
// les tests. Ne change pas d'une version a l'autre.
const char *ContourDefectName (ContourDefect kind);

// Convention d'orientation attendue. Aire signee positive = sens
// trigonometrique (CCW), repere y vers le haut.
//   - OuterCcwHoleCw : niveau pair CCW, niveau impair CW. C'est celle du
//     depot : Clipper2 la rend (contour_ops.h), buildBayStonePolygon la
//     construit (enveloppe CCW, pushAsHole retourne les trous en CW), et
//     ExtrudedMeshBuilder la retablit quand normalizeOrientation est vrai ;
//   - OuterCwHoleCcw : l'inverse, aussi valide sous NONZERO (enroulement -1) ;
//   - Unchecked : WrongOrientation n'est pas evalue.
enum class ContourOrientation { OuterCcwHoleCw, OuterCwHoleCcw, Unchecked };

// Un defaut localise. Indices ORIGINAUX : contour dans le jeu d'entree, point
// et arete dans ce contour (arete i = point i -> point (i + 1) mod n).
//   - NonFinitePoint : contours {c}, points {i} ;
//   - TooFewPoints, WrongOrientation, BadNesting, NegligibleArea : contours {c} ;
//   - ShortEdge : contours {c}, edges {i} ;
//   - SelfIntersection : contours {c, c}, edges {i, j} avec i < j ;
//   - ContourContact : contours {a, b} avec a < b, edges {i de a, j de b}.
struct ContourDefectRecord
{
	ContourDefect         kind = ContourDefect::NonFinitePoint;
	std::vector<unsigned> contours;
	std::vector<unsigned> points;
	std::vector<unsigned> edges;
};

struct ContourValidationOptions
{
	// Tolerance ABSOLUE, en unites des contours : longueur d'arete minimale et
	// distance minimale entre aretes non voisines. Absente : voir
	// DefaultContourTolerance. Fournie : doit etre finie et > 0. Un generateur
	// la derive de la taille de son modele.
	std::optional<double> tolerance;

	ContourOrientation orientation = ContourOrientation::OuterCcwHoleCw;

	// Niveau d'imbrication maximal admis (0 : enveloppes seules ; 1 : enveloppes
	// et trous). Absent : non borne.
	std::optional<unsigned> maxNestingLevel;

	// Budget de candidats visites (voir Complexite). Absent : budget par
	// defaut, voir kContourCandidatesPerEdge. Un appelant interactif le fixe a
	// une constante pour borner son temps de reponse, quitte a recevoir un
	// rapport tronque.
	std::optional<std::size_t> maxCandidates;

	// Plafond des enregistrements PAR CATEGORIE. Les compteurs, eux, ne sont
	// jamais plafonnes.
	std::size_t maxRecordsPerKind = 32;
};

struct ContourValidationReport
{
	std::array<std::size_t, kContourDefectCount> counts {};

	double      tolerance = 0.0;            // tolerance effectivement employee
	std::size_t contourCount = 0;
	std::size_t pointCount = 0;
	std::size_t contactContourPairs = 0;    // paires de contours distincts ayant au moins un ContourContact
	std::size_t edgePairsTested = 0;        // paires d'aretes examinees (phase etroite)
	std::size_t candidatesVisited = 0;      // cout de la phase large (voir Complexite)
	std::size_t candidateBudget = 0;        // budget effectivement employe
	bool        truncated = false;          // budget epuise : rapport incomplet

	// Par contour, dans l'ordre d'entree. Contour exclu (non fini, < 3 points) :
	// niveau -1 et aire 0. Contour non situe faute de budget : niveau -1.
	std::vector<int>    nestingLevel;
	std::vector<double> signedArea;

	std::vector<ContourDefectRecord> records;   // au plus maxRecordsPerKind par categorie

	std::size_t Count (ContourDefect kind) const { return counts[static_cast<std::size_t>(kind)]; }

	// Vrai si et seulement si aucune categorie n'a de defaut ET que le test est
	// alle a son terme : un rapport tronque n'etablit pas l'absence de defaut.
	bool IsValid () const;
};

// Tolerance par defaut : max(1e-6 x diagonale, 4 x FLT_EPSILON x max|coordonnee|)
// sur les points finis -- meme regle que DefaultWeldTolerance (mesh_validation.h) :
// le terme relatif suit l'echelle, le terme en ulp couvre des coordonnees float
// eloignees de l'origine. Repli a 1.0 si les deux termes sont nuls.
double DefaultContourTolerance (const std::vector<ExtrudeContour> &contours);
double DefaultContourTolerance (const Polygon2 &polygon);

// Levent std::invalid_argument si une tolerance fournie n'est pas finie et > 0.
ContourValidationReport ValidateContours (const std::vector<ExtrudeContour> &contours,
                                          const ContourValidationOptions &options = {});
ContourValidationReport ValidateContours (const Polygon2 &polygon,
                                          const ContourValidationOptions &options = {});
