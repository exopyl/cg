#include <gtest/gtest.h>

#include "../src/cggraph/core/async_evaluator.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// ===========================================================================
//  Couche A : evaluation asynchrone, coalescence, exclusion
// ===========================================================================
// Les types manipules ici sont INVENTES, comme dans les autres tests de la
// couche A : le moteur ne connait aucune bibliotheque de domaine. Ce fichier ne
// prouve donc rien sur les algorithmes -- c'est le role de
// tu_cggraph_concurrency.cpp -- et tout sur la mecanique du fil.
//
// COMMENT ON DETECTE UNE COURSE SANS ThreadSanitizer, puisque MSVC n'en a pas :
// on ne cherche pas a observer la course, on FORCE l'entrelacement qui la
// produirait et on interroge le mecanisme cense l'empecher. Deux formes, et
// elles sont symetriques :
//
//  - une EXCLUSION s'atteste par un rendez-vous qui doit ECHOUER : deux fils
//    entrent dans la section, le premier attend le second, et le second ne peut
//    pas venir. Retirer le verrou fait reussir le rendez-vous, donc rougir ;
//  - une CONCURRENCE s'atteste par un rendez-vous qui doit REUSSIR : deux fils
//    doivent se trouver ensemble dans Compute. Elargir le verrou au calcul fait
//    echouer le rendez-vous, donc rougir.
//
// Sans le second, le premier serait satisfait par un moteur qui serialise tout
// -- et l'etape entiere ne servirait a rien.

using namespace cggraph;

namespace {

// Horloge PILOTEE. Sans elle, un test de coalescence mesure l'ordonnanceur : il
// faudrait dormir plus longtemps que la fenetre, et un ordonnanceur charge
// rendrait le resultat aleatoire. Ici aucune fenetre ne s'ecoule sans qu'un
// test le decide.
class ManualClock
{
public:
	AsyncEvaluator::TimePoint Now () const
	{
		return m_origin + std::chrono::milliseconds (m_elapsedMs.load ());
	}

	void Advance (int ms) { m_elapsedMs.fetch_add (ms); }

private:
	AsyncEvaluator::TimePoint m_origin = std::chrono::steady_clock::now ();
	std::atomic<int> m_elapsedMs{ 0 };
};

// Rendez-vous a deux, avec echeance. Il rend VRAI si le pair est arrive, FAUX
// s'il ne pouvait pas venir -- et les deux reponses sont des resultats de test,
// pas des accidents.
class Rendezvous
{
public:
	bool Meet (int timeoutMs)
	{
		std::unique_lock<std::mutex> held (m_mutex);
		++m_arrived;
		if (m_arrived >= 2)
		{
			m_cv.notify_all ();
			return true;
		}
		if (m_cv.wait_for (held, std::chrono::milliseconds (timeoutMs),
		                   [this] { return m_arrived >= 2; }))
			return true;

		// On repart : sans cela, un pair arrive APRES l'echeance trouverait le
		// compteur a un et croirait avoir rencontre quelqu'un.
		--m_arrived;
		return false;
	}

private:
	std::mutex m_mutex;
	std::condition_variable m_cv;
	int m_arrived = 0;
};

// Porte : un calcul s'y arrete jusqu'a ce qu'un test l'ouvre. C'est ce qui rend
// DETERMINISTE « pendant que le calcul tourne » -- sans elle, il faudrait
// esperer que le calcul dure assez longtemps.
class Gate
{
public:
	// Le drapeau d'annulation est SCRUTE, comme le fait la boucle externe d'un
	// vrai algorithme : personne ne notifie la variable de condition en le
	// levant. Attendre une notification ici ferait durer chaque annulation
	// jusqu'a l'echeance, et le test mesurerait son propre garde-fou.
	void Wait (const std::atomic<bool> *abortFlag)
	{
		const std::chrono::steady_clock::time_point deadline =
			std::chrono::steady_clock::now () + std::chrono::seconds (5);
		std::unique_lock<std::mutex> held (m_mutex);
		while (!m_open && !(abortFlag != nullptr && abortFlag->load ())
		       && std::chrono::steady_clock::now () < deadline)
			m_cv.wait_for (held, std::chrono::milliseconds (1));
	}

