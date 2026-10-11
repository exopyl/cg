#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include "extrude_contours.h"   // ExtrudeContour

//
// Validateur de FABRICATION (impression resine), INDEPENDANT de tout
// generateur : il ne lit qu'une region 2D de matiere, en unites du modele, et
// une echelle en millimetres par unite. Il rend un rapport chiffre par categorie
// stable (PrintDefect), jamais un texte libre. Il ne juge pas la validite
// geometrique -- c'est le role de contour_validation.h et mesh_validation.h --,
// mais la conformite d'une geometrie VALIDE aux minima d'une imprimante.
//
// Region : jeu de contours fermes lu sous la regle NonZero (convention de
// contour_ops.h : enveloppes en sens positif, trous a l'inverse).
//
// Repere de calcul : la region est recentree sur le milieu de son emprise puis
// multipliee par mmPerUnit. Tout se mesure en MILLIMETRES : les rayons
// d'ouverture et les tolerances ne dependent ni de l'echelle ni de la distance a
// l'origine. Les enregistrements sont rendus dans le repere du modele, en mm.
//
// Vides : complement de la region dans un rectangle qui deborde de son emprise
// de 4 r_trou -- trous fermes ET passages entre pieces separees. Un reste du
// vide dont l'emprise n'approche pas celle de la matiere a moins de r_trou est
// un coin du rectangle : il est ignore et n'est pas compte.
//
// Mesure par OUVERTURE morphologique (erosion puis dilatation de rayon r,
// jointure ronde) : matiere avec r = minWallMm / 2, vides avec r = minHoleMm / 2.
// La partie CONSERVEE est l'erosion dilatee de r + delta, delta = max(1e-3 r,
// 16 ulp float x demi-emprise) : la tolerance d'arrondi, sans laquelle un ruban
// d'arrondi le long d'un bord conserve relierait des morceaux distincts. Le
// RESTE (region moins partie conservee) est decoupe en morceaux :
//   - contact : suite cyclique maximale d'aretes du morceau dont le milieu est a
//     moins de delta / 2 du bord de la partie conservee ; les contacts de tous
//     les contours du morceau s'additionnent. Chaque contact appartient a une
//     COMPOSANTE de la partie conservee ;
//   - profondeur : plus grande distance d'un point du BORD du morceau
//     (echantillonne au pas 0,05 r) aux aretes de contact du meme morceau,
//     PLAFONNEE a depthCapMm = 2 x max(noiseDepthRatio x r_paroi,
//     maxTipLengthMm) : minorant de la profondeur vraie, exact a 0,025 r pres
//     sur le bord sous le plafond ; 0 sans contact ;
//   - pont : un morceau dont les contacts touchent au moins DEUX composantes
//     conservees relie des masses distinctes : col, quelle que soit sa
//     profondeur ;
//   - bruit : tout autre morceau a au moins un contact et de profondeur
//     < noiseDepthRatio x r est ignore (discretisation des courbes, arrondis) ;
//   - matiere : 0 contact -> ThinIsland (piece entierement plus mince que
//     minWallMm) ; 1 -> pointe, FragileTip si profondeur > maxTipLengthMm ;
//     pont, ou >= 2 contacts -> ThinWall (col) ;
//   - vide : 0 contact -> ClosedOpening (vide entierement plus etroit que
//     minHoleMm) ; 1 -> information BluntedVoidCorner, jamais un defaut ;
//     pont, ou >= 2 contacts -> NarrowOpening (col).
// PINCEMENT : une composante conservee qui contient au moins deux composantes
// de l'erosion cache un col plus etroit que 2r sans aucun reste (deux disques
// qui se recouvrent : l'ouverture les rend tels quels). Elle donne un ThinWall
// (matiere) ou un NarrowOpening (vide) : boite de la composante, contacts =
// nombre de composantes de l'erosion, profondeur et aire 0.
// Une largeur signalee est rendue en fourchette : strictement sous le seuil de
// sa categorie, sans valeur exacte.
//
// Echelle absente : information ScaleUnset, aucune mesure, pas de verdict.
//
// Cout : par cote (matiere, vides), une erosion, une dilatation et une
// difference (Clipper2, precision 6 : pas de 1e-6 mm, via contour_ops.h), puis,
// par morceau du reste, une recherche de contact par grille et une profondeur
// par grille sur ses aretes de contact, arretee au plafond. Mesures :
// gothic_implementation.md, M4-a.
//
enum class PrintDefect
{
	ThinWall,            // col de matiere plus mince que minWallMm
	ThinIsland,          // piece de matiere entierement plus mince que minWallMm
	FragileTip,          // pointe de matiere mince sur plus de maxTipLengthMm
	ClosedOpening,       // vide entierement plus etroit que minHoleMm
	NarrowOpening,       // col de vide plus etroit que minHoleMm
	ThinSlab,            // plaque extrudee plus mince que minWallMm (CheckPrintExtrusion)
	DetailTooSmall,      // detail en relief sous minDetailMm ; produit par aucun controle actuel
	EnvelopeExceeded,    // emprise hors de l'enveloppe d'impression (CheckPrintEnvelope)
	// Informations : comptees et enregistrees, jamais un defaut.
	BluntedVoidCorner,   // coin de vide arrondi par l'impression
	ScaleUnset,          // echelle absente : aucune mesure
};

