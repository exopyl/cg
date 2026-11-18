#pragma once

#include <array>
#include <cstddef>
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
//  Document du monument "monument/v14" : valeurs du README du monument, avec
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
// src/cgarchi/schemas/monument-v14.schema.
//
// Statuts : les mots du README, liste fermee. Une valeur "manquant" n'a pas de
// cle "value" ; toute autre en a une. "lu" et "mesure" citent au moins une
// source ; "deduit" et "convention" peuvent n'en citer aucune ; "manquant" n'en
// cite aucune. Toute valeur porte son lieu dans le README ("readme").
//
// Unites (D3-37, amende par D3-52, D3-75, le registre MO, unite "ratio", et le
// registre section 10.14, unites de longueur) : chaque valeur porte "unit",
// liste fermee "m", "in", "mm", "1", "ft", "ft+in", "°", "ratio". Longueurs :
// "m", "in", "mm" et "ft" (pieds decimaux), partout ou une longueur est lue ;
// "ft+in" pour une cote en pieds et pouces, valeur [pieds, pouces], pieds
// entiers >= 1 et pouces finis >= 0, sur une valeur de longueur seulement (ni
// suite, ni polygone, ni controle). "1" pour un compte et lui seul ; "°"
// (degre) pour un angle et lui seul (entablature.soffitSlope, et le controle
// pedimentControls.rakingSlope) ; "ratio" pour un rapport "1 : r" du README et
// lui seul, valeur r reelle > 0 (pediment.rakingRun). Une valeur manquante
// porte l'unite attendue. Le document garde chaque valeur dans l'unite de sa
// source ; la conversion en metres, reference interne, est ici
// (MonumentLength), jamais dans le script : 1 in = 0,0254 m, 1 ft = 12 in =
// 0,3048 m (README section 1.1).
//
// Formes de valeur (plan 3 section 20.5, choix de format) : une cote en pieds et
// pouces est une seule valeur ; la division du triglyphe est une suite (values,
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

constexpr const char *kMonumentSchema = "monument/v14";

// Pouce anglais en metres (README section 1.1).
constexpr double kMonumentInch = 0.0254;

// Pied anglais en metres, 12 pouces (README section 1.1).
constexpr double kMonumentFoot = 0.3048;

// Statuts du README, dans l'ordre : "lu", "déduit", "mesuré", "convention",
// "manquant".
enum class MonumentStatus { Read, Deduced, Measured, Convention, Missing };

// Unites du document, dans l'ordre : "m", "in", "mm", "1", "ft", "ft+in", "°",
// "ratio".
enum class MonumentUnit { Metre, Inch, Millimetre, One, Foot, FootInch, Degree, Ratio };

struct MonumentValue
{
	std::optional<double>    value;     // vide si et seulement si status == Missing ; "ft+in" : les pieds
	double                   inches = 0.0;   // "ft+in" : les pouces de la cote ; 0 pour toute autre unite
	MonumentUnit             unit = MonumentUnit::Metre;   // presente aussi pour manquant : unite attendue
	MonumentStatus           status = MonumentStatus::Missing;
	std::vector<std::string> sources;   // identifiants de MonumentData::sources
	std::string              readme;    // lieu de la valeur dans le README : "§3.2, l. 86"
};

// Longueur `value` exprimee en `unit`, en metres, par un seul produit (ou un
// seul quotient pour "mm") : "in" vaut kMonumentInch, "ft" kMonumentFoot. Leve
// std::invalid_argument, prefixe "Monument : " et cle entre apostrophes, si
// unit est "1" (sans dimension), "ft+in" (une cote a deux nombres : passer par
// MonumentLength), "°" (un angle), "ratio" (un rapport) ou hors de
// l'enumeration.
double MonumentMetres (double value, MonumentUnit unit, std::string_view key);

// Valeur d'une longueur en metres, entree unique des longueurs : "m", "in",
// "mm" et "ft" par MonumentMetres ; "ft+in" par (pieds x 12 + pouces) x
// kMonumentInch, ou le produit par 12 est exact et le produit par le pouce vient
// en dernier, si bien que le resultat ne depend pas d'une contraction FMA (plan
// 3, MS-40). Leve std::invalid_argument, prefixe "Monument : " et cle entre
// apostrophes, si la valeur est manquante, sans dimension, un angle, un
// rapport, ou une cote "ft+in" dont les pieds ne sont pas un entier >= 1 ou
// dont les pouces ne sont pas finis et >= 0.
double MonumentLength (const MonumentValue &v, std::string_view key);

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

