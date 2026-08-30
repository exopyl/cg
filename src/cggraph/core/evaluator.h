#pragma once
//
//  Evaluateur -- tire par la sortie, memoise sur la signature.
//
// Pull et non push : evaluer un noeud n'evalue que la branche qui l'alimente.
// Le cache est une LRU indexee sur la signature (signature.h) et bornee en
// memoire. Son budget est un parametre du moteur et non une constante, parce
// qu'il vaut quelques Gio en natif contre quelques centaines de Mio sur le web.
//
// Un noeud epingle est exempt d'eviction tant que l'epingle tient : une LRU nue
// evincerait, sous pression, exactement les branches qu'on garde vivantes pour
// les comparer.
//
// Toute sortie d'echec est un statut nomme. Un booleen ferait passer un refus
// pour un autre, et un test croirait verifier ce qu'il ne verifie pas.
//
// UN FIL A LA FOIS, et ce n'est pas une consigne : l'evaluateur porte un cache
// et un memo de signatures qu'aucun verrou ne protege, parce que le rendre
// partageable couterait le benefice du cache sans qu'aucun client ne le demande
// -- l'etape 6 fait tourner UN fil de calcul. Deux fils qui l'utiliseraient
// ensemble corrompraient sa liste chainee, ce qui ne se voit pas. La seconde
// evaluation est donc REFUSEE, par un statut nomme (Busy), et deux fils qui
// evaluent en parallele prennent un evaluateur chacun -- ce qu'ils partagent est
// le graphe, dont la pre-passe est exclue par Graph::LockPrePass.
//
#include <atomic>
#include <cstddef>
#include <list>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "eval_context.h"
#include "graph.h"
#include "signature.h"

namespace cggraph
{

enum class EvalStatus
{
	Ok,
	UnknownNode,
	IncompatibleVersion,
	MissingInput,
	DrivenParameter,
	ComputeFailed,
	Aborted,

	// Une evaluation est deja en cours sur cet evaluateur -- autre fil, ou
	// reentrance depuis un collecteur de progression. Refuser vaut mieux que
	// corrompre : le cache et le memo ne sont pas partageables, et le seul
	// symptome d'un partage serait un resultat faux ou un plantage a distance.
	Busy
};

struct EvalResult
{
	EvalStatus status = EvalStatus::Ok;

	// Noeud ou le refus s'est produit, et nom du port ou du parametre en cause
	// quand le statut en designe un. Un diagnostic muet ne se distingue pas
	// d'un plantage pour celui qui le lit.
	NodeId node = kInvalidNodeId;
	std::string detail;

	bool IsOk () const { return status == EvalStatus::Ok; }
};

struct CacheStats
{
	std::size_t hits = 0;
	std::size_t misses = 0;
	std::size_t evictions = 0;
	std::size_t entries = 0;
	std::size_t bytes = 0;
};

class Evaluator
{
public:
	// Valeur de depart, pas une limite du moteur : SetMemoryBudget la remplace.
	static constexpr std::size_t kDefaultMemoryBudget = 256u * 1024u * 1024u;

	// Le graphe doit survivre a l'evaluateur. Il est pris non const parce que
	// Compute est une operation du noeud.
	explicit Evaluator (Graph &graph);

	// Reduire le budget evince immediatement ce qui depasse, hors epingles.
	void SetMemoryBudget (std::size_t bytes);
	std::size_t GetMemoryBudget () const { return m_budget; }

	// Evalue la branche qui alimente `id` et rend ses sorties. En cas de refus,
	// `outputs` est vide et le statut nomme la cause.
	//
	// Un seul fil a la fois : un appel concurrent ou reentrant rend Busy sans
	// rien toucher.
	EvalResult Evaluate (NodeId id, ValueList &outputs, EvalContext &ctx);

	// kNoSignature si le noeud est inconnu.
	Hash GetSignature (NodeId id);

	void Pin (NodeId id);
	void Unpin (NodeId id);
	bool IsPinned (NodeId id) const;

	// Vrai si la signature COURANTE du noeud est en cache. Ne touche pas
	// l'ordre d'usage : une question ne vaut pas un usage.
	bool IsCached (NodeId id);

	const CacheStats &GetStats () const { return m_stats; }
	void ResetStats ();
	void ClearCache ();

private:
	struct Entry
	{
		Hash key = kNoSignature;
		ValueList values;
		std::size_t bytes = 0;
		// Un meme resultat peut etre celui de plusieurs noeuds ; il survit tant
		// que l'un d'eux est epingle.
		std::vector<NodeId> owners;
	};

	using EntryList = std::list<Entry>;

	EvalResult EvaluateNode (NodeId id, ValueList &outputs, EvalContext &ctx);
	void RefreshBranch (NodeId id, std::set<NodeId> &seen);
	void Insert (Hash key, NodeId owner, const ValueList &values);
	void EnforceBudget ();
	bool IsEntryPinned (const Entry &entry) const;

	Graph &m_graph;
	SignatureMemo m_memo;

	EntryList m_entries;   // front = usage le plus recent
	std::unordered_map<Hash, EntryList::iterator> m_index;
	std::set<NodeId> m_pinned;

	std::size_t m_budget = kDefaultMemoryBudget;
	std::size_t m_bytes = 0;
	CacheStats m_stats;

	// Vrai le temps d'une evaluation. Atomique parce que c'est le seul membre
	// qu'un second fil a le droit de toucher -- pour se voir refuser l'entree.
	std::atomic<bool> m_running{ false };
};

} // namespace cggraph