constexpr std::size_t kPrintDefectCount = static_cast<std::size_t>(PrintDefect::ScaleUnset) + 1;

// Vrai pour BluntedVoidCorner et ScaleUnset.
constexpr bool IsPrintInformation (PrintDefect kind)
{
	return kind == PrintDefect::BluntedVoidCorner || kind == PrintDefect::ScaleUnset;
}

// Nom stable en snake_case (« thin_wall », ...), pour les diagnostics et les
// tests. Ne change pas d'une version a l'autre.
const char *PrintDefectName (PrintDefect kind);

// Defauts [H] : imprimante et resine non fixees, a etalonner sur coupons. Chaque
// valeur est un parametre de PrintConstraints ; aucune n'est relue ailleurs dans
// le code.
constexpr double kDefaultMinWallMm       = 0.4;    // paroi de matiere
constexpr double kDefaultMinHoleMm       = 0.5;    // ouverture
constexpr double kDefaultMinDetailMm     = 0.3;    // detail en relief
constexpr double kDefaultMaxTipLengthMm  = 0.3;    // partie mince d'une pointe
constexpr double kDefaultNoiseDepthRatio = 0.25;   // filtre de bruit, en fraction de r

// Domaine mesurable. minWallMm, minHoleMm et maxTipLengthMm sont dans
// [kMinPrintLengthMm, kMaxPrintLengthMm] : en dessous, le rayon d'ouverture
// n'a plus mille pas de Clipper2 ; au-dessus, ce n'est plus un seuil
// d'impression. La region, recentree, doit tenir dans une demi-emprise M telle
// que 16 ulp float x M <= kMaxRoundingShare x min(minWallMm, minHoleMm) / 2 :
// environ 1 050 mm aux seuils par defaut.
constexpr double kMinPrintLengthMm = 2e-3;
constexpr double kMaxPrintLengthMm = 1e3;
constexpr double kMaxRoundingShare = 0.01;

struct PrintConstraints
{
	// Millimetres par unite du modele. Absente : ScaleUnset, aucune mesure.
	// Fournie : finie et > 0. Pas de valeur par defaut : une echelle supposee
	// rendrait un verdict sur une piece dont la taille n'est pas fixee.
	std::optional<double> mmPerUnit;

	double minWallMm       = kDefaultMinWallMm;        // domaine mesurable
	double minHoleMm       = kDefaultMinHoleMm;        // domaine mesurable
	double maxTipLengthMm  = kDefaultMaxTipLengthMm;   // domaine mesurable
	double noiseDepthRatio = kDefaultNoiseDepthRatio;  // dans [0, 1[

	// Valides : minDetailMm fini et > 0, lu par aucun controle actuel ;
	// enveloppe utile de l'imprimante (x, y, z) finie et > 0, lue par
	// CheckPrintEnvelope seul. Absente : aucune enveloppe, pas de defaut cache.
	double                               minDetailMm = kDefaultMinDetailMm;
	std::optional<std::array<double, 3>> envelopeMm;

	// Plafond des enregistrements PAR CATEGORIE, ScaleUnset compris. Les
	// compteurs, eux, ne sont jamais plafonnes.
	std::size_t maxRecordsPerKind = 32;
};

