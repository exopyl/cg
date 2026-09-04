#pragma once
//
//  shape.boolean2d -- composer deux regions AVANT de les extruder.
//
// La brique la plus rentable du chantier « texte imprimable », et celle qu'il
// n'a pas fallu : la mesure des exports de stltext.com a montre que la reference
// n'utilise AUCUN booleen -- elle empile des coques fermees qui s'interpenetrent
// et laisse le slicer les unifier. Le socle n'en a donc pas eu besoin, ni le
// porte-clefs. Ce noeud est un DEPASSEMENT, pas une parite.
//
// Ce qu'il debloque, et qui vaut chacun sa piece :
//
//   - le socle a COQUE UNIQUE : `difference (plaque, texte)` donne le capot d'un
//     socle troue de ses lettres, la ou l'empilement de deux coques laisse une
//     membrane interne (A4 du dossier de faisabilite) ;
//   - le texte GRAVE dans une plaque (A2) ;
//   - le POCHOIR et l'emporte-piece (A5) ;
//   - le trou de vis, la fraisure, le trou de serrure (A6).
//
// ⚠ DEUX DIMENSIONS, PAS TROIS. Il opere sur l'EMPRISE et non sur le volume. Le
// depot n'a aucun booleen sur maillages et ce noeud n'en tient pas lieu : deux
// solides deja extrudes ne se soustraient pas ici. Tant que la piece est un
// empilement 2,5D -- ce qu'un texte imprime est --, composer les contours suffit
// et coute infiniment moins.
//
// ⚠ L'ORDRE COMPTE pour la difference, et pour elle seule : `A` est la matiere,
// `B` l'emporte-piece. Les deux ports sont donc nommes, et pas « entree 1 » et
// « entree 2 ».
//
#include "../../core/node.h"

namespace cggraph_nodes
{

class Boolean2dNode : public cggraph::Node
{
public:
	Boolean2dNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

} // namespace cggraph_nodes