	void Open ()
	{
		{
			const std::lock_guard<std::mutex> held (m_mutex);
			m_open = true;
		}
		m_cv.notify_all ();
	}

private:
	std::mutex m_mutex;
	std::condition_variable m_cv;
	bool m_open = false;
};

// Attente ACTIVE bornee : rend vrai des que la condition est vraie. Elle sert a
// observer un etat du fil de calcul depuis le fil appelant, ce qui est
// exactement ce que l'etape promet -- l'appelant reste vivant.
template <class Predicate>
bool WaitFor (Predicate predicate, int timeoutMs = 5000)
{
	const std::chrono::steady_clock::time_point deadline =
		std::chrono::steady_clock::now () + std::chrono::milliseconds (timeoutMs);
	while (std::chrono::steady_clock::now () < deadline)
	{
		if (predicate ())
			return true;
		std::this_thread::sleep_for (std::chrono::milliseconds (1));
	}
	return predicate ();
}

std::shared_ptr<void> CloneCount (const void *value)
{
	return std::make_shared<int> (*static_cast<const int *> (value));
}

std::size_t SizeOfCount (const void *)
{
	return sizeof (int);
}

TypeDesc MakeCountDesc ()
{
	TypeDesc desc;
	desc.name = "test.Count";
	desc.clone = &CloneCount;
	desc.sizeHint = &SizeOfCount;
	desc.mutability = TypeDesc::Forkable;
	return desc;
}

// Noeud de base : un seul port de sortie, un parametre semantique pour que deux
// instances aient deux signatures.
class CountNode : public Node
{
public:
	CountNode (const TypeDesc *type, int seed)
	{
		m_desc.typeName = "test.Count";
		m_desc.outputs.push_back ({ "sortie", type, false });
		m_type = type;
		m_seed = seed;
		GetParams ().SetInt ("seed", seed);
	}

	const NodeDesc &GetDesc () const override { return m_desc; }

	bool Compute (EvalContext &, const ValueList &, ValueList &out) override
	{
		++m_computes;
		out[0] = Value::Make (m_type, std::make_shared<int> (m_seed));
		return true;
	}

	unsigned int GetComputeCount () const { return m_computes.load (); }

protected:
	NodeDesc m_desc;
	const TypeDesc *m_type = nullptr;
	int m_seed = 0;
	std::atomic<unsigned int> m_computes{ 0 };
};

// Calcul qui s'arrete a une porte, en surveillant le jeton d'annulation.
class GatedNode : public CountNode
{
public:
	GatedNode (const TypeDesc *type, int seed, Gate &gate) : CountNode (type, seed), m_gate (gate)
	{
	}

	bool Compute (EvalContext &ctx, const ValueList &, ValueList &out) override
	{
		++m_computes;
		m_thread = std::this_thread::get_id ();
		m_entered.store (true);
		ctx.Progress (0.5f, "moitie");
		m_gate.Wait (ctx.GetCancellationFlag ());

		// Boucle externe reduite a son squelette : annule, on ne produit rien.
		if (ctx.IsAborted ())
			return false;

		out[0] = Value::Make (m_type, std::make_shared<int> (7));
		return true;
	}

	bool HasEntered () const { return m_entered.load (); }

	// Le fil sur lequel le DERNIER calcul a tourne. Ecrit par un seul fil : ce
	// noeud n'est jamais calcule deux fois en parallele.
	std::thread::id GetComputeThread () const { return m_thread; }

private:
	Gate &m_gate;
	std::thread::id m_thread;
	std::atomic<bool> m_entered{ false };
};

// Noeud dont la RELEVEE d'etat exterieur se signale et attend son pair. C'est
// l'instrument de l'exclusion de la pre-passe : si deux evaluations pouvaient
// relever en meme temps, le rendez-vous reussirait.
class ProbingSourceNode : public CountNode
{
public:
	ProbingSourceNode (const TypeDesc *type, int seed, Rendezvous &meeting, int timeoutMs)
		: CountNode (type, seed), m_meeting (meeting), m_timeoutMs (timeoutMs)
	{
		GetParams ().SetString ("source.identity", "absent", ParamRole::Semantic,
		                        ParamVisibility::Internal);
	}

