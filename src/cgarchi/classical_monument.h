#pragma once

#include <cstdint>
#include <map>
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
//  Document du monument "monument/v6" : valeurs du README du monument, avec
//  leur unite, leur statut, leurs sources et leur lieu ; conversion vers les
//  reglages du temple
// ============================================================================
//
// Le document a la forme de sa source : sections et valeurs du README, chacune
// dans l'unite ou le README l'ecrit (metres, pouces et pieds de Stuart et
// Revett, millimetres), dans le repere du README (x vers l'est, y vers le nord,
// z vers le haut depuis le dessus du stylobate, origine au centre du
// stylobate). Il est ecrit par src/cgarchi/data/temple_hephaistos_from_readme.py,
// jamais a la main, et embarque a la build (classical_monument_data.h). Schema :
// src/cgarchi/schemas/monument-v6.schema.
//
// Statuts : les mots du README, liste fermee. Une valeur "manquant" n'a pas de
// cle "value" ; toute autre en a une. "lu" et "mesure" citent au moins une
// source ; "deduit" et "convention" peuvent n'en citer aucune ; "manquant" n'en
// cite aucune. Toute valeur porte son lieu dans le README ("readme").
//
// Unites (D3-37, amende par D3-52 et D3-75) : chaque valeur porte "unit", liste
// fermee "m", "in", "mm", "1", "ft", "°" ; "1" pour un compte et lui seul ; "ft"
// pour les pieds d'une cote en pieds et pouces et pour eux seuls : cle
// "<nom>Ft", entier >= 1, a cote de la cle "<nom>In" de ses pouces ; "°" (degre)
// pour un angle et lui seul (entablature.soffitSlope). Une valeur manquante
// porte l'unite attendue. La conversion en metres est ici (MonumentLength,
// MonumentFeetAndInches), jamais dans le script : 1 in = 0,0254 m, 1 ft = 12 in
// (README l. 31).
//
// Formes de valeur (plan 3 section 20.5, choix de format) : une cote en pieds et
// pouces occupe deux cles ; la division du triglyphe est une suite (values,
// kinds) lue telle qu'ecrite, mots du README "demi-canal", "bande", "canal".
//
// Conversion de repere (registre MO2, classical_orders_implementation.md
// section 10.9) : la facade de D2-5 (rangee front, le long de X, en Y < 0) est
// l'entree du monument, la facade est du README. Front le long de y, flank le
// long de x ; (X, Y, Z) = (y, -x, z + stylobateTop), quart de tour direct ;
// cotes : est = Front, nord = Right, ouest = Back, sud = Left.

constexpr const char *kMonumentSchema = "monument/v6";

// Pouce anglais en metres (README l. 31).
constexpr double kMonumentInch = 0.0254;

// Pied anglais en metres, 12 pouces (README l. 31).
constexpr double kMonumentFoot = 0.3048;

// Statuts du README, dans l'ordre : "lu", "déduit", "mesuré", "convention",
// "manquant".
enum class MonumentStatus { Read, Deduced, Measured, Convention, Missing };

// Unites du document, dans l'ordre : "m", "in", "mm", "1", "ft", "°".
enum class MonumentUnit { Metre, Inch, Millimetre, One, Foot, Degree };

struct MonumentValue
{
	std::optional<double>    value;     // vide si et seulement si status == Missing
	MonumentUnit             unit = MonumentUnit::Metre;   // presente aussi pour manquant : unite attendue
	MonumentStatus           status = MonumentStatus::Missing;
	std::vector<std::string> sources;   // identifiants de MonumentData::sources
	std::string              readme;    // lieu de la valeur dans le README : "§3.2, l. 74"
};

// Longueur `value` exprimee en `unit`, en metres. Leve std::invalid_argument,
// prefixe "Monument : " et cle entre apostrophes, si unit est "1" (sans
// dimension), "°" (un angle) ou hors de l'enumeration. "ft" vaut kMonumentFoot ;
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
	MonumentValue euthynteria;
};

struct MonumentPeristyle   // README sections 2 et 4.2
{
	MonumentValue frontColumns, flankColumns;
	MonumentValue frontSpacing, flankSpacing, frontCornerSpacing, flankCornerSpacing;
};

// README sections 4.1, 4.4 et 5. Retraits depuis le nu de l'abaque, profondeurs
// sous le lit de l'abaque (README l. 186) ; annelets : retraits de leur sommet
// et de leur pied (l. 167).
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
	double frontSum = 0.0, flankSum = 0.0;        // section 12, sommes de facade et de flanc
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

