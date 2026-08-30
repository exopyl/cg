#include "host.h"

#include <cstdint>

#include "../file_identity.h"
#include "../node_support.h"

namespace cggraph_nodes
{
namespace flow
{

SubgraphHostNode::SubgraphHostNode (const char *typeName)
{
	m_desc.typeName = typeName;

	// Demande EXPLICITE de tirer les puits du document delegue, fausse par
	// defaut. Le defaut penche du cote qui n'ecrit rien : ouvrir un graphe ne
	// doit produire aucun fichier que personne n'a demande (nodal.md §11.4, tel
	// que l'etape 3 a du le reecrire). Semantique : la demande change ce que le
	// noeud fait.
	GetParams ().SetBool ("sinks", false);

	// Meme role et meme visibilite que `source.identity` d'un noeud source, et
	// pour la meme raison exactement : c'est lui qui invalide le cache quand le
	// document delegue change sans changer de nom.
	GetParams ().SetString ("subgraph.identity", "absent", cggraph::ParamRole::Semantic,
	                        cggraph::ParamVisibility::Internal);
}

std::string SubgraphHostNode::GetLastInnerDetail () const
{
	const std::lock_guard<std::mutex> held (m_detailMutex);
	return m_lastDetail;
}

void SubgraphHostNode::NoteResult (cggraph::EvalStatus status, const std::string &detail)
{
	m_lastStatus.store (status, std::memory_order_relaxed);
	const std::lock_guard<std::mutex> held (m_detailMutex);
	m_lastDetail = detail;
}

std::vector<std::string> SubgraphHostNode::GetWrittenPaths () const
{
	const std::lock_guard<std::mutex> held (m_writtenMutex);
	return m_written;
}

void SubgraphHostNode::NoteWritten (const std::vector<std::string> &paths)
{
	if (paths.empty ())
		return;
	const std::lock_guard<std::mutex> held (m_writtenMutex);
	m_written.insert (m_written.end (), paths.begin (), paths.end ());
}

bool SubgraphHostNode::SinksRequested () const
{
	return GetBool (GetParams (), "sinks", false);
}

void SubgraphHostNode::RefreshSideEffect ()
{
	// Les deux conditions, pas une seule. Le document contient un puits : sans la
	// demande, ce puits n'est jamais tire, donc rien n'agit ailleurs que dans la
	// sortie, donc le cache reste legitime. La demande est posee : le calcul agit
	// ailleurs, et une entree de cache dirait « deja fait » d'une ecriture qui ne
	// se reproduirait pas.
	m_desc.sideEffect = m_hasSink && SinksRequested ();
}

std::unique_ptr<SubgraphInstance> SubgraphHostNode::Open (LoadStatus &status,
                                                          std::string &detail) const
{
	std::unique_ptr<SubgraphInstance> instance (new SubgraphInstance ());
	status = LoadSubgraph (m_reference, *instance, detail);
	if (status != LoadStatus::Ok)
		return nullptr;
	return instance;
}

void SubgraphHostNode::Reload ()
{
	LoadStatus status = LoadStatus::Ok;
	std::string detail;
	const std::unique_ptr<SubgraphInstance> instance = Open (status, detail);

	if (instance == nullptr)
	{
		// Reference absente, illisible, invalide ou recursive : le noeud reste
		// VISIBLE et sans port. Il ne se connecte donc a rien et ne calcule rien,
		// ce qui est le meme parti que celui pris pour un noeud relu sous une
		// autre version de son type -- on ne fait pas disparaitre du graphe ce
		// que le document declare.
		const SubgraphInstance empty;
		m_hasSink = false;
		PublishPorts (empty);
		RefreshSideEffect ();
		NoteResult (cggraph::EvalStatus::ComputeFailed, ToString (status));
		return;
	}

	m_hasSink = !instance->sinks.empty ();
	PublishPorts (*instance);
	RefreshSideEffect ();
}

void SubgraphHostNode::SetSubgraphReference (const std::string &reference)
{
	if (reference == m_reference)
		return;
	m_reference = reference;
	Reload ();
}

void SubgraphHostNode::RefreshExternalState ()
{
	// HASH DU CONTENU, TOUJOURS -- et c'est un ECART ASSUME avec D15, qui donne
	// le stat pour regime des fichiers natifs. D15 tient parce que hacher pour
	// decider s'il faut relire OBLIGE a lire, donc annule l'economie du cache :
	// vrai d'un maillage de deux millions de triangles, FAUX d'un document de
	// graphe.
	//
	// Deux raisons, et la seconde est celle qui tranche :
	//
	//  1. le document est de l'ordre du kilo-octet, et il est de toute facon relu
	//     a chaque calcul de ce noeud -- le hachage n'ajoute pas une lecture, il
	//     l'anticipe ;
	//  2. MESURE, et c'est ce qui a fait tomber le stat ici : editer un document
	//     de graphe change tres souvent UN caractere -- « iterations: 1 » devient
	//     « iterations: 5 ». Meme taille, meme seconde, donc MEME identite par
	//     stat, donc cache servi sur un calcul qui n'est plus celui du document.
	//     Sur un maillage cette coincidence est rare ; sur un document de
	//     parametres elle est le cas NOMINAL.
	//
	// Repli sur le stat quand le fichier n'est pas lisible : les deux formes de
	// cle sont distinctes par construction (file_identity.h), donc passer de
	// l'une a l'autre ne peut pas resservir l'entree de l'autre regime.
	std::string key = "absent";
	if (!m_reference.empty ())
	{
		std::uint64_t hash = 0;
		key = HashFile (m_reference, hash) ? HashKey (hash) : StatKey (StatFile (m_reference));
	}

	const std::string previous = GetString (GetParams (), "subgraph.identity", std::string ());
	if (!GetParams ().UpdateString ("subgraph.identity", key))
		GetParams ().SetString ("subgraph.identity", key, cggraph::ParamRole::Semantic,
		                        cggraph::ParamVisibility::Internal);

	// Le descripteur ne se republie QUE si le fichier a bouge. C'est le chemin
	// nominal qui compte : sans cette garde, chaque evaluation reconstruirait les
	// ports sous le nez d'un calcul concurrent.
	if (previous != key)
		Reload ();

	// Le parametre `sinks`, lui, a pu changer sans que le fichier bouge.
	RefreshSideEffect ();
}

} // namespace flow
} // namespace cggraph_nodes
