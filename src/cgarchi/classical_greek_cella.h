#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <cgarchi/classical_greek_doric_column.h>
#include <cgarchi/classical_krepis.h>
#include <cgarchi/classical_peristyle.h>

class Mesh;

namespace cgarchi
{
namespace classical
{

// ============================================================================
//  Cella grecque distyle in antis : toichobate, sols, murs, porte, seuil,
//  antes, architrave et frise des porches, colonnes des porches
// ============================================================================
//
// Repere du temple (classical_temple.h, D2-5) : X le long de la facade, Y en
// profondeur, facade avant en Y < 0 ; axe longitudinal de la cella en X = 0 ;
// cotes z comptees depuis le dessus du stylobate, Z = stylobateTop + z.
// Unites : celles de l'appelant. Les reglages sont des longueurs de segments,
// sans signe ; les positions signees en sont deduites ici (LayoutGreekCella).
//
// Pieces, toutes des prismes droits fermes, sans moulure (registre CE-5) :
//   - toichobate : dalle pleine de la face avant a la face arriere, de demi-largeur
//     longWallSpan / 2 + longWallThickness + toichobateOverhang, de z = 0 a
//     toichobateRise (partie visible ; la partie enfouie n'est pas construite,
//     CE-3) ; son dessus est le sol des porches ;
//   - sol du naos : remplissage plein entre les nus interieurs des murs et des
//     murs de refend, du dessus du toichobate a naosFloorHeight (CE-2) ;
//   - murs longs, Left (X < 0) et Right (X > 0) : prisme d'un profil (Y, z),
//     du dessus du toichobate au dos des antes jusqu'au pied de l'architrave des
//     porches, puis au dos de l'architrave jusqu'a wallTop ;
//   - mur de refend avant, perce de la porte et echancre du seuil sur toute son
//     epaisseur ; mur de refend arriere, plein ;
//   - seuil : de la saillie devant le mur de refend avant a son nu arriere,
//     centre sur la porte, du dessus du toichobate au sol du naos (CE-3) ;
//   - antes : prismes de longueur antaHeadLength, de la face interieure
//     (nu interieur du mur moins antaInnerProjection) a la face exterieure (nu
//     exterieur plus antaOuterProjection), du dessus du toichobate au pied de
//     l'architrave des porches ;
//   - architrave et frise de chaque porche : deux blocs lisses de profondeur
//     porchEntablatureDepth, d'une face exterieure d'ante a l'autre, la face
//     exterieure a porchEntablatureFront de la ligne des axes des colonnes du
//     porche (CE-7).
// Pied de l'architrave des porches : double (float (porchColumnHeight)) plus le
// dessus du toichobate, la cote float du sommet de l'abaque d'une colonne de
// porche (comme EN10 pour l'anneau). Tout sommet est calcule en double puis
// arrondi une fois en float.

// Colonnes des porches (partie Porches) : deux par porche, de part et d'autre
// de l'axe longitudinal, aux axes avant et arriere de CellaSettings.
struct GreekPorchColumnsSettings
{
	GreekDoricColumnSettings column;   // pied sur le toichobate ; height egale en float a porchColumnHeight
	double offset = 0.0;               // distance des axes a l'axe longitudinal
};

struct CellaSettings
{
	double toichobateRise = 0.0;            // dessus du toichobate au-dessus du stylobate
	double frontPteron = 0.0;               // bord avant du stylobate -> face avant du toichobate
	double backPteron = 0.0;                // bord arriere du stylobate -> face arriere du toichobate
	double toichobateOverhang = 0.0;        // debord du toichobate sur le nu exterieur des murs longs
	double longWallThickness = 0.0;         // au nu
	double longWallSpan = 0.0;              // entre nus interieurs
	double wallTop = 0.0;                   // dessus des murs, au-dessus du stylobate
	double frontCrossWallPosition = 0.0;    // face avant du toichobate -> nu avant du mur de refend avant
	double frontCrossWallThickness = 0.0;
	double backCrossWallPosition = 0.0;     // face arriere du toichobate -> nu arriere du mur de refend arriere
	double backCrossWallThickness = 0.0;
	double doorWidth = 0.0;                 // porte du mur de refend avant
	double lintelHeight = 0.0;              // dessous du linteau au-dessus du sol du naos
	double doorJambOffset = 0.0;            // nu interieur du mur Left -> jambage Left de la porte
	double thresholdLength = 0.0;           // le long de X, centre sur la porte
	double thresholdProjection = 0.0;       // saillie devant le nu avant du mur de refend avant
	double thresholdDepth = 0.0;            // sous le mur : egale a frontCrossWallThickness
	double naosFloorHeight = 0.0;           // sol du naos au-dessus du stylobate
	double frontAntaRetreat = 0.0;          // face avant du toichobate -> face des antes avant
	double backAntaRetreat = 0.0;
	double antaInnerProjection = 0.0;       // debord interieur sur le nu interieur du mur
	double antaOuterProjection = 0.0;       // saillie sur le nu exterieur du mur
	double antaHeadLength = 0.0;            // le long du mur
	double porchColumnHeight = 0.0;         // dessus du toichobate -> dessous de l'architrave des porches
	double frontPorchAxis = 0.0;            // distance a l'origine de la ligne des axes du porche avant
	double backPorchAxis = 0.0;
	double porchArchitraveHeight = 0.0;
	double porchFriezeHeight = 0.0;
	double porchEntablatureDepth = 0.0;     // epaisseur de l'architrave et de la frise
	double porchEntablatureFront = 0.0;     // ligne des axes du porche -> face exterieure de l'architrave
	std::optional<GreekPorchColumnsSettings> porches;   // absent : cella sans colonnes de porche
};

// Colonne de porche placee : cote Front (porche avant) ou Back, rang 0 en X < 0,
// 1 en X > 0 ; translation du pied de l'axe.
struct CellaPorchColumn
{
	PeristyleSide side = PeristyleSide::Front;
	int index = 0;
	double x = 0.0, y = 0.0, z = 0.0;
};

// Nus du naos, repere du temple, cotes Z absolues : ceux des pieces de
// BuildGreekCellaPieces, au bit pres (memes expressions en double).
struct CellaNaos
{
	double inner = 0.0;                   // nu interieur des murs longs, X = +-inner
	double outer = 0.0;                   // nu exterieur des murs longs, X = +-outer
	double frontWall = 0.0, front = 0.0;  // mur de refend avant : nu avant (Y < front) et nu arriere (cote naos)
	double back = 0.0, backWall = 0.0;    // mur de refend arriere : nu avant (cote naos) et nu arriere
	double floor = 0.0;                   // dessus du sol du naos
	double wallTop = 0.0;                 // dessus des murs
};

// Architrave et frise des porches, repere du temple, cotes Z absolues : celles
// des pieces de BuildGreekCellaPieces, au bit pres (memes expressions en double).
struct CellaPorchEntablature
{
	double front = 0.0, frontBack = 0.0;   // porche avant : face exterieure (Y < 0) et dos (cote pronaos)
	double backFront = 0.0, back = 0.0;    // porche arriere : dos (cote opisthodome) et face exterieure
	double halfWidth = 0.0;                // faces exterieures des antes, X = +-halfWidth
	double architraveTop = 0.0;            // dessus de l'architrave, dessous de la frise
	double friezeTop = 0.0;                // dessus de la frise
};

// Positions deduites, repere du temple, cotes Z absolues.
struct CellaLayout
{
	CellaNaos naos;
	CellaPorchEntablature porchEntablature;
	double stylobateTop = 0.0;
	double stylobateHalfDepth = 0.0;
	double toichobateTop = 0.0;    // stylobateTop + toichobateRise
	double porchFoot = 0.0;        // double (float (porchColumnHeight)) + toichobateTop
	double top = 0.0;              // cote la plus haute de la cella
	double halfWidth = 0.0;        // demi-largeur du toichobate
	double front = 0.0, back = 0.0;   // Y des faces avant (< 0) et arriere du toichobate
	std::vector<CellaPorchColumn> porchColumns;   // Front 0, Front 1, Back 0, Back 1 ; vide sans porches
	GreekDoricColumnHalfExtents porchColumnExtents;   // nuls sans porches
};

// Contraintes de LayoutGreekCella, refus std::invalid_argument prefixe
// "Cella : " et champ entre apostrophes ('cella.doorWidth'), dans cet ordre :
//   - longueur non finie ou <= 0 ;
//   - toichobate hors du stylobate ('cella.frontPteron', 'cella.backPteron',
//     'cella.toichobateOverhang') ; face exterieure d'ante hors du toichobate
//     ('cella.antaOuterProjection') ; face interieure d'ante au-dela de l'axe
//     ('cella.antaInnerProjection') ;
//   - ordre le long de Y : antes avant, architrave avant, mur de refend avant,
//     mur de refend arriere, architrave arriere, antes arriere, sans contact
//     ('cella.frontCrossWallPosition', 'cella.backCrossWallPosition',
//     'cella.porchEntablatureDepth') ;
//   - cotes : sol du naos au-dessus du toichobate, linteau et pied de
//     l'architrave des porches sous le dessus des murs ('cella.naosFloorHeight',
//     'cella.lintelHeight', 'cella.porchColumnHeight') ;
//   - porte entre les nus interieurs, seuil plus long que la porte et plus court
//     que l'ecart des murs ('cella.doorWidth', 'cella.thresholdLength'), seuil
//     traversant le mur ('cella.thresholdDepth' differente de
//     frontCrossWallThickness), seuil sur le toichobate
//     ('cella.thresholdProjection') ;
//   - avec des colonnes de porche : reglages de la colonne (exceptions de
//     ComputeGreekDoricColumnHalfExtents, telles quelles), hauteur egale en
//     float a porchColumnHeight ('cella.porches.column.height'), pieds sur le
//     toichobate, colonnes disjointes entre elles, des antes, du seuil et des
//     murs de refend ('cella.porches.offset', 'cella.frontPorchAxis',
//     'cella.backPorchAxis').
// Garantit que les pieces de BuildGreekCellaPieces et les colonnes placees ne se
// touchent que par des faces coplanaires (registre CE-8).
CellaLayout LayoutGreekCella (const CellaSettings &settings, const KrepisLayout &krepis);

enum class CellaElement { Toichobate, NaosFloor, LongWall, CrossWall, Threshold, Anta, PorchArchitrave, PorchFrieze };

struct CellaPiece
{
	CellaElement element = CellaElement::Toichobate;
	PeristyleSide side = PeristyleSide::Front;   // Front ou Back (porche, refend, antes), Left ou Right (murs longs)
	int index = 0;                                // antes : 0 en X < 0, 1 en X > 0 ; 0 sinon
	std::unique_ptr<Mesh> mesh;                   // repere du temple, triangles, ferme
};

// Pieces de maconnerie, dans cet ordre : toichobate, sol du naos, murs longs
// Left puis Right, murs de refend Front puis Back, seuil, antes Front 0, Front
// 1, Back 0, Back 1, architrave et frise du porche Front, puis du porche Back.
// Les colonnes des porches ne sont pas construites ici : leurs placements sont
// dans `layout`, qui doit etre celui de LayoutGreekCella sur les memes
// reglages (aucune validation n'est refaite).
std::vector<CellaPiece> BuildGreekCellaPieces (const CellaSettings &settings, const CellaLayout &layout);

} // namespace classical
} // namespace cgarchi