	void RefreshExternalState () override
	{
		++m_refreshes;
		if (m_meeting.Meet (m_timeoutMs))
			m_metPeer.store (true);
		GetParams ().UpdateString ("source.identity", "releve");
	}

	bool HasMetPeer () const { return m_metPeer.load (); }
	unsigned int GetRefreshCount () const { return m_refreshes.load (); }

private:
	Rendezvous &m_meeting;
	int m_timeoutMs = 0;
	std::atomic<bool> m_metPeer{ false };
	std::atomic<unsigned int> m_refreshes{ 0 };
};

// Noeud dont le CALCUL se signale et attend son pair : le versant symetrique.
class MeetingNode : public CountNode
{
public:
	MeetingNode (const TypeDesc *type, int seed, Rendezvous &meeting, int timeoutMs)
		: CountNode (type, seed), m_meeting (meeting), m_timeoutMs (timeoutMs)
	{
	}

	bool Compute (EvalContext &, const ValueList &, ValueList &out) override
	{
		++m_computes;
		if (m_meeting.Meet (m_timeoutMs))
			m_metPeer.store (true);
		out[0] = Value::Make (m_type, std::make_shared<int> (1));
		return true;
	}

	bool HasMetPeer () const { return m_metPeer.load (); }

private:
	Rendezvous &m_meeting;
	int m_timeoutMs = 0;
	std::atomic<bool> m_metPeer{ false };
};

} // namespace

// ---------------------------------------------------------------------------
//  Le fil de calcul
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_async, the_computation_runs_on_another_thread_and_the_caller_stays_alive)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeCountDesc ());
	ASSERT_NE (type, nullptr);

	Gate gate;
	Graph graph;
	GatedNode *gated = new GatedNode (type, 1, gate);
	const NodeId id = graph.AddNode (std::unique_ptr<Node> (gated));

	AsyncEvaluator async (graph);
	async.RequestNow (id);

	// Le calcul est ENTRE et il y reste : le fil appelant, lui, continue de
	// repondre -- c'est tout l'objet de l'etape, et c'est ici qu'il se constate.
	ASSERT_TRUE (WaitFor ([gated] { return gated->HasEntered (); }));
	EXPECT_TRUE (async.IsBusy ());
	EXPECT_FALSE (async.IsIdle ());

	// La progression est lisible PENDANT le calcul : une interface gelee ne
	// pourrait pas la lire.
	ASSERT_TRUE (WaitFor ([&async] { return async.GetProgress ().t > 0.0f; }));
	EXPECT_FLOAT_EQ (async.GetProgress ().t, 0.5f);
	EXPECT_EQ (async.GetProgress ().label, std::string ("moitie"));
	EXPECT_EQ (async.GetProgress ().node, id);

	gate.Open ();
	async.WaitIdle ();

	const std::vector<AsyncEvaluator::Completed> done = async.Drain ();
	ASSERT_EQ (done.size (), 1u);
	EXPECT_EQ (done[0].result.status, EvalStatus::Ok);
	EXPECT_NE (gated->GetComputeThread (), std::this_thread::get_id ());
}

// ---------------------------------------------------------------------------
//  Coalescence -- critere 6.3
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_async, a_burst_of_requests_triggers_exactly_one_evaluation)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeCountDesc ());
	ASSERT_NE (type, nullptr);

	Graph graph;
	CountNode *node = new CountNode (type, 1);
	const NodeId id = graph.AddNode (std::unique_ptr<Node> (node));

	ManualClock clock;
	AsyncEvaluator async (graph);
	async.SetClock ([&clock] { return clock.Now (); });

	// La fenetre par defaut est celle de la conception, et elle se lit.
	EXPECT_EQ (async.GetDebounce (), std::chrono::milliseconds (150));

	// Vingt demandes en rafale, comme un curseur qu'on tire.
	for (int i = 0; i < 20; ++i)
		async.Request (id);

	// A 149 ms, rien n'a demarre : la fenetre n'est pas ecoulee. Sans horloge
	// pilotee, cette assertion serait une course avec l'ordonnanceur.
	clock.Advance (149);
	EXPECT_FALSE (WaitFor ([&async] { return async.GetStartedCount () > 0; }, 60));
	EXPECT_EQ (async.GetStartedCount (), 0u);

	clock.Advance (1);
	async.WaitIdle ();

	// UNE evaluation pour vingt demandes. Vingt, ce serait la panne que le
	// critere 6.3 nomme.
	EXPECT_EQ (async.GetStartedCount (), 1u);
	EXPECT_EQ (node->GetComputeCount (), 1u);
	EXPECT_EQ (async.Drain ().size (), 1u);
}