// README section 7.8 : cella et porches, longueurs de segments sans signe, dans le
// repere du README (est, ouest, sud). Les parties Cella et Porches (MO-P3) se
// partagent le bloc ; MissingMonumentKeys dit lesquelles chacune consomme.
struct MonumentCella
{
	MonumentValue toichobateRise, pteronEast, pteronWest, toichobateOverhang;
	MonumentValue longWallThickness, longWallSpan, wallTop;
	MonumentValue eastCrossWallPosition, eastCrossWallThickness, westCrossWallPosition, westCrossWallThickness;
	MonumentValue doorWidth, lintelHeight, doorJambOffset;   // jambage sud depuis le nu interieur du mur sud
	MonumentValue thresholdLength, thresholdProjection, thresholdDepth, naosFloorHeight;
	MonumentValue eastAntaRetreat, westAntaRetreat, antaInnerProjection, antaOuterProjection, antaHeadLength;
	MonumentValue porchColumnDiameter, porchColumnHeight, porchColumnOffset, pronaosColumnAxis, opisthodomosColumnAxis;
	MonumentValue porchArchitraveHeight, porchFriezeHeight, porchEntablatureDepth, porchEntablatureFront;
};

// Controles de la cella (bloc cellaControls, regle R13 du script), sans statut, en
// metres : coordonnees signees du repere du README, cotes z depuis le dessus du
// stylobate, grandeurs des notations « ± » (InnerY, OuterY), cotes de controle.
// 0 si le bloc est absent.
struct MonumentCellaControls
{
	double toichobateBottomZ = 0.0, toichobateEastX = 0.0, toichobateWestX = 0.0, toichobateWidth = 0.0;
	double longWallInnerY = 0.0, longWallOuterY = 0.0, longWallLength = 0.0;
	double eastCrossWallEastX = 0.0, eastCrossWallWestX = 0.0, westCrossWallWestX = 0.0, westCrossWallEastX = 0.0;
	double naosLength = 0.0, doorLintelZ = 0.0, eastAntaX = 0.0, westAntaX = 0.0, antaInnerY = 0.0, antaFaceWidth = 0.0;
	double porchArchitraveBottomZ = 0.0, porchArchitraveTopZ = 0.0, porchFriezeTopZ = 0.0;
};

// README section 7.9 : interieur du naos, longueurs de segments sans signe depuis
// les nus du naos de la section 7.8, cotes z depuis le dessus du stylobate, dans
// le repere du README (est, ouest). Colonnade : entraxe, axe de la premiere
// colonne des flancs depuis le nu est, axe ouest depuis le nu ouest (controle de
// fermeture), axes des flancs depuis les nus des murs longs ; niveau de la nef
// (pied des colonnes). Colonnes inferieures et superieures : hauteur, chapiteau
// compris, diametres inferieur et superieur, hauteur du chapiteau, cote de
// l'abaque ; cannelures (compte, aux deux etages). Epistyles : hauteur et
// sous-face. Assise XVII interieure : hauteur et epaisseur. Corniche
// interieure : hauteur et saillie. Poche du comble : plafond, debord sur les nus.
struct MonumentInterior
{
	MonumentValue spacing, eastAxisOffset, westAxisOffset, wallAxisOffset, naveLevel;
	MonumentValue lowerColumnHeight, lowerColumnLowerDiameter, lowerColumnUpperDiameter, lowerCapitalHeight, lowerAbacus;
	MonumentValue lowerEpistyleHeight, lowerEpistyleSoffit;
	MonumentValue upperColumnHeight, upperColumnLowerDiameter, upperColumnUpperDiameter, upperCapitalHeight, upperAbacus;
	MonumentValue fluteCount;
	MonumentValue upperEpistyleHeight, upperEpistyleSoffit;
	MonumentValue courseHeight, courseThickness, corniceHeight, corniceProjection;
	MonumentValue pocketCeiling, pocketMargin;
};