// Un defaut ou une information localise, en millimetres dans le repere du
// modele (coordonnees x mmPerUnit). ScaleUnset : aucun champ renseigne.
struct PrintDefectRecord
{
	PrintDefect           kind = PrintDefect::ThinWall;
	std::array<double, 4> boxMm {};       // xmin, ymin, xmax, ymax du morceau
	double                depthMm = 0.0;  // profondeur plafonnee (voir plus haut)
	double                areaMm2 = 0.0;
	unsigned              contacts = 0;
	// Fourchette de largeur : la largeur est < widthBelowMm (seuil de la
	// categorie). 0 pour une pointe et pour un coin de vide.
	double                widthBelowMm = 0.0;
	// Pile de tranches (CheckPrintSlices) : indice du resume de la tranche dans
	// PrintabilityReport::slices, et sa couche [zLowMm, zHighMm] en mm. -1 et 0
	// hors pile (CheckPrintRegion, CheckPrintExtrusion).
	int                   slice = -1;
	double                zLowMm = 0.0;
	double                zHighMm = 0.0;
};

// Resume d'une tranche du rapport fusionne de CheckPrintSlices, en mm.
struct PrintSliceSummary
{
	double zLowMm = 0.0, zHighMm = 0.0;   // couche representee
	double zCutMm = 0.0;                  // cote de la coupe ; 0 pour la silhouette
	double areaMm2 = 0.0;                 // aire de la section (NonZero)
	bool   silhouette = false;            // projection de toute la pile (vides, thin_slab)
	std::array<std::size_t, kPrintDefectCount> counts {};   // comptes de cette tranche
};

struct PrintabilityReport
{
	std::array<std::size_t, kPrintDefectCount> counts {};

	bool   evaluated = false;           // faux sans echelle
	double mmPerUnit = 0.0;             // echelle employee ; 0 sans echelle
	double wallRadius = 0.0;            // r de la matiere, en unites du modele
	double holeRadius = 0.0;            // r des vides, en unites du modele
	double depthCapMm = 0.0;            // plafond de profondeur employe

	// Statistiques du classement (morceaux du reste, bruit compris).
	std::size_t solidPieces = 0;        // morceaux du reste de la matiere
	std::size_t voidPieces = 0;         // morceaux du reste des vides, coins du rectangle exclus
	std::size_t noisePieces = 0;        // morceaux ignores comme bruit (matiere et vides)
	std::size_t bluntTips = 0;          // pointes de matiere sous maxTipLengthMm

	std::vector<PrintDefectRecord> records;   // au plus maxRecordsPerKind par categorie

	// CheckPrintSlices : une entree par tranche, dans l'ordre de la pile, puis
	// la silhouette. Vide hors pile.
	std::vector<PrintSliceSummary> slices;

	std::size_t Count (PrintDefect kind) const { return counts[static_cast<std::size_t>(kind)]; }

	// Vrai si et seulement si la mesure a eu lieu et qu'aucune categorie de
	// DEFAUT n'est comptee ; les informations n'interviennent pas. Sans echelle,
	// faux : l'absence de mesure n'etablit pas la conformite.
	bool IsPrintable () const;
};

// Controle 2D d'une region de matiere. Leve std::invalid_argument -- et rien
// d'autre sur une entree -- si une contrainte est hors de son domaine (voir
// PrintConstraints), si un point de la region n'est pas fini, ou si la region
// sort du domaine mesurable une fois convertie en millimetres. Une piece plus
// petite que la resolution (1e-6 mm) est une ThinIsland de l'emprise d'entree.
PrintabilityReport CheckPrintRegion (const std::vector<ExtrudeContour> &region,
                                     const PrintConstraints &constraints);

// Controle d'une EXTRUSION DROITE : la region, prismatique sur l'epaisseur
// `heightUnits` en z (unites du modele). Rend le rapport de CheckPrintRegion,
// plus un ThinSlab si la region a au moins un point et que l'epaisseur, en mm,
// est sous minWallMm : une plaque plus mince qu'une paroi, quelle que soit son
// emprise (une feuille d'epaisseur nulle en est une). Enregistrement : boite de
// la region en mm, widthBelowMm = minWallMm. Sans echelle : ScaleUnset seul.
// Leve comme CheckPrintRegion, et std::invalid_argument si heightUnits n'est pas
// fini ou est negatif.
PrintabilityReport CheckPrintExtrusion (const std::vector<ExtrudeContour> &region, double heightUnits,
                                        const PrintConstraints &constraints);

