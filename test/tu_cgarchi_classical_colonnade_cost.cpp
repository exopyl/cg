#if defined (_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <cgarchi/classical_colonnade.h>
#include <cgarchi/classical_orders.h>
#include <cgarchi/parameterized_classical.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_io.h"
#include "colonnade_test_support.h"

// Cout de la colonnade corinthienne en fonction du nombre de colonnes N et du niveau des ornements du chapiteau :
// faces, durees de Regenerate et de ses etapes, part de l'assemblage des copies, crete memoire ; empreintes des
// maillages de reference de l'assemblage.

using namespace cgarchi;
using namespace cgarchi::classical;

namespace
{

// Mediane de `runs` mesures (ms), apres un appel de chauffe.
double Median (int runs, const std::function<void ()> &f)
{
	f ();
	std::vector<double> ms;
	ms.reserve (static_cast<std::size_t> (runs));
	for (int i = 0; i < runs; ++i)
	{
		const auto t0 = std::chrono::steady_clock::now ();
		f ();
		ms.push_back (std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now () - t0).count ());
	}
	std::sort (ms.begin (), ms.end ());
	return ms[static_cast<std::size_t> (runs) / 2];
}

// Crete de la memoire residente du processus (Mo).
double PeakMegabytes ()
{
#if defined (_WIN32)
	PROCESS_MEMORY_COUNTERS counters {};
	if (!GetProcessMemoryInfo (GetCurrentProcess (), &counters, sizeof counters))
		return -1.0;
	return static_cast<double> (counters.PeakWorkingSetSize) / (1024.0 * 1024.0);
#else
	rusage usage {};
	if (getrusage (RUSAGE_SELF, &usage) != 0)
		return -1.0;
	return static_cast<double> (usage.ru_maxrss) / 1024.0;
#endif
}

std::string Environment (const char *name)
{
#if defined (_MSC_VER)
	char *value = nullptr;
	std::size_t length = 0;
	if (_dupenv_s (&value, &length, name) != 0 || value == nullptr)
		return {};
	const std::string text (value);
	std::free (value);
	return text;
#else
	const char *value = std::getenv (name);
	return value ? std::string (value) : std::string ();
#endif
}

std::unique_ptr<ParameterizedClassicalColonnade> MakeColonnade (int order, int columns, int level)
{
	auto colonnade = std::make_unique<ParameterizedClassicalColonnade> ();
	std::vector<Parameter> ps = colonnade->GetParameters ();
	for (Parameter &p : ps)
	{
		if (p.GetName () == "Order") p.SetInt (order);
		if (p.GetName () == "Columns") p.SetInt (columns);
		if (p.GetName () == "Capital ornaments") p.SetInt (level);
	}
	colonnade->Regenerate ();
	return colonnade;
}

// FNV-1a 64 bits des comptes, des sommets float (octets) et des indices des faces.
std::uint64_t Fingerprint (const Mesh &mesh)
{
	std::uint64_t h = 1469598103934665603ULL;
	const auto add = [&h] (const void *data, std::size_t n) {
		const unsigned char *b = static_cast<const unsigned char *> (data);
		for (std::size_t i = 0; i < n; ++i)
			h = (h ^ b[i]) * 1099511628211ULL;
	};
	const int nv = mesh.GetNVertices (), nf = mesh.GetNFaces ();
	add (&nv, sizeof nv);
	add (&nf, sizeof nf);
	for (int v = 0; v < nv; ++v)
	{
		float p[3] {};
		mesh.GetVertex (static_cast<unsigned> (v), p);
		add (p, sizeof p);
	}
	for (int f = 0; f < nf; ++f)
		for (int k = 0; k < 3; ++k)
		{
			const int index = mesh.GetFaceVertex (static_cast<unsigned> (f), k);
			add (&index, sizeof index);
		}
	return h;
}

const char *LevelName (int level)
{
	return level == 0 ? "None" : level == 1 ? "Light" : "Heavy";
}

} // namespace