// Controles de l'interieur (bloc interiorControls, regle R13 du script), sans
// statut, en metres : coordonnees signees du repere du README (nu est, axes des
// flancs de l'est vers l'ouest, axe et nu ouest, emprise de la poche),
// grandeurs des notations « ± » (axes lateraux, demi-largeur de la poche),
// longueur du naos et cotes z. 0 si le bloc est absent.
struct MonumentInteriorControls
{
	double eastWallX = 0.0;
	std::array<double, 5> flankAxesX {};
	double westAxisX = 0.0, westWallX = 0.0, naosLength = 0.0, lateralAxisY = 0.0;
	double lowerAbacusTopZ = 0.0, lowerEpistyleTopZ = 0.0, upperAbacusTopZ = 0.0, upperEpistyleTopZ = 0.0;
	double courseTopZ = 0.0, corniceTopZ = 0.0, pocketBottomZ = 0.0;
	double pocketEastX = 0.0, pocketWestX = 0.0, pocketHalfWidthY = 0.0;
};

// README section 7.10 : assise XVII exterieure (epicranitis et couronnement de
// la frise des porches). Hauteur sur le dessus des murs ; face interieure depuis
// le nu interieur des murs du naos ; hauteurs du listel et du filet, saillies du
// listel et de la bande a meandre sur le nu, en pouces de [S].
struct MonumentEpicranitis
{
	MonumentValue height, innerOffset, listelHeight, filletHeight, listelProjection, bandProjection;
};

// Controles du couronnement (bloc epicranitisControls, regle R13 du script),
// sans statut, en metres : dessus commun, dessous et hauteur du couronnement de
// la frise des porches, cotes z depuis le dessus du stylobate ; grandeurs des
// notations « ± » (face interieure de l'epicranitis, nu de la bande a meandre
// des murs longs) ; nus des bandes des couronnements est et ouest, coordonnees
// signees du repere du README. 0 si le bloc est absent.
struct MonumentEpicranitisControls
{
	double topZ = 0.0, porchCrownBottomZ = 0.0, porchCrownHeight = 0.0;
	double innerFaceY = 0.0, wallBandY = 0.0, eastBandX = 0.0, westBandX = 0.0;
};

// README section 7.10 : prolongement est de l'entablement des porches, longueur
// par cote, des faces exterieures des antes a la face interieure de
// l'entablement du peristyle.
struct MonumentPorchExtension
{
	MonumentValue eastSpan;
};

// Controles du prolongement est (bloc porchExtensionControls, regle R13), sans
// statut, en metres : grandeur de la face interieure de l'entablement du
// peristyle (« ± »), dessous de l'architrave prolongee. 0 si le bloc est absent.
struct MonumentPorchExtensionControls
{
	double ringInnerY = 0.0, architraveFootZ = 0.0;
};

// README section 7.11 : plafond du pteron est et corniche des fronts des porches.
// Poutres (nombre et rangs de caissons : comptes) ; bout ouest depuis le nu de la
// frise des porches ; massif de bout depuis la face interieure de l'entablement
// du peristyle ; saillie de la corniche, ouverture et cadre des caissons en
// pouces de [S].
struct MonumentEastCeiling
{
	MonumentValue beamCount, beamSpacing, beamWidth, beamHeight, beamWestBearing, endMassifWidth, corniceHeight,
	              corniceProjection, bandHeight, corniceDepth, cofferPitch, cofferOpening, cofferFrame, cofferDepth,
	              cofferRows;
};

// Controles du plafond est (bloc eastCeilingControls, regle R13), sans statut, en
// metres : grandeurs des notations « ± », cotes z depuis le dessus du stylobate,
// coordonnees signees du repere du README, longueurs. 0 si le bloc est absent.
struct MonumentEastCeilingControls
{
	double beamOuterAxisY = 0.0, beamBottomZ = 0.0, beamTopZ = 0.0, beamWestEndX = 0.0, beamEastEndX = 0.0;
	double beamLength = 0.0, clearBay = 0.0, massifFaceY = 0.0, edgeBay = 0.0, corniceTopZ = 0.0, bandTopZ = 0.0;
	double slabThickness = 0.0, cofferFloor = 0.0, cofferRun = 0.0, cofferMargin = 0.0;
};

// README section 7.11 : plafonds des pterons nord et sud. Poutres transversales
// (pas, premier axe depuis l'axe transversal, nombres a l'est et a l'ouest :
// comptes), section, bout cote peristyle depuis la face interieure de
// l'entablement du flanc, rangs de caissons.
struct MonumentFlankCeilings
{
	MonumentValue beamSpacing, firstBeamOffset, eastBeamCount, westBeamCount, beamWidth, beamHeight, peristyleGap,
	              cofferRows;
};

