#pragma once

#include <array>
#include <memory>
#include <vector>

#include <cgarchi/classical_greek_cella_crown.h>
#include <cgarchi/classical_greek_pediment.h>
#include <cgarchi/classical_peristyle.h>

class Mesh;

namespace cgarchi
{
namespace classical
{

// ============================================================================
//  Plafonds du peristyle de l'Hephaisteion : pteron est (poutres, corniche a
//  anthemion, bandeau, dalles a caissons) et corniche du porche ouest
// ============================================================================
//
// Repere du temple (classical_temple.h, D2-5) : X le long de la facade, Y en
// profondeur, facade avant (est) en Y < 0 ; cotes Z absolues. Les reglages sont
// des longueurs de segments, sans signe, comptees depuis des nus deja definis
// (couronnement de la cella, face interieure de l'anneau de l'entablement) ; les
// positions signees en sont deduites ici. Registre CE-32 et suivantes
// (classical_orders_implementation.md section 10.16).
//
// Toute expression a x b + c passe par un produit arrondi en double (volatile),
// jamais contracte en FMA : memes positions sur toute cible.

// Caissons d'une dalle (CE-37) : grille carree au pas `pitch`, ouverture carree
// `opening` centree dans la case, fond en retrait de `frame` sur chaque bord de
// l'ouverture ; deux ressauts de depth / 2 (perles omises).
struct CofferSettings
{
	double pitch = 0.0, opening = 0.0, frame = 0.0, depth = 0.0;
};

// Corniche a anthemion et bandeau des fronts des porches (CE-36), blocs
// rectangulaires : corniche de hauteur `height`, en saillie `projection` sur le
// nu de la frise ; bandeau de hauteur `bandHeight`, au nu ; tous deux de
// profondeur `depth` vers l'interieur du porche.
struct PorchCorniceSettings
{
	double height = 0.0, projection = 0.0, bandHeight = 0.0, depth = 0.0;
};

struct EastCeilingSettings
{
	int beamCount = 0;                // poutres paralleles a l'axe longitudinal, centrees sur lui (CE-35)
	double beamSpacing = 0.0;         // d'axe en axe
	double beamWidth = 0.0;
	double beamHeight = 0.0;          // du dessus du couronnement au dessus des poutres
	double beamWestBearing = 0.0;     // nu de la frise du porche avant -> bout ouest des poutres
	double endMassifWidth = 0.0;      // face interieure de l'anneau -> plan du massif de bout (non construit, CE-40)
	PorchCorniceSettings cornice;
	CofferSettings coffers;
	int cofferRows = 0;               // rangs de caissons par travee, en travers (X)
};

// Positions deduites, repere du temple, cotes Z absolues.
struct EastCeilingLayout
{
	EpicranitisLayout crown;                    // celui du couronnement (copie)
	std::vector<double> beamAxes;               // X croissants : (k - (n - 1) / 2) beamSpacing, un seul produit
	std::vector<std::array<double, 2>> bays;    // [X0, X1], n + 1 travees, de -massif a +massif
	double massif = 0.0;                        // |X| du plan du massif : ringInnerHalfWidth - endMassifWidth
	double westEnd = 0.0, eastEnd = 0.0;        // Y des bouts des poutres : porch.front + beamWestBearing ; -ringInnerHalfDepth
	double beamFoot = 0.0, beamTop = 0.0;       // crown.top ; crown.top + beamHeight
	double corniceTop = 0.0, bandTop = 0.0;     // crown.top + cornice.height ; corniceTop + cornice.bandHeight
	int cofferColumns = 0;                      // caissons le long de Y : floor ((porch.front - eastEnd) / pitch)
	RoofPocket eastPocket, westPocket;          // poches du comble (CE-34)
};

// Contraintes de LayoutGreekEastCeiling, refus std::invalid_argument prefixe
// "Plafond est : " et champ entre apostrophes ('eastCeiling.beamSpacing'), dans
// cet ordre :
//   1. longueur non finie ou <= 0, compte < 1 ;
//   2. 'beamSpacing' <= 'beamWidth' ('beamSpacing') ;
//   3. face exterieure de la poutre extreme au plan du massif ou au-dela,
//      comparees en float ('beamSpacing') ;
//   4. massif au plus a la face exterieure des antes ('endMassifWidth') ;
//   5. 'beamWestBearing', puis 'corniceDepth', plus grand que l'epaisseur de
//      l'entablement des porches ;
//   6. 'beamHeight' au plus la hauteur de la corniche et du bandeau ;
//   7. 'cofferDepth' : cotes du pied de la dalle, des deux ressauts et du dessus
//      non strictement croissantes une fois arrondies en float (profondeur au
//      moins l'epaisseur de la dalle, ou ressaut confondu) ;
//   8. 'cofferOpening' au moins le pas ; puis deux 'cofferFrame' au moins
//      l'ouverture ;
//   9. 'cofferRows' fois le pas plus grand que la travee la plus etroite ;
//  10. aucun caisson dans la longueur libre, du nu de la frise du porche avant a
//      la face interieure de l'anneau ('cofferPitch') ;
//  11. 'corniceProjection' au moins cette longueur libre ;
//  12. deux lignes voisines de la grille des caissons (en travers de chaque
//      travee, le long de la longueur libre) confondues une fois arrondies en
//      float : 'cofferFrame' si un fond ou un cadre s'ecrase, 'cofferOpening'
//      si un plein s'ecrase ; detail::GridSolidMesh ne leve donc pas.
// `crown` : la mise en place du couronnement ; `ringInnerHalfWidth`,
// `ringInnerHalfDepth` : la face interieure de l'anneau, par l'expression de
// SweepRectangularRing.
EastCeilingLayout LayoutGreekEastCeiling (const EastCeilingSettings &settings, const EpicranitisLayout &crown,
                                         double ringInnerHalfWidth, double ringInnerHalfDepth);

// Plafonds des pterons nord et sud (CE-38, CE-39, CE-42) : poutres transversales,
// paralleles a X, de part et d'autre de l'axe transversal (Y = 0), et dalles
// entre leurs faces. Chaque poutre va de la face interieure de l'epicranitis
// (wallEnd, |X| = crown.innerFace) au bout cote peristyle (peristyleEnd, a
// peristyleGap de la face interieure de l'anneau), en console sur l'epicranitis
// ; pied sur le dessus du couronnement, dessus a crown.top + beamHeight. Dalles
// du dessus du bandeau du plafond est (slabFoot) au dessus des poutres, caissons
// de la grille du plafond est.
struct FlankCeilingSettings
{
	double beamSpacing = 0.0, firstBeamOffset = 0.0;
	int frontBeamCount = 0, backBeamCount = 0;   // README : est, ouest
	double beamWidth = 0.0, beamHeight = 0.0;
	double peristyleGap = 0.0;                   // face interieure de l'anneau -> bout cote peristyle (CE-42)
	int cofferRows = 0;
};

struct FlankCeilingLayout
{
	std::vector<double> beamAxes;                // Y croissants : -(f + k s) pour k = front - 1 .. 0, puis f + k s
	double wallEnd = 0.0;                        // |X| : crown.innerFace
	double peristyleEnd = 0.0;                   // |X| : ringInnerHalfWidth - peristyleGap
	double naosOuter = 0.0;                      // |X| : nu exterieur des murs longs, debut de la course des caissons
	double beamFoot = 0.0, beamTop = 0.0;        // crown.top ; crown.top + beamHeight
	double slabFoot = 0.0;                       // bandTop du plafond est
	int cofferColumns = 0;                       // le long de X : floor ((peristyleEnd - naosOuter) / pitch)
	std::array<RoofPocket, 2> pockets;           // Left (X < 0), Right : wallEnd a peristyleEnd, faces des poutres extremes
};

// Contraintes de LayoutGreekFlankCeilings, refus std::invalid_argument prefixe
// "Plafonds des flancs : " et champ entre apostrophes
// ('flankCeilings.beamSpacing'), dans cet ordre :
//   1. longueur non finie ou <= 0, compte < 1 (eastBeamCount, westBeamCount,
//      cofferRows : noms du document) ;
//   2. 'beamSpacing' <= 'beamWidth' ('beamSpacing') ;
//   3. 'firstBeamOffset' <= beamWidth / 2 : les deux poutres centrales se
//      toucheraient ou se recouvriraient ('firstBeamOffset') ;
//   4. face de la derniere poutre est au contact du dos de l'entablement du
//      porche avant ou au-dela, de meme a l'ouest contre celui du porche arriere,
//      comparees en float ('eastBeamCount', 'westBeamCount', CE-39) ;
//   5. bout cote peristyle au plus au nu exterieur des murs longs, en float
//      ('peristyleGap' : ne laisse pas de portee au-dela du nu exterieur des
//      murs) ;
//   6. cotes du pied des dalles (dessus du bandeau du plafond est), des deux
//      ressauts et du dessus des poutres non strictement croissantes en float
//      ('beamHeight' : pas de dalle sous les caissons) ;
//   7. aucun caisson dans [nu exterieur des murs, bout cote peristyle]
//      ('peristyleGap') ;
//   8. 'cofferRows' fois le pas plus grand que la travee la plus etroite :
//      beamSpacing - beamWidth, ou 2 firstBeamOffset - beamWidth pour la travee
//      centrale ; puis deux lignes voisines de la grille des caissons
//      confondues en float ('cofferRows', le champ du plafond est nomme dans
//      le message).
// `east`, `eastLayout` : le plafond est mis en place (couronnement, grille et
// profondeur des caissons, dessus du bandeau) ; `ringInnerHalfWidth` : la face
// interieure de l'anneau.
FlankCeilingLayout LayoutGreekFlankCeilings (const FlankCeilingSettings &settings, const EastCeilingSettings &east,
                                            const EastCeilingLayout &eastLayout, double ringInnerHalfWidth);

enum class CeilingElement { Beam, Cornice, Band, Slab };

struct CeilingPiece
{
	CeilingElement element = CeilingElement::Beam;
	PeristyleSide side = PeristyleSide::Front;
	int index = 0;
	std::unique_ptr<Mesh> mesh;   // repere du temple, triangles, ferme
};

// Pieces du pteron est et du porche ouest, dans cet ordre (33 avec 7 poutres) :
//   - poutres (Beam, Front, rang par X croissants), boites : X axe +- largeur / 2,
//     Y [eastEnd, westEnd], Z [beamFoot, beamTop] ;
//   - corniches (Cornice, Front, rang de la travee), une par travee : X de la
//     travee, Y [porch.front - projection, porch.front + depth], Z [crown.top,
//     corniceTop] ;
//   - bandeaux (Band, Front, rang de la travee) : Y [porch.front, porch.front +
//     depth], Z [corniceTop, bandTop] ;
//   - corniche (Cornice, Back, 0) et bandeau (Band, Back, 0) du porche ouest,
//     d'une face exterieure d'ante a l'autre : Y [porch.back - depth, porch.back
//     + projection] et [porch.back - depth, porch.back] ;
//   - dalles (Slab, Front, rang de la travee), une par travee, solide de
//     detail::GridSolidMesh (CE-38) : X de la travee, Y [eastEnd, westEnd],
//     Z [bandTop, beamTop] ; `cofferRows` rangs de caissons en X, centres dans la
//     travee, et `cofferColumns` caissons le long de Y, centres dans
//     [eastEnd, porch.front].
// `layout` doit etre celui de LayoutGreekEastCeiling sur les memes reglages
// (aucune validation n'est refaite).
std::vector<CeilingPiece> BuildGreekEastCeilingPieces (const EastCeilingSettings &settings, const EastCeilingLayout &layout);

// Pieces des pterons nord et sud, 58 avec 7 et 8 poutres par cote : pour Left
// (X < 0), puis Right, les poutres (Beam, cote, rang par Y croissants), boites :
// X de wallEnd a peristyleEnd, Y axe +- largeur / 2, Z [beamFoot, beamTop] ; puis
// les dalles (Slab, cote, rang par Y croissants), une entre chaque paire de
// poutres voisines, de leurs faces, X de wallEnd a peristyleEnd, Z [slabFoot,
// beamTop], `cofferRows` rangs de caissons centres dans la travee et
// `cofferColumns` caissons le long de X centres dans [naosOuter, peristyleEnd].
// Aucune dalle au-dela des poutres extremes. `layout` doit etre celui de
// LayoutGreekFlankCeilings sur les memes reglages.
std::vector<CeilingPiece> BuildGreekFlankCeilingPieces (const FlankCeilingSettings &settings, const EastCeilingSettings &east,
                                                        const FlankCeilingLayout &layout);

} // namespace classical
} // namespace cgarchi
