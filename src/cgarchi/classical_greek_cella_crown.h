#pragma once

#include <memory>
#include <vector>

#include <cgarchi/classical_greek_cella.h>
#include <cgarchi/classical_peristyle.h>

class Mesh;

namespace cgarchi
{
namespace classical
{

// ============================================================================
//  Couronnement de la cella : assise XVII exterieure (epicranitis des murs et
//  couronnement de la frise des porches) et prolongement est de l'entablement
//  des porches au-dessus des portiques lateraux
// ============================================================================
//
// Repere du temple (classical_temple.h, D2-5) : X le long de la facade, Y en
// profondeur, facade avant en Y < 0 ; cotes z comptees depuis le dessus du
// stylobate, Z = stylobateTop + z. Unites : celles de l'appelant. Les reglages
// sont des longueurs de segments, sans signe, comptees depuis les nus de la
// cella (CellaNaos, CellaPorchEntablature) ; les positions signees en sont
// deduites ici. Registre CE-23 et suivantes (classical_orders_implementation.md
// section 10.16).
//
// Dessus commun : top = naos.wallTop + height, calcule une fois ; il couronne
// les murs (sur leur dessus) et la frise des porches (sur son dessus,
// CellaPorchEntablature::friezeTop). Profil de chaque face moulee, de bas en
// haut : bande a meandre en saillie bandProjection sur le nu, puis filet et
// listel, de hauteur filletHeight + listelHeight, en saillie listelProjection
// (le filet a la saillie du listel, registre CE-25) ; la bande prend le reste de
// la hauteur, du pied de l'assise a top - (listelHeight + filletHeight).
//
// Pieces, toutes fermees : un corps lisse par assise (boite), puis une bande
// moulee par face moulee, prisme du profil le long de la face, d'onglet a 45
// degres aux angles rentrants, a bout droit ailleurs :
//   - epicranitis des murs longs, Left et Right : de la face interieure
//     (naos.inner + innerOffset, face de l'assise XVII interieure du naos) au nu
//     exterieur, du dos de l'architrave du porche avant a celui du porche
//     arriere ;
//   - face interieure de l'assise sur les murs longs des porches, Front et Back,
//     rang 0 en X < 0, 1 en X > 0 : du nu interieur a la face interieure, du dos
//     de l'architrave du porche a la face de l'assise XVII interieure du mur de
//     refend (naos.front - innerOffset, naos.back + innerOffset) ;
//   - assise des murs de refend, Front et Back : entre les nus interieurs des
//     murs longs, de la face cote porche du mur de refend a la face de l'assise
//     XVII interieure ;
//   - couronnement de la frise de chaque porche, Front et Back : d'une face
//     exterieure d'ante a l'autre, sur toute l'epaisseur de la frise, du dessus
//     de la frise a top ;
//   - bandes moulees (registre CE-26) : nus exterieurs des murs longs ; dans
//     chaque porche, face exterieure et face cote porche du couronnement, face
//     cote porche du mur de refend, nus interieurs des murs longs ; au dos du
//     couronnement, hors des murs, du nu exterieur des murs a la face exterieure
//     des antes. Bouts des couronnements coupes au droit des faces exterieures
//     des antes, sans retour (registre CE-27).

struct EpicranitisSettings
{
	double height = 0.0;             // assise XVII exterieure, du dessus des murs a son dessus
	double innerOffset = 0.0;        // nu interieur des murs du naos -> face interieure de l'epicranitis
	double listelHeight = 0.0;       // listel, en haut
	double filletHeight = 0.0;       // filet, sous le listel
	double listelProjection = 0.0;   // saillie du listel et du filet sur le nu
	double bandProjection = 0.0;     // saillie de la bande a meandre sur le nu
};

// Positions deduites, repere du temple, cotes Z absolues.
struct EpicranitisLayout
{
	CellaNaos             naos;          // ceux de la cella
	CellaPorchEntablature porch;         // ceux de la cella
	double innerFace = 0.0;              // face interieure de l'epicranitis, X = +-(naos.inner + innerOffset)
	double frontCourse = 0.0;            // naos.front - innerOffset
	double backCourse = 0.0;             // naos.back + innerOffset
	double top = 0.0;                    // naos.wallTop + height
	double step = 0.0;                   // dessus de la bande : top - (listelHeight + filletHeight)
	// Emprise des pieces : |X| <= halfWidth, front <= Y <= back.
	double halfWidth = 0.0, front = 0.0, back = 0.0;
};

// Contraintes de LayoutGreekEpicranitis, refus std::invalid_argument prefixe
// "Couronnement : " et champ entre apostrophes ('epicranitis.height'), dans cet
// ordre :
//   - longueur non finie ou <= 0 ;
//   - face interieure hors de l'epaisseur d'un mur long ou d'un mur de refend
//     ('epicranitis.innerOffset') ;
//   - dessus au plus au dessus de la frise des porches ('epicranitis.height') ;
//   - listel et filet d'au moins la hauteur de l'assise sur la frise ou sur les
//     murs, qui ne laissent pas de bande ('epicranitis.listelHeight') ;
//   - saillie la plus forte du profil (bande ou listel, champ nomme) qui porte le
//     bout d'onglet d'une bande au dos du couronnement sur la face exterieure des
//     antes ou au-dela, puis qui fait se rejoindre ou se croiser les bouts
//     d'onglet opposes d'un porche ; les bouts sont compares en float, comme les
//     sommets des bandes, si bien que BuildGreekEpicranitisPieces ne leve pas le
//     std::logic_error de detail::StripMesh.
// Garantit que les pieces de BuildGreekEpicranitisPieces ne se touchent, et ne
// touchent la cella et l'assise XVII interieure du naos d'epaisseur innerOffset,
// que par des faces coplanaires. Le controle contre l'anneau de l'entablement et
// l'interieur est fait par la mise en place du temple (classical_temple.h).
EpicranitisLayout LayoutGreekEpicranitis (const EpicranitisSettings &settings, const CellaLayout &cella);

enum class EpicranitisElement { Course, PorchCourse, CrossCourse, Crown, Moulding };

struct EpicranitisPiece
{
	EpicranitisElement element = EpicranitisElement::Course;
	PeristyleSide side = PeristyleSide::Left;
	int index = 0;
	std::unique_ptr<Mesh> mesh;   // repere du temple, triangles, ferme
};

// Pieces, dans cet ordre (26) : epicranitis Left, Right (Course) ; faces
// interieures des murs longs des porches Front 0, Front 1, Back 0, Back 1
// (PorchCourse) ; murs de refend Front, Back (CrossCourse) ; couronnements de la
// frise Front, Back (Crown) ; bandes moulees (Moulding) : nus exterieurs des
// murs longs Left 0, Right 0, puis, pour le porche Front puis Back, rangs 0 a 6 :
// face exterieure du couronnement, face cote porche du couronnement, mur de
// refend, nu interieur du mur long en X < 0, en X > 0, dos du couronnement hors
// des murs en X < 0, en X > 0. `layout` doit etre celui de
// LayoutGreekEpicranitis sur les memes reglages (aucune validation n'est
// refaite).
std::vector<EpicranitisPiece> BuildGreekEpicranitisPieces (const EpicranitisSettings &settings,
                                                           const EpicranitisLayout &layout);

// ----------------------------------------------------------------------------
//  Prolongement est
// ----------------------------------------------------------------------------
//
// Architrave, frise et couronnement du porche avant (Front) prolonges de chaque
// cote, Left (X < 0) et Right (X > 0), de la face exterieure des antes a la face
// interieure de l'anneau de l'entablement du peristyle : meme epaisseur et memes
// dessus que l'entablement du porche, couronnement au profil de l'epicranitis,
// moule sur ses deux faces, bouts droits. Pied de l'architrave : celui de
// l'anneau (entablatureFoot), sur les abaques du peristyle (registre CE-29).

struct PorchExtensionSettings
{
	double eastSpan = 0.0;   // face exterieure des antes -> face interieure de l'anneau, controle de fermeture
};

struct PorchExtensionLayout
{
	double inner = 0.0;   // bout cote antes, |X| = porch.halfWidth
	double outer = 0.0;   // bout cote anneau, |X| = face interieure de l'anneau
	double foot = 0.0;    // dessous de l'architrave : entablatureFoot
};

// Ecart admis entre la face exterieure des antes plus eastSpan et la face
// interieure de l'anneau : la demi-unite du millimetre ecrit au README.
constexpr double kPorchExtensionClosure = 5e-4;

// Contraintes, refus std::invalid_argument prefixe "Prolongement : " et champ
// entre apostrophes, dans cet ordre : eastSpan non finie ou <= 0
// ('porchExtension.eastSpan') ; face interieure de l'anneau au plus a la face
// exterieure des antes, comparees en float ('porchExtension') ; eastSpan qui ne
// ferme pas l'ecart de l'une a l'autre a kPorchExtensionClosure pres, valeur
// ecrite dans le message ('porchExtension.eastSpan') ; pied au moins au dessus de
// l'architrave des porches ('porchExtension').
// `ringInnerHalfWidth` : la demi-largeur de la face interieure de l'anneau, par
// l'expression de SweepRectangularRing (halfWidth + (-innerFace)) ;
// `entablatureFoot` : le dessous de l'anneau.
PorchExtensionLayout LayoutGreekPorchExtension (const PorchExtensionSettings &settings, const EpicranitisLayout &crown,
                                               double ringInnerHalfWidth, double entablatureFoot);

enum class PorchExtensionElement { Architrave, Frieze, Crown, Moulding };

struct PorchExtensionPiece
{
	PorchExtensionElement element = PorchExtensionElement::Architrave;
	PeristyleSide side = PeristyleSide::Left;   // Left (X < 0) ou Right (X > 0)
	int index = 0;                               // bande : 0 face exterieure, 1 dos
	std::unique_ptr<Mesh> mesh;                  // repere du temple, triangles, ferme
};

// Pieces, dans cet ordre (10) : pour Left puis Right, architrave, frise, corps du
// couronnement, bande de la face exterieure, bande du dos. Le profil est celui
// de `crown` et de `settings` (EpicranitisSettings) ; les layouts doivent etre
// ceux des mises en place sur les memes reglages.
std::vector<PorchExtensionPiece> BuildGreekPorchExtensionPieces (const EpicranitisSettings &settings,
                                                                 const EpicranitisLayout &crown,
                                                                 const PorchExtensionLayout &layout);

} // namespace classical
} // namespace cgarchi