// Controles des plafonds des flancs (bloc flankCeilingsControls, regle R13), sans
// statut, en metres. 0 si le bloc est absent.
struct MonumentFlankCeilingsControls
{
	double lastEastAxisX = 0.0, lastWestAxisX = 0.0, wallEndY = 0.0, peristyleEndY = 0.0, beamLength = 0.0;
	double cofferRun = 0.0, cofferMargin = 0.0;
};

// Blocs presents au document. Un bloc absent est une partie non documentee : ses
// valeurs restent celles par defaut, manquantes.
struct MonumentBlocks
{
	bool krepis = false, peristyle = false, columns = false, entablature = false, pediment = false, cella = false,
	     interior = false, epicranitis = false, porchExtension = false, eastCeiling = false, flankCeilings = false;
	bool controls = false, krepisControls = false, columnControls = false, entablatureControls = false,
	     pedimentControls = false, cellaControls = false, interiorControls = false, epicranitisControls = false,
	     porchExtensionControls = false, eastCeilingControls = false, flankCeilingsControls = false;
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
	MonumentValue architraveHeight, taeniaHeight, regulaHeight, guttaeHeight, architraveFaceHeight;
	MonumentValue friezeHeight, triglyphCapitalHeight;
	MonumentValue corniceHeight, mutulePlateHeight, dripHeight, coronaHeight, filletHeight, crownHeight;
	MonumentValue architraveTopProjection, architraveFootProjection, taeniaProjection, triglyphFace;
	MonumentValue triglyphCapitalBandProjection, triglyphCapitalListelProjection;
	MonumentValue triglyphCapitalListelHeight, triglyphCapitalBandHeight;   // partage, convention de la source
	MonumentValue metopeFace;                                               // nu des metopes
	MonumentValue mutulePlateProjection, dripProjection, coronaProjection, filletProjection, crownProjection;
	MonumentValue innerFace;                                                // face interieure du peristyle
	MonumentValue triglyphWidth;
	MonumentValue metopeWidth;                                              // metope lue : controle
	MonumentDivisions triglyphDivisions;
	MonumentValue channelDepth;                                             // profondeur mesuree : controle de EN11
	MonumentValue regulaProjection, regulaGuttaCount, regulaGuttaDiameter, regulaGuttaGap;
	MonumentValue regulaGuttaTopProjection, regulaGuttaFootProjection;
	MonumentValue mutuleWidth, mutuleDepth;
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
	MonumentValue         apexRise, rakingRun, rakingProjection, tympanumFace;
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
	MonumentCella                      cella;
	MonumentCellaControls              cellaControls;
	MonumentInterior                   interior;
	MonumentInteriorControls           interiorControls;
	MonumentEpicranitis                epicranitis;
	MonumentEpicranitisControls        epicranitisControls;
	MonumentPorchExtension             porchExtension;
	MonumentPorchExtensionControls     porchExtensionControls;
	MonumentEastCeiling                eastCeiling;
	MonumentEastCeilingControls        eastCeilingControls;
	MonumentFlankCeilings              flankCeilings;
	MonumentFlankCeilingsControls      flankCeilingsControls;
	MonumentBlocks                     present;
};

