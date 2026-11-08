// Bancs de la colonne classique : cotes sans maillage
// (ComputeClassicalColumnLevels) contre maillage complet
// (BuildClassicalColumnMesh), memes reglages.
//
// Argument `order` : 0 toscan, 2 ionique (volutes comprises), 4 composite.
// Reglages par defaut sinon (module 1, sans piedestal ni entablement, 48
// segments radiaux, 6 segments de courbe).

#include <exception>
#include <memory>

#include <benchmark/benchmark.h>

#include <cgarchi/parameterized_classical.h>
#include "mesh.h"

#include "allocation_counter.h"

using namespace cgarchi;

namespace
{
	ClassicalColumnSettings ColumnSettings (const benchmark::State &state)
	{
		ClassicalColumnSettings s;
		s.order = static_cast<int> (state.range(0));
		return s;
	}

	// Vrai si la colonne se construit et si ses cotes se calculent ; sinon,
	// motif dans `state`. Une exception sortie d'une fonction de banc
	// terminerait toute la passe. `withFaces` publie le nombre de faces du
	// maillage, pour le seul banc qui le construit.
	bool BuildsCleanly (benchmark::State &state, const ClassicalColumnSettings &s, bool withFaces)
	{
		try
		{
			const std::unique_ptr<Mesh> mesh = BuildClassicalColumnMesh(s);
			ComputeClassicalColumnLevels(s);
			if (withFaces)
				state.counters["faces"] = benchmark::Counter(static_cast<double>(mesh->GetNFaces()));
			return true;
		}
		catch (const std::exception &e)
		{
			state.SkipWithError(e.what());
			return false;
		}
	}
}

// Cotes du pied, du fut, du chapiteau et du sommet, sur le seul profil.
void BM_ClassicalColumn_Levels (benchmark::State &state)
{
	const ClassicalColumnSettings s = ColumnSettings(state);
	if (!BuildsCleanly(state, s, false))
		return;
	MeasureWithAllocations(state, [&] {
		ClassicalColumnLevels levels = ComputeClassicalColumnLevels(s);
		benchmark::DoNotOptimize(levels);
	});
}
BENCHMARK(BM_ClassicalColumn_Levels)->ArgName("order")->Arg(0)->Arg(2)->Arg(4)->Unit(benchmark::kMicrosecond);

// Maillage complet : profil, tour, coussinets ioniques.
void BM_ClassicalColumn_Mesh (benchmark::State &state)
{
	const ClassicalColumnSettings s = ColumnSettings(state);
	if (!BuildsCleanly(state, s, true))
		return;
	MeasureWithAllocations(state, [&] {
		const std::unique_ptr<Mesh> mesh = BuildClassicalColumnMesh(s);
		benchmark::DoNotOptimize(mesh->GetNFaces());
	});
}
BENCHMARK(BM_ClassicalColumn_Mesh)->ArgName("order")->Arg(0)->Arg(2)->Arg(4)->Unit(benchmark::kMicrosecond);
