#pragma once
//
//  Compteur d'allocations du programme de bancs.
//
// allocation_counter.cpp remplace l'operator new global de cg_bench. Hors d'une
// fenetre de comptage, il se reduit a une lecture atomique et a malloc : le
// temps mesure n'en porte pas le cout. Dans la fenetre, chaque appel a
// operator new (size_t) est compte, quel que soit le thread : les fils de la
// fusion parallele des barres y entrent.
//
// Hors du compte : malloc direct (tampons de Polygon2::tesselate, par exemple)
// et operator new aligne (align_val_t). Sous Windows, les DLL tierces gardent
// leur propre operator new et ne sont pas comptees ; sous ELF, celui de
// l'executable interpose celui des .so, qui sont alors comptees.
//
#include <cstdint>
#include <functional>

#include <benchmark/benchmark.h>

struct AllocationCount
{
	std::int64_t calls = 0;   // appels a operator new
	std::int64_t bytes = 0;   // octets demandes
};

// Allocations faites, tous threads confondus, pendant f ().
AllocationCount CountAllocations (const std::function<void ()> &f);

// Publie `count` par les compteurs `allocs` et `alloc_bytes` de `state`
// (valeurs par appel, sans moyenne de taux).
void PublishAllocations (benchmark::State &state, const AllocationCount &count);

// Mesure f () dans la boucle de `state`, puis compte les allocations d'un appel
// supplementaire, hors chronometre. f est appele directement dans la boucle,
// sans std::function.
template <class F>
void MeasureWithAllocations (benchmark::State &state, F &&f)
{
	for (auto _ : state)
		f();
	PublishAllocations(state, CountAllocations(f));
}
