#pragma once
//
//  Fusion de maillages -- deux solides, une seule piece a exporter.
//
// LE NOEUD QUE L'EMPILEMENT 2,5D ATTENDAIT. Un socle et des lettres en relief ne
// partagent pas leur plage de Z : ce sont deux extrusions, donc deux maillages,
// et il faut bien les rendre en UN. C'est ce que fait ce noeud, et rien de plus.
//
// ⚠ CE N'EST PAS UNE UNION BOOLEENNE, et la difference est de fond. Les deux
// coques sont CONCATENEES : si elles s'interpenetrent, les faces internes
// restent, et la piece n'est pas une variete au sens strict. Ce n'est pas un
// pis-aller subi -- c'est exactement ce que produit stltext.com (mesure dans
// src/cgmesh/docs/text3d_print_module_feasibility.md, §1 bis : socle de 12
// triangles interpenetrant les lettres de 0,75 mm), et ce que tout slicer
// unifie sans broncher quand on pose deux corps l'un sur l'autre.
//
// Le depot n'a AUCUN booleen 3D sur maillages, et ce noeud n'en est pas un
// deguise. La coque unique etanche s'obtient autrement : en soustrayant en 2D
// l'emprise des lettres au capot du socle, donc en amont, sur les contours.
//
// SECONDE ENTREE OPTIONNELLE, et c'est ce qui rend le noeud utilisable dans un
// graphe FIXE : une page qui propose « socle : aucun » ne peut pas debrancher un
// noeud, elle peut seulement ne rien lui donner. Sans plaque, la fusion rend les
// lettres telles quelles au lieu de faire echouer l'evaluation.
//
#include "../../core/node.h"

namespace cggraph_nodes
{

class MergeMeshNode : public cggraph::Node
{
public:
	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

} // namespace cggraph_nodes
