#pragma once
//
//  Base des trois noeuds qui delèguent un calcul a un document.
//
// Ce que la base tient, et que les trois partagent mot pour mot :
//
//  - la REFERENCE. Elle vient du GRAPHE, pas des parametres : c'est le champ
//    "subgraph" du document, prevu par la serialisation de l'etape 3 et
//    transmis au noeud par Graph::SetNodeSubgraph. Aucune migration n'a ete
//    necessaire, ce qui est exactement ce que ce champ avait ete ecrit d'avance
//    pour eviter ;
//
//  - l'IDENTITE du document reference. La reference est hachee dans la signature
//    depuis l'etape 3 -- mais elle ne hache qu'un NOM. Editer le document
//    delegue sans en changer le nom laisserait le cache resservir l'ancien
//    resultat, ce qui est le defaut de gravite elevee du corpus, revenu par une
//    porte neuve. Le noeud releve donc l'etat exterieur du fichier delegue,
//    exactement comme un noeud source releve celui du fichier qu'il lit, et
//    verse le releve a un parametre Semantic et Internal ;
//
//  - le DESCRIPTEUR, qui est porte par l'INSTANCE et non par le type. Deux
//    noeuds `flow.subgraph` referencant deux documents n'ont ni les memes ports
//    ni le meme drapeau d'effet de bord. C'est ce que « un SubgraphNode avec ses
//    propres ports » veut dire ;
//
//  - la PROPAGATION DE L'EFFET DE BORD, qui n'etait ecrite nulle part et sans
//    laquelle le critere 9.4 tomberait en silence. Un document qui contient un
//    puits fait de son hote un noeud a effet de bord -- sinon le cache exterieur
//    servirait le second appel et la seconde ecriture n'aurait pas lieu. Le
//    drapeau vaut « le document contient un puits ET on a demande a les tirer » :
//    des que la demande manque, aucun puits n'est tire, donc rien n'agit
//    ailleurs, donc la mise en cache redevient legitime.
//
// ⚠ LIMITE DE CONCURRENCE, DECLAREE PLUTOT QUE MASQUEE. RefreshExternalState
// s'execute pendant la pre-passe d'une evaluation, qui peut etre concurrente du
// Compute d'une AUTRE evaluation. Le releve n'ecrit rien tant que le fichier
// delegue n'a pas bouge -- c'est le chemin nominal. S'il a bouge, il republie le
// descripteur, que le Compute concurrent est en train de lire. Le contrat est
// donc : un noeud de flux s'evalue concurremment tant que le document qu'il
// delegue ne change pas SOUS ces evaluations. Aucun filet ne le tient ; le
// declarer vaut mieux que de fabriquer un detecteur qui n'existe pas.
//
#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "../../core/node.h"
#include "../node_support.h"
#include "subgraph_support.h"

namespace cggraph_nodes
{
namespace flow
{

class SubgraphHostNode : public cggraph::Node, public FileSink
{
public:
	const cggraph::NodeDesc &GetDesc () const override { return m_desc; }

	// Les fichiers que les puits du DOCUMENT ont ecrits, republies au nom de
	// l'hote. Sans cela, un pilote en tete haute rendrait une liste vide pour un
	// graphe qui a bel et bien produit N fichiers -- la frontiere du sous-graphe
	// cacherait ses effets a celui-la meme qui les a demandes.
	std::vector<std::string> GetWrittenPaths () const override;

	void SetSubgraphReference (const std::string &reference) override;
	void SetSubgraphDocument (const std::string &document) override;
	void RefreshExternalState () override;

	const std::string &GetSubgraphReference () const { return m_reference; }
	const std::string &GetSubgraphDocument () const { return m_document; }

	// INSTRUMENTS. Node::Compute ne rend qu'un booleen : le statut nomme de
	// l'evaluateur INTERNE serait perdu a la frontiere, et l'exterieur ne verrait
	// qu'un ComputeFailed muet. Ces deux accesseurs le republient sans toucher a
	// la signature de Compute, que D12 fige.
	cggraph::EvalStatus GetLastInnerStatus () const
	{
		return m_lastStatus.load (std::memory_order_relaxed);
	}
	std::string GetLastInnerDetail () const;

	// Nombre de PASSES reellement executees depuis la construction. Sans lui,
	// « le sous-graphe a tourne n fois » ne se distingue pas de « il a tourne une
	// fois et le cache a servi le reste » -- c'est-a-dire le defaut meme que la
	// cle de liaison existe pour empecher.
	unsigned int GetPassCount () const { return m_passes.load (std::memory_order_relaxed); }

protected:
	explicit SubgraphHostNode (const char *typeName);

	// Publie les ports de l'hote depuis la frontiere du document charge. Appelee
	// a chaque (re)chargement, et une fois avec une instance vide quand la
	// reference est retiree.
	virtual void PublishPorts (const SubgraphInstance &instance) = 0;

	// Charge le document reference. Nul en cas de refus, `status` et `detail`
	// disant lequel.
	std::unique_ptr<SubgraphInstance> Open (LoadStatus &status, std::string &detail) const;

	bool SinksRequested () const;

	// Republie m_desc.sideEffect. A appeler des que le document ou le parametre
	// `sinks` a pu changer.
	void RefreshSideEffect ();

	void NoteResult (cggraph::EvalStatus status, const std::string &detail);
	void NoteWritten (const std::vector<std::string> &paths);
	void NotePass () { m_passes.fetch_add (1u, std::memory_order_relaxed); }

	cggraph::NodeDesc m_desc;

	// Vrai si le document reference contient au moins un noeud a effet de bord,
	// a quelque profondeur que ce soit.
	bool m_hasSink = false;

private:
	void Reload ();

	std::string m_reference;

	// Document EMBARQUE. Il exclut m_reference -- le graphe fait s'exclure les
	// deux formes --, et c'est lui qui rend le parent autonome : rien n'est lu
	// sur disque au calcul, donc rien a relever non plus, son texte etant sa
	// propre identite.
	std::string m_document;

	std::atomic<cggraph::EvalStatus> m_lastStatus{ cggraph::EvalStatus::Ok };
	std::atomic<unsigned int> m_passes{ 0u };
	mutable std::mutex m_detailMutex;
	std::string m_lastDetail;

	// Meme forme que le puits de l'etape 3 : rendue PAR VALEUR sous verrou, et
	// ACCUMULEE d'un calcul a l'autre -- c'est l'accumulation qui rend verifiable
	// le nommage deterministe d'un graphe tire deux fois.
	mutable std::mutex m_writtenMutex;
	std::vector<std::string> m_written;
};

} // namespace flow
} // namespace cggraph_nodes