TEST (TEST_cggraph_async, a_request_outside_the_window_triggers_a_second_evaluation)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeCountDesc ());
	ASSERT_NE (type, nullptr);

	Graph graph;
	const NodeId id = graph.AddNode (std::unique_ptr<Node> (new CountNode (type, 1)));

	ManualClock clock;
	AsyncEvaluator async (graph);
	async.SetClock ([&clock] { return clock.Now (); });

	async.Request (id);
	clock.Advance (150);
	async.WaitIdle ();
	ASSERT_EQ (async.GetStartedCount (), 1u);

	// Versant symetrique, et il est indispensable : sans lui, un moteur qui
	// n'evaluerait JAMAIS deux fois satisferait le test precedent.
	async.Request (id);
	clock.Advance (150);
	async.WaitIdle ();
	EXPECT_EQ (async.GetStartedCount (), 2u);
	EXPECT_EQ (async.Drain ().size (), 2u);
}

TEST (TEST_cggraph_async, the_window_is_a_parameter_of_the_engine)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeCountDesc ());
	ASSERT_NE (type, nullptr);

	Graph graph;
	const NodeId id = graph.AddNode (std::unique_ptr<Node> (new CountNode (type, 1)));

	ManualClock clock;
	AsyncEvaluator async (graph);
	async.SetClock ([&clock] { return clock.Now (); });
	async.SetDebounce (std::chrono::milliseconds (400));
	EXPECT_EQ (async.GetDebounce (), std::chrono::milliseconds (400));

	async.Request (id);

	// 150 ms suffisaient sous la fenetre par defaut ; ici non. Une constante
	// codee en dur laisserait ce cas rouge.
	clock.Advance (150);
	EXPECT_FALSE (WaitFor ([&async] { return async.GetStartedCount () > 0; }, 60));

	clock.Advance (250);
	async.WaitIdle ();
	EXPECT_EQ (async.GetStartedCount (), 1u);
	async.Drain ();
}

TEST (TEST_cggraph_async, a_new_request_aborts_the_computation_it_replaces)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeCountDesc ());
	ASSERT_NE (type, nullptr);

	Gate gate;
	Graph graph;
	GatedNode *slow = new GatedNode (type, 1, gate);
	const NodeId slowId = graph.AddNode (std::unique_ptr<Node> (slow));
	const NodeId quickId = graph.AddNode (std::unique_ptr<Node> (new CountNode (type, 2)));

	ManualClock clock;
	AsyncEvaluator async (graph);
	async.SetClock ([&clock] { return clock.Now (); });

	async.RequestNow (slowId);
	ASSERT_TRUE (WaitFor ([slow] { return slow->HasEntered (); }));

	// La demande neuve leve le jeton : le calcul en cours se termine sans
	// produire. Sans cela, la fenetre coalescerait les demandes mais les calculs
	// s'empileraient quand meme -- « un curseur agite empile des calculs morts ».
	async.Request (quickId);
	clock.Advance (150);
	async.WaitIdle ();

	const std::vector<AsyncEvaluator::Completed> done = async.Drain ();
	ASSERT_EQ (done.size (), 2u);
	EXPECT_EQ (done[0].node, slowId);
	EXPECT_EQ (done[0].result.status, EvalStatus::Aborted);
	EXPECT_EQ (done[1].node, quickId);
	EXPECT_EQ (done[1].result.status, EvalStatus::Ok);
}

