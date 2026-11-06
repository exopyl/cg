// Bancs de tessellation et d'extrusion d'une region 2D (cgmesh), sur la pierre de
// la fenetre gothique : la region que Regenerate extrude, a l'echelle monde de la
// baie par defaut (largeur 200).
//
// Argument `rec2` : 0 = reglages par defaut, 1 = Recursion 2.

#include <exception>
#include <stdexcept>

#include <benchmark/benchmark.h>

#include <cgarchi/parameterized_gothic.h>
#include "mesh.h"
#include "polygon2.h"
#include "polygon_extrude.h"
#include "profile2d.h"

#include "allocation_counter.h"

using namespace cgarchi;

namespace
{
	// Region de pierre ; leve si elle est vide ou en erreur. Appelee par
	// StoneOrSkip seulement : une exception sortie d'une fonction de banc
	// terminerait toute la passe.
	Polygon2 StonePolygon (bool recursionTwo)
	{
		ParameterizedGothicWindow window;
		GothicWindowSettings s;
		if (recursionTwo)
			s.recursion = 2;
		window.SetSettings(s);
		GothicBuildReport report;
		Polygon2 poly = window.BuildStonePolygon(&report);
		if (report.HasErrors() || poly.get_n_contours() == 0)
			throw std::runtime_error("region de pierre vide ou en erreur");
		return poly;
	}

	// Region de pierre dans `poly` ; faux, avec le motif dans `state`, si elle
	// ne se construit pas.
	bool StoneOrSkip (benchmark::State &state, Polygon2 &poly)
	{
		try
		{
			poly = StonePolygon(state.range(0) != 0);
			return true;
		}
		catch (const std::exception &e)
		{
			state.SkipWithError(e.what());
			return false;
		}
	}

	void PublishShape (benchmark::State &state, const Polygon2 &poly, const Mesh &mesh)
	{
		state.counters["contours"] = benchmark::Counter(static_cast<double>(poly.get_n_contours()));
		state.counters["faces"] = benchmark::Counter(static_cast<double>(mesh.GetNFaces()));
	}
}

// Tessellation de la region (GLU), rangee dans un Mesh plat.
void BM_StonePolygon_Tessellate (benchmark::State &state)
{
	Polygon2 poly;
	if (!StoneOrSkip(state, poly))
		return;
	Mesh mesh;
	MeasureWithAllocations(state, [&] {
		tessellateToMesh(poly, mesh, 0.0);
		benchmark::DoNotOptimize(mesh.GetNFaces());
	});
	PublishShape(state, poly, mesh);
}
BENCHMARK(BM_StonePolygon_Tessellate)->ArgName("rec2")->Arg(0)->Arg(1)->Unit(benchmark::kMillisecond);

// Extrusion droite (parois verticales) entre z = 0 et z = 20.
void BM_StonePolygon_Extrude (benchmark::State &state)
{
	Polygon2 poly;
	if (!StoneOrSkip(state, poly))
		return;
	Mesh mesh;
	MeasureWithAllocations(state, [&] {
		extrudeToMesh(poly, mesh, 0.0, 20.0);
		benchmark::DoNotOptimize(mesh.GetNFaces());
	});
	PublishShape(state, poly, mesh);
}
BENCHMARK(BM_StonePolygon_Extrude)->ArgName("rec2")->Arg(0)->Arg(1)->Unit(benchmark::kMillisecond);

// Extrusion profilee entre z = 0 et z = 20. Argument `profile` : 0 = chanfrein
// (2 x 4, deux points), 1 = cavet (2 x 4, 6 segments).
void BM_StonePolygon_ExtrudeProfiled (benchmark::State &state)
{
	Polygon2 poly;
	if (!StoneOrSkip(state, poly))
		return;
	const Profile2D profile = state.range(1) == 0 ? chamferSplayProfile(2.0, 4.0)
	                                              : cavettoSplayProfile(2.0, 4.0);
	Mesh mesh;
	MeasureWithAllocations(state, [&] {
		extrudeProfiledToMesh(poly, profile, mesh, 0.0, 20.0);
		benchmark::DoNotOptimize(mesh.GetNFaces());
	});
	PublishShape(state, poly, mesh);
}
BENCHMARK(BM_StonePolygon_ExtrudeProfiled)
	->ArgNames({"rec2", "profile"})
	->ArgsProduct({{0, 1}, {0, 1}})
	->Unit(benchmark::kMillisecond);
