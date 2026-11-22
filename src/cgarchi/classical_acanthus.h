#pragma once

#include <memory>
#include <vector>

#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_profile.h>
#include "mesh.h"

namespace cgarchi::classical
{

// Niveau de detail des ornements du chapiteau. None : aucun ornement.
enum class OrnamentDetail { None, Light, Heavy };

// Rang ou famille d'une piece d'ornement du chapiteau corinthien (lettres de
// la planche XXV de Vignole).
enum class CapitalPieceKind
{
	LowerLeaf,    // F · foglie di sotto
	MiddleLeaf,   // E · foglie di mezzo
	MinorLeaf,    // D · foglie minori
	Cauliculus,   // C · caulicoli : tige
	Volute,       // C : volute d'angle
	Helix,        // C : helice
	Fleuron       // G · fiore
};

struct AcanthusOptions
{
	OrnamentDetail detail = OrnamentDetail::Light;
	double scale = 1.0;           // unites monde par partie de module ; fini, > 0
	// Secteurs du tour qui portera l'ornement (LatheOptions::radialSegments ; un
	// profil cannele prend la grille de ses cannelures) : le dos des fleurons se
	// cale sur la corde de ce maillage.
	int    radialSegments = 48;
};

// Plage d'une piece dans CapitalOrnament::mesh, intervalles semi-ouverts. Les
// faces d'une piece ne referencent que les sommets de sa plage.
struct CapitalPiece
{
	CapitalPieceKind kind = CapitalPieceKind::LowerLeaf;
	unsigned faceBegin = 0, faceEnd = 0;
	unsigned vertexBegin = 0, vertexEnd = 0;
};

// Ornements du chapiteau en un seul maillage ; `pieces` couvre ses faces et ses
// sommets, contigus, dans l'ordre de construction. `mesh` n'est jamais nul.
struct CapitalOrnament
{
	std::unique_ptr<Mesh>      mesh;
	std::vector<CapitalPiece> pieces;
};

// Feuilles d'acanthe des rangs F (foglie di sotto) et E (foglie di mezzo),
// tiges des caulicoles (C), feuilles D (foglie minori), volutes et helices des
// caulicoles autour de la corbeille
// corinthienne, repere du tour (axe Z, y du profil fois `scale`) : 8 pieces par
// rang F, E, C, F et C a pi/8 + k pi/4, E a k pi/4 (detail::RankTheta, phase
// des planches XXV et XXVI de Vignole), et deux feuilles D par tige, D interieur
// vers l'axe de face et D exterieur vers la corne (detail::MinorLeafSide) ;
// une volute (vers la corne) et une helice (vers l'axe de face) par tige
// (detail::BuildVoluteRibbon, DefaultVoluteSpec, DefaultHelixSpec) ; un
// fleuron G au milieu de chaque face de l'abaque, theta = k pi/2
// (detail::BuildFleuron, DefaultFleuronSpec), dos cale sur le tour du
// profil a options.radialSegments.
// Cotes de la corbeille lues dans les membres d'ornement du chapiteau
// (detail::ReadCapitalEnvelope) ; F et E sont le gabarit Olive du niveau plaque
// sur sa porteuse (detail::BuildLeafSheet), D le gabarit OliveMinor
// (detail::DefaultMinorLeafPlacement), chaque tige un tube cannele
// (detail::BuildCauliculusSheet). F, E et C : une piece par rang est construite
// puis copiee par rotation de k pi/4 ; D : les quatre poses des tiges 0 et 1
// sont construites puis copiees par rotation de pi/2 ; volutes et helices : les
// pieces des tiges 0 et 1 (images miroir par theta = pi/4) sont construites puis
// copiees par rotation de pi/2 ; fleurons : celui de la face 0 est construit
// puis copie par rotation de k pi/2. `pieces` : 8 F, 8 E, 8 C, theta croissant,
// puis 16 D, tige k croissante, D interieur puis D exterieur, puis 8 volutes et
// 8 helices, tige k croissante, puis 4 G, theta croissant ; une piece F, E, C ou
// D est une coque fermee ; une volute ou une helice, deux (ruban, puis oeil) ; un
// fleuron G, deux (rosace, puis tige) ; ses sommets et ses faces dans l'ordre de
// sa construction. Light et Heavy different par les gabarits et l'echantillonnage
// des tiges ; None ne construit rien mais valide le profil comme les autres.
// Leve std::invalid_argument si l'ordre n'est pas corinthien, si les options
// sont hors domaine (scale non fini ou <= 0, detail hors de l'enumeration) ou
// si le profil ne porte pas les membres du chapiteau, quel que soit `detail`.
CapitalOrnament BuildCorinthianOrnament (const OrderData &order, const ColumnProfile &profile,
                                         const AcanthusOptions &options = {});

} // namespace cgarchi::classical
