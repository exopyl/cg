#include "allocation_counter.h"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

// Remplacement de l'operator new global de cg_bench. Les blocs sont ceux de
// malloc, sans en-tete : un bloc alloue par une DLL et libere ici, ou l'inverse,
// reste valide tant que les deux cotes partagent le CRT (/MD sous MSVC, comme
// tout le depot).
namespace
{
	std::atomic<bool>         g_counting {false};
	std::atomic<std::int64_t> g_calls {0};
	std::atomic<std::int64_t> g_bytes {0};
}

void *operator new (std::size_t size)
{
	if (g_counting.load(std::memory_order_relaxed))
	{
		g_calls.fetch_add(1, std::memory_order_relaxed);
		g_bytes.fetch_add(static_cast<std::int64_t>(size), std::memory_order_relaxed);
	}
	void *p = std::malloc(size == 0 ? 1 : size);
	if (!p)
		throw std::bad_alloc();
	return p;
}

void operator delete (void *p) noexcept { std::free(p); }
void operator delete (void *p, std::size_t) noexcept { std::free(p); }

AllocationCount CountAllocations (const std::function<void ()> &f)
{
	g_calls.store(0);
	g_bytes.store(0);
	g_counting.store(true);
	try
	{
		f();
	}
	catch (...)
	{
		g_counting.store(false);
		throw;
	}
	g_counting.store(false);
	return {g_calls.load(), g_bytes.load()};
}

void PublishAllocations (benchmark::State &state, const AllocationCount &count)
{
	state.counters["allocs"] = benchmark::Counter(static_cast<double>(count.calls));
	state.counters["alloc_bytes"] = benchmark::Counter(static_cast<double>(count.bytes));
}
