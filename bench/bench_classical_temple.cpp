// Bancs du temple : placements seuls (LayoutTemple) et maillage assemble
// (BuildTempleMesh).
//
// Argument `size` : 0, temple synthetique de 34 colonnes (7 x 12, colonnes de
// cgarchi_test::SyntheticTempleSettings, test/greek_doric_test_support.h) ;
// 1, le meme peristyle a 186 x 187 colonnes (742), point le plus proche de la
// borne kTempleMaxFaces avec ces colonnes ; 2, temple du README (document du
// monument, MonumentTempleSettings), 34 colonnes, sans son entablement ; 3, le
// temple synthetique de size 0 avec l'entablement synthetique
// (SyntheticEntablatureSettings : anneau et 72 triglyphes) ; 4, temple du
// README avec son entablement (anneau et 72 triglyphes). Compteurs : faces du
// maillage, et allocations d'un appel (alloc_bytes : octets demandes, cumules ;
// un majorant de la memoire de pointe).

#include <exception>
#include <initializer_list>
#include <memory>

#include <benchmark/benchmark.h>

#include <cgarchi/classical_monument.h>
#include <cgarchi/classical_temple.h>
#include "mesh.h"
#include "../test/greek_doric_test_support.h"

#include "allocation_counter.h"

using namespace cgarchi::classical;

namespace
{
	TempleSettings Settings (const benchmark::State &state)
	{
		if (state.range(0) == 2 || state.range(0) == 4)
		{
			TempleSettings readme = MonumentTempleSettings(HephaistosMonument());
			if (state.range(0) == 2)
				readme.entablature.reset();
			return readme;
		}
		TempleSettings t = cgarchi_test::SyntheticTempleSettings();
		const bool large = state.range(0) == 1;
		t.peristyle.front = large ? 186 : 7;
		t.peristyle.flank = large ? 187 : 12;
		const PeristyleLayout p = LayoutPeristyle(t.peristyle);
		t.krepis.stylobateHalfWidth = p.halfWidth + 0.61;
		t.krepis.stylobateHalfDepth = p.halfDepth + 0.67;
		if (state.range(0) == 3)
			t.entablature = cgarchi_test::SyntheticEntablatureSettings();
		return t;
	}

	// Vrai si les reglages se lisent (document du monument compris) ; sinon, motif dans `state`.
	bool SettingsOf (benchmark::State &state, TempleSettings &out)
	{
		try
		{
			out = Settings(state);
			return true;
		}
		catch (const std::exception &e)
		{
			state.SkipWithError(e.what());
			return false;
		}
	}

	// Vrai si le temple se construit ; sinon, motif dans `state`.
	bool BuildsCleanly (benchmark::State &state, const TempleSettings &t)
	{
		try
		{
			const std::unique_ptr<Mesh> mesh = BuildTempleMesh(t);
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

void BM_ClassicalTemple_Layout (benchmark::State &state)
{
	TempleSettings t;
	if (!SettingsOf(state, t))
		return;
	MeasureWithAllocations(state, [&] {
		TempleLayout layout = LayoutTemple(t);
		benchmark::DoNotOptimize(layout);
	});
}
BENCHMARK(BM_ClassicalTemple_Layout)->ArgName("size")->Arg(0)->Arg(1)->Arg(2)->Arg(3)->Arg(4)->Unit(benchmark::kMicrosecond);

void BM_ClassicalTemple_Mesh (benchmark::State &state)
{
	TempleSettings t;
	if (!SettingsOf(state, t) || !BuildsCleanly(state, t))
		return;
	MeasureWithAllocations(state, [&] {
		const std::unique_ptr<Mesh> mesh = BuildTempleMesh(t);
		benchmark::DoNotOptimize(mesh->GetNFaces());
	});
}
BENCHMARK(BM_ClassicalTemple_Mesh)->ArgName("size")->Arg(0)->Arg(1)->Arg(2)->Arg(3)->Arg(4)->Unit(benchmark::kMillisecond);

// Cout propre de l'entablement du point size 3 (7 x 12 synthetique), hors
// temple : argument `part` 0, l'anneau seul (SweepRectangularRing de la
// section) ; 1, les 72 prismes seuls (BuildTriglyphMesh) ; 2, les deux ; 3, les
// ornements seuls de l'entablement SyntheticOrnamentSettings (72 regulae et
// leurs 432 gouttes, 148 mutules), une validation par piece (BuildOrnamentMesh).
// Placements calcules une fois, hors de la mesure.
void BM_GreekEntablature_Pieces (benchmark::State &state)
{
	TempleSettings t = cgarchi_test::SyntheticTempleSettings();
	t.peristyle.front = 7;
	t.peristyle.flank = 12;
	const PeristyleLayout p = LayoutPeristyle(t.peristyle);
	t.krepis.stylobateHalfWidth = p.halfWidth + 0.61;
	t.krepis.stylobateHalfDepth = p.halfDepth + 0.67;
	const bool ornaments = state.range(0) == 3;
	t.entablature = ornaments ? cgarchi_test::SyntheticOrnamentSettings() : cgarchi_test::SyntheticEntablatureSettings();
	const TempleLayout layout = LayoutTemple(t);
	const GreekEntablatureSettings &e = *t.entablature;
	const bool ring = state.range(0) == 0 || state.range(0) == 2, prisms = state.range(0) == 1 || state.range(0) == 2;
	std::size_t faces = 0;
	MeasureWithAllocations(state, [&] {
		faces = 0;
		if (ornaments)
			for (const auto *list : {&layout.ornaments->regulae, &layout.ornaments->mutules})
				for (const OrnamentPiece &o : *list)
					faces += BuildOrnamentMesh(e, layout.peristyle, *layout.ornaments, o, layout.entablatureFoot)->GetNFaces();
		if (ring)
		{
			const std::unique_ptr<Mesh> m = SweepRectangularRing(GreekEntablatureSection(e), layout.peristyle.halfWidth,
			                                                     layout.peristyle.halfDepth, layout.entablatureFoot);
			faces += m->GetNFaces();
		}
		if (prisms)
			for (const FriezePiece &q : layout.frieze->triglyphs)
				faces += BuildTriglyphMesh(e, layout.peristyle, q, layout.entablatureFoot)->GetNFaces();
		benchmark::DoNotOptimize(faces);
	});
	state.counters["faces"] = benchmark::Counter(static_cast<double>(faces));
}
BENCHMARK(BM_GreekEntablature_Pieces)->ArgName("part")->Arg(0)->Arg(1)->Arg(2)->Arg(3)->Unit(benchmark::kMicrosecond);
