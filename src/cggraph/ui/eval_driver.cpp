#include "eval_driver.h"

namespace cggraph_ui
{

// ---------------------------------------------------------------------------
//  Pilote a fil -- une facade, rien de plus
// ---------------------------------------------------------------------------

ThreadedEvalDriver::ThreadedEvalDriver (cggraph::Graph &graph) : m_async (graph)
{
}

void ThreadedEvalDriver::Request (cggraph::NodeId id)
{
	m_async.Request (id);
}

void ThreadedEvalDriver::RequestNow (cggraph::NodeId id)
{
	m_async.RequestNow (id);
}

void ThreadedEvalDriver::Cancel ()
{
	m_async.Cancel ();
}

bool ThreadedEvalDriver::IsBusy () const
{
	return m_async.IsBusy ();
}

bool ThreadedEvalDriver::IsIdle () const
{
	return m_async.IsIdle ();
}

void ThreadedEvalDriver::WaitIdle ()
{
	m_async.WaitIdle ();
}

void ThreadedEvalDriver::Pump ()
{
	// Rien. Le fil avance seul ; c'est toute la difference entre les deux
	// pilotes, et c'est pour qu'un hote puisse appeler Pump sans savoir lequel
	// il tient que cette methode existe ici.
}

std::vector<EvalDriver::Completed> ThreadedEvalDriver::Drain ()
{
	return m_async.Drain ();
}

EvalDriver::Progress ThreadedEvalDriver::GetProgress () const
{
	return m_async.GetProgress ();
}

unsigned int ThreadedEvalDriver::GetStartedCount () const
{
	return m_async.GetStartedCount ();
}

bool ThreadedEvalDriver::SetProgressSink (cggraph::EvalContext::ProgressSink)
{
	// Refus nomme -- voir eval_driver.h. Le collecteur d'AsyncEvaluator tourne
	// sur le fil de calcul ; y brancher l'hote lui ferait appeler du rendu
	// depuis ce fil, exactement ce que le §7.3 ecarte.
	return false;
}

// ---------------------------------------------------------------------------
//  Pilote en ligne -- le calcul a lieu dans Pump ()
// ---------------------------------------------------------------------------

InlineEvalDriver::InlineEvalDriver (cggraph::Graph &graph)
	: m_evaluator (graph), m_clock ([] { return std::chrono::steady_clock::now (); })
{
	m_deadline = m_clock ();
	m_context.SetCancellationFlag (&m_cancel);

	// Le collecteur fait DEUX choses, et il n'y a qu'un fil pour les faire :
	// deposer la progression, comme le pilote a fil, puis appeler l'hote s'il
	// en a installe un. C'est ce second appel qui rend le pompage de frame
	// possible sur un hote mono-fil -- il n'y a personne d'autre pour relever.
	m_context.SetProgressSink ([this] (float t, const char *label) {
		m_progress.t = t;
		m_progress.label = label != nullptr ? label : "";
		if (m_hostSink)
			m_hostSink (t, label);
	});
}

void InlineEvalDriver::SetClock (Clock clock)
{
	m_clock = clock ? clock : Clock ([] { return std::chrono::steady_clock::now (); });
}

void InlineEvalDriver::Request (cggraph::NodeId id)
{
	// La demande nouvelle remplace l'ancienne, jamais une file : la file ferait
	// calculer les etats intermediaires d'un curseur qu'on deplace.
	m_pending = id;
	m_hasPending = true;
	m_deadline = m_clock () + m_window;

	// Le calcul en cours -- s'il y en a un, donc si l'on est reentrant depuis le
	// collecteur -- ne repond plus a ce qu'on demande.
	if (m_busy)
		m_cancel.store (true);
}

void InlineEvalDriver::RequestNow (cggraph::NodeId id)
{
	m_pending = id;
	m_hasPending = true;
	m_deadline = m_clock ();
	if (m_busy)
		m_cancel.store (true);
}

void InlineEvalDriver::Cancel ()
{
	m_hasPending = false;
	m_pending = cggraph::kInvalidNodeId;
	m_cancel.store (true);
}

bool InlineEvalDriver::IsBusy () const
{
	return m_busy;
}

bool InlineEvalDriver::IsIdle () const
{
	return !m_busy && !m_hasPending;
}

void InlineEvalDriver::WaitIdle ()
{
	// Il n'y a pas d'autre fil pour avancer : attendre, ici, c'est pomper. ⚠ Le
	// piege d'AsyncEvaluator::WaitIdle est repris TEL QUEL, et c'est voulu : une
	// horloge figee dont personne n'avance l'aiguille fait tourner cette boucle
	// sans fin, exactement comme elle fait attendre l'autre sans fin. Deux
	// pilotes qui ne piegeraient pas de la meme facon seraient pires.
	while (!IsIdle ())
		Pump ();
}

void InlineEvalDriver::Pump ()
{
	if (m_busy || !m_hasPending)
		return;
	if (m_clock () < m_deadline)
		return;

	const cggraph::NodeId id = m_pending;
	m_hasPending = false;
	m_pending = cggraph::kInvalidNodeId;
	m_busy = true;
	++m_started;
	m_progress = Progress ();
	m_progress.node = id;
	m_cancel.store (false);

	Completed completed;
	completed.node = id;
	completed.result = m_evaluator.Evaluate (id, completed.outputs, m_context);
	// Juste apres Evaluate et sur le MEME fil : les vignettes du parcours
	// voyagent avec le resultat.
	completed.previews = m_evaluator.TakeRunPreviews ();
	completed.stats = m_evaluator.TakeRunStats ();

	m_busy = false;
	m_done.push_back (std::move (completed));
}

std::vector<EvalDriver::Completed> InlineEvalDriver::Drain ()
{
	std::vector<Completed> out;
	out.swap (m_done);
	return out;
}

EvalDriver::Progress InlineEvalDriver::GetProgress () const
{
	return m_progress;
}

unsigned int InlineEvalDriver::GetStartedCount () const
{
	return m_started;
}

bool InlineEvalDriver::SetProgressSink (cggraph::EvalContext::ProgressSink sink)
{
	m_hostSink = std::move (sink);
	return true;
}

// ---------------------------------------------------------------------------
//  Choix du pilote
// ---------------------------------------------------------------------------

bool HostHasThreads ()
{
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
	return false;
#else
	return true;
#endif
}

std::unique_ptr<EvalDriver> MakeEvalDriver (cggraph::Graph &graph, EvalMode mode)
{
	const bool threaded = mode == EvalMode::Threaded
	                      || (mode == EvalMode::Auto && HostHasThreads ());
	if (threaded)
		return std::unique_ptr<EvalDriver> (new ThreadedEvalDriver (graph));
	return std::unique_ptr<EvalDriver> (new InlineEvalDriver (graph));
}

} // namespace cggraph_ui
