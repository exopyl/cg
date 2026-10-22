#pragma once

// Microarchitecture gothique 3D (Segall et al., SIGGRAPH 2025) : plan 2D
// parametre, groupe de symetrie, validation et developpement du plan.
// Plan : src/cgarchi/docs/gothic_microarchitecture_plan.md ; execution :
// src/cgarchi/docs/gothic_microarchitecture_implementation.md.
//
// Aucune exception ne sort de cette API : les fonctions elementaires rendent
// une valeur ou un std::optional, la validation remplit un MicroarchReport.
// Limite : un epuisement de la memoire hors du lecteur JSON (std::bad_alloc)
// n'est pas intercepte.
// Deux positions du plan se comparent par leur distance, a une tolerance pres.

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <cgmath/TVector2.h>
#include <cgmath/TVector3.h>

namespace cgarchi::microarch
{

// ---------------------------------------------------------------- limites

// Bornes de la specification, celles du schema gothic-microarchitecture-v1.
// Les plafonds de taille bornent le developpement du plan, en O(B |G|^2) au
// pire (B courbes de base, |G| <= 2 * kMaxRotationOrder elements).
struct SpecLimits
{
	static constexpr int         max_rotation_order = 64;
	static constexpr std::size_t max_points         = 4096;
	static constexpr std::size_t max_curves         = 1024;
	// Repliques au plus : courbes de base x elements du groupe, dedoublonnages
	// compris (decision M16). Borne le cout de BuildNetwork, en O(N^2) au pire.
	static constexpr std::size_t max_replicas       = 16384;
	// Taille maximale du texte d'une specification JSON, controlee avant le parsing
	// (decision M27, Q-J6-7) : 4 Mio.
	static constexpr std::size_t max_text_bytes     = std::size_t(1) << 22;
	// Rayon d'arc maximal rapporte a l'echelle D (decision M12). Au-dela, la
	// representation (centre, rayon, angles) perd sur les extremites une erreur
	// de l'ordre de 1e-16 * r, qui atteint tol.point = 1e-9 * D vers r/D = 1e7.
	static constexpr double      max_arc_radius_ratio = 1e6;
};

// ---------------------------------------------------------------- tolerances

// Valeurs RELATIVES a l'echelle D du dessin (Scale), sauf prc_snap, sans dimension.
struct Tolerances
{
	double point     = 1e-9;   // coincidence en plan, extremites d'arc, dedoublonnage
	double parameter = 1e-7;   // egalite de deux abscisses curvilignes
	double height    = 1e-7;   // coincidence en z
	double prc_snap  = 1e-3;   // selection explicite du PRC : ecart en u = s/L
};

// Bornes superieures incluses des tolerances ; la borne inferieure (0) est exclue.
struct ToleranceBounds
{
	static constexpr double point     = 1e-3;
	static constexpr double parameter = 1e-2;
	static constexpr double height    = 1e-2;
	static constexpr double prc_snap  = 0.5;
};

// Tolerances en unites du dessin : point, parameter et height multiplies par scale.
struct ResolvedTolerances
{
	double scale     = 1.0;
	double point     = 0.0;
	double parameter = 0.0;
	double height    = 0.0;
	double prc_snap  = 0.0;
};

ResolvedTolerances Resolve (const Tolerances &tol, double scale);

// ---------------------------------------------------------------- courbes du plan

struct PlanSegment
{
	Vector2d start;
	Vector2d end;
};

// Arc exact : 0 < |sweep| < 2*pi ; sweep > 0 pour le sens trigonometrique.
struct PlanArc
{
	Vector2d center;
	double   radius      = 0.0;
	double   start_angle = 0.0;   // radians
	double   sweep       = 0.0;   // radians, signe
};

using PlanCurve = std::variant<PlanSegment, PlanArc>;

// Parametre s : longueur d'arc dans [0, Length], dans le sens de parcours.
double   Length     (const PlanCurve &curve);
Vector2d PointAt    (const PlanCurve &curve, double s);
Vector2d TangentAt  (const PlanCurve &curve, double s);   // unitaire, sens de parcours
Vector2d StartPoint (const PlanCurve &curve);
Vector2d EndPoint   (const PlanCurve &curve);

// Abscisse s du point p s'il est sur la courbe a `tol` pres (distance en plan),
// rabattue dans [0, Length] ; vide sinon.
std::optional<double> ParameterOf (const PlanCurve &curve, const Vector2d &p, double tol);

// Arc de centre `center` allant de `start` a `end` dans le sens donne.
// Rayon : |start - center| ; l'extremite d'arrivee est sur la demi-droite
// [center, end). Balayage dans (0, 2*pi] si ccw, dans [-2*pi, 0) sinon ; un
// balayage nul (start et end sur la meme demi-droite) vaut 2*pi en valeur absolue.
PlanArc MakeArc (const Vector2d &start, const Vector2d &end, const Vector2d &center, bool ccw);

// ---------------------------------------------------------------- symetries

// Groupe C_n(p) ou D_n(p) : rotations d'ordre n autour de center et, si
// reflection_axis_deg est donne, reflexions d'axes passant par center.
struct SymmetrySpec
{
	Vector2d              center;
	int                   rotation_order = 1;   // n, dans [1, SpecLimits::max_rotation_order]
	std::optional<double> reflection_axis_deg;  // angle psi de l'axe, degres, dans [0, 180)
};

// g = R^k Q^r : R rotation de 2*pi/n, Q reflexion d'axe psi.
struct SymmetryElement
{
	int  k         = 0;
	bool reflected = false;
};

// Identite en tete, puis R^1..R^(n-1), puis R^0 Q..R^(n-1) Q : n ou 2n elements.
// Precondition : une symetrie validee (ValidateStructure). Si n est hors de
// [1, SpecLimits::max_rotation_order], le resultat est vide.
std::vector<SymmetryElement> Elements (const SymmetrySpec &sym);

// n est ramene dans [1, SpecLimits::max_rotation_order].
Vector2d  Apply (const SymmetrySpec &sym, SymmetryElement g, const Vector2d &p);
// Image exacte qui conserve le sens : Apply(g, c)(s) = Apply(g, c(s)).
PlanCurve Apply (const SymmetrySpec &sym, SymmetryElement g, const PlanCurve &curve);

struct Axis
{
	Vector2d origin;
	Vector2d direction;   // unitaire
};

// Axe de R^k Q : angle psi + k*pi/n. n axes si reflexion, sinon aucun.
// Meme precondition qu'Elements : vide si n est hors de [1, max_rotation_order].
std::vector<Axis> MirrorAxes (const SymmetrySpec &sym);

// ---------------------------------------------------------------- intersections du plan

// Point commun a deux courbes : s1 sur la premiere, s2 sur la seconde (ou
// abscisse sur l'axe pour IntersectAxis). `tangential` est geometrique : les
// supports (droite, cercle) sont tangents, paralleles ou confondus en ce point,
// a la tolerance pres. Un contact bout a bout se reconnait a at_end1 && at_end2.
struct CurveHit
{
	Vector2d xy;
	double   s1         = 0.0;
	double   s2         = 0.0;
	bool     tangential = false;
	bool     at_end1    = false;   // s1 a tol pres d'une extremite de la premiere courbe ; s1 vaut alors 0 ou L exactement
	bool     at_end2    = false;   // idem pour la seconde ; toujours faux pour un axe
};

// Portion commune a deux courbes de meme support, de longueur superieure a la
// tolerance. s1_begin <= s1_end ; s2_begin et s2_end sont les abscisses, sur la
// seconde courbe, des points s1_begin et s1_end : s2_begin > s2_end si les sens
// de parcours sont opposes.
struct CurveOverlap
{
	double s1_begin = 0.0;
	double s1_end   = 0.0;
	double s2_begin = 0.0;
	double s2_end   = 0.0;
};

// Points communs tries par s1 croissant, deux a deux distants de plus de la
// tolerance ; portions communes triees par s1_begin. Deux arcs co-circulaires
// peuvent avoir deux portions communes, et un contact en plus.
struct Intersection
{
	std::vector<CurveHit>     hits;
	std::vector<CurveOverlap> overlaps;
};

// Intersection en forme close, intervalles fermes : un contact a une extremite
// est un point commun. `tol_point` est une distance en unites du dessin
// (ResolvedTolerances::point). Le resultat ne depend pas de l'ordre des
// arguments, a l'echange de s1 et s2 pres. Une courbe de longueur nulle ou non
// finie, un arc de balayage hors de (0, 2*pi) en valeur absolue, ou une
// tolerance negative ou non finie, donne un resultat vide. Pres d'une tangence,
// une extremite commune a tol pres est le point rendu.
Intersection Intersect (const PlanCurve &a, const PlanCurve &b, double tol_point);

// Intersection avec la droite infinie portee par `axis` ; s2 est l'abscisse
// du point sur l'axe, depuis axis.origin. Un segment porte par l'axe (ses deux
// extremites a tol_point pres) ne donne aucun point.
std::vector<CurveHit> IntersectAxis (const PlanCurve &curve, const Axis &axis, double tol_point);

// ---------------------------------------------------------------- profil lateral

// Ogive du plan lateral (s, z), s etant la longueur d'arc du plan : deux arcs
// de cercle raccordes G1 au PRC (s_prc, z_prc), tangentes verticales aux deux
// bouts. Arc 1 de centre (r_start, z_start), arc 2 de centre
// (length - r_end, z_end) : voir StartCenter et EndCenter.
struct OgeeProfile
{
	double length  = 0.0;
	double z_start = 0.0;
	double z_end   = 0.0;
	double s_prc   = 0.0;
	double z_prc   = 0.0;
	double r_start = 0.0;
	double r_end   = 0.0;
	bool   monotonic = true;   // |z_end - z_start| >= length
};

enum class ProfileStatus
{
	Ok,
	NonFiniteInput,      // une entree ou une valeur derivee (difference, rayons) n'est pas finie
	NonPositiveLength,   // length <= 0
	PrcOutOfRange,       // s_prc hors de l'intervalle ouvert (0, length)
	Flat,                // |z_end - z_start| <= tol_height
	InvalidTolerance,    // tol_height negative ou non finie
};

// `profile` n'est rempli que si status == Ok.
struct ProfileResult
{
	ProfileStatus status = ProfileStatus::Ok;
	OgeeProfile   profile;
};

// Forme close : PRC sur la corde, r_start = s_prc (1 + k^2) / 2,
// r_end = (length - s_prc) (1 + k^2) / 2, avec k = |z_end - z_start| / length.
// Profil non monotone si k < 1 : construit, avec monotonic = false.
ProfileResult BuildOgeeProfile (double length, double z_start, double z_end, double s_prc, double tol_height);

// Hauteur au point d'abscisse s, rabattue dans [0, length]. HeightAt(0) et
// HeightAt(length) valent exactement z_start et z_end. s NaN : NaN (tangente
// de composantes NaN pour SideTangentAt).
double   HeightAt      (const OgeeProfile &profile, double s);
// Tangente unitaire dans le plan (s, z), orientee vers les s croissants.
Vector2d SideTangentAt (const OgeeProfile &profile, double s);
// Centres des deux arcs dans le plan (s, z).
Vector2d StartCenter   (const OgeeProfile &profile);
Vector2d EndCenter     (const OgeeProfile &profile);

// ---------------------------------------------------------------- diagnostics

// Codes stables, noms snake_case ; ajout en fin uniquement.
enum class MicroarchError
{
	JsonParse,
	JsonSchema,
	JsonType,
	JsonValue,
	JsonMissing,
	DuplicateId,
	UnknownReference,
	DegenerateCurve,
	ArcEndpointMismatch,
	DegenerateScale,
	UnsupportedMultiLevel,
	SelfReversedReplica,
	RedundantBasicCurve,
	PlanOverlap,
	ProfileFlat,
	ProfileInvalid,
	PrcSelectionNoCandidate,
	SegmentEndpointMismatch,   // un segment ne commence ou ne finit pas a son point
	NearlyFlatArc,             // rayon > SpecLimits::max_arc_radius_ratio * D
	SampleLimitExceeded,       // SampleNetwork : plus de SamplingLimits::max_network_points points
	TooManyReplicas,           // courbes x elements > SpecLimits::max_replicas
	TooManyContacts,           // BuildNetwork : plus de NetworkLimits::max_contacts points de contact
	ExportWriteFailed,         // WriteNetworkObj : fichier impossible a ouvrir ou a ecrire
	TubeInvalidParameter,      // BuildTubeMesh : parametre hors de ses bornes
	TubeLimitExceeded,         // BuildTubeMesh : plus de TubeLimits::max_triangles triangles
	JsonTooLarge,              // texte JSON de plus de SpecLimits::max_text_bytes octets
	SolidInvalidParameter,     // BuildSolidMesh : parametre hors de ses bornes
	SolidLimitExceeded,        // BuildSolidMesh : grille ou triangles au-dela de SolidLimits
	SolidGeneratorDefect,      // CheckSolidPrintable : solide non sain ou hors de sa tolerance de discretisation
};

enum class MicroarchWarning
{
	FieldIgnored,
	PrcMidpointFallback,
	PrcFromOtherBasicCurve,
	PrcTie,
	PrcOnFalseIntersection,
	ProfileNotMonotonic,
	TangentialContact,
	EndpointHeightConflict,
	PrcNearEndpoint,           // PRC automatique a moins de PrcLimits::near_endpoint_u d'un bout
	JunctionHeightSpread,      // jonction dont les incidences s'etalent en z de plus de tol.height
	TubeRadiusHidesCrossing,   // BuildTubeMesh : un croisement a min_dz <= 2 r
	SolidUnplannedFusion,      // CheckSolidPrintable : composantes du graphe reunies par le solide (bloquant)
	SolidFloatingComponent,    // CheckSolidPrintable : composante qui ne touche pas la base (bloquant)
	SolidWallTooThin,          // CheckSolidPrintable : 2r < minWallMm (bloquant)
	SolidClosedCell,           // CheckSolidPrintable : caracteristique d'Euler differente du graphe (information)
	SolidForkNeck,             // CheckSolidPrintable : col d'une fourche (information)
	SolidUnverified,           // CheckSolidPrintable : au-dela du seuil de verification (M33), verdict non rendu
	SolidTriangleEstimateHigh, // BuildSolidMesh : estimation 3 A / h^2 au-dela du plafond (information, pas un refus)
	SolidSealedCavity,         // CheckSolidPrintable : cavite scellee, piege a resine (information)
	SolidUnplannedSplit,       // CheckSolidPrintable : plus de composantes que le graphe (bloquant)
	SolidDiscretizationUnchecked,   // CheckSolidPrintable : aucun sommet hors des fourches, controle sans objet
};

constexpr std::size_t kMicroarchErrorCount   = static_cast<std::size_t>(MicroarchError::SolidGeneratorDefect) + 1;
constexpr std::size_t kMicroarchWarningCount = static_cast<std::size_t>(MicroarchWarning::SolidDiscretizationUnchecked) + 1;

const char *MicroarchErrorName   (MicroarchError kind);
const char *MicroarchWarningName (MicroarchWarning kind);

// `code` est la valeur de MicroarchError ou de MicroarchWarning selon `error`.
// `path` designe la donnee fautive : "curves[2].prc.u", "points[0].xy"...
struct MicroarchDiagnostic
{
	bool        error = false;
	int         code  = 0;
	std::string path;
	std::string message;
};

constexpr std::size_t kMicroarchReportMaxEntries = 64;

// Meme semantique que GothicBuildReport : les compteurs comptent tout, les
// entrees sont plafonnees a kMicroarchReportMaxEntries.
struct MicroarchReport
{
	std::array<std::size_t, kMicroarchErrorCount>   errorCounts {};
	std::array<std::size_t, kMicroarchWarningCount> warningCounts {};
	std::vector<MicroarchDiagnostic>                entries;

