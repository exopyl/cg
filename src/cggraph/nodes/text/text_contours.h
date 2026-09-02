#pragma once
//
//  TextContours -- une police et un texte deviennent un contour 2D.
//
// Premier des deux producteurs de `cgmesh.ExtrudeContours`, avec svg.contours.
// Il remplace la moitie amont de l'ancien text.extrude, monolithique, qui
// enchainait mise en page, aplatissement et extrusion sans que rien ne puisse
// s'intercaler.
//
// ⚠ LES GLYPHES SONT TOUJOURS FUSIONNES en une region unique (Clipper2,
// NonZero), la ou l'ancien noeud ne le faisait que sur demande. Le motif est le
// TYPE de sortie : une liste plate de contours ne peut pas porter le decoupage
// par glyphe dont l'ancien chemin se servait pour extruder lettre par lettre, et
// extruder d'un seul tenant des lettres non fusionnees laisserait des murs
// internes la ou elles se touchent. Le prix est une passe Clipper2 meme sur un
// texte sans chevauchement.
//
// LA PLAQUE DE SUPPORT est ici et non sur l'extrudeur, parce qu'elle est un
// CONTOUR de plus, fondu aux lettres par la meme union. La porter en aval
// demanderait de deplacer l'union, ce qui la generaliserait au SVG -- un autre
// chantier.
//
// ⚠ Support et lettres partagent la PROFONDEUR, qui se regle sur shape.extrude :
// un socle plus epais que les lettres exige un booleen 3D, capacite que le depot
// n'a pas.
//
#include <atomic>

#include "../../core/node.h"

namespace cggraph_nodes
{

class TextContoursNode : public cggraph::Node
{
public:
	explicit TextContoursNode (const std::string &text = "Text");

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;

	// La memoisation par glyphe vit dans text_to_contours : « MISSISSIPPI »
	// place onze glyphes et n'en aplatit que quatre. Rendus separement plutot
	// que par un TextExtrudeStats& : ils sont ecrits pendant Compute, et deux
	// evaluations concurrentes les incrementeraient ensemble. Meme choix que
	// LoadFontNode::m_parses.
	unsigned int GetGlyphsPlaced () const { return m_glyphsPlaced.load (); }
	unsigned int GetGlyphsFlattened () const { return m_glyphsFlattened.load (); }

private:
	std::atomic<unsigned int> m_glyphsPlaced{ 0 };
	std::atomic<unsigned int> m_glyphsFlattened{ 0 };
};

} // namespace cggraph_nodes