// Une tranche d'une pile 2,5D : la section de matiere `region` (unites du
// modele, regle NonZero), coupee a zCut, representant la couche [zLow, zHigh]
// (zLow <= zCut <= zHigh, unites du modele). L'appelant choisit les coupes :
// la pile ne fait aucune hypothese sur leur nombre ni sur leur espacement.
struct PrintSlice
{
	double                      zLow = 0.0;
	double                      zHigh = 0.0;
	double                      zCut = 0.0;
	std::vector<ExtrudeContour> region;
};

// Controle d'une PILE de tranches. Rapport FUSIONNE : comptes sommes sur les
// tranches, plafond d'enregistrements par categorie sur l'ensemble, chaque
// enregistrement porte sa tranche et sa couche en mm ; un resume par tranche
// (PrintabilityReport::slices), puis celui de la silhouette.
//   - matiere, PAR TRANCHE : regles de CheckPrintRegion cote matiere (ThinWall,
//     ThinIsland, FragileTip). Un defaut present dans plusieurs tranches est
//     compte dans chacune ;
//   - vides, sur la SILHOUETTE seule (reunion NonZero des sections, projection
//     de la pile) : regles de CheckPrintRegion cote vides (ClosedOpening,
//     NarrowOpening, BluntedVoidCorner). Un vide traversant est le complement
//     de la silhouette : un vide d'une tranche que la matiere d'une autre
//     recouvre n'est pas une ouverture ;
//   - ThinSlab : silhouette non vide et hauteur de la pile (max zHigh -
//     min zLow), en mm, sous minWallMm ; enregistre sur la silhouette.
// Pile vide : evaluee, sans resume ni defaut. Sans echelle : ScaleUnset seul,
// aucun resume. Leve comme CheckPrintRegion (chaque section et la silhouette
// sont des regions), et std::invalid_argument si une cote n'est pas finie ou
// si zLow <= zCut <= zHigh n'est pas tenu.
PrintabilityReport CheckPrintSlices (const std::vector<PrintSlice> &slices,
                                     const PrintConstraints &constraints);

// Comptes pour l'AFFICHAGE : par categorie, le maximum sur les resumes de
// tranche, plus ce qu'aucune tranche ne porte (comptes moins leur somme sur les
// tranches : enveloppe, echelle absente). Hors pile (slices vide) : les comptes
// eux-memes. Un defaut vu dans plusieurs tranches n'y compte qu'une fois ; il
// n'y a pas de dedoublonnage geometrique, donc deux defauts distincts de deux
// tranches peuvent n'en faire qu'un. Le verdict (IsPrintable) lit les comptes.
std::array<std::size_t, kPrintDefectCount> PrintDisplayCounts (const PrintabilityReport &report);

// Emprise d'une piece, en unites du modele : coins bas et haut (x, y, z).
struct PrintExtent
{
	std::array<double, 3> lo {};
	std::array<double, 3> hi {};
};

// Controle d'ENVELOPPE : la boite `extentUnits`, convertie en mm, tient-elle
// dans constraints.envelopeMm ? Elle tient si l'une des six permutations de ses
// axes (rotations de 90 degres) la loge, egalite comprise ; c'est le cas si et
// seulement si ses cotes triees sont chacune <= celles, triees, de
// l'enveloppe. Sinon un EnvelopeExceeded : boite en x, y dans boxMm, en z dans
// zLowMm / zHighMm, repere du modele en mm ; widthBelowMm 0.
// Biais : optimiste en z (supports et radeau ignores : l'enveloppe est un
// volume utile) et pour une impression inclinee ; pessimiste pour une pose en
// diagonale. « Tient » se lit « tient posee droite, sans supports ».
// Sans echelle : rapport vide, evaluated faux, SANS ScaleUnset (le controle 2D
// le porte deja). Avec echelle et sans enveloppe : evalue, aucun compte.
// Leve std::invalid_argument si une contrainte est hors de son domaine, si
// l'emprise n'est pas finie ou a lo > hi sur un axe, ou si ses cotes debordent
// une fois converties en mm.
PrintabilityReport CheckPrintEnvelope (const PrintExtent &extentUnits, const PrintConstraints &constraints);

// Echelle qui donne a une piece large de `widthUnits` (unites du modele) la
// largeur imprimee `printedWidthMm` : printedWidthMm / widthUnits. Leve
// std::invalid_argument si l'une n'est pas finie et > 0, ou si le quotient ne
// l'est pas.
double MmPerUnitForPrintedWidth (double printedWidthMm, double widthUnits);