// Instrument, hors suite (--gtest_also_run_disabled_tests). Grille N = 1, 2, 3, 4, 6, 8, 12, niveaux Light et Heavy,
// ordre corinthien, reglages par defaut : faces, sommets, banniere ; medianes (21 mesures si N <= 4, 5 sinon) de
// Regenerate, de la colonne seule (BuildClassicalColumnMesh), de l'assemblage (AssembleColonnadeMesh du produit, sur
// les pieces de colonnade_test::MakeColonnadePieces : N copies, entablement et ornements) et des normales ; part de
// l'assemblage = assemblage / Regenerate ; E (N) = faces - N x colonne. Puis les empreintes FNV-1a de reference de
// l'assemblage : 5 ordres x N = 1, 3, 12 (Capital ornaments par defaut), corinthien None, Light, Heavy x N = 1, 4.
// CG_COLONNADE_CASE = "niveau,N" : ce seul cas, une generation, crete memoire du processus (un processus par cas).
// CG_COLONNADE_DIR : CSV de la grille et GLB de la colonnade a N = 4, Light et Heavy.
TEST (TEST_cgarchi_classical_colonnade_cost, DISABLED_corinthian_colonnade_cost)
{
	const int corinthian = static_cast<int> (Order::Corinthian);
	const std::string single = Environment ("CG_COLONNADE_CASE");
	if (!single.empty ())
	{
		int level = 0, columns = 0;
		char comma = 0;
		std::istringstream fields (single);
		ASSERT_TRUE ((fields >> level >> comma >> columns) && comma == ',') << single;
		const double before = PeakMegabytes ();
		const auto colonnade = MakeColonnade (corinthian, columns, level);
		ASSERT_TRUE (colonnade->LastError ().empty ()) << colonnade->LastError ();
		std::printf ("CRETE %s,%d,%u,%.1f,%.1f\n", LevelName (level), columns, colonnade->GetMesh ()->GetNFaces (), before,
		             PeakMegabytes ());
		return;
	}

	const std::string dir = Environment ("CG_COLONNADE_DIR");
	std::ostringstream csv;
	csv << "niveau,N,faces,sommets,E_N,banniere,regenerate_ms,colonne_ms,assemblage_ms,normales_ms,part_assemblage\n";

	for (const int level : {1, 2})
		for (const int columns : {1, 2, 3, 4, 6, 8, 12})
		{
			const int runs = columns <= 4 ? 21 : 5;
			const auto colonnade = MakeColonnade (corinthian, columns, level);
			ASSERT_TRUE (colonnade->LastError ().empty ()) << colonnade->LastError ();
			const unsigned faces = static_cast<unsigned> (colonnade->GetMesh ()->GetNFaces ());
			const unsigned vertices = static_cast<unsigned> (colonnade->GetMesh ()->GetNVertices ());
			const bool banner = !colonnade->Warnings ().empty ();
			if (!dir.empty () && columns == 4)
				EXPECT_EQ (MeshIO::export_glb (*colonnade->GetMesh (), (dir + "/colonnade_4_" + LevelName (level) + ".glb").c_str ()), 0);
			const double regenerate = Median (runs, [&] { colonnade->Regenerate (); });

			ClassicalColumnSettings s;
			s.order = corinthian;
			s.capitalOrnaments = level;
			const double columnTime = Median (runs, [&] { BuildClassicalColumnMesh (s); });
			const colonnade_test::ColonnadePieces pieces = colonnade_test::MakeColonnadePieces (corinthian, columns, level);
			const std::vector<const Mesh *> rest = pieces.Rest ();
			const double assembly = Median (runs, [&] { AssembleColonnadeMesh (*pieces.column, pieces.axes, pieces.scale, rest); });
			const double normals = Median (runs, [&] { colonnade->GetMesh ()->ComputeNormals (); });
			const unsigned entablature = faces - static_cast<unsigned> (columns) * static_cast<unsigned> (pieces.column->GetNFaces ());
			char row[256];
			std::snprintf (row, sizeof row, "%s,%d,%u,%u,%u,%d,%.3f,%.3f,%.3f,%.3f,%.3f\n", LevelName (level), columns, faces, vertices,
			               entablature, banner ? 1 : 0, regenerate, columnTime, assembly, normals, assembly / regenerate);
			csv << row;
			std::printf ("COUT %s", row);
			std::fflush (stdout);
		}

	struct Reference
	{
		int order, level, columns;
	};
	std::vector<Reference> references;
	for (int o = 0; o < kOrderCount; ++o)
		for (const int columns : {1, 3, 12})
			references.push_back ({o, -1, columns});
	for (const int level : {0, 1, 2})
		for (const int columns : {1, 4})
			references.push_back ({corinthian, level, columns});
	for (const Reference &r : references)
	{
		auto colonnade = std::make_unique<ParameterizedClassicalColonnade> ();
		std::vector<Parameter> ps = colonnade->GetParameters ();
		for (Parameter &p : ps)
		{
			if (p.GetName () == "Order") p.SetInt (r.order);
			if (p.GetName () == "Columns") p.SetInt (r.columns);
			if (r.level >= 0 && p.GetName () == "Capital ornaments") p.SetInt (r.level);
		}
		colonnade->Regenerate ();
		ASSERT_TRUE (colonnade->LastError ().empty ()) << colonnade->LastError ();
		std::printf ("FNV ordre %d, ornements %s, %d colonnes : %u faces, %016llx\n", r.order, r.level < 0 ? "defaut" : LevelName (r.level),
		             r.columns, colonnade->GetMesh ()->GetNFaces (),
		             static_cast<unsigned long long> (Fingerprint (*colonnade->GetMesh ())));
	}
	if (!dir.empty ())
	{
		std::ofstream out (dir + "/colonnade_cost.csv");
		out << csv.str ();
	}
}