// ---------------------------------------------------------------------------
//  Un evaluateur, un fil -- le refus nomme prend la place d'une corruption
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_async, a_second_evaluation_on_the_same_evaluator_is_refused_by_name)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeCountDesc ());
	ASSERT_NE (type, nullptr);

	Gate gate;
	Graph graph;
	GatedNode *gated = new GatedNode (type, 1, gate);
	const NodeId id = graph.AddNode (std::unique_ptr<Node> (gated));

	Evaluator evaluator (graph);

	std::atomic<bool> cancelled (false);
	EvalContext first;
	first.SetCancellationFlag (&cancelled);
	ValueList firstOutputs;
	EvalResult firstResult;

	std::thread worker (
		[&] { firstResult = evaluator.Evaluate (id, firstOutputs, first); });

	ASSERT_TRUE (WaitFor ([gated] { return gated->HasEntered (); }));

	// Le meme evaluateur, un second fil, pendant que le premier calcule. Le
	// cache et le memo ne sont proteges par aucun verrou : sans ce refus, les
	// deux appels se partageraient une liste chainee, ce qui ne se voit pas.
	EvalContext second;
	ValueList secondOutputs;
	const EvalResult refused = evaluator.Evaluate (id, secondOutputs, second);
	EXPECT_EQ (refused.status, EvalStatus::Busy);
	EXPECT_TRUE (secondOutputs.empty ());

	gate.Open ();
	worker.join ();
	EXPECT_EQ (firstResult.status, EvalStatus::Ok);

	// Le drapeau retombe : l'evaluateur redevient utilisable.
	ValueList thirdOutputs;
	EvalContext third;
	EXPECT_EQ (evaluator.Evaluate (id, thirdOutputs, third).status, EvalStatus::Ok);
}

// ---------------------------------------------------------------------------
//  Pre-passe exclusive, calcul concurrent -- les deux moities de C1/C2
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_async, two_evaluations_never_refresh_the_same_graph_at_the_same_time)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeCountDesc ());
	ASSERT_NE (type, nullptr);

	// UN SEUL noeud source, partage par les deux evaluations : c'est la seule
	// forme ou la relevee d'etat exterieur peut se marcher dessus.
	Rendezvous meeting;
	Graph graph;
	ProbingSourceNode *probe = new ProbingSourceNode (type, 1, meeting, 150);
	const NodeId id = graph.AddNode (std::unique_ptr<Node> (probe));

	Evaluator left (graph);
	Evaluator right (graph);
	EvalContext leftCtx;
	EvalContext rightCtx;
	ValueList leftOut;
	ValueList rightOut;

	std::thread a ([&] { left.Evaluate (id, leftOut, leftCtx); });
	std::thread b ([&] { right.Evaluate (id, rightOut, rightCtx); });
	a.join ();
	b.join ();

	// Les deux ont bien releve -- donc les deux sont passees par la section --
	// mais JAMAIS ensemble. Retirer Graph::LockPrePass fait reussir le
	// rendez-vous et rougir cette ligne.
	EXPECT_EQ (probe->GetRefreshCount (), 2u);
	EXPECT_FALSE (probe->HasMetPeer ());
}

TEST (TEST_cggraph_async, two_evaluations_DO_compute_at_the_same_time)
{
	TypeRegistry registry;
	const TypeDesc *type = registry.Register (MakeCountDesc ());
	ASSERT_NE (type, nullptr);

	// Deux instances distinctes : deux calculs, un seul graphe.
	Rendezvous meeting;
	Graph graph;
	MeetingNode *leftNode = new MeetingNode (type, 1, meeting, 2000);
	MeetingNode *rightNode = new MeetingNode (type, 2, meeting, 2000);
	const NodeId leftId = graph.AddNode (std::unique_ptr<Node> (leftNode));
	const NodeId rightId = graph.AddNode (std::unique_ptr<Node> (rightNode));

	Evaluator left (graph);
	Evaluator right (graph);
	EvalContext leftCtx;
	EvalContext rightCtx;
	ValueList leftOut;
	ValueList rightOut;

	std::thread a ([&] { left.Evaluate (leftId, leftOut, leftCtx); });
	std::thread b ([&] { right.Evaluate (rightId, rightOut, rightCtx); });
	a.join ();
	b.join ();

	// LE VERSANT QUI EMPECHE LE PRECEDENT D'ETRE DECORATIF : si le verrou de
	// pre-passe couvrait Compute, ou si le moteur serialisait les evaluations,
	// ce rendez-vous echouerait et l'etape entiere serait sans objet.
	EXPECT_TRUE (leftNode->HasMetPeer ());
	EXPECT_TRUE (rightNode->HasMetPeer ());
}
