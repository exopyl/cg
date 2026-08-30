#pragma once
//
//  mesh.io.load_parts -- un fichier multi-objets devient une SUITE de maillages.
//
// C'est la source que ForEach attendait, et le cas d'usage que nodal.md §11.1
// nomme : une decomposition rend N pieces, chacune traitee puis exportee
// separement. Le meme fichier lu par `mesh.io.load` rendrait UN maillage aplati,
// dont les pieces ne se distinguent plus.
//
// Emballe VMeshesIO::load, qui deroute sur l'extension MINUSCULEE -- contrairement
// a Mesh::save, un chemin en ".OBJ" est donc reconnu ici. Corps lu, et deux
// choses qu'il ne dit pas :
//
//  - l'import OBJ lit le fichier DEUX FOIS : une passe complete par Mesh::load
//    pour la geometrie aplatie, puis une passe texte pour retrouver les
//    frontieres d'objets ('o', ou 'g' quand le fichier n'a pas d'objet). Le
//    cout est donc le double de celui de mesh.io.load, pour la meme geometrie ;
//  - un fichier SANS 'o' ni 'g' rend UNE piece nommee "default". La suite n'est
//    donc jamais vide sur un OBJ valide, et « une seule piece » ne se distingue
//    pas de « pas de decoupage » -- ce qui est le comportement voulu, mais qu'il
//    faut savoir avant d'y brancher un ForEach.
//
// NATIF SEULEMENT : vmeshes_io.cpp n'est pas dans la liste EMSCRIPTEN de
// src/cgmesh/CMakeLists.txt. Voir catalog.cpp et le CMakeLists de cette couche.
//
#include <atomic>
#include <string>

#include "../../core/node.h"

namespace cggraph_nodes
{

class LoadPartsNode : public cggraph::Node
{
public:
	explicit LoadPartsNode (const std::string &path = std::string ());

	const cggraph::NodeDesc &GetDesc () const override;
	void RefreshExternalState () override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;

	// Lectures REELLES depuis la construction : sans ce compteur, « le cache a
	// servi » ne se distingue pas de « le fichier a ete relu a l'identique ».
	unsigned int GetReadCount () const { return m_reads.load (); }

private:
	std::atomic<unsigned int> m_reads{ 0 };
};

} // namespace cggraph_nodes
