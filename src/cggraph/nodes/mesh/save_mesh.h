#pragma once
//
//  SaveMesh -- puits : ecrit le maillage recu et ne produit aucune valeur.
//
// Emballe Mesh::save, qui deroute sur l'extension et rend 0 en cas de succes,
// -1 pour une extension inconnue. Corps lu : la comparaison d'extension y est
// faite par strcmp sur les quatre ou cinq derniers caracteres du chemin, sans
// normalisation de casse -- contrairement a load. Un chemin en ".OBJ" n'est
// donc reconnu ni ecrit, et ce noeud rendra un echec plutot que rien.
//
// ZERO sortie, et EFFET DE BORD declare : le noeud agit ailleurs que dans sa
// sortie, il n'est donc jamais mis en cache. Sans ce drapeau, re-evaluer un
// graphe inchange n'ecrirait pas le fichier une seconde fois, et effacer le
// fichier hors du graphe ne le ferait pas reapparaitre -- le cache dirait
// « deja fait » d'une chose qui ne l'est plus. Ses entrees, elles, sont mises en
// cache comme les autres : reecrire ne recalcule pas la branche amont.
//
// NOMMAGE DETERMINISTE : le chemin peut porter le jeton "{hash}", remplace par
// une empreinte du CONTENU recu -- sommets et triangles. Un nom de fichier
// produit se deduit de ce qu'on ecrit ou de l'indice de l'element, JAMAIS d'un
// compteur d'executions : sinon deux executions du meme graphe sur la meme
// entree produiraient deux fichiers, ce qui casse a la fois le rejeu en tete
// haute et les tests d'integration qui comparent les sorties.
//
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include "../../core/node.h"
#include "../node_support.h"

namespace cggraph_nodes
{

class SaveMeshNode : public cggraph::Node, public FileSink
{
public:
	explicit SaveMeshNode (const std::string &path = std::string ());

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;

	// Nombre d'ecritures REELLES depuis la construction -- meme motif que le
	// compteur de lectures de la source : sans lui, « le cache a servi » ne se
	// distingue pas de « le fichier a ete reecrit a l'identique ».
	unsigned int GetWriteCount () const { return m_writes.load (); }

	std::vector<std::string> GetWrittenPaths () const override;

private:
// COMPTEUR D'INSTANCE, et il est ecrit PENDANT Compute : deux evaluations
// concurrentes l'incrementeraient ensemble. Atomique, donc -- un compteur
// d'instruments ne vaut pas qu'on renonce au fil separe, et le deplacer dans le
// resultat d'evaluation obligerait la couche A a porter un champ de domaine.
	std::atomic<unsigned int> m_writes{ 0 };

	// Pas un instrument : une SORTIE, que le pilote en tete haute lit pour nommer
	// les fichiers produits. Un push_back concurrent est indefini, et rendre une
	// reference sur un vecteur qu'un autre fil fait croitre l'est tout autant --
	// d'ou la copie sous verrou. Elle s'accumule d'une execution a l'autre : c'est
	// ce qui rend verifiable le nommage deterministe d'un graphe tire deux fois.
	mutable std::mutex m_writtenMutex;
	std::vector<std::string> m_written;
};

} // namespace cggraph_nodes
