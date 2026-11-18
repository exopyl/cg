#pragma once

#include <memory>
#include <vector>

#include <cgarchi/classical_greek_cella.h>
#include <cgarchi/classical_greek_doric_column.h>
#include <cgarchi/classical_greek_pediment.h>
#include <cgarchi/classical_peristyle.h>

class Mesh;

namespace cgarchi
{
namespace classical
{

// ============================================================================
//  Interieur du naos : colonnade dorique a deux etages en Pi ouvert vers
//  l'avant, epistyles, corniche interieure, assise XVII et poche du comble
// ============================================================================
//
// Repere du temple (classical_temple.h, D2-5) : X le long de la facade, Y en
// profondeur, facade avant en Y < 0 ; cotes z comptees depuis le dessus du
// stylobate, Z = stylobateTop + z. Unites : celles de l'appelant. Les reglages
// sont des longueurs de segments, sans signe, comptees depuis les nus du naos de
// la cella (CellaNaos) ; les positions signees en sont deduites ici
// (LayoutGreekNaosInterior). Registre CE-15 et suivantes
// (classical_orders_implementation.md section 10.16).
//
// Colonnade : un Pi ouvert vers l'avant. Flancs Left (X < 0) et Right (X > 0)
// aux axes X = -+a, a = inner - wallAxisOffset, colonnes aux axes
// Y = front + frontAxisOffset + k spacing, k de 0 a n - 1, de l'avant vers
// l'arriere ; la derniere est la colonne d'angle du fond. n - 1 est le nombre
// entier de travees qui ferme la longueur du naos avec backAxisOffset (controle,
// a 1e-9 pres de la longueur). Fond (Back) : la rangee Y = axe de la derniere
// colonne des flancs, colonnes d'axe X = -a + j (2 a / m), j de 1 a m - 1, m le
// nombre entier de travees de 2 a sur spacing (controle, de meme). Deux etages
// superposes aux memes axes : colonnes inferieures du sol du naos (naveLevel) au
// dessous de l'epistyle inferieur, colonnes superieures du dessus de
// l'epistyle inferieur au dessous de l'epistyle superieur. Pied de chaque
// epistyle : double (float (height)) plus le pied de ses colonnes, la cote float
// du sommet de leurs abaques (comme EN10).
//
// Pieces, toutes des prismes droits fermes, lisses, sans moulure ni bande :
//   - assise XVII : anneau de 4 prismes dans l'epaisseur des murs, de leur nu
//     interieur vers l'exterieur sur courseThickness, du dessus des murs
//     (wallTop) a wallTop + courseHeight ; Left et Right sur toute la longueur,
//     coins compris, Front et Back entre eux ;
//   - epistyle inferieur, puis superieur : trois prismes en Pi, Left, Right et
//     Back, de section soffit centree sur la ligne des axes, d'onglet a 45 degres
//     au retour arriere ; les flancs partent du nu arriere du mur de refend avant
//     (front) ;
//   - corniche interieure : trois prismes en Pi sur l'epistyle superieur, du
//     dessus de l'epistyle a + corniceHeight, de la face cote nef de l'epistyle
//     plus corniceProjection a sa face cote mur, d'onglet a 45 degres au retour
//     arriere, partant de front.
// Poche du comble (RoofPocket, registre CE-14) : emprise des nus du naos elargis
// de pocketMargin de chaque cote, plafond a stylobateTop + pocketCeiling.

struct NaosInteriorSettings
{
	double spacing = 0.0;               // entraxe, sur les trois cotes
	double frontAxisOffset = 0.0;       // nu arriere du mur de refend avant -> axe de la 1re colonne des flancs
	double backAxisOffset = 0.0;        // nu avant du mur de refend arriere -> axe de la rangee du fond
	double wallAxisOffset = 0.0;        // nu interieur des murs longs -> axe des flancs
	double naveLevel = 0.0;             // pied des colonnes au-dessus du stylobate ; celui du sol du naos
	GreekDoricColumnSettings lowerColumn;   // height : pied -> dessous de l'epistyle inferieur
	GreekDoricColumnSettings upperColumn;   // height : dessus de l'epistyle inferieur -> dessous de l'epistyle superieur
	double lowerEpistyleHeight = 0.0;
	double lowerEpistyleSoffit = 0.0;   // largeur, centree sur la ligne des axes
	double upperEpistyleHeight = 0.0;
	double upperEpistyleSoffit = 0.0;
	double courseHeight = 0.0;          // assise XVII
	double courseThickness = 0.0;       // depuis le nu interieur des murs, vers l'exterieur
	double corniceHeight = 0.0;
	double corniceProjection = 0.0;     // saillie sur la face cote nef de l'epistyle superieur
	double pocketCeiling = 0.0;         // plafond de la poche du comble, au-dessus du stylobate
	double pocketMargin = 0.0;          // debord de la poche sur les nus du naos
};

enum class NaosInteriorStorey { Lower, Upper };

// Colonne placee : etage, cote (Left, Right, Back) et rang dans son cote (flancs
// de l'avant vers l'arriere, fond de X < 0 vers X > 0) ; translation du pied de
// l'axe.
struct NaosInteriorColumn
{
	NaosInteriorStorey storey = NaosInteriorStorey::Lower;
	PeristyleSide side = PeristyleSide::Left;
	int index = 0;
	double x = 0.0, y = 0.0, z = 0.0;
};

// Positions deduites, repere du temple, cotes Z absolues.
struct NaosInteriorLayout
{
	CellaNaos naos;                 // ceux de la cella
	double axisX = 0.0;             // a : axes des flancs a X = -+a
	double frontAxisY = 0.0;        // axe de la 1re colonne des flancs
	double backAxisY = 0.0;         // axe de la rangee du fond
	double lowerFoot = 0.0;         // stylobateTop + naveLevel
	double lowerEpistyleFoot = 0.0; // double (float (lowerColumn.height)) + lowerFoot
	double upperFoot = 0.0;         // lowerEpistyleFoot + lowerEpistyleHeight
	double upperEpistyleFoot = 0.0; // double (float (upperColumn.height)) + upperFoot
	double corniceFoot = 0.0;       // upperEpistyleFoot + upperEpistyleHeight
	double corniceTop = 0.0;        // corniceFoot + corniceHeight
	double courseTop = 0.0;         // naos.wallTop + courseHeight
	double top = 0.0;               // cote la plus haute de l'interieur
	std::vector<NaosInteriorColumn> columns;   // etage inferieur, puis superieur ; Left, Back, Right
	GreekDoricColumnHalfExtents lowerExtents, upperExtents;
	RoofPocket pocket;              // poche du comble, zTop absolu
};

// Contraintes de LayoutGreekNaosInterior, refus std::invalid_argument prefixe
// "Interieur : " et champ entre apostrophes ('interior.spacing'), dans cet
// ordre :
//   - longueur non finie ou <= 0 ;
//   - pied des colonnes different du sol du naos ('interior.naveLevel' : la nef
//     abaissee n'est pas construite) ;
//   - travees des flancs qui ne ferment pas la longueur du naos
//     ('interior.backAxisOffset'), ou travees du fond qui ne ferment pas
//     l'ecart des flancs ('interior.wallAxisOffset'), a 1e-9 pres de la
//     longueur fermee ; flancs au-dela de l'axe longitudinal
//     ('interior.wallAxisOffset') ;
//   - reglages des colonnes (exceptions de ComputeGreekDoricColumnHalfExtents,
//     telles quelles) ;
//   - colonnes au contact d'un mur ou entre elles ('interior.wallAxisOffset',
//     'interior.frontAxisOffset', 'interior.backAxisOffset', 'interior.spacing') ;
//   - epistyle au contact d'un mur long ou du mur de refend arriere, ou qui
//     passe l'axe longitudinal ('interior.lowerEpistyleSoffit',
//     'interior.upperEpistyleSoffit') ; corniche qui passe l'axe longitudinal
//     ('interior.corniceProjection') ;
//   - assise XVII qui sort d'un mur ('interior.courseThickness') ;
//   - poche qui ne contient pas l'assise ('interior.pocketMargin'), ou plafond
//     au plus a la cote la plus haute de l'interieur ('interior.pocketCeiling').
// Garantit que les pieces de BuildGreekNaosInteriorPieces et les colonnes
// placees ne se touchent que par des faces coplanaires, et ne touchent la cella
// que par des faces coplanaires (registre CE-15 et suivantes). La poche n'est
// validee contre le comble que par BuildGreekPedimentPieceMeshes.
NaosInteriorLayout LayoutGreekNaosInterior (const NaosInteriorSettings &settings, const CellaLayout &cella);

enum class NaosInteriorElement { Course, LowerEpistyle, UpperEpistyle, Cornice };

struct NaosInteriorPiece
{
	NaosInteriorElement element = NaosInteriorElement::Course;
	PeristyleSide side = PeristyleSide::Left;   // assise : les quatre cotes ; Pi : Left, Right, Back
	int index = 0;
	std::unique_ptr<Mesh> mesh;                  // repere du temple, triangles, ferme
};

// Pieces, dans cet ordre : assise XVII Left, Right, Front, Back ; epistyle
// inferieur Left, Right, Back ; epistyle superieur Left, Right, Back ; corniche
// Left, Right, Back. Les colonnes ne sont pas construites ici : leurs placements
// sont dans `layout`, qui doit etre celui de LayoutGreekNaosInterior sur les
// memes reglages (aucune validation n'est refaite).
std::vector<NaosInteriorPiece> BuildGreekNaosInteriorPieces (const NaosInteriorSettings &settings,
                                                             const NaosInteriorLayout &layout);

} // namespace classical
} // namespace cgarchi
