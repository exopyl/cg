// Bancs de la colonne dorique grecque : profil seul
// (BuildGreekDoricColumnProfile) et maillage complet
// (BuildGreekDoricColumnMesh).
//
// Argument `resolution` : 0, jeu d'essai synthetique
// (cgarchi_test::SyntheticGreekDoricColumnSettings,
// test/greek_doric_test_support.h) ; 1, le meme a la resolution maximale
// (bornes kGreekDoricMax* : cannelures, echantillons, segments du fut et de
// la levre de l'echine, listels alternes en gradins ; section B, qui ajoute
// un meridien de paroi par cannelure, et rainure creusee sous ses cannelures) ;
// 2, le jeu synthetique en section a trois centres
// (cgarchi_test::ThreeCentredFlutes, rapport 0,19), qui ajoute le point de
// tangence a la grille de chaque demi-cannelure.

#include <exception>
#include <memory>

#include <benchmark/benchmark.h>

#include <cgarchi/classical_greek_doric_column.h>
#include "mesh.h"
#include "../test/greek_doric_test_support.h"

#include "allocation_counter.h"

using namespace cgarchi::classical;

namespace
{
	GreekDoricColumnSettings Settings (const benchmark::State &state)
	{
		GreekDoricColumnSettings s = cgarchi_test::SyntheticGreekDoricColumnSettings();
		if (state.range(0) == 1)
		{
			s.fluteCount = kGreekDoricMaxFlutes;
			s.fluteSamples = kGreekDoricMaxFluteSamples;
			s.shaftSegments = kGreekDoricMaxShaftSegments;
			s.curveSegments = kGreekDoricMaxCurveSegments;
			// Listels alternes : chaque jonction est un gradin.
			const GreekDoricAnnulet narrow = s.annulets.front(), wide = s.annulets.back();
			s.annulets.clear();
			for (int k = 0; k < kGreekDoricMaxAnnulets; ++k)
				s.annulets.push_back(k % 2 == 0 ? narrow : wide);
			s.fluteSection = FluteSection::Semicircle;
			cgarchi_test::FitGrooveUnderFlutes(s);
		}
		else if (state.range(0) == 2)
			cgarchi_test::ThreeCentredFlutes(s);
		return s;
	}

	// Vrai si la colonne se construit ; sinon, motif dans `state`. `withFaces`
	// publie le nombre de faces du maillage.
	bool BuildsCleanly (benchmark::State &state, const GreekDoricColumnSettings &s, bool withFaces)
	{
		try
		{
			const std::unique_ptr<Mesh> mesh = BuildGreekDoricColumnMesh(s);
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

void BM_GreekDoricColumn_Profile (benchmark::State &state)
{
	const GreekDoricColumnSettings s = Settings(state);
	if (!BuildsCleanly(state, s, false))
		return;
	MeasureWithAllocations(state, [&] {
		ColumnProfile profile = BuildGreekDoricColumnProfile(s);
		benchmark::DoNotOptimize(profile);
	});
}
BENCHMARK(BM_GreekDoricColumn_Profile)->ArgName("resolution")->Arg(0)->Arg(1)->Arg(2)->Unit(benchmark::kMicrosecond);

void BM_GreekDoricColumn_Mesh (benchmark::State &state)
{
	const GreekDoricColumnSettings s = Settings(state);
	if (!BuildsCleanly(state, s, true))
		return;
	MeasureWithAllocations(state, [&] {
		const std::unique_ptr<Mesh> mesh = BuildGreekDoricColumnMesh(s);
		benchmark::DoNotOptimize(mesh->GetNFaces());
	});
}
BENCHMARK(BM_GreekDoricColumn_Mesh)->ArgName("resolution")->Arg(0)->Arg(1)->Arg(2)->Unit(benchmark::kMicrosecond);
