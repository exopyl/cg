#pragma once
//
//  Deux operations sur des CONTOURS, en amont de l'extrusion.
//
// Elles vivent avant `shape.extrude` et non apres, et c'est tout leur interet :
// une region 2D se decale, se borne, se compose pour presque rien, la ou la
// meme chose sur un maillage demanderait un booleen 3D que le depot n'a pas.
//
// ---------------------------------------------------------------------------
//  shape.contours.offset -- la PLAQUE SILHOUETTE, et le halo en general
// ---------------------------------------------------------------------------
//
// Dilate ou retrecit la region. Une plaque qui suit la forme des lettres a
// distance constante, c'est ce noeud avec un `delta` positif, puis une extrusion
// sur sa propre plage de Z.
//
// ⚠ CONNEXITE NON GARANTIE, et c'est LE piege de cette forme. Le halo ne relie
// deux lettres que si `delta` vaut au moins la moitie de l'espace qui les
// separe ; en dessous, il rend une plaque PAR LETTRE et la piece sort en
// morceaux. Geometriquement correct, pratiquement inutilisable. Le noeud publie
// donc le nombre de morceaux (`GetPieceCount`), pour que la chose soit SUE avant
// l'impression et non decouverte apres.
//
// ⚠ Les contre-formes se REFERMENT : dilater vers l'exterieur retrecit les
// trous d'autant, et le centre du `o` disparait des que `delta` atteint sa
// demi-largeur. C'est voulu pour une plaque de FOND -- sinon on verrait a
// travers --, et rien d'autre n'est propose ici.
//
// ---------------------------------------------------------------------------
//  shape.contours.plate -- la plaque RECTANGULAIRE
// ---------------------------------------------------------------------------
//
// L'emprise des contours d'entree, plus une marge, coins eventuellement
// arrondis. C'est la forme de socle qu'offre stltext.com (une boite de douze
// triangles, cf. la mesure du §1 bis dans
// src/cgmesh/docs/text3d_print_module_feasibility.md).
//
// ⚠ CE N'EST PAS `TextExtrudeOptions::Support`, et il ne faut pas les confondre.
// Ce support-la est un contour de plus jete dans l'union des glyphes, donc a la
// MEME profondeur d'extrusion que les lettres -- ce qui fait disparaitre les
// lettres dans la silhouette de la plaque a profondeur egale. Ici la plaque sort
// SEULE, en dehors de toute union, pour etre extrudee sur sa propre plage de Z
// et fusionnee ensuite (`mesh.merge`). C'est precisement la contrainte que
// text_extrude.h annonce sans l'adoucir, et que ce chemin leve.
//
// ORIENTATION -- les deux noeuds la rendent dans la convention de Clipper2
// (enveloppes en sens positif, trous a l'inverse). Pour la plaque, le sens est
// aligne sur le plus grand contour d'entree : une plaque tracee a l'envers des
// lettres les SOUSTRAIRAIT au lieu de porter de la matiere, et le meme graphe
// rendrait un socle avec une police et un pochoir avec une autre.
//
#include <atomic>

#include "../../core/node.h"

namespace cggraph_nodes
{

class ContourOffsetNode : public cggraph::Node
{
public:
	ContourOffsetNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;

	// Morceaux distincts du dernier resultat. 1 = une seule piece ; au-dela, le
	// halo n'a pas relie les formes (cf. l'en-tete).
	unsigned int GetPieceCount () const { return m_pieces.load (); }

	// Le compte est le GARDE-FOU de la silhouette : il n'a de valeur que s'il
	// atteint l'ecran (cf. Node::PublishStats).
	void PublishStats (std::vector<cggraph::NodeStat> &out) const override;

private:
	std::atomic<unsigned int> m_pieces { 0 };
};

class ContourPlateNode : public cggraph::Node
{
public:
	ContourPlateNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

} // namespace cggraph_nodes
