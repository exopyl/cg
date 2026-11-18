#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include <cgarchi/classical_greek_ceiling.h>
#include <cgarchi/classical_greek_cella.h>
#include <cgarchi/classical_greek_cella_crown.h>
#include <cgarchi/classical_greek_doric_column.h>
#include <cgarchi/classical_greek_entablature.h>
#include <cgarchi/classical_greek_naos_interior.h>
#include <cgarchi/classical_greek_pediment.h>
#include <cgarchi/classical_krepis.h>
#include <cgarchi/classical_peristyle.h>
#include <cgarchi/classical_temple_refinement.h>

namespace cgarchi
{
namespace classical
{

// ============================================================================
//  Temple : krepis, puis peristyle de colonnes doriques grecques posees sur le
//  stylobate ; placements, puis maillage
// ============================================================================
//
// Repere (D2-5) : X le long de la facade, Y en profondeur (facade en Y < 0),
// Z vertical ; krepis et axes centres sur l'origine ; pied du degre le plus bas
// en z = 0 (D3-16). Unites : celles de l'appelant.
//
// Conventions (registre TE1 a TE6, classical_orders_implementation.md section
// 10.8) : deux jeux de reglages, colonne courante et colonne d'angle (TE1) ;
// peristyle centre sur le stylobate (TE2) ; pied de chaque colonne dans le
// dessus du stylobate, bord compris (TE3) ; aucun contact entre colonnes (TE4) ;
// pieces fermees concatenees sans union, seuls recouvrements les contacts plans
// L1 (degres) et L2 (pied sur stylobate), et, avec un entablement, L3 a L6
// (TE5, ci-dessous) ; travee d'angle simple et symetrique (TE6,
// LayoutPeristyle) ; pied de l'entablement au sommet float des colonnes
// (EN10).
//
// Entablement optionnel (classical_greek_entablature.h) : absent, le temple est
// celui de la krepis et du peristyle seuls, au bit pres. Present, il ajoute un
// anneau balaye pose sur les abaques et un prisme par placement de triglyphe
// pose sur la frise de l'anneau, en L pour une piece d'angle (EN14, sous les
// strategies d'angle) ; les metopes sont la face de l'anneau (EN5). La strategie
// d'angle ne change ni le peristyle ni les colonnes. Recouvrements admis en plus
// de L1 et L2 (TE5) : L3, dessous de l'anneau sur le dessus de chaque abaque ;
// L4, dos d'un triglyphe sur la face de frise de l'anneau, sur les deux faces de
// frise de son angle pour une piece d'angle ; L5, dessous d'un triglyphe sur le
// ressaut de la bande sous la frise ; L6, dessus d'un triglyphe sous le ressaut
// de la bande au-dessus ; L7, sous une strategie d'angle, arete du coin (b, b)
// d'une piece d'angle confondue au bit pres avec l'arete d'onglet de la frise
// de l'anneau : apres soudure, une arete non manifold par piece d'angle et une
// composante de moins.
//
// Ornements optionnels de l'entablement (regulae, mutules) : une partie
// par regula avec ses gouttes, une par mutule, apres les triglyphes. Contacts
// admis en plus (TE5) : L8, dessus d'une regula sous le ressaut de la tenia ;
// L9, dos d'une regula sur la face de la bande friezeBand - 2 (deux faces pour
// une regula d'angle) ; L10, tete d'une goutte sous le dessous de sa regula ;
// L11, dessus d'un mutule sous le ressaut de la bande du dessus ; L12, dos d'un
// mutule sur la face de sa bande ; L13, sous une strategie d'angle, coins
// (R, R) et (dos, dos) du dessus de la regula d'angle sur deux sommets d'onglet
// de l'anneau, et diagonale (dos, dos)-(R, R) de son couvercle sur l'arete
// d'onglet du ressaut de la tenia : apres soudure, une arete non manifold
// (4 faces) et une composante de moins par regula d'angle ; L14, dessous d'un
// mutule sur le dessus de la bande sous la sienne, la ou son dos est en retrait
// sur elle. Une goutte est une composante a part. Sous sous-face inclinee
// (EN22, mutules.rise > 0) : L11, dessus du mutule sur la face inclinee
// de l'anneau, coplanaire au bit pres (memes couples (d, z)) ; aucun L14 ;
// L15, tete d'une goutte de mutule sur le dessous de son mutule, coplanaire a
// l'arrondi float pres : ValidateMesh ne la juge coplanaire que si cet ecart
// reste sous sa tolerance (registre TE5 ; CT3 valide a 1e-4 m, D3-80) ;
// le sommet avant de la tete avant touche l'arete avant-basse du mutule, sans
// traversee.
//
// Fronton optionnel (classical_greek_pediment.h), qui demande un entablement :
// absent, le temple est celui de l'entablement seul, au bit pres. Present, il
// ajoute apres les mutules la corniche rampante Front, la corniche rampante
// Back et le comble. Contacts admis en plus (TE5) : L16, dessous du comble sur
// le dessus de l'anneau, coplanaires ; L17, dos de chaque corniche rampante sur
// l'about du comble (plan du tympan), coplanaires ; L18, face de pied de chaque
// bras de chaque corniche rampante a plat sur le dessus de l'anneau,
// coplanaires (meme expression de z au bit pres). Apres soudure, 4 aretes non
// manifold (par fronton, les deux aretes haut-dos, portees chacune par 4
// faces) ; le comble et les deux corniches rampantes ne font qu'une composante.
//
// Raffinement optionnel (classical_temple_refinement.h ; registre RF1 a RF5,
// section 10.12) : absent, identique au temple sans raffinement, au bit pres.
// Present, chaque colonne est cisaillee a pied fixe (sommet de l'abaque deplace
// de (inclinationX, inclinationY) de son placement), puis chaque sommet de
// chaque piece est releve de TempleRefinementLift sur sa position horizontale
// finale, avant un seul arrondi float ; la krepis, l'anneau et le fronton sont
// re-tessellees par les stations. Les contacts horizontaux et inclines ne sont
// plus coplanaires qu'a l'ecart d'interpolation pres (registre RF6, TE5).
//
// Cella optionnelle (classical_greek_cella.h ; registre CE-1 a CE-9, section
// 10.16) : absente, le temple est celui des autres parties, au bit pres.
// Presente, ses pieces s'ajoutent apres toutes les autres, celles du comble
// comprises, puis les colonnes des porches : sans interieur, le maillage sans
// cella est un prefixe du maillage avec cella. Refusee avec un raffinement
// (CE-9). Contacts
// admis en plus (TE5, CE-8), tous coplanaires : C1, dessous du toichobate sur le
// dessus du stylobate ; C2, pieds des murs, du sol du naos, du seuil, des antes
// et des colonnes des porches sur le dessus du toichobate ; C3, faces du sol du
// naos contre les murs et le seuil ; C4, bouts des murs de refend contre les nus
// interieurs des murs longs ; C5, seuil dans l'echancrure du mur de refend
// avant ; C6, dos des antes contre les bouts des murs longs ; C7, dessous de
// l'architrave des porches sur les antes, les murs longs et les abaques des
// colonnes des porches, et son dos contre les murs longs ; C8, dessous de la
// frise sur l'architrave et son dos contre les murs longs.
//
// Interieur optionnel du naos (classical_greek_naos_interior.h ; registre CE-15
// et suivantes, section 10.16), qui demande une cella : absent, le temple est
// celui des autres parties, au bit pres. Present, ses pieces s'ajoutent apres
// les colonnes des porches, puis ses colonnes, et, avec un fronton, le comble
// recoit la poche de l'interieur (RoofPocket, CE-14). Le maillage avec
// interieur et celui sans interieur ont donc en commun, au bit pres : tout ce
// qui precede le comble ; les 6 premiers sommets du comble ; les sommets de la
// cella et des colonnes des porches, decales des 8 sommets de la poche (leurs
// indices de faces aussi). Sans fronton, le maillage sans interieur en est un
// prefixe. Refuse avec un raffinement, avant la cella (CE-9). Contacts admis en
// plus (TE5), tous coplanaires sauf les onglets : I1, pieds des colonnes
// inferieures sur le sol du naos ; I2, dessus des abaques sous les epistyles et
// pieds des colonnes superieures sur l'epistyle inferieur ; I3, onglets des
// epistyles et de la corniche au retour du fond, dans le plan vertical a 45
// degres ; I4, bouts avant des epistyles contre le mur de refend avant et
// l'assise XVII avant ; I5, corniche sur l'epistyle superieur ; I6, assise XVII
// sur le dessus des murs, ses coins entre eux ; I7, faces exterieures de
// l'assise XVII contre les parois de la poche du comble.
//
// Couronnement optionnel de la cella (classical_greek_cella_crown.h ; registre
// CE-23 et suivantes, section 10.16), qui demande une cella : absent, le temple
// est celui des autres parties, au bit pres ; present, ses pieces s'ajoutent
// apres les colonnes de l'interieur, si bien que le maillage sans couronnement
// en est un prefixe. Prolongement est optionnel, qui demande le couronnement et
// l'entablement : ses pieces suivent celles du couronnement. Refuses avec un
// raffinement (CE-9). Contacts admis en plus (TE5), tous coplanaires, aucun avec
// le comble : K1, corps et bandes des murs sur le dessus des murs ; K2,
// couronnements et leurs bandes sur le dessus de la frise des porches ; K3, face
// interieure de l'epicranitis et bouts des assises des porches contre l'assise
// XVII interieure ; K4, bouts des assises des murs contre le dos des
// couronnements ; K5, joints de l'assise : corps entre eux, dos des bandes sur
// les corps, onglets des bandes entre eux (plan vertical a 45 degres) ; K6,
// architrave prolongee sur les abaques du peristyle, et bouts du prolongement
// contre la face interieure de l'anneau ; K7 et K8, bouts de l'architrave, de
// la frise et du couronnement prolonges contre ceux des porches, faces
// confondues (DuplicateFace) quand les sections sont egales.

struct TempleSettings
{
	KrepisSettings           krepis;
	PeristyleSettings        peristyle;
	GreekDoricColumnSettings column;         // les 2 (front + flank) - 8 colonnes courantes
	GreekDoricColumnSettings cornerColumn;   // les 4 colonnes d'angle
	std::optional<GreekEntablatureSettings> entablature;   // absent : krepis et peristyle seuls
	std::optional<GreekPedimentSettings>    pediment;      // absent : temple sans fronton, au bit pres ; demande un entablement
	std::optional<TempleRefinementSettings> refinement;    // absent : temple sans raffinement, au bit pres
	std::optional<CellaSettings>            cella;         // absent : temple sans cella, au bit pres ; refuse le raffinement
	std::optional<NaosInteriorSettings>     interior;      // absent : temple sans interieur, au bit pres ; demande une cella
	std::optional<EpicranitisSettings>      epicranitis;   // absent : temple sans couronnement, au bit pres ; demande une cella
	std::optional<PorchExtensionSettings>   porchExtension;   // absent : sans prolongement est ; demande le couronnement
	std::optional<EastCeilingSettings>      eastCeiling;      // absent : sans plafond est ; demande le prolongement est
	std::optional<FlankCeilingSettings>     flankCeilings;    // absent : sans plafonds des flancs ; demande le plafond est
};

enum class TempleElement
{
	KrepisStep, Column, Entablature, Triglyph, Regula, Mutule, RakingCornice, Roof,
	Toichobate, NaosFloor, LongWall, CrossWall, Threshold, Anta, PorchArchitrave, PorchFrieze, PorchColumn,
	InteriorCourse, InteriorLowerEpistyle, InteriorUpperEpistyle, InteriorCornice, InteriorLowerColumn,
	InteriorUpperColumn,
	EpicranitisCourse, EpicranitisPorchCourse, EpicranitisCrossCourse, PorchCrown, CrownMoulding,
	ExtensionArchitrave, ExtensionFrieze, ExtensionCrown, ExtensionMoulding,
	CeilingBeam, PorchCornice, PorchBand, CeilingSlab
};

// Adresse stable (DG-4) : ne depend que des comptes et de la strategie d'angle
// de la frise, pas des longueurs. Par defaut, index = -1 : TempleAddress {} ne
// designe aucun element.
struct TempleAddress
{
	TempleElement element = TempleElement::KrepisStep;
	PeristyleSide side = PeristyleSide::Front;   // colonnes et triglyphes ; Front pour un degre et l'anneau
	// Degre : 0 stylobate, puis vers le bas. Colonne : rang dans son cote, dans
	// l'ordre de PeristyleLayout::columns, 0 pour l'angle qui ouvre le cote.
	// Anneau de l'entablement : 0. Triglyphe : rang de son placement dans son
	// cote (FriezePiece::rank) ; une piece d'angle a le rang 0 du cote qu'elle
	// ouvre. Regula : rang du triglyphe qui la porte (OrnamentPiece::rank), une
	// goutte n'ayant pas d'adresse propre. Mutule : rang le long de son cote.
	// Corniche rampante : 0, de cote Front ou Back. Comble : 0, cote Front ; avec
	// plusieurs poches, rang de la tranche par Y croissants (depend des longueurs,
	// registre CE-34).
	// Cella (CellaPiece) : son cote et son rang ; colonne de porche : cote de
	// son porche, rang 0 en X < 0, 1 en X > 0. Interieur (NaosInteriorPiece) :
	// son cote, rang 0 ; colonne de l'interieur : son cote et son rang
	// (NaosInteriorColumn). Couronnement (EpicranitisPiece) et prolongement est
	// (PorchExtensionPiece) : le cote et le rang de la piece. Plafonds
	// (CeilingPiece) : le cote et le rang de la piece.
	int index = -1;
};

// Adresse d'une piece de la cella : element du meme nom, cote et rang de la
// piece.
TempleAddress TempleAddressOf (const CellaPiece &piece);

// Adresse d'une piece de l'interieur : InteriorCourse, InteriorLowerEpistyle,
// InteriorUpperEpistyle ou InteriorCornice, cote et rang de la piece.
TempleAddress TempleAddressOf (const NaosInteriorPiece &piece);

// Adresse d'une piece du couronnement : EpicranitisCourse, EpicranitisPorchCourse,
// EpicranitisCrossCourse, PorchCrown ou CrownMoulding, cote et rang de la piece.
TempleAddress TempleAddressOf (const EpicranitisPiece &piece);

// Adresse d'une piece du prolongement est : ExtensionArchitrave, ExtensionFrieze,
// ExtensionCrown ou ExtensionMoulding, cote et rang de la piece.
TempleAddress TempleAddressOf (const PorchExtensionPiece &piece);

// Adresse d'une piece des plafonds : CeilingBeam, PorchCornice, PorchBand ou
// CeilingSlab, cote et rang de la piece.
TempleAddress TempleAddressOf (const CeilingPiece &piece);

struct TemplePlacement   // une colonne
{
	TempleAddress address;
	double x = 0.0, y = 0.0, z = 0.0;   // translation du pied de l'axe ; z = stylobateTop
	bool corner = false;                // reglages cornerColumn si vrai, column sinon
	// Raffinement : deplacement horizontal du sommet de l'abaque (RF4), vers
	// l'interieur, perpendiculaire a la rangee : Front (0, i), Back (0, -i), Right
	// (-i, 0), Left (i, 0) ; angle, les deux composantes. 0 sans raffinement.
	double inclinationX = 0.0, inclinationY = 0.0;
};

struct TempleLayout
{
	KrepisLayout                 krepis;
	PeristyleLayout              peristyle;
	GreekDoricColumnHalfExtents  columnExtents;         // colonne courante
	GreekDoricColumnHalfExtents  cornerColumnExtents;   // colonne d'angle
	std::vector<TemplePlacement> columns;               // ordre de peristyle.columns
	// Avec un entablement : pied, double (float (column.height)) + stylobateTop,
	// et frise (triglyphes et metopes, ces dernieres sans maillage). Sans : 0 et
	// absente.
	double                       entablatureFoot = 0.0;
	std::optional<FriezeLayout>  frieze;
	// Avec un entablement a regulae ou a mutules : LayoutGreekOrnaments ; absent
	// sinon.
	std::optional<OrnamentLayout> ornaments;
	// Avec un raffinement : stations nominales (NominalRefinementStations),
	// deplacees hors des coordonnees structurelles au maillage ; absentes sinon.
	std::optional<RefinementStations> stations;
	// Avec une cella : LayoutGreekCella sur la krepis ; absente sinon.
	std::optional<CellaLayout> cella;
	// Avec un interieur : LayoutGreekNaosInterior sur la cella ; absent sinon.
	std::optional<NaosInteriorLayout> interior;
	// Avec un couronnement : LayoutGreekEpicranitis sur la cella ; absent sinon.
	std::optional<EpicranitisLayout> epicranitis;
	// Avec un prolongement est : LayoutGreekPorchExtension ; absent sinon.
	std::optional<PorchExtensionLayout> porchExtension;
	// Avec un plafond est : LayoutGreekEastCeiling ; absent sinon.
	std::optional<EastCeilingLayout> eastCeiling;
	// Avec des plafonds des flancs : LayoutGreekFlankCeilings ; absents sinon.
	std::optional<FlankCeilingLayout> flankCeilings;
};

// Valide les composants : krepis, peristyle, puis les deux jeux de reglages de
// colonne ; leurs exceptions remontent telles quelles, avec le prefixe de leur
// composant. Leve ensuite std::invalid_argument, prefixe "Temple : " et chemin
// du champ entre apostrophes, si, sur les axes places par LayoutPeristyle
// (e_n, e_c : demi-etendues `overall` des colonnes courante et d'angle) :
//   - deux axes voisins d'une rangee sont a une distance au plus egale a la
//     somme des demi-etendues de leurs colonnes (e_c aux deux bouts, e_n entre) ;
//     le champ est l'entraxe d'angle pour la premiere et la derniere travee
//     ('peristyle.frontCornerSpacing', 'peristyle.flankCornerSpacing'),
//     l'entraxe courant sinon ('peristyle.frontSpacing', 'peristyle.flankSpacing') ;
//   - une rangee de deux colonnes a sa travee au plus egale a 2 max (e_c, e_n)
//     quand les rangees opposees, qui se font face a travers elle, ont au moins
//     trois colonnes ;
//   - a l'un des quatre angles, les deux voisines, l'une sur la facade, l'autre
//     sur le flanc, sont decalees sur les deux axes d'au plus la somme de leurs
//     demi-etendues ('peristyle.frontCornerSpacing') ;
//   - le pied d'une colonne, courante ou d'angle, de demi-etendue `foot` de son
//     jeu, sort du dessus du stylobate : |x| + foot > stylobateHalfWidth
//     ('krepis.stylobateHalfWidth'), |y| + foot > stylobateHalfDepth
//     ('krepis.stylobateHalfDepth').
// Garantit que les boites alignees sur les axes de deux colonnes placees, de
// demi-cote `overall`, sont disjointes (registre TE4), et que le carre de
// demi-cote `foot` de chaque pied est dans le dessus du stylobate. Pour les
// colonnes elles-memes, la condition est exacte quand les anneaux les plus
// larges des deux colonnes sont a la meme cote et sont des abaques ; elle est
// seulement suffisante sinon : deux sections rondes peuvent avoir des
// projections qui se recouvrent sans se toucher, et une colonne courante et une
// colonne d'angle de hauteurs differentes (acceptees) ont leurs anneaux les
// plus larges a des cotes differentes. Ne garantit pas la validite au sens de
// ValidateMesh d'un maillage du temple (D3-19) : elle demande en plus que toute
// separation de ce maillage depasse 2 fois sa tolerance de soudure, qui depend
// du maillage entier (condition de domaine, non imposee).
// Avec un entablement, valide en plus, dans cet ordre : ses reglages
// (ValidateGreekEntablatureSettings, exceptions "Entablement : " remontees
// telles quelles) ; l'egalite en float des hauteurs des deux jeux de colonne,
// sinon std::invalid_argument "Temple : 'cornerColumn.height'" (l'anneau ne peut
// poser sur deux hauteurs) ; la frise sur les axes places (LayoutGreekFrieze) ;
// les ornements, s'il y en a (LayoutGreekOrnaments). Avec un fronton, ensuite :
// std::invalid_argument "Temple : 'pediment' demande un entablement" s'il n'y en
// a pas, puis ses reglages (ValidateGreekPedimentSettings, exceptions
// "Fronton : " remontees telles quelles), puis le pied du haut de la corniche
// rampante, refus "Fronton : 'pediment.rakingRun'" s'il passe l'avant du
// couronnement (le meme que BuildGreekPedimentPieceMeshes), avant tout refus du
// raffinement. Avec un raffinement, ensuite : ses
// reglages (ValidateTempleRefinementSettings, exceptions "Raffinement : "
// remontees telles quelles) ; puis TE4 elargie de |i| (inclination) : les memes
// controles, chaque somme de demi-etendues augmentee de |i| (seules les colonnes
// voisines d'un angle se rapprochent, de i au plus), refus std::invalid_argument
// "Temple : 'refinement'" ; puis le majorant des faces des pieces a stations
// (krepis, anneau coupe, Lambda, comble), refus "Raffinement :
// 'refinement.chordDeviation'" au-dela de kTempleMaxFaces. Le deplacement des
// stations, qui demande le maillage, n'est pas fait ici (BuildTempleMesh).
// Enfin la borne de la calotte de l'abaque (registre RF5) : 1,1 F2 >
// kRefinementMaxTolerance / 2, F2 = (|f_XX| + |f_YY| + 2 |f_XY|) h^2 / 8,
// refus "Raffinement : 'refinement.sagittaFront', 'refinement.sagittaRight',
// 'refinement.sagittaBack' et 'refinement.sagittaLeft' ...". Puis la borne
// basse de la seconde famille (registre RF5), sur les maillages des deux
// colonnes : sur chaque axe a pas fini, p / 2 > plus long bloc fusionne
// [at - distance, at + distance] des sommets de pied et de dessus d'abaque
// (kRefinementClearance) et, avec un fronton, des pieds +-foot[k] de la
// corniche rampante en X (kRefinementClearance max (1, rakingRun))
// (LongestClearanceBlock), sinon refus "Raffinement :
// 'refinement.chordDeviation' donne sur l'axe X (ou Y) une fenetre de station".
// Avec une cella, d'abord, avant tout refus du raffinement : std::invalid_argument
// "Temple : 'cella' et 'refinement' ..." si les deux sont presents (CE-9 : la
// cella n'a pas de raffinements, README section 7.8, decision D8, et le champ de
// courbure ne s'y applique pas) ; puis LayoutGreekCella (exceptions "Cella : "
// telles quelles) ; puis "Temple : 'cella'" si l'emprise du toichobate touche la
// boite de demi-cote `overall` d'une colonne du peristyle, si, avec un
// entablement, elle n'est pas strictement a l'interieur de la face interieure de
// l'anneau (demi-distances des axes moins innerFace), ou si la cote la plus haute
// de la cella depasse le dessus de l'anneau, sous le comble (CE-4).
// Avec un interieur, avant la cella : std::invalid_argument "Temple :
// 'interior' et 'refinement' ..." avec un raffinement (CE-9), puis "Temple :
// 'interior' demande une cella" sans cella ; apres la cella,
// LayoutGreekNaosInterior (exceptions "Interieur : " telles quelles). La poche
// n'est validee contre le comble qu'au maillage (BuildGreekPedimentPieceMeshes,
// refus "Fronton : 'roofPocket...'").
// Avec un couronnement, avant la cella : "Temple : 'epicranitis' et
// 'refinement' ..." avec un raffinement (CE-9), puis "Temple : 'epicranitis'
// demande une cella" sans cella ; avec un prolongement est, de meme, "Temple :
// 'porchExtension' et 'refinement' ...", puis "Temple : 'porchExtension' demande
// le couronnement" sans couronnement, et "... demande un entablement" sans
// entablement. Apres l'interieur : LayoutGreekEpicranitis (exceptions
// "Couronnement : " telles quelles) ; "Temple : 'epicranitis.innerOffset'" avec un
// interieur dont l'assise XVII n'a pas cette epaisseur (CE-24) ; avec un
// entablement, "Temple : 'epicranitis'" si l'emprise du couronnement n'est pas
// strictement dans la face interieure de l'anneau, ou si son dessus n'est pas
// sous le dessus de l'anneau (CE-4 etendu) ; puis LayoutGreekPorchExtension
// (exceptions "Prolongement : " telles quelles), sur la face interieure de
// l'anneau par l'expression de SweepRectangularRing, ses bouts y etant egaux.
// Avec un plafond est, avant la cella : "Temple : 'eastCeiling' et 'refinement'
// ..." avec un raffinement (CE-9), puis "... demande le prolongement est" sans
// prolongement (lequel demande un entablement) ; avec des
// plafonds des flancs, de meme, "Temple : 'flankCeilings' et 'refinement' ...",
// "... demande le plafond est". Apres le prolongement : LayoutGreekEastCeiling (exceptions "Plafond
// est : " telles quelles), sur la face interieure de l'anneau (RingInnerOf) ;
// avec un fronton, "Temple : 'eastCeiling.bandHeight' met le dessus de la corniche
// et du bandeau du porche ouest au contact ou au-dessus d'un pan du comble" si la
// poche ouest tombe sous le critere de poche du fronton (detail::PocketMeetsRoof),
// en double puis en float, puis "Temple : 'eastCeiling.beamHeight' met le dessus
// des pieces au contact ou au-dessus d'un pan du comble" si la poche est y tombe ;
// puis LayoutGreekFlankCeilings (exceptions "Plafonds des flancs : "
// telles quelles) et, avec un fronton, "Temple : 'flankCeilings.peristyleGap'
// laisse le dessus des poutres et des dalles au contact ou au-dessus d'un pan du
// comble" si l'une des deux poches des flancs tombe sous le meme critere (CE-42).
void ValidateTempleSettings (const TempleSettings &settings);

// Placements, sans maillage. Valide d'abord, comme ValidateTempleSettings.
TempleLayout LayoutTemple (const TempleSettings &settings);

struct TempleMeshPart   // plage d'une piece dans le maillage
{
	TempleAddress address;
	unsigned firstVertex = 0, vertexCount = 0;
	unsigned firstFace = 0, faceCount = 0;
};

// Borne d'interface, pas une valeur du domaine (D3-17) : elle borne la memoire
// du maillage assemble.
constexpr std::size_t kTempleMaxFaces = std::size_t (1) << 24;

// Un maillage a la borne tient en indices unsigned et en tailles size_t d'une
// cible 32 bits (WebAssembly) : 3 indices par face, et au plus 3 sommets de
// 3 coordonnees par face. Le nombre maximal de faces d'un tour n'est pas une
// constante de compilation (la grille du tour recoit a l'execution les angles
// des croisements de plan) : le compte des faces et des sommets s'accumule donc
// sans debordement, chaque piece comparee a la marge restante avant d'etre
// ajoutee (CountTempleMeshFaces).
static_assert (9 * kTempleMaxFaces <= 0xFFFFFFFFu,
               "le maillage a la borne doit tenir en indices et en tailles de 32 bits");

// Vrai si un maillage de `faces` faces respecte la borne.
constexpr bool TempleMeshFacesWithinBound (std::size_t faces)
{
	return faces <= kTempleMaxFaces;
}

// Nombre de faces du maillage de BuildTempleMesh, sans l'assembler : seuls la
// krepis, les deux tours et, avec un entablement, l'anneau et ses pieces
// (BuildEntablaturePieceMeshes), et avec un fronton, ses trois pieces
// (BuildGreekPedimentPieceMeshes), sont construits. Valide d'abord, comme LayoutTemple ;
// leve std::invalid_argument ('peristyle') des que le compte depasserait
// kTempleMaxFaces faces, ou 3 kTempleMaxFaces sommets, compare apres chaque
// piece construite. Les refus "Frise : " de BuildTriglyphMesh (prisme degenere
// ou au contact d'un voisin une fois arrondi en float) et "Ornement : " de
// BuildOrnamentMesh (regula droite au contact, en float, du plan de dos des
// regulae d'un cote voisin ; sommets d'une regula, d'une goutte ou d'un mutule
// confondus une fois arrondis en float) et "Fronton : sommets confondus en
// float" et 'pediment.apexRise' (faite confondu en float avec le dessus de
// l'anneau) de BuildGreekPedimentPieceMeshes remontent tels quels, ici comme
// dans BuildTempleMesh ; LayoutTemple et ValidateTempleSettings, qui ne
// maillent pas, ne les voient pas. Le refus 'pediment.rakingRun' de la
// construction est celui de la mise en place, qui le leve d'abord.
std::size_t CountTempleMeshFaces (const TempleSettings &settings);

// Krepis (BuildKrepisMesh), puis, par placement et dans l'ordre de
// layout.columns, une copie translatee du tour de la colonne courante ou de la
// colonne d'angle (deux tours en tout), accumulees en un bloc, triangles
// seulement : chaque position est calculee en double (sommet float du tour plus
// translation) et arrondie une fois en float ; le pied de chaque colonne est
// donc, au bit pres, a la cote float (stylobateTop) du dessus du stylobate.
// Parties, dans l'ordre du maillage : un degre par KrepisLayout::steps
// (adresse KrepisStep, index du degre), puis une colonne par placement, puis,
// avec un entablement, l'anneau (adresse Entablature, index 0 ;
// SweepRectangularRing de GreekEntablatureSection sur les demi-distances des
// axes, a la cote entablatureFoot) et un prisme par placement de triglyphe,
// en L pour une piece d'angle, dans l'ordre de FriezeLayout::triglyphs (adresse
// Triglyph, cote et rang du placement ; BuildTriglyphMesh), puis, avec des
// ornements, une regula avec ses gouttes par placement de
// OrnamentLayout::regulae (adresse Regula, cote et rang du triglyphe) et un
// mutule par placement de OrnamentLayout::mutules (adresse Mutule, cote et
// rang) ; toutes les pieces hors anneau sont construites apres une seule
// validation des reglages, a la mise en place ; puis, avec un
// fronton, la corniche rampante Front (adresse RakingCornice, Front, 0), la
// corniche rampante Back (RakingCornice, Back, 0) et le comble (Roof, Front, 0 ;
// avec plusieurs poches, ses tranches par Y croissants, Roof, Front, rang),
// par BuildGreekPedimentPieceMeshes sur les demi-distances des axes, a la cote
// entablatureFoot. Toute cote de l'entablement vaut float (entablatureFoot
// + z) : le dessous de l'anneau est, au bit pres, a la cote du sommet des
// abaques. Les plages sont contigues et couvrent le maillage. Valide d'abord, comme
// LayoutTemple ; leve aussi std::invalid_argument ('peristyle') si le maillage
// depasserait kTempleMaxFaces faces, comme CountTempleMeshFaces, avant
// l'allocation du resultat. N'ecrit dans `layout` et `parts` qu'en cas de
// succes.
// Avec un raffinement : stations deplacees (DisplaceRefinementStations) hors des
// axes des colonnes, des contours des degres, des sections de l'anneau et du
// polygone rampant, des bords des triglyphes, regulae et mutules et du faite ;
// krepis, anneau et fronton par leurs surcharges a stations (degres de tailles
// inegales, parties dans le meme ordre et aux memes adresses ; anneau : section
// coupee dans la largeur de ses aretes horizontales, RefineRingSectionWidth au
// pas min (pitchX, pitchY), hors des cotes des pieces d'angle et des
// coordonnees interdites en u, puis boucles a stations propres, marge p / 16) ;
// carte de chaque sommet : translation, cisaillement d'une colonne,
// x += inclinationX (z_l / H), y += inclinationY (z_l / H), z_l la cote du
// sommet dans le tour, H = double (float (height)) (pied inchange, sommet de
// l'abaque deplace d'exactement l'inclinaison), puis z += TempleRefinementLift
// (raffinement, stylobateHalfWidth, stylobateHalfDepth, x, y), un seul arrondi
// float. Refus du raffinement, std::invalid_argument prefixe "Raffinement : ",
// dans cet ordre : ceux de LayoutTemple (reglages ; majorant des faces des
// pieces a stations au-dela de kTempleMaxFaces, 'refinement.chordDeviation') ;
// puis, apres les seuls maillages des colonnes, une station sans place a plus
// du plancher float, ou hors des blocs de la seconde famille (pieds et dessus
// d'abaque, sur l'axe de leur rangee, les deux a l'angle ; pieds +-foot[k] de
// la corniche rampante en X : StationClearance)
// (DisplaceRefinementStations, 'refinement.chordDeviation') ; puis, avant toute
// grille, ceux des coupes de l'anneau (RefineRingSectionWidth). Refus
// "Anneau : " et "Fronton : " des surcharges a stations, tels quels.
// CountTempleMeshFaces a les memes refus.
// Avec une cella, apres le comble : les pieces de BuildGreekCellaPieces, dans
// leur ordre (adresse TempleAddressOf, translation nulle), puis une copie
// translatee du tour de la colonne de porche par placement de
// CellaLayout::porchColumns (adresse PorchColumn, cote et rang du placement) ;
// pied de chaque colonne de porche a la cote float du dessus du toichobate.
// Avec un interieur : comble creuse de la poche de NaosInteriorLayout::pocket
// (BuildGreekPedimentPieceMeshes) ; apres les colonnes des porches, les pieces
// de BuildGreekNaosInteriorPieces, dans leur ordre (adresse TempleAddressOf,
// translation nulle), puis une copie translatee du tour de la colonne
// inferieure ou superieure par placement de NaosInteriorLayout::columns
// (adresse InteriorLowerColumn ou InteriorUpperColumn, cote et rang).
// Avec un couronnement, apres les colonnes de l'interieur : les pieces de
// BuildGreekEpicranitisPieces, puis, avec un prolongement est, celles de
// BuildGreekPorchExtensionPieces, dans leur ordre (adresse TempleAddressOf,
// translation nulle).
// Avec un plafond est, apres le prolongement : les pieces de
// BuildGreekEastCeilingPieces, puis, avec des plafonds des flancs, celles de
// BuildGreekFlankCeilingPieces, dans leur ordre (adresse TempleAddressOf,
// translation nulle). Poches du comble (CE-34), dans cet ordre : interieur,
// plafond est (EastCeilingLayout::eastPocket), porche ouest (westPocket),
// flancs Left puis Right (FlankCeilingLayout::pockets) ;
// plusieurs poches donnent le comble en tranches.
std::unique_ptr<Mesh> BuildTempleMesh (const TempleSettings &settings, TempleLayout *layout = nullptr,
                                       std::vector<TempleMeshPart> *parts = nullptr);

} // namespace classical
} // namespace cgarchi
