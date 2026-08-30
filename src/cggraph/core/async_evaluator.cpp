#include "async_evaluator.h"

namespace cggraph
{

AsyncEvaluator::AsyncEvaluator (Graph &graph)
	: m_evaluator (graph), m_clock ([] { return std::chrono::steady_clock::now (); })
{
	m_deadline = m_clock ();
	m_context.SetCancellationFlag (&m_cancel);

	// Le collecteur tourne sur le FIL DE CALCUL. Il ne fait que deposer une
	// copie : appeler l'hote depuis ici l'obligerait a etre sur, et la
	// conception veut que l'hote declenche sa frame quand il lit, pas quand on
	// l'appelle.
	m_context.SetProgressSink ([this] (float t, const char *label) {
		const std::lock_guard<std::mutex> held (m_mutex);
		m_progress.t = t;
		m_progress.label = label != nullptr ? label : "";
	});

	m_worker = std::thread ([this] { Loop (); });
}

AsyncEvaluator::~AsyncEvaluator ()
{
	{
		const std::lock_guard<std::mutex> held (m_mutex);
		m_stop = true;
		m_hasPending = false;
	}
	m_cancel.store (true);
	m_wake.notify_all ();
	m_worker.join ();
}

void AsyncEvaluator::SetDebounce (std::chrono::milliseconds window)
{
	const std::lock_guard<std::mutex> held (m_mutex);
	m_window = window;
}

std::chrono::milliseconds AsyncEvaluator::GetDebounce () const
{
	const std::lock_guard<std::mutex> held (m_mutex);
	return m_window;
}

void AsyncEvaluator::SetClock (Clock clock)
{
	const std::lock_guard<std::mutex> held (m_mutex);
	m_clock = clock ? clock : Clock ([] { return std::chrono::steady_clock::now (); });
}

void AsyncEvaluator::Request (NodeId id)
{
	{
		const std::lock_guard<std::mutex> held (m_mutex);
		m_pending = id;
		m_hasPending = true;
		m_deadline = m_clock () + m_window;

		// Le calcul en cours est abandonne : une demande neuve le remplace, et
		// son resultat ne serait plus celui qu'on demande. C'est tout l'objet de
		// l'annulation cooperative -- sans elle, la fenetre coalescerait les
		// demandes mais les calculs, eux, s'empileraient quand meme.
		if (m_busy)
			m_cancel.store (true);
	}
	m_wake.notify_all ();
}

void AsyncEvaluator::RequestNow (NodeId id)
{
	{
		const std::lock_guard<std::mutex> held (m_mutex);
		m_pending = id;
		m_hasPending = true;
		m_deadline = m_clock ();
		if (m_busy)
			m_cancel.store (true);
	}
	m_wake.notify_all ();
}

void AsyncEvaluator::Cancel ()
{
	{
		const std::lock_guard<std::mutex> held (m_mutex);
		m_hasPending = false;
		m_pending = kInvalidNodeId;
	}
	m_cancel.store (true);
	m_wake.notify_all ();
	m_settled.notify_all ();
}

bool AsyncEvaluator::IsBusy () const
{
	const std::lock_guard<std::mutex> held (m_mutex);
	return m_busy;
}

bool AsyncEvaluator::IsIdle () const
{
	const std::lock_guard<std::mutex> held (m_mutex);
	return !m_busy && !m_hasPending;
}

void AsyncEvaluator::WaitIdle () const
{
	std::unique_lock<std::mutex> held (m_mutex);
	m_settled.wait (held, [this] { return !m_busy && !m_hasPending; });
}

std::vector<AsyncEvaluator::Completed> AsyncEvaluator::Drain ()
{
	const std::lock_guard<std::mutex> held (m_mutex);
	std::vector<Completed> ready;
	ready.swap (m_done);
	return ready;
}

AsyncEvaluator::Progress AsyncEvaluator::GetProgress () const
{
	const std::lock_guard<std::mutex> held (m_mutex);
	return m_progress;
}

unsigned int AsyncEvaluator::GetStartedCount () const
{
	const std::lock_guard<std::mutex> held (m_mutex);
	return m_started;
}

void AsyncEvaluator::Loop ()
{
	std::unique_lock<std::mutex> held (m_mutex);
	while (!m_stop)
	{
		if (!m_hasPending)
		{
			m_wake.wait (held);
			continue;
		}

		// L'echeance se relit A CHAQUE TOUR, sur l'horloge courante : c'est ce
		// qui coalesce. Une demande arrivee pendant l'attente l'a repoussee, et
		// le tour suivant attend la nouvelle -- N demandes rapprochees ne
		// franchissent cette ligne qu'une fois.
		const TimePoint now = m_clock ();
		if (now < m_deadline)
		{
			m_wake.wait_for (held, m_deadline - now);
			continue;
		}

		const NodeId id = m_pending;
		m_hasPending = false;
		m_pending = kInvalidNodeId;
		m_busy = true;
		++m_started;
		m_progress = Progress ();
		m_progress.node = id;
		m_cancel.store (false);

		held.unlock ();

		// HORS VERROU, et c'est le point de l'etape : le calcul dure, le fil
		// appelant continue de poser des demandes et de retirer des resultats.
		Completed completed;
		completed.node = id;
		completed.result = m_evaluator.Evaluate (id, completed.outputs, m_context);

		held.lock ();
		m_busy = false;
		m_done.push_back (std::move (completed));
		m_settled.notify_all ();
	}
}

} // namespace cggraph
