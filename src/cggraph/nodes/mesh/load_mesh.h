#pragma once
//
//  LoadMesh -- source de maillage, depuis un fichier NATIF.
//
// Emballe Mesh::load, qui deroute sur l'extension et rend 0 en cas de succes.
// Corps lu, et il porte un piege : import_obj rend 0 -- donc SUCCES -- quand on
// lui passe un nom de fichier nul (mesh_io_obj.cpp:266). L'adaptateur ne passe
// jamais de nom nul et verifie en plus que le maillage rendu porte des sommets :
// un code de retour ne suffit pas ici a decider qu'il s'est passe quelque chose.
//
// Identite de source : stat (mtime + taille), releve avant le calcul de la
// signature, et hash SUR DEMANDE quand le parametre "verifyHash" est pose --
// pour un systeme de fichiers dont l'horloge n'est pas fiable. Le chemin de
// decision par defaut n'ouvre donc pas le fichier.
//
#include <atomic>

#include "../../core/node.h"

namespace cggraph_nodes
{

class LoadMeshNode : public cggraph::Node
{
public:
	explicit LoadMeshNode (const std::string &path = std::string ());

	const cggraph::NodeDesc &GetDesc () const override;
	void RefreshExternalState () override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;

	// Nombre de lectures REELLES du fichier depuis la construction. C'est
	// l'instrument de l'identite de source : il ne bouge pas quand le cache
	// sert, il bouge quand le fichier a change. Un compteur pose ici plutot que
	// bricole cote test, faute de quoi le critere n'aurait pas d'instrument.
	unsigned int GetReadCount () const { return m_reads.load (); }

private:
// COMPTEUR D'INSTANCE, et il est ecrit PENDANT Compute : deux evaluations
// concurrentes l'incrementeraient ensemble. Atomique, donc -- un compteur
// d'instruments ne vaut pas qu'on renonce au fil separe, et le deplacer dans le
// resultat d'evaluation obligerait la couche A a porter un champ de domaine.
	std::atomic<unsigned int> m_reads{ 0 };
};

} // namespace cggraph_nodes
