#include <cgarchi/bar_fusion_parallel.h>

#include <algorithm>
#include <atomic>
#include <exception>
#include <vector>

#ifndef __EMSCRIPTEN__
#include <new>
#include <thread>
#endif

namespace cgarchi
{

namespace
{
	std::atomic<unsigned int> g_forcedLimit { 0 };

#ifndef __EMSCRIPTEN__
	// Fils auxiliaires en cours d'emprunt, tous appels confondus.
	std::atomic<unsigned int> g_helpersInUse { 0 };

	// Emprunt d'au plus `wanted` fils auxiliaires sur un budget de `budget`
	// fils pour le processus ; rendus a la destruction.
	class HelperLease
	{
	public:
		HelperLease (unsigned int wanted, unsigned int budget)
		{
			unsigned int used = g_helpersInUse.load();
			for (;;)
			{
				const unsigned int take = (used < budget) ? std::min(wanted, budget - used) : 0u;
				if (take == 0)
					return;
				if (g_helpersInUse.compare_exchange_weak(used, used + take))
				{
					m_count = take;
					return;
				}
			}
		}
		~HelperLease () { g_helpersInUse.fetch_sub(m_count); }
		HelperLease (const HelperLease &) = delete;
		HelperLease &operator= (const HelperLease &) = delete;

		unsigned int count () const { return m_count; }

	private:
		unsigned int m_count = 0;
	};

	// Joint, a la sortie de portee, tout fil encore joignable : les fils
	// lisent des donnees de la pile appelante, et un std::thread joignable
	// detruit appellerait std::terminate.
	struct JoinAll
	{
		std::vector<std::thread> &threads;
		~JoinAll ()
		{
			for (std::thread &t : threads)
				if (t.joinable())
					t.join();
		}
	};
#endif
}

unsigned int barFusionThreadLimit ()
{
#ifdef __EMSCRIPTEN__
	return 1;
#else
	unsigned int n = g_forcedLimit.load();
	if (n == 0)
	{
		n = std::thread::hardware_concurrency();
		if (n == 0)
			n = kBarFusionMaxThreads;
	}
	return std::clamp(n, 1u, kBarFusionMaxThreads);
#endif
}

unsigned int setBarFusionThreadLimit (unsigned int threads)
{
	return g_forcedLimit.exchange(threads);
}

void barFusionParallelFor (std::size_t n, const std::function<void (std::size_t)> &task)
{
	if (n == 0)
		return;
#ifndef __EMSCRIPTEN__
	const unsigned int limit = barFusionThreadLimit();
	const unsigned int wanted = static_cast<unsigned int>(std::min<std::size_t>(n, limit) - 1);
	const HelperLease lease(wanted, limit - 1);
	// Sans memoire pour les exceptions des taches, la boucle sequentielle
	// ci-dessous garde le contrat : ordre des taches, premiere exception.
	std::vector<std::exception_ptr> errors;
	if (lease.count() > 0)
		try { errors.resize(n); }
		catch (const std::bad_alloc &) { errors.clear(); }
	if (!errors.empty())
	{
		std::atomic<std::size_t> next { 0 };
		auto work = [&]() {
			for (std::size_t i = next++; i < n; i = next++)
			{
				try { task(i); }
				catch (...) { errors[i] = std::current_exception(); }
			}
		};
		{
			std::vector<std::thread> threads;
			const JoinAll guard { threads };
			// Un fil qui ne peut pas etre lance (std::system_error, ou
			// std::bad_alloc de la reservation ou du lancement) n'est pas une
			// erreur : les taches restantes passent aux fils deja lances et a
			// l'appelant.
			try
			{
				threads.reserve(lease.count());
				for (unsigned int t = 0; t < lease.count(); ++t)
					threads.emplace_back(work);
			}
			catch (...) {}
			work();
		}
		for (const std::exception_ptr &e : errors)
			if (e)
				std::rethrow_exception(e);
		return;
	}
#endif
	for (std::size_t i = 0; i < n; ++i)
		task(i);
}

} // namespace cgarchi