// Lecture stricte d'un document "monument/v14" (json_reader::StrictReader). Leve
// std::runtime_error si le document est refuse ; le message porte chaque refus,
// une ligne chacun, "monument/v14 <chemin> : <motif>". Blocs facultatifs (registre
// MO-P2) : krepis, peristyle, columns, entablature, pediment, cella, interior,
// epicranitis, porchExtension, eastCeiling et flankCeilings ; un bloc present
// porte toutes ses cles. Refus :
// texte illisible ; cle en double ; schema autre que "monument/v14" ; cle
// inconnue, a tout niveau ; cle absente d'un
// bloc present (controls.frontSum et controls.flankSum exceptees) ; peristyle sans
// controls, controls sans peristyle, krepisControls sans krepis, columnControls
// sans columns, entablatureControls sans entablature, pedimentControls sans
// pediment, cellaControls sans cella, interior sans cella, interiorControls sans
// interior, epicranitis sans cella, epicranitisControls sans epicranitis,
// porchExtension sans epicranitis, porchExtensionControls sans porchExtension,
// eastCeiling sans porchExtension, eastCeilingControls sans eastCeiling,
// flankCeilings sans eastCeiling, flankCeilingsControls sans flankCeilings
// ("requires block \"<partie>\"") ; controle de cellaControls,
// d'interiorControls, d'epicranitisControls, de porchExtensionControls,
// d'eastCeilingControls ou de flankCeilingsControls non fini ; null ; type faux ; status hors des cinq ; unit hors des huit,
// "1", "°" ou "ratio" sur une longueur, "ft+in" ailleurs que sur une valeur de
// longueur (suite, polygone, controle, profil), autre que "1" sur un compte,
// autre que "°" sur un angle, autre que "ratio" sur un rapport ; value absente
// pour un statut autre que manquant, presente pour manquant ; longueur, angle ou
// rapport non fini ou <= 0 ; compte non entier ou < 1 ; cote "ft+in" qui n'est
// pas une liste de deux nombres, pieds non entiers ou < 1, pouces < 0 ; source inconnue de la table, ou
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
// (data/temple_hephaistos.json), "temple-ares" (data/temple_ares.json) ou
// "temple-parthenon" (data/temple_parthenon.json), lu une fois. Leve std::invalid_argument, prefixe "Monument : " et nom entre
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
// Memes regles d'erreur que les autres conversions (MonumentLength ;
// regulaGuttaCount et mutuleGuttaPerRow comme des comptes), plus les refus
// nommes :
//   - 'entablature.triglyphCapitalBandHeight' si bande + listel s'ecarte de la
//     hauteur du chapiteau de plus de 1e-9 in, longueur de reference ;
//   - 'entablature.triglyphDivisions' si la suite est vide, de listes de tailles
//     differentes, sans demi-canal a chaque bout, avec un demi-canal a
//     l'interieur, sans canal, ou si les canaux, ou les deux demi-canaux, n'ont
//     pas tous la meme largeur ;
//   - 'entablature.soffitSlope' si elle manque, n'est pas en degres, ou n'est pas
//     dans ]0, 90[ degres.
// Onze bandes, de bas en haut, valeurs telles qu'ecrites (EN9) : face
// lisse de l'architrave, inclinee de la saillie du pied a celle du haut ; zone
// des regulae et des gouttes, verticale au nu du haut de l'architrave ; tenia ;
// frise (friezeBand = 3), au nu des metopes, hauteur de la frise moins le
// chapiteau du triglyphe ; bande et listel du chapiteau du triglyphe, continus
// (EN8) ; plaque des mutules, verticale au dos des mutules sous la sous-face
// inclinee (EN21, EN22, D3-67) : avec P = mutulePlateProjection et L = mutuleDepth,
// theta = soffitSlope x pi / 180, dos P - L cos theta, calcule dans l'unite
// commune de P et L puis converti en metres en dernier (un seul produit par le
// pouce pour deux valeurs en pouces), ou depuis P et L en metres si leurs
// unites different (registre section 10.14) ;
// bec sous le larmier ; larmier ; filet ; listel de couronnement. innerFace =
// entablature.innerFace si elle est donnee (MonumentLength), saillie du pied de la
// face de l'architrave sinon (EN1 en repli, CE-32). Triglyphe : largeur en pieds et pouces,
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
// (pivot de la sous-face), montee L sin theta, meme regle d'unite que le dos. Gouttes des
// mutules (EN23, EN24, D3-76 a D3-78) : diametres de tete et vides entre tetes lus,
// du devant vers le dos ; mutuleGuttaPerRow par rang ; axes extremes a
// mutuleGuttaEdgeAxis ; hauteur mutuleGuttaHeight ; conicity, radiale, la moitie de
// l'evasement en diametre mutuleGuttaFootFlare ; 8 cotes (format). Reglages non
// valides ici (LayoutTemple et ValidateGreekEntablatureSettings
// les valident).
GreekEntablatureSettings MonumentEntablatureSettings (const MonumentData &data);

// Fronton du README (section 9 ; registre PE1, PE2' a PE4', PE5,
// classical_orders_implementation.md section 10.11) :
//   - apexRise = MonumentLength (apexRise), (pieds x 12 + pouces) x
//     kMonumentInch pour une cote "ft+in", jusqu'au haut de la corniche
//     rampante ;
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
// (MonumentPedimentSettings). Sans cella : le temple exterieur, que les
// raffinements acceptent ; la cella s'y ajoute par MonumentCellaSettings.
TempleSettings MonumentTempleSettings (const MonumentData &data);

