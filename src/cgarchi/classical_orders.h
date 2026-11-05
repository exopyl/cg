#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cgarchi
{
namespace classical
{

// ============================================================================
//  Les cinq ordres d'apres Vignole : modele type des donnees de generation
// ============================================================================
//
// Source : src/cgarchi/data/cinq_ordres.json (copie a l'identique de
// Architecture/Antique/cinq_ordres.json, documente en section 9.0 de
// cinq_ordres.html). Unites : PARTIES de module (12 au module pour le toscan et
// le dorique, 18 pour les trois autres) ; le module est le demi-diametre
// inferieur du fut.
//
// Chaque nombre du JSON est lu par son champ exact « f » (entier ou fraction
// a/b), et non par son arrondi decimal « v ».
//
// Lecture stricte : une forme de moulure, une forme de plan ou un jeu de cles
// de saillie hors de ceux enumeres ci-dessous fait echouer la lecture
// (std::runtime_error nommant le chemin JSON). Une variante nouvelle des donnees
// ne se perd donc pas en silence.

enum class Order { Tuscan, Doric, Ionic, Corinthian, Composite };
constexpr int kOrderCount = 5;

// Cle de l'ordre dans le JSON : "toscan", "dorique", "ionique", "corinthien",
// "composite".
const char *OrderKey (Order order);

// conventions.plans du JSON.
enum class PlanForm
{
	Revolution,      // "revolution"      : cercle de rayon distance_axe + s
	Square,          // "carre"           : carre de demi-cote distance_axe + s
	ConcaveSquare,   // "carre_concave"   : abaque corinthien / composite
	IonicOriented,   // "oriente_ionique" : faces a volutes du chapiteau ionique
	Linear           // "lineaire"        : entablement file le long de la facade
};

// conventions.formes du JSON.
enum class MouldingForm
{
	Fillet,       // "listel"
	Face,         // "face"
	Ornament,     // "ornement"
	Ovolo,        // "ove"
	Cavetto,      // "cavet"
	CymaRecta,    // "doucine"
	CymaReversa,  // "talon"
	Torus,        // "tore"
	Scotia,       // "scotie"
	Conge         // "conge"
};

// Saillie au bas et au haut d'un membre, comptee depuis la face de reference
// du bloc.
struct Span
{
	double bottom = 0.0;
	double top    = 0.0;
};

// Les six structures de saillie presentes dans le JSON.
enum class ProjectionKind
{
	Constant,      // {s}
	Range,         // {s_bas, s_haut}
	Torus,         // {attache, s_max}
	Scotia,        // {s_haut, fond, s_bas}
	TwoCuts,       // {diagonale:{...}, face:{...}}, chaque coupe {s} ou {s_bas, s_haut}
	CoreEnvelope   // {noyau:{s_bas, s_haut}, enveloppe:{s_bas, s_haut}}
};

struct Projection
{
	ProjectionKind kind = ProjectionKind::Constant;

	// Constant : (s, s). Range : (s_bas, s_haut). Torus : (attache, attache).
	// Scotia : (s_bas, s_haut). TwoCuts : coupe diagonale. CoreEnvelope : noyau.
	Span main;

	// Torus : s_max. Scotia : fond. Sans objet sinon.
	double extremum = 0.0;

	// TwoCuts : coupe par le milieu d'une face.
	Span face;

	// CoreEnvelope : surface extreme atteinte par le decor.
	Span envelope;
};

struct Member
{
	std::string  name;
	MouldingForm form = MouldingForm::Face;
	double       h    = 0.0;
	Projection   projection;
	PlanForm     plan = PlanForm::Revolution;   // apres heritage du plan du bloc
	bool         planOverridden = false;        // « plan » porte par le membre
};

struct Block
{
	std::string key;                          // "base", "corniche_mutules", ...
	double      axisDistance = 0.0;           // distance_axe
	PlanForm    plan = PlanForm::Revolution;
	std::optional<double> totalHeight;        // hauteur_totale (absente : fut_haut)
	std::optional<double> totalProjection;    // saillie_totale
	std::vector<std::string> outsideModule;   // hors_module : noms de membres
	std::vector<Member> members;              // de HAUT en BAS, comme le JSON

	bool IsOutsideModule (const Member &member) const;
};

enum class DiminutionMethod
{
	Cosine,    // methode_1 (toscan, dorique)
	Conchoid   // methode_2 (ionique, corinthien, composite)
};

struct OrderGeneral
{
	double column_M = 0.0;
	double base_M = 0.0;
	double shaft_M = 0.0;
	double capital_M = 0.0;
	double lowerDiameter_p = 0.0;
	double upperDiameter_p = 0.0;
	double entablature_M = 0.0;
	double pedestal_M = 0.0;
	double totalWithoutPedestal_M = 0.0;
	double totalWithPedestal_M = 0.0;
	double intercolumniation_M = 0.0;   // entrecolonnement_M : distance nu a nu des futs au pied
	DiminutionMethod diminution = DiminutionMethod::Cosine;
};

// ornements.volute du JSON (ordre ionique), en parties. Les valeurs sont des
// chaines (« 16 », « 9 »...) lues par ParseExactNumber.
struct IonicVoluteData
{
	double height = 0.0;            // hauteur : cathete
	double eyeDiameter = 0.0;       // oeil_diametre
	double centreFromTop = 0.0;     // centre_depuis_haut
	double centreFromBottom = 0.0;  // centre_depuis_bas
	double top = 0.0;               // distances_au_centre.haut
	double right = 0.0;             // distances_au_centre.droite
	double bottom = 0.0;            // distances_au_centre.bas
	double left = 0.0;              // distances_au_centre.gauche
	double catheteSpacing = 0.0;    // cathetes_entraxe
	double eyeFromAxis = 0.0;       // centre_oeil_depuis_axe
	double topBelowCapital = 0.0;   // haut_volute_sous_sommet_chapiteau
	double faceSpacing = 0.0;       // faces_des_volutes_en_plan
	double cushionWidth = 0.0;      // largeur_coussinet
	double listelFace = 0.0;        // listel_face : saillie du listel sur la face
};

// Section d'une cannelure (ornements.cannelures).
enum class FluteSection
{
	Segment60,    // dorique A : arc de 60 degres, rayon = corde, fleche 0,134 corde
	Semicircle    // dorique B, ionique, corinthien, composite : demi-cercle sur la corde
};

// ornements.cannelures du JSON. Les regles textuelles (arc de 60 degres,
// demi-cercle, corde = 2 r sin(pi/N)) sont encodees dans classical_flutes.h ;
// seuls les nombres sont lus ici.
struct FluteData
{
	int  count = 0;                 // nombre
	bool listels = false;           // listels (dorique) ou separees_par == "listels"
	// au_pied (ordres a listels), en parties : listel, cannelure, profondeur.
	double footListel = 0.0;
	double footFlute = 0.0;
	double footDepth = 0.0;
	std::vector<FluteSection> sections;   // dorique : A puis B ; sinon demi-cercle
};

// ---------------------------------------------------------------------------
//  Ornements d'entablement (ornements.* du JSON), en parties
// ---------------------------------------------------------------------------
//
// Les ornements portes par un membre de la corniche y sont lies par une table
// en code (EntablatureOrnamentMember) : le membre doit exister une seule fois,
// de forme « ornement » et de saillie {s}, sinon la lecture echoue.

// Triglyphe dorique (ornements.triglyphe, ornements.metope). Hauteur egale a
// celle de la frise, chapiteau a celle du membre du bas de chaque corniche, et
// angle des canaux pleins a 2 atan((largeur/2)/profondeur) : verifies a la
// lecture. Les hauteurs des denticules, mutules et blocs sont verifiees
// contre celles de leur membre lie.
struct TriglyphData
{
	double width = 0.0;              // largeur
	double height = 0.0;             // hauteur (= hauteur de la frise)
	double capitalHeight = 0.0;      // chapiteau_h (membre de la corniche)
	double spacing = 0.0;            // entraxe
	double projection = 0.0;         // saillie_sur_metope (metope au nu de la frise)
	double metopeWidth = 0.0;        // metope.largeur
	// divisions, de gauche a droite : largeur et nature (canal ou bande ; les
	// demi-canaux des bords sont des canaux de profondeur halfChannelDepth).
	std::vector<double> divisionWidths;
	std::vector<bool>   divisionIsChannel;
	double channelAngleDeg = 0.0;    // canaux_detail.angle
	double channelDepth = 0.0;       // canaux_detail.profondeur
	double halfChannelDepth = 0.0;   // canaux_detail.profondeur_demi_canal
};

// Regula et gouttes sous la tenia (ornements.gouttes).
struct GuttaeData
{
	double regulaHeight = 0.0;
	double guttaHeight = 0.0;
	int    count = 0;
	double diameterBottom = 0.0;
	double diameterTop = 0.0;        // petite base contre la regula
	double spacing = 0.0;
};

struct DentilData
{
	double width = 0.0, gap = 0.0, height = 0.0;
	double depth = 0.0;              // recul_du_fond : de la face au fond
	double back = 0.0;               // fond_depuis_nu_frise
};

struct MutuleData
{
	double width = 0.0, height = 0.0;
	double face = 0.0;               // plan : face, depuis le nu de la frise
	double back = 0.0;               // plan : arriere
	int    rows = 0, perRow = 0;     // gouttes
	double guttaPitch = 0.0, guttaHeight = 0.0, guttaDiameterBottom = 0.0, guttaDiameterTop = 0.0;
	double listelHeight = 0.0;       // listel sous la mutule, d'ou pendent les gouttes
};

// Gouttes du plafond de la corniche a denticules (dorique).
struct SoffitGuttaeData
{
	int    rows = 0, perRow = 0;
	double pitchAlong = 0.0, pitchDepth = 0.0;
	double from = 0.0, to = 0.0;     // bande de la sous-face, en saillie
	double diameterBottom = 0.0;
};

struct ModillionData
{
	double width = 0.0, coffer = 0.0, spacing = 0.0, length = 0.0;
};

struct BlockRowData
{
	double width = 0.0, gap = 0.0, height = 0.0, depth = 0.0;
};

struct EntablatureOrnaments
{
	std::optional<TriglyphData>     triglyph;
	std::optional<GuttaeData>       guttae;
	std::optional<DentilData>       dentils;
	std::optional<MutuleData>       mutules;
	std::optional<SoffitGuttaeData> soffitGuttae;
	std::optional<ModillionData>    modillions;
	std::optional<BlockRowData>     blocks;
};

// Membre de corniche qui porte un ornement : bloc et nom exacts du JSON.
struct OrnamentMember
{
	const char *block = nullptr;
	const char *member = nullptr;
};

enum class EntablatureOrnament { Dentils, Mutules, Modillions, Blocks };

// Entraxe des colonnes d'une colonnade, en parties : entrecolonnement (nu a
// nu au pied, HTML 4.1 et 4.2 : « 5 1/2 M = trois entraxes moins un diametre »
// au dorique) plus le diametre inferieur.
struct OrderData;
double AxisSpacing (const OrderData &order);

// Table du lien ornement -> membre ; nul si l'ordre n'a pas cet ornement.
const OrnamentMember *EntablatureOrnamentMember (const std::string &orderKey, EntablatureOrnament ornament);

// Le membre lie (EntablatureOrnamentMember) dans les donnees de l'ordre. Leve
// std::invalid_argument s'il est absent.
const Member &LinkedOrnamentMember (const OrderData &order, EntablatureOrnament ornament);

struct OrderData
{
	Order        order = Order::Tuscan;
	std::string  key;
	int          partsPerModule = 12;
	OrderGeneral general;
	std::vector<Block> blocks;                  // ordre_des_blocs_de_haut_en_bas
	std::optional<IonicVoluteData> volute;      // ornements.volute, si present
	std::optional<FluteData> flutes;            // ornements.cannelures, si present
	EntablatureOrnaments entablatureOrnaments;

	// Nul si le bloc n'existe pas pour cet ordre.
	const Block *FindBlock (std::string_view key) const;
};

// Un element de la liste `controles` du JSON.
struct Control
{
	std::string order;
	std::string block;
	std::string label;       // champ « controle »
	std::string expected;    // champ « attendu », texte
	std::string obtained;    // champ « obtenu », texte
	bool        ok = false;
};

struct OrdersData
{
	std::string version;
	std::array<OrderData, kOrderCount> orders;
	std::vector<Control> controls;

	// diminution.methode_2 : CD de la planche (en modules), et CD calcule par
	// l'auteur des donnees pour les ordres ou il le donne.
	double conchoidCDPlate_M = 0.0;

	// conventions.conges_du_fut.regle : congés du fut en quart de cercle, de
	// rayon egal a la saillie du listel raccorde (regle encodee dans
	// BuildColumnProfile).
	std::string shaftFilletRule;

	// conventions.plans.face_arriere_entablement.cas_colonnade_isolee
	// .coupe_symetrique : face arriere d'une colonnade isolee.
	std::string symmetricEntablatureRule;
	std::vector<std::pair<std::string, double>> conchoidCDComputed_M;

	const OrderData &Get (Order order) const { return orders[static_cast<int> (order)]; }
};

// Lecture stricte d'un texte JSON au format de cinq_ordres.json.
OrdersData ParseClassicalOrders (std::string_view jsonText);

// Les donnees embarquees, lues une fois. Une lecture en echec leve a chaque
// appel.
const OrdersData &ClassicalOrders ();

// Valeur d'un nombre ecrit « a/b », « a » ou en decimal. Leve
// std::invalid_argument sur un texte illisible.
double ParseExactNumber (std::string_view text);

// Recalcul d'un controle depuis les profils.
struct ControlCheck
{
	const Control *control = nullptr;
	bool   computable = false;   // faux : mesure sur la gravure, sans profil
	double expected   = 0.0;     // « attendu »
	double recomputed = 0.0;
	bool   matches    = false;   // |recomputed - expected| <= 1e-9 * max(1, |expected|)
};

// Trois familles de controles sont recalculables : « somme des hauteurs »
// (membres hors module exclus), « saillie maximale » (plus grande saillie lue
// sur les membres du bloc) et « largeur en plan : <membre> » (2 x (distance_axe
// + s)). Les « mesure sur la gravure » ne le sont pas : computable = false.
// Leve std::invalid_argument si le champ « attendu » d'un controle
// recalculable n'est pas un nombre (ParseExactNumber) : un controle illisible
// n'est pas un controle passe.
std::vector<ControlCheck> RecomputeControls (const OrdersData &data);

} // namespace classical
} // namespace cgarchi