	void        Record (MicroarchError kind, std::string path, std::string message);
	void        Record (MicroarchWarning kind, std::string path, std::string message);
	std::size_t Count  (MicroarchError kind) const   { return errorCounts[static_cast<std::size_t>(kind)]; }
	std::size_t Count  (MicroarchWarning kind) const { return warningCounts[static_cast<std::size_t>(kind)]; }
	std::size_t ErrorCount () const;
	std::size_t WarningCount () const;
	bool        HasErrors () const   { return ErrorCount() > 0; }
	bool        HasWarnings () const { return WarningCount() > 0; }
	void        Merge (const MicroarchReport &other);
};

// ---------------------------------------------------------------- specification d'entree

struct LevelSpec
{
	int    id   = 0;     // >= 0
	double base = 0.0;
};

struct PointSpec
{
	std::string           id;   // UTF-8 valide, non vide
	Vector2d              xy;
	bool                  ground = false;
	std::optional<double> height;   // au-dessus de la base du niveau ; exclusif de ground
	double Z () const { return ground ? 0.0 : height.value_or(0.0); }
};

struct PrcSelection
{
	std::optional<double> u;   // u = s/L dans (0, 1) ; vide : Algorithme 1
};

struct BasicCurveSpec
{
	std::string  id;            // UTF-8 valide, non vide
	PlanCurve    plan;          // extremites : les points start_point et end_point
	int          start_point = -1;
	int          end_point   = -1;
	int          level       = 0;
	PrcSelection prc;
};

struct MicroarchSpec
{
	std::vector<LevelSpec>      levels {LevelSpec{}};
	SymmetrySpec                symmetry;
	std::vector<PointSpec>      points;
	std::vector<BasicCurveSpec> curves;
	Tolerances                  tolerances;
};

// Echelle D : diagonale de la boite englobante des points (coordonnees finies) et
// du centre de symetrie. Les centres d'arc en sont exclus. Elle peut etre infinie
// si les coordonnees sont proches de la limite des doubles.
double Scale (const MicroarchSpec &spec);

// Base du niveau `level` (LevelSpec::base) ; 0 si le niveau n'est pas declare.
double LevelBase (const MicroarchSpec &spec, int level);

// ---------------------------------------------------------------- validation et developpement

// Courbe du plan developpe : image exacte d'une courbe de base par un element.
struct PlanReplica
{
	int             basic = -1;   // indice dans MicroarchSpec::curves
	SymmetryElement element;
	int             level = 0;
	PlanCurve       plan;
};

struct DevelopedPlan
{
	ResolvedTolerances       tol;
	std::vector<PlanReplica> curves;   // repliques dedoublonnees, ordre (courbe de base, element)
};

// Controles structurels (references, ids, courbes degenerees, niveaux, bornes,
// tailles, echelle). Rend true sans erreur.
bool ValidateStructure (const MicroarchSpec &spec, MicroarchReport &report);

// Etape 1 complete : structure, puis geometrie du plan developpe
// (self_reversed_replica, redundant_basic_curve, endpoint_height_conflict).
bool Validate (const MicroarchSpec &spec, MicroarchReport &report);

// Validate, puis repliques. Vide si une erreur a ete relevee.
std::optional<DevelopedPlan> DevelopPlan (const MicroarchSpec &spec, MicroarchReport &report);

// ---------------------------------------------------------------- PRC (etape 2)

// Origine d'un candidat PRC, et regle qui a fixe le PRC. Ajout en fin uniquement.
enum class PrcRule
{
	ReplicaFixedPoint,   // palier 1 : point fixe d'un element (axe miroir, centre), s = s'
	OtherBasicCurve,     // palier 2 : intersection avec une autre courbe de base ou ses repliques
	Midpoint,            // palier 3 : milieu de la courbe
	Explicit,            // selection explicite (prc.u) ; regle du resultat seulement
	ReplicaCrossing,     // intersection avec une replique de la courbe, s != s' : fausse (A8)
};

struct PrcCandidate
{
	double   s = 0.0;
	Vector2d xy;
	PrcRule  tier = PrcRule::Midpoint;
	bool     tangential = false;
};

struct PrcResult
{
	double                    s    = 0.0;
	PrcRule                   rule = PrcRule::Midpoint;
	std::vector<PrcCandidate> candidates;   // tous les candidats vus, tries par s
};

// Seuil de l'avertissement prc_near_endpoint (decision M13, defaut revisable) :
// un PRC retenu par l'Algorithme 1 avec min(u, 1 - u) < near_endpoint_u, u = s/L,
// donne un arc de profil tres court (« aiguille »). Le PRC est garde.
struct PrcLimits
{
	static constexpr double near_endpoint_u = 0.05;
};

// PRC de chaque courbe de base, dans l'ordre de spec.curves (Algorithme 1 et
// selection explicite, plan d'execution §6.2). `plan` : DevelopPlan(spec).
// Candidats : abscisses dans (tol.point, L - tol.point) (A-3), fusionnes a
// tol.parameter pres en gardant le palier le plus fort. Sans prc.u : premier
// palier non vide, candidat le plus proche du milieu ; a egalite (tol.parameter)
// le plus petit s et l'avertissement prc_tie. Avec prc.u : candidat le plus
// proche en u = s/L parmi tous ; au-dela de prc_snap, erreur
// prc_selection_no_candidate. Vide si une erreur a ete relevee.
// Point fixe : |l(s) - g(l(s))| <= tol.point pour un element g != id (axes a
// tol.point / 2, centre a tol.point / (2 sin(pi/n))) ; un croisement de l avec
// sa replique g(l) qui n'est pas un point fixe est un ReplicaCrossing.
// Precondition : `plan` vient de DevelopPlan sur ce meme `spec` (memes courbes,
// meme symetrie, memes tolerances).
std::optional<std::vector<PrcResult>> ComputePrcs (const MicroarchSpec &spec, const DevelopedPlan &plan, MicroarchReport &report);

// ---------------------------------------------------------------- courbe 3D (etape 4)

// PRC et profil d'une courbe de base.
struct BasicCurveResult
{
	PrcResult   prc;
	OgeeProfile profile;
};

// PRC (ComputePrcs), puis profil biarc de chaque courbe de base : longueur du
// plan, hauteurs des points de depart et d'arrivee (base du niveau comprise),
// s_prc. Profil plat : erreur profile_flat ; autre refus : profile_invalid ;
// profil non monotone : avertissement profile_not_monotonic ; tous sur curves[i].
// Meme precondition que ComputePrcs. Vide si une erreur a ete relevee.
std::optional<std::vector<BasicCurveResult>> BuildBasicCurves (const MicroarchSpec &spec, const DevelopedPlan &plan,
                                                               MicroarchReport &report);

// Courbe spatiale : courbe du plan exacte et profil exact, sans echantillon.
// Les repliques partagent le profil de leur courbe de base, copie par valeur.
struct SpaceCurve
{
	int             basic = -1;   // indice dans MicroarchSpec::curves
	SymmetryElement element;
	int             level = 0;
	PlanCurve       plan;         // replique exacte : Apply(symmetry, element, basic.plan)
	OgeeProfile     profile;
};

// Une SpaceCurve par replique de plan.curves, dans le meme ordre. `basics` est
// parallele a spec.curves (BuildBasicCurves). Vide si une replique designe une
// courbe de base absente de `basics`.
std::vector<SpaceCurve> BuildSpaceCurves (const DevelopedPlan &plan, const std::vector<BasicCurveResult> &basics);

// (PointAt(plan, s), HeightAt(profile, s)), s longueur d'arc du plan (M2).
Vector3d PointAt (const SpaceCurve &curve, double s);

// ---------------------------------------------------------------- echantillonnage (sortie seulement, M1)

struct SamplingParams
{
	double max_angle = 0.087266462599716;   // 5 degres, en radians
};

// Abscisses d'echantillonnage, strictement croissantes ; 0, s_prc et L en font
// partie exactement. Chaque arc du profil est echantillonne uniformement en
// angle ; le nombre de pas borne a max_angle l'angle de chaque pas sur l'arc du
// profil et, pour un arc du plan, l'angle balaye en plan (un pas dphi deplace s
// de r |sin phi| dphi). max_angle non fini ou
// non positif : seulement 0, s_prc et L. Au plus SamplingLimits::max_steps pas
// par arc du profil.
struct SamplingLimits
{
	static constexpr int max_steps = 1 << 16;
	// Points au plus pour tout un reseau (decision M15).
	static constexpr std::size_t max_network_points = 10000000;
};
std::vector<double> SampleParameters (const SpaceCurve &curve, const SamplingParams &params);

// ---------------------------------------------------------------- reseau (etape 5)

// Point d'une courbe du reseau : indice dans Network::curves, abscisse s.
struct Incidence
{
	int    curve = -1;
	double s     = 0.0;
};

// Point commun en 3D de deux courbes distinctes au moins, a tol.height pres.
struct Junction
{
	Vector3d               position;      // evaluation exacte de la premiere incidence
	std::vector<Incidence> incidences;    // triees (curve, s)
	double                 max_gap = 0.0; // ecart 3D maximal entre incidences
	bool                   prc     = false;   // une incidence est le PRC de sa courbe
	bool                   ground  = false;   // a la base du niveau, a tol.height pres
};

struct HeightGroup
{
	double                 z = 0.0;       // moyenne du groupe
	std::vector<Incidence> incidences;    // triees (curve, s)
};

// Croisement en plan de courbes a des hauteurs distinctes : au moins 2 groupes.
struct VisualCrossing
{
	Vector2d                 xy;
	std::vector<HeightGroup> groups;      // par z croissant
	double                   min_dz = 0.0;   // plus petit ecart entre groupes voisins
};

struct Network
{
	ResolvedTolerances            tol;
	std::vector<LevelSpec>        levels;
	std::vector<BasicCurveResult> basics;     // parallele a spec.curves
	std::vector<SpaceCurve>       curves;     // repliques, ordre de DevelopedPlan::curves
	std::vector<Junction>         junctions;
	std::vector<VisualCrossing>   crossings;
	std::vector<Incidence>        free_ends;  // extremites hors de toute jonction
};

// Classement d'un noeud du plan (expose pour les tests). Deux incidences d'une
// meme courbe sont le meme point si |ds| <= tol_abscissa et |dz| <= tol_height :
// elles sont fusionnees ; tri par z, coupure ou l'ecart depasse
// tol_height ; un groupe d'au moins 2 courbes distinctes est une jonction (prc et
// ground restent faux : ils dependent du reseau) ; au moins 2 groupes font un
// croisement visuel. La fusion compare a la derniere incidence gardee de la meme
// courbe, sans chainage : la plus petite abscisse est gardee.
struct NodeIncidence
{
	Incidence incidence;
	Vector3d  position;
};
struct NodeClassification
{
	std::vector<Junction>         junctions;
	std::optional<VisualCrossing> crossing;
};
NodeClassification ClassifyNode (const Vector2d &xy, std::vector<NodeIncidence> incidences,
                                 double tol_abscissa, double tol_height);

// Plafonds du reseau (decision M16) et zone tangente maximale (decision M17).
struct NetworkLimits
{
	// Points de contact (paires d'incidences) au plus, compte pendant le balayage
	// (decision M16-bis). Borne la memoire de BuildNetwork et la taille de sa sortie
	// JSON : sous 1 Go de pic au total au plafond (mesures du plan d'execution, §10.10).
	std::size_t max_contacts = std::size_t(1) << 17;   // 131 072
	// Zone tangente : un contact tangent dont la portion commune a tol.point pres,
	// d'un cote du point, depasse tangent_zone_ratio * D est un recouvrement.
	static constexpr double tangent_zone_ratio = 1e-3;
};

// Etapes 1 a 5. Intersections de toutes les paires de repliques d'un meme niveau
// (paires filtrees par boites englobantes elargies de 2 tol.point, sans effet sur
// le resultat). Un
// recouvrement est l'erreur plan_overlap sur curves[i], i le plus petit indice
// de base de la paire. Un contact tangent qui n'est pas bout a bout
// (at_end1 && at_end2) donne tangential_contact sur la courbe de base de chaque
// replique ; si sa zone a tol.point pres depasse NetworkLimits::tangent_zone_ratio * D
// d'un cote, c'est plan_overlap (M17). Au-dela de limits.max_contacts points de
// contact, erreur too_many_contacts sur "curves" (M16). Une jonction dont les
// incidences s'etalent en z de plus de tol.height (groupes chaines, §6.5) donne
// junction_height_spread (M19). Noeuds : points tries par x, regroupes a tol.point d'un
// representant fixe, sans chainage ; puis fusion des noeuds qui partagent une
// incidence (meme courbe, |ds| <= tol.parameter et |dz| <= tol.height). Cette
// fusion compare les incidences voisines dans l'ordre (courbe, s) et se chaine,
// alors que la deduplication de ClassifyNode ne se chaine pas : une jonction peut
// garder deux incidences d'une meme courbe a plus de tol.parameter, et
// Junction::incidences ne compte pas les passages d'une courbe. Vide si une
// erreur a ete relevee.
std::optional<Network> BuildNetwork (const MicroarchSpec &spec, MicroarchReport &report,
                                     const NetworkLimits &limits = NetworkLimits{});

// Polylignes 3D, une par courbe du reseau (SampleParameters, puis PointAt).
// Les points sont d'abord comptes, sans etre alloues : au-dela de
// SamplingLimits::max_network_points, erreur sample_limit_exceeded sur le
// pseudo-chemin "sampling.max_angle" et resultat vide.
std::vector<std::vector<Vector3d>> SampleNetwork (const Network &network, const SamplingParams &params,
                                                  MicroarchReport &report);

// Nombre de points que SampleNetwork produirait, sans les allouer ; le comptage
// s'arrete des que `limit` est depasse (resultat > limit).
std::size_t CountNetworkSamples (const Network &network, const SamplingParams &params, std::size_t limit);

// ---------------------------------------------------------------- graphe du reseau

// Graphe d'un reseau, comparable a la topologie d'un solide qui le suit : une
// composante de tubes assez fins a pour caracteristique d'Euler 2 (1 - cycles), donc
// la somme sur les composantes vaut 2 (vertices - edges).
//   sommets : les jonctions, puis les extremites libres ;
//   aretes  : les morceaux de courbe entre deux abscisses de coupure consecutives,
//             distinctes a tol.parameter pres. Les coupures d'une courbe sont 0, L et
//             les abscisses de ses incidences dans les jonctions ; une incidence a
//             tol.parameter d'un bout est ce bout. Deux incidences d'une meme courbe
//             dans une meme jonction (chainage, §11.2 risque 7) donnent une boucle.
// Une composante touche la base si l'un de ses sommets est a tol.height de la base
// du niveau de sa courbe, ou au-dessous.
// contract_length > 0 : un morceau de longueur 3D <= contract_length est contracte
// (ses deux sommets identifies, le morceau retire ; une boucle courte disparait), et
// `vertices` compte les sommets apres identification. C'est le graphe que suit un
// solide de rayon r pour contract_length = 2r : un morceau de longueur l <= 2r est
// dans la boule de rayon r centree en son milieu, que le solide contient.
// contract_length = 0 : graphe brut, vertices = jonctions + extremites libres.
struct NetworkGraph
{
	std::size_t       vertices = 0, edges = 0, components = 0;
	std::vector<bool> component_touches_base;   // une entree par composante
	// Composante de chaque sommet (jonctions d'abord, puis extremites libres).
	std::vector<std::size_t> vertex_component;
	// 2 (sommets - aretes) de chaque composante, sommets comptes comme `vertices`.
	std::vector<long>        component_euler;
};
NetworkGraph BuildNetworkGraph (const Network &network, double contract_length = 0.0);

} // namespace cgarchi::microarch