// Cella du README (section 7.8 ; registre CE-1 a CE-9,
// classical_orders_implementation.md section 10.16), sans colonnes de porche :
// chaque longueur par MonumentLength, un seul produit, dans le repere du temple
// (MO2) : est = Front, ouest = Back ; pteronEast -> frontPteron, pteronWest ->
// backPteron, eastCrossWall* -> frontCrossWall*, westCrossWall* ->
// backCrossWall*, eastAntaRetreat -> frontAntaRetreat, westAntaRetreat ->
// backAntaRetreat, pronaosColumnAxis -> frontPorchAxis, opisthodomosColumnAxis ->
// backPorchAxis ; doorJambOffset, depuis le nu interieur du mur sud (y < 0 au
// README, X < 0 au temple), est celui du mur Left. Les autres cles gardent leur
// nom. Memes regles d'erreur que les autres conversions ; reglages non valides
// ici (LayoutGreekCella et LayoutTemple les valident).
CellaSettings MonumentCellaSettings (const MonumentData &data);

// Colonnes des porches (registre CE-6) : la colonne courante du README
// (MonumentColumnSettings (data, false)), diametre inferieur
// cella.porchColumnDiameter et hauteur cella.porchColumnHeight ; le chapiteau et
// le diametre superieur sont ceux du peristyle (README section 7.3, section 11
// point 4), le fut seul est raccourci ; cannelures au meme rapport profondeur /
// corde, entasis mise a l'echelle du fut : fleche et cote du maximum
// multipliees par le rapport des hauteurs de fut, porche sur peristyle. Ecart
// des axes : cella.porchColumnOffset. Memes regles d'erreur que
// MonumentColumnSettings.
GreekPorchColumnsSettings MonumentPorchSettings (const MonumentData &data);

// Interieur du README (section 7.9 ; registre CE-15 et suivantes,
// classical_orders_implementation.md section 10.16) : longueurs par
// MonumentLength, dans le repere du temple (MO2) : eastAxisOffset ->
// frontAxisOffset, westAxisOffset -> backAxisOffset ; les autres cles gardent
// leur nom. Colonnes de chaque etage (registre CE-16 a CE-18), depuis la
// colonne courante du README (MonumentColumnSettings (data, false)) :
//   - diametres inferieur et superieur et hauteur lus ; fluteCount cannelures,
//     au rapport profondeur / corde du peristyle (section homothetique) ;
//   - entasis : fleche multipliee par le rapport des diametres inferieurs,
//     cote du maximum par le rapport des hauteurs de fut (meme position
//     relative sur le fut) ;
//   - chapiteau : hauteurs de ses membres (rainure, gorgerin, annelets, droite
//     et levre de l'echine, abaque) multipliees par kh = hauteur lue / hauteur
//     du chapiteau du peristyle ; saillies depuis le sommet du fut (GD5)
//     multipliees par kp = (abaque lu / 2 - diametre superieur lu / 2) /
//     saillie de l'abaque du peristyle, si bien que l'abaque est celui lu et
//     contient l'echine reduite ; rayon de la levre multiplie par le rapport
//     des demi-cordes jonction-tete (arc semblable) ; rainure par GD3d sur le
//     nouveau fut.
// Memes regles d'erreur que les autres conversions (fluteCount comme un
// compte) ; reglages non valides ici (LayoutGreekNaosInterior et LayoutTemple
// les valident).
NaosInteriorSettings MonumentInteriorSettings (const MonumentData &data);

// Couronnement du README (section 7.10 ; registre CE-23 et suivantes) : chaque
// longueur par MonumentLength (les pouces de [S] convertis par un seul produit),
// les cles gardent leur nom. Memes regles d'erreur que les autres conversions ;
// reglages non valides ici (LayoutGreekEpicranitis et LayoutTemple les
// valident).
EpicranitisSettings MonumentEpicranitisSettings (const MonumentData &data);

// Prolongement est du README (section 7.10) : eastSpan par MonumentLength.
// Memes regles d'erreur ; reglage non valide ici (LayoutGreekPorchExtension).
PorchExtensionSettings MonumentPorchExtensionSettings (const MonumentData &data);

