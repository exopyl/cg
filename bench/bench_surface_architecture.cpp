// Bancs des fonctions de surface_architecture reecrites en phase 2 du plan 1 :
// les quatre echantillonneurs d'arc et le balayage d'une moulure.
//
// Argument `npts` : nombre d'echantillons de l'arc.

#include <benchmark/benchmark.h>

#include <cgarchi/surface_architecture.h>
#include "mesh.h"
#include "profile2d.h"

#include "allocation_counter.h"

using namespace cgarchi;

namespace
{
	unsigned SampleCount (const benchmark::State &state)
	{
		return static_cast<unsigned>(state.range(0));
	}

	// npts impair : sommet unique de l'arc brise, balayage sans arete
	// non-manifold (debt_cgarchi.md).
	constexpr long long kSmall = 33, kLarge = 1025;
}

void BM_SampleArcBrise (benchmark::State &state)
{
	const unsigned npts = SampleCount(state);
	MeasureWithAllocations(state, [npts] {
		ArcSamples s = sampleArcBrise(330.f, 660.f, npts, 0.f);
		benchmark::DoNotOptimize(s.points.data());
	});
}
BENCHMARK(BM_SampleArcBrise)->ArgName("npts")->Arg(kSmall)->Arg(kLarge);

void BM_SampleArcAccolade (benchmark::State &state)
{
	const unsigned npts = SampleCount(state);
	MeasureWithAllocations(state, [npts] {
		ArcSamples s = sampleArcAccolade(5.f, 3.f, .3f, npts);
		benchmark::DoNotOptimize(s.points.data());
	});
}
BENCHMARK(BM_SampleArcAccolade)->ArgName("npts")->Arg(kSmall)->Arg(kLarge);

void BM_SampleArcAnseDePanier (benchmark::State &state)
{
	const unsigned npts = SampleCount(state);
	MeasureWithAllocations(state, [npts] {
		ArcSamples s = sampleArcAnseDePanier(2.f, .3f, npts);
		benchmark::DoNotOptimize(s.points.data());
	});
}
BENCHMARK(BM_SampleArcAnseDePanier)->ArgName("npts")->Arg(kSmall)->Arg(kLarge);

void BM_SampleArcRampant (benchmark::State &state)
{
	const unsigned npts = SampleCount(state);
	MeasureWithAllocations(state, [npts] {
		ArcSamples s = sampleArcRampant(1.f, .6f, npts);
		benchmark::DoNotOptimize(s.points.data());
	});
}
BENCHMARK(BM_SampleArcRampant)->ArgName("npts")->Arg(kSmall)->Arg(kLarge);

// Bourdon (rollBarProfile (5), 12 segments) balaye le long d'un arc brise.
void BM_ExtrudeMoulureAlongCurve (benchmark::State &state)
{
	const ArcSamples curve = sampleArcBrise(330.f, 660.f, SampleCount(state), 0.f);
	const Profile2D moulure = rollBarProfile(5.0);
	Mesh mesh;
	MeasureWithAllocations(state, [&] {
		extrudeMoulureAlongCurve(curve, moulure, mesh);
		benchmark::DoNotOptimize(mesh.GetNFaces());
	});
	state.counters["faces"] = benchmark::Counter(static_cast<double>(mesh.GetNFaces()));
}
BENCHMARK(BM_ExtrudeMoulureAlongCurve)->ArgName("npts")->Arg(kSmall)->Arg(kLarge)->Unit(benchmark::kMicrosecond);
