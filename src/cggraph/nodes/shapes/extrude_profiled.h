#pragma once
//
//  shape.extrude.profiled -- l'extrusion dont l'ARETE SUPERIEURE suit un profil.
//
// Le meme metier que `shape.extrude`, a une difference pres, et c'est celle qui
// justifie un noeud a part plutot qu'un parametre : ce noeud a une ENTREE de
// plus, un profil. Un parametre « forme d'arete » aurait fige la famille des
// profils dans une enumeration que deux fichiers epellent ; un port la laisse
// OUVERTE -- `profile.chamfer`, `profile.cavetto`, et ce qui viendra.
//
// C'est ce que `shapes/profile.h` annonçait : « le meme noeud alimente la
// moulure d'une baie gothique, et alimentera le biseau d'un texte ».
//
// TROIS FORMES pour deux producteurs, et ce n'est pas une economie de bouts de
// ficelle -- c'est le constat que « chanfrein » et « biseau » ne sont pas deux
// formes mais deux reglages d'une seule :
//
//   chanfrein  profile.chamfer, largeur == profondeur (coupe a 45 degres) ;
//   biseau     profile.chamfer, largeur != profondeur (coupe a angle libre) ;
//   conge      profile.cavetto -- et c'est un arrondi CONVEXE sous la lecture de
//              ce consommateur, verifie par le volume et non deduit
//              (tu_cgmesh_extrude_profiled.cpp). Le dossier de faisabilite
//              annonçait un producteur de plus ; il n'en fallait aucun.
//
// ⚠ CE QUE LE PROFIL DETRUIT. Le decalage retenu est INTERIEUR (decision D1) :
// l'emprise au sol reste nominale, mais un trait plus mince que deux fois la
// largeur du profil DISPARAIT. C'est correct, c'est mesurable, et c'est dit --
// `GetVanishedPieces` compte les morceaux perdus. Le mode dilatation, qui ne
// detruit rien mais fait grossir la piece, est un parametre.
//
#include <atomic>

#include "../../core/node.h"

namespace cggraph_nodes
{

class ExtrudeProfiledNode : public cggraph::Node
{
public:
	ExtrudeProfiledNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;

	// Morceaux que le profil a fait disparaitre au dernier calcul. Zero quand
	// rien n'a ete perdu.
	unsigned int GetVanishedPieces () const { return m_vanished.load (); }

	// Sommets qu'aucun anneau n'a reclames. Reste a zero sur tous les cas
	// mesures ; s'il grimpe, la couture des couronnes ne tient plus.
	unsigned int GetSteinerPoints () const { return m_steiner.load (); }

	void PublishStats (std::vector<cggraph::NodeStat> &out) const override;

private:
	std::atomic<unsigned int> m_vanished { 0 };
	std::atomic<unsigned int> m_steiner { 0 };
};

} // namespace cggraph_nodes
