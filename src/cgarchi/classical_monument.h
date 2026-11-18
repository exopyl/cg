#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <cgarchi/classical_greek_doric_column.h>
#include <cgarchi/classical_greek_entablature.h>
#include <cgarchi/classical_krepis.h>
#include <cgarchi/classical_peristyle.h>
#include <cgarchi/classical_temple.h>

namespace cgarchi
{
namespace classical
{

// ============================================================================
//  Document du monument "monument/v9" : valeurs du README du monument, avec
//  leur unite, leur statut, leurs sources et leur lieu ; conversion vers les
//  reglages du temple
// ============================================================================
//
// Le document a la forme de sa source : sections et valeurs du README, chacune
// dans l'unite ou le README l'ecrit (metres, pouces et pieds de Stuart et
// Revett, millimetres), dans le repere du README (x vers l'est, y vers le nord,
// z vers le haut depuis le dessus du stylobate, origine au centre du
// stylobate). Il est ecrit par le script du monument (src/cgarchi/data/
// <monument>_from_readme.py, moteur monument_from_readme.py),
// jamais a la main, et embarque a la build (classical_monument_data.h). Schema :
// src/cgarchi/schemas/monument-v9.schema.
//
// Statuts : les mots du README, liste fermee. Une valeur "manquant" n'a pas de
// cle "value" ; toute autre en a une. "lu" et "mesure" citent au moins une
// source ; "deduit" et "convention" peuvent n'en citer aucune ; "manquant" n'en
// cite aucune. Toute valeur porte son lieu dans le README ("readme").
//
// Unites (D3-37, amende par D3-52, D3-75 et le registre MO, unite "ratio") :
// chaque valeur porte "unit", liste fermee "m", "in", "mm", "1", "ft", "°",
// "ratio" ; "1" pour un compte et lui seul ; "ft" pour les pieds d'une cote en
// pieds et pouces et pour eux seuls : cle "<nom>Ft", entier >= 1, a cote de la
// cle "<nom>In" de ses pouces ; "°" (degre) pour un angle et lui seul
// (entablature.soffitSlope, et le controle pedimentControls.rakingSlope) ;
// "ratio" pour un rapport "1 : r" du README et lui seul, valeur r reelle > 0
// (pediment.rakingRun). Une valeur manquante
// porte l'unite attendue. La conversion en metres est ici (MonumentLength,
// MonumentFeetAndInches), jamais dans le script : 1 in = 0,0254 m, 1 ft = 12 in
// (README section 1.1).
//
// Formes de valeur (plan 3 section 20.5, choix de format) : une cote en pieds et
// pouces occupe deux cles ; la division du triglyphe est une suite (values,
// kinds) lue telle qu'ecrite, mots du README "demi-canal", "bande", "canal" ;
// les hauteurs de la corniche rampante, de meme (mots "listel", "filet",
// "larmier", "gorge de la sous-face (bec)") ; la section de la corniche
// rampante est un polygone, deux listes u et h de meme rang (regle R11 du
// script, plan 3 section 24.23.7).
//
// Conversion de repere (registre MO2, classical_orders_implementation.md
// section 10.9) : la facade de D2-5 (rangee front, le long de X, en Y < 0) est
// l'entree du monument, la facade est du README. Front le long de y, flank le
// long de x ; (X, Y, Z) = (y, -x, z + stylobateTop), quart de tour direct ;
// cotes : est = Front, nord = Right, ouest = Back, sud = Left.

constexpr const char *kMonumentSchema = "monument/v9";

// Pouce anglais en metres (README section 1.1).
constexpr double kMonumentInch = 0.0254;

// Pied anglais en metres, 12 pouces (README section 1.1).
constexpr double kMonumentFoot = 0.3048;

// Statuts du README, dans l'ordre : "lu", "déduit", "mesuré", "convention",
// "manquant".
enum class MonumentStatus { Read, Deduced, Measured, Convention, Missing };

// Unites du document, dans l'ordre : "m", "in", "mm", "1", "ft", "°", "ratio".
enum class MonumentUnit { Metre, Inch, Millimetre, One, Foot, Degree, Ratio };

struct MonumentValue
{
	std::optional<double>    value;     // vide si et seulement si status == Missing
	MonumentUnit             unit = MonumentUnit::Metre;   // presente aussi pour manquant : unite attendue
	MonumentStatus           status = MonumentStatus::Missing;
	std::vector<std::string> sources;   // identifiants de MonumentData::sources
	std::string              readme;    // lieu de la valeur dans le README : "§3.2, l. 86"
};

// Longueur `value` exprimee en `unit`, en metres. Leve std::invalid_argument,
// prefixe "Monument : " et cle entre apostrophes, si unit est "1" (sans
// dimension), "°" (un angle), "ratio" (un rapport) ou hors de l'enumeration. "ft" vaut kMonumentFoot ;
// une cote en pieds et pouces passe par MonumentFeetAndInches.
double MonumentMetres (double value, MonumentUnit unit, std::string_view key);

// Valeur d'une longueur en metres. Leve std::invalid_argument, prefixe
// "Monument : " et cle entre apostrophes, si la valeur est manquante, sans
// dimension, un angle, ou en pieds (moitie d'une cote en pieds et pouces :
// passer par MonumentFeetAndInches).
double MonumentLength (const MonumentValue &v, std::string_view key);

// Cote en pieds et pouces, en metres : (pieds x 12 + pouces) x kMonumentInch.
// `key` est la cle commune, sans suffixe ("entablature.friezeHeight") ; les
// refus nomment "<key>Ft" ou "<key>In". Leve std::invalid_argument, prefixe
// "Monument : ", si l'une des deux valeurs manque, si les pieds ne sont pas en
// "ft" ou pas un entier >= 1, ou si les pouces ne sont pas en "in", finis et
// >= 0. Le produit par 12 est exact et le produit par kMonumentInch vient en
// dernier : le resultat ne depend pas d'une contraction FMA (plan 3, MS-40).
double MonumentFeetAndInches (const MonumentValue &feet, const MonumentValue &inches, std::string_view key);

// Rapport "1 : r" du README : la valeur r. Leve std::invalid_argument, prefixe
// "Monument : " et cle entre apostrophes, si la valeur manque ou si son unite
// n'est pas "ratio".
double MonumentRatio (const MonumentValue &v, std::string_view key);

struct MonumentSourceDocument
{
	std::string   document;    // chemin du README, relatif a la racine des documents
	std::string   sha256;      // 64 chiffres hexadecimaux minuscules
	std::uint64_t bytes = 0;
	std::string   derivedBy;   // script qui a ecrit le document, relatif au depot
};

struct MonumentKrepis      // README section 3.2
{
	MonumentValue stylobateWidth, stylobateLength, stepCount;
	MonumentValue stylobateHeight, secondStepHeight, thirdStepHeight;
	MonumentValue secondStepTread;   // giron du 2e degre : son debord au-dela du stylobate
	MonumentValue thirdStepTread;    // giron du 3e degre : son debord au-dela du 2e
	MonumentValue cornerMargin;      // debord du stylobate au-dela du nu des colonnes d'angle (controle)
	MonumentValue euthynteriaHeight; // hauteur de l'euthynteria, assise pleine sous le 3e degre
	MonumentValue euthynteriaTread;  // ressaut de l'euthynteria au-dela du pied du 3e degre
};

struct MonumentPeristyle   // README sections 2 et 4.2
{
	MonumentValue frontColumns, flankColumns;
	MonumentValue frontSpacing, flankSpacing, frontCornerSpacing, flankCornerSpacing;
};

// README sections 4.1, 4.4 et 5. Retraits depuis le nu de l'abaque, profondeurs
// sous le lit de l'abaque (README section 5.2, profil de l'echine) ; annelets :
// retraits de leur sommet et de leur pied (section 5.2, ligne "Annelets").
struct MonumentColumns
{
	MonumentValue lowerDiameter, cornerLowerDiameter, upperDiameter, height;
	MonumentValue fluteCount, fluteDepth;
	MonumentValue entasisSagitta, entasisHeight;
	MonumentValue abacusHeight, abacusHalfWidth, echinusHeight;
	MonumentValue echinusJunctionDepth, echinusJunctionRetreat, echinusTopRetreat, echinusLipRadius;
	MonumentValue annuletCount, annuletsHeight, annuletsTopRetreat, annuletsBottomRetreat;
	MonumentValue hypotrachelionHeight, grooveHeight, grooveDepth;
};

// Resultats imprimes par le README, sans statut, en metres. Le lieu de chacun
// est lu et controle par le lecteur, sans etre conserve.
struct MonumentControls
{
	std::vector<double> frontAxesY, flankAxesX;   // section 4.3, repere du README, strictement croissants
	// Section 12, sommes de facade et de flanc ; cles facultatives, vides si absentes.
	std::optional<double> frontSum, flankSum;
};

// Controles de la krepis (bloc krepisControls), memes regles que MonumentControls :
// emprise de l'euthynteria imprimee par le README, largeur (facade) et longueur.
// Cles exigees du bloc : valeurs significatives si et seulement si
// MonumentBlocks::krepisControls ; 0 sinon.
struct MonumentKrepisControls
{
	double footprintWidth = 0.0, footprintLength = 0.0;
};

// Blocs presents au document. Un bloc absent est une partie non documentee : ses
// valeurs restent celles par defaut, manquantes.
struct MonumentBlocks
{
	bool krepis = false, peristyle = false, columns = false, entablature = false, pediment = false;
	bool controls = false, krepisControls = false, columnControls = false, entablatureControls = false,
	     pedimentControls = false;
};

// Controle de la colonne : nombre imprime par le README, avec son unite.
struct MonumentControlValue
{
	double       value = 0.0;
	MonumentUnit unit = MonumentUnit::Metre;
};

// Points d'un profil, de haut en bas : profondeurs strictement croissantes.
struct MonumentProfileControl
{
	std::vector<double> depths, retreats;
	MonumentUnit        unit = MonumentUnit::Metre;
};

// Controles de la colonne (README sections 4.4, 5 et 12), sans statut.
struct MonumentColumnControls
{
	MonumentControlValue   shaftHeight, footFluteChord, fluteCentralRadius, fluteLateralRadius, topFluteDepth;
	MonumentControlValue   cornerFluteChord, cornerFluteDepth;
	MonumentControlValue   capitalHeight, annuletsAndRelief, upperRadius, echinusLipRetreat;
	MonumentProfileControl echinusProfile;
};

// Suite de largeurs lue par la regle R5 du script (README section 6.3, ligne
// "Division du triglyphe"), mots du README "demi-canal", "bande", "canal".
enum class MonumentDivisionKind { HalfChannel, Band, Channel };

struct MonumentDivisions
{
	std::vector<double>               values;   // > 0, finies, de gauche a droite
	std::vector<MonumentDivisionKind> kinds;    // meme taille que values
	MonumentUnit                      unit = MonumentUnit::Metre;
	MonumentStatus                    status = MonumentStatus::Missing;   // jamais manquant au document
	std::vector<std::string>          sources;
	std::string                       readme;
};

// README sections 6.1 a 6.4 : entablement du peristyle. Hauteurs ; saillies
// depuis la ligne des axes des colonnes (README section 6.2). Regulae et
// gouttes (section 6.1, ligne "gouttes (sous la regula)" ; section 6.2, lignes
// "Regulae" et "Gouttes des regulae") : saillie de la face de la regula ; nombre
// de gouttes par regula, diametre et vide (au pied, EN19) ; saillies de la
// generatrice avant en tete et au pied. Mutules (section 6.4, lignes "Mutules"
// et "Profondeur des mutules") : largeur en pieds et pouces ; profondeur, du
// devant au dos, le long de la sous-face. Sous-face du geison (section 6.4,
// ligne "Sous-face du geison") : pente, en degres. Gouttes des mutules
// (section 6.4, ligne "Gouttes des mutules") : diametres de tete des trois
// rangs, du devant vers le dos ; vides entre tetes ; hauteur, normale a la
// sous-face ; axe des gouttes extremes d'un rang depuis le bord du mutule ;
// gouttes par rang (compte) ; evasement du pied, en diametre.
struct MonumentEntablature
{
	MonumentValue architraveHeightFt, architraveHeightIn, taeniaHeight, regulaHeight, guttaeHeight,
	              architraveFaceHeight;
	MonumentValue friezeHeightFt, friezeHeightIn, triglyphCapitalHeight;
	MonumentValue corniceHeightFt, corniceHeightIn, mutulePlateHeight, dripHeight, coronaHeight, filletHeight,
	              crownHeight;
	MonumentValue architraveTopProjection, architraveFootProjection, taeniaProjection, triglyphFace;
	MonumentValue triglyphCapitalBandProjection, triglyphCapitalListelProjection;
	MonumentValue triglyphCapitalListelHeight, triglyphCapitalBandHeight;   // partage, convention de la source
	MonumentValue metopeFace;                                               // nu des metopes
	MonumentValue mutulePlateProjection, dripProjection, coronaProjection, filletProjection, crownProjection;
	MonumentValue innerFace;                                                // face interieure du peristyle
	MonumentValue triglyphWidthFt, triglyphWidthIn;
	MonumentValue metopeWidthFt, metopeWidthIn;                             // metope lue : controle
	MonumentDivisions triglyphDivisions;
	MonumentValue channelDepth;                                             // profondeur mesuree : controle de EN11
	MonumentValue regulaProjection, regulaGuttaCount, regulaGuttaDiameter, regulaGuttaGap;
	MonumentValue regulaGuttaTopProjection, regulaGuttaFootProjection;
	MonumentValue mutuleWidthFt, mutuleWidthIn, mutuleDepth;
	MonumentValue soffitSlope;                                              // "°"
	MonumentValue mutuleGuttaFrontDiameter, mutuleGuttaMiddleDiameter, mutuleGuttaRearDiameter;
	MonumentValue mutuleGuttaFrontGap, mutuleGuttaRearGap, mutuleGuttaHeight, mutuleGuttaEdgeAxis;
	MonumentValue mutuleGuttaPerRow, mutuleGuttaFootFlare;
};

// Controles de l'entablement (README sections 6.1 a 6.4 et 12), sans statut ;
// les comptes sont en "1", entiers >= 1. Angle de la frise (README section
// 6.3, ligne "Metopes d'angle") : premiere solution, celle du modele (EN15) :
// centre du triglyphe d'angle au-dela de l'axe d'angle, distance d'angle de
// facade (entraxe d'angle plus ce centre), metopes d'angle de facade et de
// flanc, decalage de l'intermediaire vers l'angle ; seconde solution
// (intermediaire au milieu de la travee) : metopes interieure et exterieure, de
// facade et de flanc. Gouttes des regulae (section 6.1, ligne "gouttes (sous la
// regula)" ; section 12, ligne "Regula") : marge de chaque bout, reste de la
// largeur de la regula, nombre ; dos des mutules depuis l'axe, sous une
// sous-face horizontale (section 6.4, ligne "Profondeur des mutules"). Gouttes
// des mutules (section 6.4, ligne "Gouttes des mutules") : nombre de rangs et
// diviseur du pas (comptes), axes des rangs du milieu et de l'arriere depuis
// l'arete avant, pas en largeur, evasement en rayon, sa valeur non arrondie,
// avant du pied de la goutte avant depuis l'axe. Profondeur sous la pente
// (section 6.4, ligne "Profondeur des mutules") : marge arriere des tetes,
// projection horizontale, dos depuis l'axe. Sous-face du geison (section 6.4,
// ligne "Sous-face du geison") : cotes du pivot, de l'arete avant-haute du
// mutule, du dessous au dos et du haut du bec, depuis le stylobate ; montee ;
// ressaut vertical au dos ; saillie du pivot.
struct MonumentEntablatureControls
{
	MonumentControlValue total, corniceSum, triglyphDivisionSum, frontMetope, flankMetope;
	MonumentControlValue metopeCount, frontTriglyphs, flankTriglyphs;
	MonumentControlValue channelBottom, metopeRecess;
	MonumentControlValue cornerTriglyphOffset, frontCornerDistance, frontCornerMetope, flankCornerMetope;
	MonumentControlValue intermediateShift;
	MonumentControlValue frontMidBayInnerMetope, frontMidBayOuterMetope, flankMidBayInnerMetope, flankMidBayOuterMetope;
	MonumentControlValue regulaGuttaMargin, regulaGuttaShortfall, regulaGuttaCountCheck, mutuleBack;
	MonumentControlValue mutuleGuttaRows, mutuleGuttaPitchCount;
	MonumentControlValue mutuleGuttaMiddleAxis, mutuleGuttaRearAxis, mutuleGuttaPitch;
	MonumentControlValue mutuleRearMargin, mutuleHorizontalDepth, mutuleInclinedBack;
	MonumentControlValue soffitPivotZ, mutuleFrontTopZ, soffitBackZ, soffitRise, geisonBackStep, dripTopZ;
	MonumentControlValue soffitPivotProjection;
	MonumentControlValue mutuleGuttaRadialFlare, mutuleGuttaRadialFlareExact, mutuleGuttaFrontFootProjection;
};

// Membres de la corniche rampante nommes par la suite des hauteurs (README
// section 9, ligne "Corniche rampante : hauteur"), dans l'ordre : "listel",
// "filet", "larmier",
// "gorge de la sous-face (bec)".
enum class MonumentRakingMember { Listel, Fillet, Corona, Drip };

// Suite des hauteurs normales de la corniche rampante (regle R5 du script),
// de haut en bas, telle qu'ecrite.
struct MonumentRakingHeights
{
	std::vector<double>               values;   // > 0, finies, de haut en bas
	std::vector<MonumentRakingMember> kinds;    // meme taille que values
	MonumentUnit                      unit = MonumentUnit::Metre;
	MonumentStatus                    status = MonumentStatus::Missing;   // jamais manquant au document
	std::vector<std::string>          sources;
	std::string                       readme;
};

// Polygone lu par la regle R11 du script : sommets (u[k], h[k]) dans l'ordre du
// README, ferme du dernier au premier.
struct MonumentPolygon
{
	std::vector<double>      u, h;   // meme taille, au moins 3 ; finies et >= 0
	MonumentUnit             unit = MonumentUnit::Metre;
	MonumentStatus           status = MonumentStatus::Missing;   // jamais manquant au document
	std::vector<std::string> sources;
	std::string              readme;
};

// README section 9 : fronton. Hauteur du dessus de la corniche horizontale au
// haut de la corniche rampante dans l'axe, en pieds et pouces (ligne « Hauteur
// du fronton ») ; pente 1 : r, rapport (« Pente des rampants ») ; saillie de la
// corniche rampante depuis le nu du tympan, en pieds et pouces, et nu du tympan
// depuis l'axe des colonnes de la facade ; hauteurs des membres ; section (u
// depuis le nu du tympan, h normale a la pente depuis le dessous de la
// section) ; sima, acroteres et tuiles.
struct MonumentPediment
{
	MonumentValue         apexRiseFt, apexRiseIn, rakingRun, rakingProjectionFt, rakingProjectionIn, tympanumFace;
	MonumentRakingHeights rakingHeights;
	MonumentPolygon       rakingSection;
	MonumentValue         sima;
};

// Controles du fronton (README section 9), sans statut : hauteur ; pente, en
// degres, et pied du haut de la corniche rampante sur le dessus de la corniche
// horizontale, depuis l'axe du temple (eaveHalfSpan, egout du comble) ; hauteur
// de la corniche rampante, normale en pouces et en metres, verticale en pouces
// et en metres ; saillie, nu du tympan, saillies du listel, du larmier et du
// filet du bas, ecart des larmiers.
struct MonumentPedimentControls
{
	MonumentControlValue apexRise, rakingSlope, eaveHalfSpan;
	MonumentControlValue rakingHeight, rakingHeightM, rakingVerticalHeight, rakingVerticalHeightM;
	MonumentControlValue rakingProjection, tympanumFaceM, rakingListelProjection, rakingCoronaProjection;
	MonumentControlValue rakingBottomFilletProjection, rakingCoronaGap;
};

struct MonumentData
{
	std::string                        monument;
	MonumentSourceDocument             source;
	std::map<std::string, std::string> sources;   // identifiant -> libelle
	MonumentKrepis                     krepis;
	MonumentPeristyle                  peristyle;
	MonumentColumns                    columns;
	MonumentControls                   controls;
	MonumentColumnControls             columnControls;
	MonumentEntablature                entablature;
	MonumentEntablatureControls        entablatureControls;
	MonumentPediment                   pediment;
	MonumentPedimentControls           pedimentControls;
	MonumentKrepisControls             krepisControls;
	MonumentBlocks                     present;
};

// Lecture stricte d'un document "monument/v9" (json_reader::StrictReader). Leve
// std::runtime_error si le document est refuse ; le message porte chaque refus,
// une ligne chacun, "monument/v9 <chemin> : <motif>". Blocs facultatifs (registre
// MO-P2) : krepis, peristyle, columns, entablature et pediment ; un bloc present
// porte toutes ses cles. Refus : texte illisible ; cle en double ; schema autre
// que "monument/v9" ; cle inconnue, a tout niveau ; cle absente d'un bloc
// present (controls.frontSum et controls.flankSum exceptees) ; peristyle sans
// controls, controls sans peristyle, krepisControls sans krepis, columnControls
// sans columns, entablatureControls sans entablature, pedimentControls sans
// pediment ("requires block \"<partie>\"") ; null ; type faux ; status hors des cinq ; unit hors des sept,
// "1", "ft", "°" ou "ratio" sur une longueur, autre que "1" sur un compte, autre
// que "ft" sur une cle "<nom>Ft", autre que "°" sur un angle, autre que "ratio"
// sur un rapport ; value absente pour un statut autre que manquant, presente
// pour manquant ; longueur, angle ou rapport non fini ou <= 0 ; compte ou pieds non entiers ou < 1 ; source inconnue de la table, ou
// entree de la table jamais citee ; lu ou mesure sans source ; manquant avec une
// source ; readme vide ; sha256 hors de [0-9a-f]{64} ; bytes < 1 ; liste de
// coordonnees de controle vide, coordonnee non finie, ou coordonnees non
// strictement croissantes ; controle de longueur non fini ou <= 0, ou sans
// dimension ; controle de compte autre que "1" ou non entier >= 1 ; profil de
// controle vide, de listes de longueurs differentes, ou de profondeurs non
// strictement croissantes ; suite de divisions ou de hauteurs vide, de listes de
// tailles differentes, de valeur non finie ou <= 0, de mot hors de sa liste, sans
// dimension, ou manquante ; polygone de moins de 3 sommets, de listes u et h de
// tailles differentes, de coordonnee non finie ou < 0, sans dimension, ou
// manquant ; controle d'angle autre que "°".
MonumentData ParseMonument (std::string_view jsonText);

// Document embarque du monument `name` : "temple-hephaistos"
// (data/temple_hephaistos.json) ou "temple-ares" (data/temple_ares.json), lu une
// fois. Leve std::invalid_argument, prefixe "Monument : " et nom entre
// apostrophes, pour tout autre nom ; une lecture en echec leve a chaque appel. La
// reference designe un objet statique local : valide pour la duree du programme,
// premiere lecture sure entre fils.
const MonumentData &EmbeddedMonument (std::string_view name);

// EmbeddedMonument ("temple-hephaistos").
const MonumentData &HephaistosMonument ();

// Point du repere du README dans le repere du temple (D2-5, D3-16) :
// (X, Y, Z) = (y, -x, z + stylobateTop), stylobateTop etant la cote du dessus
// du stylobate au-dessus du pied de la krepis (KrepisLayout::stylobateTop).
struct MonumentFramePoint
{
	double x = 0.0, y = 0.0, z = 0.0;
};
MonumentFramePoint TempleFrameFromMonument (double x, double y, double z, double stylobateTop);

// Conversions vers les reglages du temple : facade du README le long de X,
// flancs le long de Y (voir l'en-tete). Levent std::invalid_argument, prefixe
// "Monument : " et cle entre apostrophes ('krepis.thirdStepHeight'), si une
// valeur employee est manquante, si une longueur employee est sans dimension
// (MonumentLength), si un compte n'est pas un entier >= 1, ou si stepCount
// n'est pas 3 (les trois hauteurs de degre du schema). Krepis (KR3, amende par
// la convention Q6) : stylobate, puis lowerSteps de haut en bas, 2e degre,
// 3e degre et euthynteria, assise pleine de hauteur euthynteriaHeight et de
// debord euthynteriaTread sur les deux axes, pied de la krepis a son pied. Les
// reglages rendus ne sont pas valides par la conversion : LayoutKrepis,
// LayoutPeristyle et LayoutTemple les valident.
KrepisSettings    MonumentKrepisSettings (const MonumentData &data);
PeristyleSettings MonumentPeristyleSettings (const MonumentData &data);

// Colonne du README (section 5), courante ou d'angle (corner : diametre
// inferieur cornerLowerDiameter, tout le reste identique, README section 4.4).
// Memes regles d'erreur que MonumentKrepisSettings, plus un refus si
// grooveDepth n'est pas manquante (GD3d ne vaut que pour elle) et si
// annuletCount depasse kGreekDoricMaxAnnulets (avant toute allocation) ; reglages non
// valides ici (LayoutTemple et BuildGreekDoricColumnProfile les valident).
// Conversion (R = upperDiameter / 2, A = abacusHalfWidth, retraits depuis le nu
// de l'abaque ; registre classical_orders_implementation.md section 10.6) :
//   - entasis : f = entasisSagitta, zf = entasisHeight lu (GD1) ;
//   - cannelures a trois centres, rapport profondeur / corde
//     fluteDepth / (lowerDiameter sin (pi / N)), au diametre inferieur courant
//     pour les deux jeux (section homothetique, README section 4.4, ligne
//     "Cannelures au pied" ; GD2a) ;
//   - rainure portee par le chapiteau du modele, sous le gorgerin :
//     hypotrachelionHeight = gorgerin + rainure, grooveHeight = rainure (GD3c) ;
//     grooveDepth = R (1 - fond des cannelures au sommet) + grooveHeight
//     (GD3d, la profondeur etant manquante au README) ;
//   - annelets : annuletCount gradins egaux de hauteur et de retrait, faces
//     verticales et lits horizontaux, face puis lit (GD3a) ; le listel du haut
//     a pour saillie celle du pied de l'echine, au bit pres ;
//   - echine : pied au retrait du sommet des annelets, jonction et tete aux
//     retraits et profondeurs du profil, levre de rayon echinusLipRadius (GD4a) ;
//   - abaque : demi-cote A, soit une saillie A - R (GD5a) ;
//   - resolution : valeurs par defaut des reglages.
GreekDoricColumnSettings MonumentColumnSettings (const MonumentData &data, bool corner);

// Entablement du README (sections 6.1 a 6.4 ; registre EN1, EN2, EN5, EN8, EN9,
// EN11, EN15, EN17 a EN24, classical_orders_implementation.md section 10.10).
// Memes regles d'erreur que les autres conversions (MonumentLength,
// MonumentFeetAndInches ; regulaGuttaCount et mutuleGuttaPerRow comme des
// comptes), plus les refus nommes :
//   - 'entablature.innerFace' si elle n'est pas manquante (EN1 ne vaut que
//     manquante) ;
//   - 'entablature.triglyphCapitalBandHeight' si bande + listel s'ecarte de la
//     hauteur du chapiteau de plus de 1e-9 in ;
//   - 'entablature.triglyphDivisions' si la suite est vide, de listes de tailles
//     differentes, sans demi-canal a chaque bout, avec un demi-canal a
//     l'interieur, sans canal, ou si les canaux, ou les deux demi-canaux, n'ont
//     pas tous la meme largeur ;
//   - 'entablature.mutulePlateProjection' ou 'entablature.mutuleDepth' si elle
//     manque ou n'est pas en pouces (le dos des mutules se calcule en pouces) ;
//   - 'entablature.soffitSlope' si elle manque, n'est pas en degres, ou n'est pas
//     dans ]0, 90[ degres.
// Onze bandes, de bas en haut, valeurs en pouces telles qu'ecrites (EN9) : face
// lisse de l'architrave, inclinee de la saillie du pied a celle du haut ; zone
// des regulae et des gouttes, verticale au nu du haut de l'architrave ; tenia ;
// frise (friezeBand = 3), au nu des metopes, hauteur de la frise moins le
// chapiteau du triglyphe ; bande et listel du chapiteau du triglyphe, continus
// (EN8) ; plaque des mutules, verticale au dos des mutules sous la sous-face
// inclinee (EN21, EN22, D3-67) : avec P = mutulePlateProjection et L = mutuleDepth
// en pouces, theta = soffitSlope x pi / 180, dos (P - L cos theta) x kMonumentInch ;
// bec sous le larmier ; larmier ; filet ; listel de couronnement. innerFace = saillie du
// pied de la face de l'architrave (EN1). Triglyphe : largeur en pieds et pouces,
// face, divisions lues, canal pour tout mot autre que "bande" ; profondeurs par
// l'angle droit (EN11) : canal, la moitie de sa largeur ; demi-canal, sa
// largeur. La profondeur mesuree (channelDepth) n'entre pas. Angle de la frise
// (EN15) : CornerTriglyphEqualMetopes, triglyphe d'angle et deux metopes egales
// dans la travee d'angle (README section 6.3, ligne "Metopes d'angle", premiere
// solution) ; le peristyle garde
// la contraction relevee (MonumentPeristyleSettings), la contraction calculee
// n'etant que rapportee (ReportDoricCorners). Regulae (EN17 a EN20) : hauteur
// regulaHeight, face regulaProjection, regulaGuttaCount gouttes de diametre au
// pied regulaGuttaDiameter (EN19), vide regulaGuttaGap, generatrice avant de
// regulaGuttaFootProjection au pied a regulaGuttaTopProjection en tete, 8 cotes
// (format) ; hauteur des gouttes : zone des regulae moins la regula. Mutules
// (EN21, EN22) : bande 6, largeur en pieds et pouces, face mutulePlateProjection
// (pivot de la sous-face), montee (L sin theta) x kMonumentInch. Gouttes des
// mutules (EN23, EN24, D3-76 a D3-78) : diametres de tete et vides entre tetes lus,
// du devant vers le dos ; mutuleGuttaPerRow par rang ; axes extremes a
// mutuleGuttaEdgeAxis ; hauteur mutuleGuttaHeight ; conicity, radiale, la moitie de
// l'evasement en diametre mutuleGuttaFootFlare ; 8 cotes (format). Reglages non
// valides ici (LayoutTemple et ValidateGreekEntablatureSettings
// les valident).
GreekEntablatureSettings MonumentEntablatureSettings (const MonumentData &data);

// Fronton du README (section 9 ; registre PE1, PE2' a PE4', PE5,
// classical_orders_implementation.md section 10.11) :
//   - apexRise = (apexRiseFt x 12 + apexRiseIn) x kMonumentInch
//     (MonumentFeetAndInches), jusqu'au haut de la corniche rampante ;
//   - rakingRun = MonumentRatio (rakingRun), la pente 1 : r ;
//   - tympanumFace = MonumentLength (tympanumFace) ;
//   - rakingSection : un sommet (u, z) par sommet (u[k], h[k]) du polygone, dans
//     son ordre, chaque coordonnee convertie par un seul produit
//     (MonumentMetres) ; u depuis le nu du tympan, z la hauteur normale a la pente.
// La suite des hauteurs (rakingHeights) n'entre pas : elle n'est que confrontee
// au polygone par les tests. X_e n'est pas un reglage : la bibliotheque le
// derive de l'anneau, et n'en fait qu'une borne. Memes regles d'erreur que les autres conversions, plus les
// refus nommes 'pediment.rakingSection' si le polygone est manquant, vide, ou de
// listes de tailles differentes, et 'pediment.sima' si elle n'est pas manquante
// (PE4 ne vaut que manquante). Reglages non valides ici (LayoutTemple et
// ValidateGreekPedimentSettings les valident).
GreekPedimentSettings MonumentPedimentSettings (const MonumentData &data);

// Temple du README : krepis, peristyle, colonne courante, colonne d'angle,
// entablement (MonumentEntablatureSettings) et fronton
// (MonumentPedimentSettings).
TempleSettings MonumentTempleSettings (const MonumentData &data);

// ============================================================================
//  Mode partiel : seules les parties completement renseignees sont construites
// ============================================================================
//
// Registre MO-P1 a MO-P6 (classical_orders_implementation.md section 10.13).
// Parties, dans l'ordre des dependances : krepis <- peristyle (axes) <- colonnes
// <- entablement <- fronton (MO-P3). Une partie est complete quand son bloc est
// present et que toutes les cles que sa conversion consomme sont renseignees
// (MO-P1) ; une cle dont la conversion exige qu'elle soit manquante
// (columns.grooveDepth, entablature.innerFace, pediment.sima) n'est pas
// consommee. Une partie n'est construite que si elle est complete et que toutes
// ses dependances le sont. Le peristyle n'a pas de geometrie propre (MO-P4) :
// sans colonnes, il n'est que valide par LayoutPeristyle (MO-P5).

enum class MonumentPart { Krepis, Peristyle, Columns, Entablature, Pediment };

// Built : construite. CompleteNotBuilt : complete, dependances completes, sans
// geometrie (peristyle sans colonnes, valide par LayoutPeristyle). Incomplete :
// bloc present, cles consommees manquantes (missingKeys). Absent : bloc absent.
// Blocked : complete, une dependance ne l'est pas (blockedBy). Un bloc absent
// est Absent et un bloc incomplet Incomplete, quelles que soient ses dependances.
enum class MonumentPartState { Built, CompleteNotBuilt, Incomplete, Absent, Blocked };

struct MonumentPartReport
{
	MonumentPart              part = MonumentPart::Krepis;
	MonumentPartState         state = MonumentPartState::Absent;
	std::vector<std::string>  missingKeys;   // Incomplete : cles consommees manquantes, ordre du document
	std::optional<MonumentPart> blockedBy;   // Blocked : premiere dependance non complete, depuis la krepis
};

struct MonumentBuild
{
	std::unique_ptr<Mesh>             mesh;     // jamais nul ; vide si aucune partie n'est construite
	std::vector<TempleMeshPart>       parts;    // plages du maillage, comme BuildTempleMesh
	std::array<MonumentPartReport, 5> report;   // indice : static_cast<int> (MonumentPart)
};

// Cles consommees par la conversion de `part` et manquantes dans `data`, chemin
// complet ("columns.height"), dans l'ordre du document : toutes les cles
// consommees si le bloc est absent. Une suite (entablature.triglyphDivisions) ou
// un polygone (pediment.rakingSection) est manquant si son statut l'est ou s'il
// est vide. Cles consommees : krepis 10 (toutes sauf cornerMargin), peristyle 6,
// colonnes 21 (toutes sauf grooveDepth), entablement 48 (toutes sauf
// architraveHeightFt, architraveHeightIn, corniceHeightFt, corniceHeightIn,
// innerFace, metopeWidthFt, metopeWidthIn et channelDepth), fronton 5
// (apexRiseFt, apexRiseIn, rakingRun, tympanumFace, rakingSection).
std::vector<std::string> MissingMonumentKeys (const MonumentData &data, MonumentPart part);

// Maillage des parties construites et rapport des cinq parties. Aiguillage :
//   - colonnes construites : TempleSettings des parties construites
//     (entablement et fronton seulement s'ils le sont), puis BuildTempleMesh ;
//     les cinq parties construites, exactement
//     BuildTempleMesh (MonumentTempleSettings (data)) ;
//   - sinon krepis construite : LayoutPeristyle si le peristyle est complete
//     et non bloque, puis BuildKrepisMesh, une partie KrepisStep par degre
//     (adresse KrepisStep, index du degre, 16 sommets et 12 faces chacune) ;
//   - sinon maillage vide.
// Les exceptions des conversions et des validations (LayoutPeristyle,
// LayoutKrepis, LayoutTemple) remontent telles quelles. Le peristyle construit
// avec les colonnes est Built. TE3 et TE4 (pieds sur le stylobate, colonnes
// disjointes) ne sont controles qu'avec les colonnes.
MonumentBuild BuildMonumentMesh (const MonumentData &data);

} // namespace classical
} // namespace cgarchi
