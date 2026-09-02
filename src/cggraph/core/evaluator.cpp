#include "evaluator.h"

namespace cggraph
{

namespace
{

EvalResult MakeResult (EvalStatus status, NodeId node, const std::string &detail)
{
	EvalResult result;
	result.status = status;
	result.node = node;
	result.detail = detail;
	return result;
}

// Remet le drapeau d'occupation quel que soit le chemin de sortie -- il y en a
// une dizaine.
class RunningGuard
{
public:
	explicit RunningGuard (std::atomic<bool> &flag) : m_flag (flag) {}
	~RunningGuard () { m_flag.store (false); }

	RunningGuard (const RunningGuard &) = delete;
	RunningGuard &operator= (const RunningGuard &) = delete;

private:
	std::atomic<bool> &m_flag;
};

std::size_t MeasureBytes (const ValueList &values)
{
	std::size_t total = 0;
	for (const Value &value : values)
		total += value.GetSizeHint ();
	return total;
}

} // namespace

Evaluator::Evaluator (Graph &graph)
	: m_graph (graph)
{
}

void Evaluator::SetMemoryBudget (std::size_t bytes)
{
	m_budget = bytes;
	EnforceBudget ();
}

Hash Evaluator::GetSignature (NodeId id)
{
	// Le memo ne vaut que pour un etat donne du graphe : un parametre modifie
	// entre deux appels le rendrait faux.
	m_memo.clear ();
	return Signature (m_graph, id, m_memo);
}

void Evaluator::Pin (NodeId id)
{
	m_pinned.insert (id);
}

void Evaluator::Unpin (NodeId id)
{
	m_pinned.erase (id);
}

bool Evaluator::IsPinned (NodeId id) const
{
	return m_pinned.find (id) != m_pinned.end ();
}

bool Evaluator::IsCached (NodeId id)
{
	const Hash key = GetSignature (id);
	if (key == kNoSignature)
		return false;
	return m_index.find (key) != m_index.end ();
}

void Evaluator::ResetStats ()
{
	const std::size_t entries = m_stats.entries;
	const std::size_t bytes = m_stats.bytes;
	m_stats = CacheStats ();
	m_stats.entries = entries;
	m_stats.bytes = bytes;
}

void Evaluator::ClearCache ()
{
	m_entries.clear ();
	m_index.clear ();
	m_bytes = 0;
	m_stats.entries = 0;
	m_stats.bytes = 0;
}

bool Evaluator::IsEntryPinned (const Entry &entry) const
{
	for (NodeId owner : entry.owners)
		if (m_pinned.find (owner) != m_pinned.end ())
			return true;
	return false;
}

void Evaluator::EnforceBudget ()
{
	// L'entree la plus recemment utilisee n'est jamais la victime : evincer ce
	// qu'on vient de calculer reviendrait a ne rien mettre en cache. Une entree
	// seule plus grosse que le budget survit donc, et le budget est depasse --
	// le refuser couterait un recalcul a chaque evaluation.
	while (m_bytes > m_budget && m_entries.size () > 1)
	{
		EntryList::iterator victim = m_entries.end ();
		for (EntryList::iterator it = --m_entries.end (); it != m_entries.begin (); --it)
			if (!IsEntryPinned (*it))
			{
				victim = it;
				break;
			}
		if (victim == m_entries.end ())
			break;

		m_bytes -= victim->bytes;
		m_index.erase (victim->key);
		m_entries.erase (victim);
		++m_stats.evictions;
	}

	m_stats.entries = m_entries.size ();
	m_stats.bytes = m_bytes;
}

void Evaluator::Insert (Hash key, NodeId owner, const ValueList &values)
{
	Entry entry;
	entry.key = key;
	entry.values = values;
	entry.bytes = MeasureBytes (values);
	entry.owners.push_back (owner);

	m_entries.push_front (entry);
	m_index[key] = m_entries.begin ();
	m_bytes += entry.bytes;
	EnforceBudget ();
}

void Evaluator::RefreshBranch (NodeId id, std::set<NodeId> &seen)
{
	if (!seen.insert (id).second)
		return;

	Node *node = m_graph.FindNode (id);
	if (node == nullptr)
		return;

	node->RefreshExternalState ();
	for (NodeId up : m_graph.GetUpstream (id))
		if (up != kInvalidNodeId)
			RefreshBranch (up, seen);
}

EvalResult Evaluator::Evaluate (NodeId id, ValueList &outputs, EvalContext &ctx)
{
	// Une seule evaluation a la fois sur un evaluateur donne. Le refus est
	// nomme, et il prend la place d'une corruption silencieuse du cache.
	bool idle = false;
	if (!m_running.compare_exchange_strong (idle, true))
		return MakeResult (EvalStatus::Busy, id, std::string ());
	const RunningGuard running (m_running);

	m_memo.clear ();
	m_runPreviews.clear ();
	outputs.clear ();

	{
		// PRE-PASSE, exclusive au niveau du GRAPHE. Elle fait deux choses, et
		// elles sont indissociables : elle releve l'etat exterieur des sources,
		// puis elle FIGE la signature de toute la branche dans le memo.
		//
		// Indissociables parce que la premiere ecrit ce que la seconde lit. Les
		// separer laisserait une seconde evaluation reecrire l'identite entre les
		// deux, et la signature calculee ici designerait alors le fichier de
		// l'autre -- « le cache sert un resultat qui n'est pas celui qu'il
		// annonce », par la porte de l'etat exterieur.
		//
		// Figer sert aussi a ce que la suite ne relise plus AUCUN parametre hors
		// de Compute : EvaluateNode retrouve toutes ses signatures dans le memo.
		const std::unique_lock<std::mutex> prePass = m_graph.LockPrePass ();

		// Avant la signature, jamais apres : la signature de la racine agrege
		// celle de toute la branche, donc une source relevee en descendant serait
		// relevee trop tard pour l'index du cache.
		std::set<NodeId> seen;
		RefreshBranch (id, seen);

		Signature (m_graph, id, m_memo);
	}

	return EvaluateNode (id, outputs, ctx);
}

EvalResult Evaluator::EvaluateNode (NodeId id, ValueList &outputs, EvalContext &ctx)
{
	outputs.clear ();

	Node *node = m_graph.FindNode (id);
	if (node == nullptr)
		return MakeResult (EvalStatus::UnknownNode, id, std::string ());

	// Refus avant l'annulation, avant le cache et avant les parametres : un
	// noeud relu d'un document ecrit sous une autre version du type est VISIBLE
	// dans le graphe et ne se calcule pas. Le refus ne depend donc d'aucun etat
	// d'execution -- il dirait la meme chose sur un graphe qu'on vient d'annuler.
	// Le calculer quand meme reviendrait a interpreter des parametres au sens
	// d'une version qui n'est plus la sienne, et a rendre faux sans planter.
	if (!node->IsVersionCompatible ())
		return MakeResult (EvalStatus::IncompatibleVersion, id, node->GetDesc ().typeName);

	if (ctx.IsAborted ())
		return MakeResult (EvalStatus::Aborted, id, std::string ());

	// Refus avant toute lecture du cache : le diagnostic ne doit pas dependre
	// de ce qui s'y trouve. Aucun evaluateur d'expression n'existe ; la valeur
	// se stocke, se hache et se refuse, elle ne se devine pas.
	const ParamEntry *driven = node->GetParams ().FindDriven ();
	if (driven != nullptr)
		return MakeResult (EvalStatus::DrivenParameter, id, driven->name);

	const NodeDesc &desc = node->GetDesc ();

	const Hash key = Signature (m_graph, id, m_memo);
	std::unordered_map<Hash, EntryList::iterator>::iterator cached = m_index.find (key);
	if (cached != m_index.end ())
	{
		EntryList::iterator entry = cached->second;
		bool known = false;
		for (NodeId owner : entry->owners)
			if (owner == id)
			{
				known = true;
				break;
			}
		if (!known)
			entry->owners.push_back (id);

		m_entries.splice (m_entries.begin (), m_entries, entry);
		++m_stats.hits;
		outputs = entry->values;
		RecordPreviews (id, outputs);
		return EvalResult ();
	}

	ValueList inputs (desc.inputs.size ());
	for (std::size_t i = 0; i < desc.inputs.size (); ++i)
	{
		const Link *link = m_graph.FindInputLink (id, static_cast<PortIdx> (i));
		if (link == nullptr)
		{
			// Une entree optionnelle non alimentee reste vide et le calcul suit ;
			// le noeud en decide par Value::IsEmpty. La signature distingue deja
			// les deux cas, le cache ne peut donc pas servir l'un pour l'autre.
			if (desc.inputs[i].optional)
				continue;
			return MakeResult (EvalStatus::MissingInput, id, desc.inputs[i].name);
		}

		ValueList upstream;
		const EvalResult result = EvaluateNode (link->from, upstream, ctx);
		if (!result.IsOk ())
			return result;

		if (link->fromPort >= upstream.size ())
			return MakeResult (EvalStatus::MissingInput, id, desc.inputs[i].name);
		inputs[i] = upstream[link->fromPort];
	}

	ValueList produced (desc.outputs.size ());
	++m_stats.misses;
	const bool computed = node->Compute (ctx, inputs, produced);

	// LE CONTEXTE AVANT LE CODE DE RETOUR, et l'ordre est le sujet.
	//
	// Un calcul interrompu rend un resultat partiel : il ne doit pas entrer au
	// cache, ou la signature designerait une valeur incomplete. Tester d'abord le
	// contexte fait que l'annulation se nomme Aborted quoi qu'ait rendu Compute,
	// et un adaptateur annule rend donc `false` comme n'importe quel echec : il
	// n'a plus a connaitre cet ordre pour obtenir le bon statut.
	//
	// CONTREPARTIE ACCEPTEE, en connaissance de cause : une panne REELLE survenue
	// pendant une annulation sera rapportee Aborted et non ComputeFailed. Le
	// diagnostic exact d'une panne qu'on etait de toute facon en train
	// d'abandonner a ete juge moins precieux que de retirer aux ~40 adaptateurs a
	// venir la charge de connaitre l'ordre des tests de l'evaluateur.
	if (ctx.IsAborted ())
		return MakeResult (EvalStatus::Aborted, id, std::string ());

	if (!computed)
		return MakeResult (EvalStatus::ComputeFailed, id, std::string ());

	// Un noeud a effet de bord n'entre pas au cache, et cela SUFFIT : la lecture
	// du cache plus haut ne peut pas trouver ce qui n'y a jamais ete mis. Une
	// seconde garde a la lecture serait une branche morte, verifiee comme telle
	// -- la retirer ne change aucun test.
	if (!desc.sideEffect)
		Insert (key, id, produced);
	outputs = produced;
	RecordPreviews (id, outputs);
	return EvalResult ();
}

void Evaluator::RecordPreviews (NodeId id, const ValueList &outputs)
{
	// La PREMIERE sortie qui sache se representer, et on s'arrete la : un noeud
	// a rarement deux sorties visuelles, et en afficher plusieurs demanderait a
	// l'interface de choisir -- ce qui n'est pas une decision du moteur.
	for (const Value &value : outputs)
	{
		Thumbnail thumb;
		if (value.GetThumbnail (kPreviewMaxSide, thumb) && !thumb.IsEmpty ())
		{
			m_runPreviews[id] = std::move (thumb);
			return;
		}
	}
}

} // namespace cggraph