// Plafond est du README (section 7.11 ; registre CE-32 et suivantes) : chaque
// longueur par MonumentLength (les pouces de [S] convertis par un seul produit),
// beamCount et cofferRows comme des comptes (RequiredCount, comme fluteCount) ;
// corniceHeight, corniceProjection, bandHeight et corniceDepth vont a
// EastCeilingSettings::cornice, cofferPitch, cofferOpening, cofferFrame et
// cofferDepth a EastCeilingSettings::coffers. Memes regles d'erreur que les
// autres conversions ; reglages non valides ici (LayoutGreekEastCeiling et
// LayoutTemple les valident).
EastCeilingSettings MonumentEastCeilingSettings (const MonumentData &data);

// Plafonds des flancs du README (section 7.11) : longueurs par MonumentLength,
// comptes comme fluteCount ; est et ouest du README deviennent front et back du
// temple (MO2) : eastBeamCount -> frontBeamCount, westBeamCount ->
// backBeamCount. Memes regles d'erreur ; reglages non valides ici.
FlankCeilingSettings MonumentFlankCeilingSettings (const MonumentData &data);

// ============================================================================
//  Mode partiel : seules les parties completement renseignees sont construites
// ============================================================================
//
// Registre MO-P1 a MO-P6 (classical_orders_implementation.md section 10.13).
// Dependances (MO-P3, table, chaque partie avec toutes ses dependances, dans
// l'ordre de l'enumeration) : peristyle <- krepis ; colonnes <- krepis,
// peristyle ; entablement <- krepis, peristyle, colonnes ; fronton <- krepis,
// peristyle, colonnes, entablement ; cella <- krepis ; porches <- krepis,
// peristyle, colonnes, cella ; interieur <- krepis, peristyle, colonnes, cella
// (ses colonnes derivent de celle du peristyle) ; epicranitis <- krepis, cella ;
// prolongement est <- krepis, peristyle, colonnes, entablement, cella,
// epicranitis (il pose sur les abaques et bute contre l'anneau) ; plafond est <-
// les memes, plus le prolongement est (il pose sur son couronnement et bute
// contre l'anneau) ; plafonds des flancs <- les memes, plus le plafond est
// (grille des caissons, dessous des dalles). Aucun plafond ne depend du fronton
// ni de l'interieur : sans fronton, pas de comble ni de poche. Pour les cinq
// premieres parties, c'est la chaine
// krepis <- peristyle <- colonnes <- entablement <- fronton. Une partie est
// complete quand son bloc est present et que toutes les cles que sa conversion
// consomme sont renseignees (MO-P1) ; une cle dont la conversion exige qu'elle
// soit manquante (columns.grooveDepth, pediment.sima) n'est pas consommee, ni une
// cle facultative, a repli : entablature.innerFace (EN1). Cella et porches partagent le bloc cella ; l'interieur,
// l'epicranitis, le prolongement est et les plafonds ont chacun le leur
// (interior, epicranitis, porchExtension, eastCeiling, flankCeilings). Une partie n'est
// construite que si elle est complete et que toutes ses dependances le sont. Le
// peristyle n'a pas de geometrie propre (MO-P4) : sans colonnes, il n'est que
// valide par LayoutPeristyle (MO-P5).

enum class MonumentPart
{
	Krepis, Peristyle, Columns, Entablature, Pediment, Cella, Porches, Interior, Epicranitis, PorchExtension,
	EastCeiling, FlankCeilings
};

constexpr std::size_t kMonumentPartCount = 12;

// Dependances de `part` (MO-P3), dans l'ordre de l'enumeration.
std::vector<MonumentPart> MonumentPartDependencies (MonumentPart part);

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
	std::optional<MonumentPart> blockedBy;   // Blocked : premiere dependance non complete, dans l'ordre de la table
};

struct MonumentBuild
{
	std::unique_ptr<Mesh>                              mesh;     // jamais nul ; vide si aucune partie n'est construite
	std::vector<TempleMeshPart>                        parts;    // plages du maillage, comme BuildTempleMesh
	std::array<MonumentPartReport, kMonumentPartCount> report;   // indice : static_cast<int> (MonumentPart)
};

