#pragma once
//
//  mesh.color -- la couleur de la piece devient une donnee du DOCUMENT.
//
// Elle etait un reglage de PAGE : une pastille dans la section « Vue », a cote
// du fil de fer et du recadrage. Elle n'en est pas un. Le fil de fer et la
// camera regardent la piece ; la couleur lui APPARTIENT -- on la choisit une
// fois, on veut la retrouver en rouvrant le document, et on veut pouvoir la
// piloter depuis le graphe comme n'importe quel autre reglage. D'ou un noeud.
//
// ⚠ CE QUE LA COULEUR TRAVERSE, ET CE QU'ELLE NE TRAVERSE PAS. Elle vit dans le
// maillage (un materiau, comme celui d'un relief colore) et le viewer la rend.
// Elle ne franchit AUCUN des deux exports actuels : le STL binaire ne porte pas
// de couleur -- le format n'en a pas --, et `graphExportObj` rend un OBJ minimal
// SANS mtllib, donc sans materiaux (cf. l'en-tete de js/template.js). Un export
// colore demanderait un chemin qui porte les matieres ; ce n'est pas ce noeud.
//
// ⚠ IL NE REPEINT PAS CE QUI EST DEJA PEINT. Seules les faces SANS materiau
// recoivent la couleur ; celles qui en portent un gardent le leur. Un relief
// colore branche ici garde donc sa palette au lieu de s'aplatir en un ton
// unique, et le noeud reste sur d'insertion n'importe ou. Une seule couleur est
// un seul materiau : REPEINDRE une palette demanderait de designer QUELLES
// faces, c'est-a-dire une selection, qui n'existe pas encore.
//
// Les deux comptes sont publies (Node::PublishStats) pour que ce partage se
// CONSTATE au lieu de se supposer.
//
#include <atomic>

#include "../../core/node.h"

namespace cggraph_nodes
{

class ColorMeshNode : public cggraph::Node
{
public:
	ColorMeshNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;

	void PublishStats (std::vector<cggraph::NodeStat> &out) const override;

	unsigned int GetPaintedFaces () const { return m_painted.load (); }
	unsigned int GetKeptFaces () const { return m_kept.load (); }

private:
	std::atomic<unsigned int> m_painted { 0 };
	std::atomic<unsigned int> m_kept { 0 };
};

} // namespace cggraph_nodes
