#pragma once
//
//  SvgExtrudeColored -- un fichier SVG devient un solide PEINT, en un noeud.
//
// MONOLITHIQUE, et c'est un choix assume : contours, marqueterie, extrusion et
// palette en un seul calcul. Le decoupage habituel du catalogue -- un producteur
// de contours, un extrudeur partage -- ne peut pas porter cette chaine : une
// liste plate d'`ExtrudeContour` ne transporte ni la couleur d'une forme, ni son
// rang, ni la frontiere entre deux formes voisines. La porter demanderait un
// type de valeur SERIALISE de plus, donc un nom irreversible, pour une
// composition dont personne n'a encore l'usage.
//
// `svg.contours` n'est PAS remplace : les documents qui l'enchainent avec
// `shape.extrude` continuent de s'ouvrir et de se calculer. Ce noeud est une
// seconde voie, pas une migration.
//
// ⚠ CE QUE `useSvgColors` GOUVERNE, et pourquoi il gouverne DEUX choses. La
// couleur seule ne suffit pas : le SVG est un ordre de peintre, ou une forme
// recouvre celles du dessous. Peindre sans retirer la matiere cachee laisserait
// deux capots coplanaires et superposes -- le rendu les fait clignoter, et le
// volume compte deux fois la meme matiere. La bascule met donc a la fois
// `perShapeMaterials` et `overlapPolicy` (cf. import_svg.h). L'API C++ garde les
// deux separes, parce que la marqueterie a un sens sans palette et que la
// non-regression se verifie l'une sans l'autre.
//
// ⚠ LES COULEURS NE FRANCHISSENT AUCUN EXPORT. Le STL n'en porte pas -- le
// format n'en a pas -- et l'OBJ rendu par maker est minimal, sans mtllib. Elles
// sont VISIBLES et non exportables ; c'est le meme constat que pour `mesh.color`.
//
#include <atomic>

#include "../../core/node.h"

namespace cggraph_nodes
{

class SvgExtrudeColoredNode : public cggraph::Node
{
public:
	SvgExtrudeColoredNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;

	void PublishStats (std::vector<cggraph::NodeStat> &out) const override;

private:
	// Ce que le document PERD ou que le calcul a change, publie pour que cela se
	// constate au lieu de se supposer.
	std::atomic<unsigned int> m_gradientShapes { 0 };
	std::atomic<unsigned int> m_hiddenShapes { 0 };
	std::atomic<unsigned int> m_subtractedShapes { 0 };
	std::atomic<unsigned int> m_materials { 0 };
};

} // namespace cggraph_nodes