// Cles consommees par la conversion de `part` et manquantes dans `data`, chemin
// complet ("columns.height"), dans l'ordre du document : toutes les cles
// consommees si le bloc est absent. Une suite (entablature.triglyphDivisions) ou
// un polygone (pediment.rakingSection) est manquant si son statut l'est ou s'il
// est vide. Cles consommees : krepis 10 (toutes sauf cornerMargin), peristyle 6,
// colonnes 21 (toutes sauf grooveDepth), entablement 45 (toutes sauf
// architraveHeight, corniceHeight, innerFace, facultative a repli, metopeWidth et
// channelDepth),
// fronton 4 (apexRise, rakingRun, tympanumFace, rakingSection), cella 30 (toutes
// sauf porchColumnDiameter et porchColumnOffset), porches 3 (porchColumnDiameter,
// porchColumnHeight, porchColumnOffset), la hauteur consommee aussi par la cella
// (pied de l'architrave des porches) ; les axes des porches sont lus par la
// cella, dont les porches dependent ; interieur 26 (toutes) ; epicranitis 6
// (toutes) ; prolongement est 1 (eastSpan).
std::vector<std::string> MissingMonumentKeys (const MonumentData &data, MonumentPart part);

// Document prive de la partie `part` : son bloc et ses controles retires
// (drapeau faux, valeurs par defaut), puis, de proche en proche, tout bloc qui
// exigeait un bloc retire (table MO-P2 du lecteur : controles, interieur et
// epicranitis sur la cella, prolongement est sur l'epicranitis). Cella et
// Porches retirent le meme bloc, cella. Le resultat respecte MO-P2.
MonumentData MonumentWithoutPart (const MonumentData &data, MonumentPart part);

// Temple exterieur du document : MonumentWithoutPart (data, Cella), qui retire la
// cella, les porches, l'interieur, l'epicranitis, le prolongement est et les
// plafonds, avec leurs controles ; ces parties sont alors Absent au rapport.
MonumentData MonumentExterior (const MonumentData &data);

// Rapport des douze parties, celui que BuildMonumentMesh rend, sans maillage ni
// validation : Absent, Incomplete (missingKeys), Blocked (blockedBy, premiere
// dependance non complete dans l'ordre de la table), Built pour une partie
// complete dont toutes les dependances le sont, sauf le peristyle :
// CompleteNotBuilt quand les colonnes ne sont pas construites. Leve d'abord
// std::invalid_argument "Monument : '<bloc>' demande le bloc '<bloc>'" si les
// drapeaux `present` contredisent les dependances de blocs du lecteur (MO-P2),
// par la meme table : un MonumentData modifie a la main ne peut pas presenter un
// bloc sans celui qu'il exige.
std::array<MonumentPartReport, kMonumentPartCount> ReportMonumentParts (const MonumentData &data);

// Maillage des parties construites et rapport des douze parties. Aiguillage :
//   - colonnes construites : TempleSettings des parties construites
//     (entablement, fronton, cella, interieur, epicranitis, prolongement est,
//     plafond est et plafonds des flancs seulement s'ils le sont ; colonnes des
//     porches avec la cella si les porches le sont), puis BuildTempleMesh ; les
//     douze parties construites,
//     exactement BuildTempleMesh de MonumentTempleSettings (data) augmente de
//     MonumentCellaSettings (data), de MonumentPorchSettings (data), de
//     MonumentInteriorSettings (data), de MonumentEpicranitisSettings (data), de
//     MonumentPorchExtensionSettings (data), de MonumentEastCeilingSettings
//     (data) et de MonumentFlankCeilingSettings (data) ; sans cella au document,
//     exactement BuildTempleMesh (MonumentTempleSettings (data)) ;
//   - sinon krepis construite : LayoutPeristyle si le peristyle est complete
//     et non bloque, puis BuildKrepisMesh, une partie KrepisStep par degre
//     (adresse KrepisStep, index du degre, 16 sommets et 12 faces chacune),
//     puis, si la cella est construite, ses pieces (LayoutGreekCella,
//     BuildGreekCellaPieces ; adresse TempleAddressOf), sans colonnes de porche,
//     puis, si l'epicranitis est construite, ses pieces (LayoutGreekEpicranitis,
//     BuildGreekEpicranitisPieces), sans controle contre l'anneau ;
//   - sinon maillage vide.
// Le refus MO-P2 de ReportMonumentParts, puis les exceptions des conversions et des validations (LayoutPeristyle,
// LayoutKrepis, LayoutTemple) remontent telles quelles. Le peristyle construit
// avec les colonnes est Built. TE3 et TE4 (pieds sur le stylobate, colonnes
// disjointes) ne sont controles qu'avec les colonnes.
MonumentBuild BuildMonumentMesh (const MonumentData &data);

} // namespace classical
} // namespace cgarchi
