#pragma once
//
//  LoadFont -- source de police, depuis un buffer EN MEMOIRE.
//
// Noeud separe par obligation, non par gout : une ressource fixee a la
// construction d'un objet monolithique n'a pas de port, donc pas de lien, donc
// rien qui la relie a la signature de ce qui la consomme. En faire un noeud lui
// rend un port -- et, accessoirement, la rend partageable entre plusieurs
// consommateurs sans etre relue.
//
// Identite versee : HASH DU BUFFER, jamais un stat. Il n'y a ici ni fichier ni
// horloge -- sous WebAssembly les octets arrivent du JavaScript et le chemin
// temporaire est souvent supprime avant le premier calcul. Le nom, lui, est
// DECORATIF : il ne participe pas a la signature, le renommer ne recalcule
// rien.
//
// Font n'est pas copiable par declaration -- elle possede son buffer, dont
// stb_truetype ne garde qu'un pointeur. Le type de lien qu'elle porte est donc
// Immutable et sans clone, et c'est le compilateur qui l'impose.
//
#include <atomic>
#include <vector>

#include "../../core/node.h"
#include "../node_support.h"

namespace cggraph_nodes
{

class LoadFontNode : public cggraph::Node, public ByteSource
{
public:
	LoadFontNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;

	// Pose les octets et verse aussitot leur hash a la signature : la valeur du
	// parametre d'identite ne peut pas se desynchroniser du buffer, puisque
	// c'est la meme operation qui ecrit les deux.
	void SetBytes (std::vector<unsigned char> bytes) override;

	// Purement decoratif, hors signature.
	void SetName (const std::string &name) override;

	// Nombre d'analyses REELLES du buffer depuis la construction.
	unsigned int GetParseCount () const { return m_parses.load (); }

private:
	std::vector<unsigned char> m_bytes;

// COMPTEUR D'INSTANCE, et il est ecrit PENDANT Compute : deux evaluations
// concurrentes l'incrementeraient ensemble. Atomique, donc -- un compteur
// d'instruments ne vaut pas qu'on renonce au fil separe, et le deplacer dans le
// resultat d'evaluation obligerait la couche A a porter un champ de domaine.
	std::atomic<unsigned int> m_parses{ 0 };
};

} // namespace cggraph_nodes
