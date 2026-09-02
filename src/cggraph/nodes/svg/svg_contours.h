#pragma once
//
//  SvgContours -- un fichier SVG devient un contour 2D.
//
// Second producteur de `cgmesh.ExtrudeContours`, avec text.contours. Il fait
// entrer dans le graphe une capacite qui n'y etait PAS : l'extrusion SVG
// n'existait que par createSvgExtrusion, dans la facade web, sans noeud.
//
// ⚠ LA REGLE DE REMPLISSAGE de chaque forme -- even-odd ou non-zero -- est
// resolue par svg_to_contours, en amont de ce noeud. Une liste plate de contours
// ne peut pas la transporter, et elle n'est connue nulle part ailleurs : les
// contours qui sortent d'ici sont donc deja des REGIONS, orientees pour NonZero.
//
// Le chemin arrive par un PORT, comme pour les trois chargeurs : c'est ce qui
// permet a un file.ref amont de porter l'identite du fichier, donc de rendre le
// document reproductible et le cache correct.
//
#include "../../core/node.h"

namespace cggraph_nodes
{

class SvgContoursNode : public cggraph::Node
{
public:
	SvgContoursNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

} // namespace cggraph_nodes
