#pragma once
//
//  LoadImage -- source d'image, depuis un buffer EN MEMOIRE.
//
// Meme motif que LoadFontNode, et pour la meme raison : une ressource fixee a la
// construction d'un objet monolithique n'a pas de port, donc pas de lien, donc
// rien qui la relie a la signature de ce qui la consomme. En faire un noeud lui
// rend un port -- et la rend partageable entre plusieurs consommateurs sans
// etre relue. C'est ce qui permet a « Image to puzzle » et a « Blocs pixelises »
// de partir de la MEME source dans un seul document.
//
// Identite versee : HASH DU BUFFER, jamais un stat. Il n'y a ici ni fichier ni
// horloge -- sous WebAssembly les octets arrivent du JavaScript, et maker
// aujourd'hui les depose dans un fichier MEMFS qu'il doit garder vivant pour la
// duree de la page faute de quoi la relecture echoue. Le nom, lui, est
// DECORATIF : il ne participe pas a la signature, le renommer ne recalcule rien.
//
// Le format est reconnu au CONTENU par Img::load_from_memory, et non a
// l'extension : PNG, JPEG, BMP, TGA, PNM. Un nom de fichier trompeur ne fait
// donc pas echouer le decodage, contrairement au chemin par Img::load.
//
#include <atomic>
#include <string>
#include <vector>

#include "../../core/node.h"
#include "../node_support.h"

namespace cggraph_nodes
{

class LoadImageNode : public cggraph::Node, public ByteSource
{
public:
	LoadImageNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;

	// Pose les octets et verse aussitot leur hash a la signature : la valeur du
	// parametre d'identite ne peut pas se desynchroniser du buffer, puisque c'est
	// la meme operation qui ecrit les deux.
	void SetBytes (std::vector<unsigned char> bytes) override;

	// Purement decoratif, hors signature.
	void SetName (const std::string &name) override;

	// Nombre de DECODAGES reels du buffer depuis la construction. Sert a
	// verifier, par un test, que le cache evite bien le travail : deux
	// evaluations sur les memes octets ne doivent decoder qu'une fois.
	unsigned int GetDecodeCount () const { return m_decodes.load (); }

private:
	std::vector<unsigned char> m_bytes;

	// Ecrit PENDANT Compute, donc potentiellement par deux evaluations
	// concurrentes : atomique. Meme choix que LoadFontNode::m_parses, et meme
	// motif -- un compteur d'instrumentation ne vaut pas qu'on renonce au fil
	// separe.
	std::atomic<unsigned int> m_decodes{ 0 };
};

} // namespace cggraph_nodes
