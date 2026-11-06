// Bancs de la fenetre gothique (ParameterizedGothicWindow::Regenerate) et du bloc
// (CreateBlock).
//
// Chaque banc refuse de mesurer une construction en echec : un Regenerate qui
// rend le triangle de repli coute peu et fausserait la mesure sans le dire.
//
// Les bancs de la fenetre sont chronometres en temps reel (UseRealTime) : la
// fusion des barres travaille sur plusieurs fils, et le temps CPU du seul fil
// appelant en sous-estime la duree.

#include <memory>
#include <string>

#include <benchmark/benchmark.h>

#include <cgarchi/bar_fusion_parallel.h>
#include <cgarchi/parameterized_gothic.h>
#include <cgarchi/surface_architecture.h>

#include "allocation_counter.h"

using namespace cgarchi;

namespace
{
	// Limite de fils de la fusion des barres pour la duree d'un banc ; la
	// precedente est rendue ensuite. 0 : limite laissee a son defaut.
	class ScopedThreadLimit
	{
	public:
		explicit ScopedThreadLimit (unsigned int threads)
			: m_forced(threads != 0), m_previous(m_forced ? setBarFusionThreadLimit(threads) : 0) {}
		~ScopedThreadLimit () { if (m_forced) setBarFusionThreadLimit(m_previous); }
		ScopedThreadLimit (const ScopedThreadLimit &) = delete;
		ScopedThreadLimit &operator= (const ScopedThreadLimit &) = delete;
	private:
		bool         m_forced;
		unsigned int m_previous;
	};

	// Regenere `window` une fois ; faux, avec le motif dans `state`, si la
	// construction porte une erreur ou ne rend pas de maillage.
	bool RegeneratesCleanly (ParameterizedGothicWindow &window, benchmark::State &state)
	{
		window.Regenerate();
		const Mesh *mesh = window.GetMesh();
		if (window.LastBuildReport().HasErrors() || !mesh || mesh->GetNFaces() <= 1)
		{
			state.SkipWithError("Regenerate rend une erreur ou le triangle de repli");
			return false;
		}
		state.counters["faces"] = benchmark::Counter(static_cast<double>(mesh->GetNFaces()));
		return true;
	}

	void RunRegenerate (benchmark::State &state, const GothicWindowSettings &settings, unsigned int threads)
	{
		const ScopedThreadLimit limit(threads);
		ParameterizedGothicWindow window;
		window.SetSettings(settings);
		if (!RegeneratesCleanly(window, state))
			return;
		MeasureWithAllocations(state, [&window] {
			window.Regenerate();
			benchmark::DoNotOptimize(window.GetMesh());
		});
	}

	GothicWindowSettings RecursionTwo ()
	{
		GothicWindowSettings s;
		s.recursion = 2;
		return s;
	}
}

// Reglages par defaut (profil Flat), limite de fils par defaut.
void BM_GothicWindow_Regenerate_Default (benchmark::State &state)
{
	RunRegenerate(state, GothicWindowSettings(), 0);
}
BENCHMARK(BM_GothicWindow_Regenerate_Default)->Unit(benchmark::kMillisecond)->UseRealTime();

// Recursion 2, le reste par defaut.
void BM_GothicWindow_Regenerate_Recursion2 (benchmark::State &state)
{
	RunRegenerate(state, RecursionTwo(), 0);
}
BENCHMARK(BM_GothicWindow_Regenerate_Recursion2)->Unit(benchmark::kMillisecond)->UseRealTime();

// Barres fusionnees : profil 2 (Roll), 3 (Keel) ou 4 (Ogee), au defaut ou en
// Recursion 2 corps 0, sous 1, 4 ou 8 fils. Ce sont les cas de debt_cgarchi.md
// (Regenerate a barres fusionnees, mesures `costr`).
void BM_GothicWindow_Regenerate_FusedBars (benchmark::State &state)
{
	GothicWindowSettings s;
	s.profile = static_cast<int>(state.range(0));
	if (state.range(1) != 0)
	{
		s.recursion = 2;
		s.bodyHeight = 0.f;
	}
	RunRegenerate(state, s, static_cast<unsigned int>(state.range(2)));
}
BENCHMARK(BM_GothicWindow_Regenerate_FusedBars)
	->ArgNames({"profile", "rec2", "threads"})
	->ArgsProduct({{2, 3, 4}, {0, 1}, {1, 4, 8}})
	->Unit(benchmark::kMillisecond)
	->UseRealTime();

// CreateBlock, arguments par defaut.
void BM_CreateBlock (benchmark::State &state)
{
	MeasureWithAllocations(state, [] {
		std::unique_ptr<Mesh> block = CreateBlock();
		benchmark::DoNotOptimize(block.get());
	});
}
BENCHMARK(BM_CreateBlock)->Unit(benchmark::kMicrosecond);

// Bloc parametre : CreateBlock puis ComputeNormals, comme le catalogue.
void BM_GothicBlock_Regenerate (benchmark::State &state)
{
	ParameterizedGothicBlock block;
	MeasureWithAllocations(state, [&block] {
		block.Regenerate();
		benchmark::DoNotOptimize(block.GetMesh());
	});
}
BENCHMARK(BM_GothicBlock_Regenerate)->Unit(benchmark::kMicrosecond);