// Suite de largeurs lue par la regle R5 du script (README l. 253), mots du
// README "demi-canal", "bande", "canal".
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
// depuis la ligne des axes des colonnes (README l. 224). Regulae et gouttes
// (l. 212, 231, 232) : saillie de la face de la regula ; nombre de gouttes par
// regula, diametre et vide (au pied, EN19) ; saillies de la generatrice avant
// en tete et au pied. Mutules (l. 273, 274) : largeur en pieds et pouces ;
// profondeur, du devant au dos, le long de la sous-face. Sous-face du geison
// (l. 276) : pente, en degres. Gouttes des mutules (l. 275) : diametres de tete
// des trois rangs, du devant vers le dos ; vides entre tetes ; hauteur, normale
// a la sous-face ; axe des gouttes extremes d'un rang depuis le bord du mutule ;
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
// les comptes sont en "1", entiers >= 1. Angle de la frise (README l. 255-256) :
// premiere solution, celle du modele (EN15) : centre du triglyphe d'angle
// au-dela de l'axe d'angle, distance d'angle de facade (entraxe d'angle plus ce
// centre), metopes d'angle de facade et de flanc, decalage de l'intermediaire
// vers l'angle ; seconde solution (intermediaire au milieu de la travee) :
// metopes interieure et exterieure, de facade et de flanc. Gouttes des regulae
// (l. 212, 277) : marge de chaque bout, reste de la largeur de la regula,
// nombre ; dos des mutules depuis l'axe, sous une sous-face horizontale
// (l. 274). Gouttes des mutules (l. 275) : nombre de rangs et diviseur du pas
// (comptes), axes des rangs du milieu et de l'arriere depuis l'arete avant, pas
// en largeur, evasement en rayon, sa valeur non arrondie, avant du pied de la
// goutte avant depuis l'axe. Profondeur sous la pente (l. 274) : marge arriere
// des tetes, projection horizontale, dos depuis l'axe. Sous-face du geison
// (l. 276) : cotes du pivot, de l'arete avant-haute du mutule, du dessous au dos
// et du haut du bec, depuis le stylobate ; montee ; ressaut vertical au dos ;
// saillie du pivot.
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
};

// Lecture stricte d'un document "monument/v6" (json_reader::StrictReader). Leve
// std::runtime_error si le document est refuse ; le message porte chaque refus,
// une ligne chacun, "monument/v6 <chemin> : <motif>". Refus : texte illisible ;
// cle en double ; schema autre que "monument/v6" ; cle inconnue ou absente, a
// tout niveau ; null ; type faux ; status hors des cinq ; unit hors des six,
// "1", "ft" ou "°" sur une longueur, autre que "1" sur un compte, autre que "ft"
// sur une cle "<nom>Ft", autre que "°" sur un angle ; value absente pour un
// statut autre que manquant, presente pour manquant ; longueur ou angle non fini
// ou <= 0 ; compte ou pieds non entiers ou < 1 ; source inconnue de la table, ou
// entree de la table jamais citee ; lu ou mesure sans source ; manquant avec une
// source ; readme vide ; sha256 hors de [0-9a-f]{64} ; bytes < 1 ; liste de
// coordonnees de controle vide, coordonnee non finie, ou coordonnees non
// strictement croissantes ; controle de longueur non fini ou <= 0, ou sans
// dimension ; controle de compte autre que "1" ou non entier >= 1 ; profil de
// controle vide, de listes de longueurs differentes, ou de profondeurs non
// strictement croissantes ; suite de divisions vide, de listes de tailles
// differentes, de largeur non finie ou <= 0, de mot hors des trois, sans
// dimension, ou manquante.
MonumentData ParseMonument (std::string_view jsonText);

// Le document embarque (data/temple_hephaistos.json), lu une fois. Une lecture
// en echec leve a chaque appel. La reference designe un objet statique local :
// valide pour la duree du programme, premiere lecture sure entre fils.
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
// (MonumentLength), si un compte n'est pas un entier >= 1, si stepCount n'est pas 3 (les trois hauteurs
// du schema), ou si l'euthynteria n'est pas manquante (KR3 l'ajouterait comme
// degre, sans debord donne). Les reglages rendus ne sont pas valides par la
// conversion : LayoutKrepis, LayoutPeristyle et LayoutTemple les valident.
KrepisSettings    MonumentKrepisSettings (const MonumentData &data);
PeristyleSettings MonumentPeristyleSettings (const MonumentData &data);

// Colonne du README (section 5), courante ou d'angle (corner : diametre
// inferieur cornerLowerDiameter, tout le reste identique, README l. 134).
// Memes regles d'erreur que MonumentKrepisSettings, plus un refus si
// grooveDepth n'est pas manquante (GD3d ne vaut que pour elle) et si
// annuletCount depasse kGreekDoricMaxAnnulets (avant toute allocation) ; reglages non
// valides ici (LayoutTemple et BuildGreekDoricColumnProfile les valident).
// Conversion (R = upperDiameter / 2, A = abacusHalfWidth, retraits depuis le nu
// de l'abaque ; registre classical_orders_implementation.md section 10.6) :
//   - entasis : f = entasisSagitta, zf = entasisHeight lu (GD1) ;
//   - cannelures a trois centres, rapport profondeur / corde
//     fluteDepth / (lowerDiameter sin (pi / N)), au diametre inferieur courant
//     pour les deux jeux (section homothetique, README l. 130 ; GD2a) ;
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
// dans la travee d'angle (README l. 256, premiere solution) ; le peristyle garde
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

// Temple du README : krepis, peristyle, colonne courante, colonne d'angle et
// entablement (MonumentEntablatureSettings).
TempleSettings MonumentTempleSettings (const MonumentData &data);

} // namespace classical
} // namespace cgarchi
