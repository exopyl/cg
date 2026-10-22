#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <cgarchi/bar_fusion.h>
#include <cgarchi/bar_fusion_parallel.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/parameterized.h"
#include <cgarchi/parameterized_gothic.h>
#include "../src/cgmesh/polygon2.h"
#include "../src/cgmesh/profile2d.h"

using namespace cgarchi;   // fichier de test de cgarchi seul

// ===========================================================================
//  Fusion des barres : execution parallele
// ===========================================================================
//
// Deux familles de contrats : ceux de barFusionParallelFor (chaque tache une
// fois, plafond de fils par appel et pour le processus, exception du plus
// petit indice), et l'independance de la sortie de la fusion vis-a-vis du
// nombre de fils, octet par octet.

namespace
{
	using namespace std::chrono_literals;

	// Limite de fils forcee pour la duree d'un test ; la precedente est
	// rendue ensuite.
	struct ForcedThreads
	{
		explicit ForcedThreads(unsigned int n) : previous(setBarFusionThreadLimit(n)) {}
		~ForcedThreads() { setBarFusionThreadLimit(previous); }
		const unsigned int previous;
		ForcedThreads(const ForcedThreads &) = delete;
		ForcedThreads &operator=(const ForcedThreads &) = delete;
	};

	// Taches assez longues pour se chevaucher : compte le plus grand nombre
	// de taches simultanees.
	struct Overlap
	{
		std::atomic<int> active{0};
		std::atomic<int> peak{0};

		void run(std::chrono::milliseconds d)
		{
			const int now = ++active;
			int p = peak.load();
			while (now > p && !peak.compare_exchange_weak(p, now)) {}
			std::this_thread::sleep_for(d);
			--active;
		}
	};

	using Ring = std::vector<Vector2d>;

	Ring rect(double x0, double y0, double x1, double y1)
	{
		return {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
	}

	Ring reversed(Ring r) { return Ring(r.rbegin(), r.rend()); }

	Ring circleRing(double cx, double cy, double r, int n)
	{
		Ring ring;
		for (int k = 0; k < n; ++k)
		{
			const double t = 2.0 * 3.14159265358979323846 * k / n;
			ring.push_back({cx + r * std::cos(t), cy + r * std::sin(t)});
		}
		return ring;
	}

	Polygon2 plateOf(const std::vector<Ring> &rings)
	{
		Polygon2 p;
		p.alloc_contours((int)rings.size());
		for (size_t c = 0; c < rings.size(); ++c)
		{
			std::vector<float> f;
			for (const Vector2d &v : rings[c])
			{
				f.push_back((float)v.x);
				f.push_back((float)v.y);
			}
			p.add_contour((unsigned int)c, (unsigned int)rings[c].size(), f.data());
		}
		return p;
	}

	void setParam(IParameterized &shape, const std::string &name, double value)
	{
		for (Parameter &p : shape.GetParameters())
		{
			if (p.GetName() != name)
				continue;
			switch (p.GetType())
			{
			case Parameter::FLOAT: p.SetFloat(static_cast<float>(value)); return;
			case Parameter::INT:
			case Parameter::ENUM:  p.SetInt(static_cast<int>(value)); return;
			case Parameter::BOOL:  p.SetBool(value != 0.0); return;
			case Parameter::STRING: break;
			}
			ADD_FAILURE() << "parametre non numerique : " << name;
			return;
		}
		ADD_FAILURE() << "parametre inconnu : " << name;
	}

	// Plaque percee de deux cercles dont les barres se recouvrent et d'une
	// ouverture au bord, dont la barre deborde : plusieurs niveaux, noeud
	// plaque x empreinte, anneaux et capot.
	struct FusionCase
	{
		Polygon2 plate;
		std::vector<FusedBar> bars;
	};

	FusionCase closeBars()
	{
		const Ring big = circleRing(-30, 0, 20, 96), small = circleRing(8, 0, 10, 64), edge = rect(50, -20, 75, 20);
		return {plateOf({rect(-80, -80, 80, 80), reversed(big), reversed(small), reversed(edge)}),
		        {{big, 1.0}, {small, 0.6}, {edge, 1.0}}};
	}

	// Seize ouvertures rondes de rayons et d'echelles varies sur une plaque
	// carree : niveaux nombreux, dont plusieurs repares.
	FusionCase gridOfBars()
	{
		std::vector<Ring> rings = {rect(-100, -100, 100, 100)};
		std::vector<FusedBar> bars;
		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < 4; ++j)
			{
				const Ring c = circleRing(-75 + 50 * i, -75 + 50 * j, 18 + 3 * ((i + j) % 3), 48);
				rings.push_back(reversed(c));
				bars.push_back({c, 0.5 + 0.15 * ((i * 4 + j) % 4)});
			}
		return {plateOf(rings), bars};
	}

	struct FusedShell
	{
		std::vector<float> vertices;
		std::vector<unsigned int> triangles;
		FusedBarsResult result;
	};

	FusedShell fuse(const FusionCase &c, const Profile2D &profile)
	{
		Mesh m;
		FusedShell s;
		s.result = fuseBarsOnPlate(c.plate, c.bars, profile, 20.0, m);
		s.vertices = m.GetVertices();
		s.triangles = m.GetTriangles();
		return s;
	}
}

TEST(TEST_cgarchi_bar_fusion_parallel, every_index_runs_once_whatever_the_thread_count)
{
	for (const unsigned int threads : {1u, 2u, 3u, kBarFusionMaxThreads})
	{
		const ForcedThreads forced(threads);
		for (const size_t n : {size_t(0), size_t(1), size_t(5), size_t(64)})
		{
			std::vector<std::atomic<int>> hits(n);
			barFusionParallelFor(n, [&](size_t i) { ++hits[i]; });
			for (size_t i = 0; i < n; ++i)
				EXPECT_EQ(hits[i].load(), 1) << "fils " << threads << ", n " << n << ", i " << i;
		}
	}
}

TEST(TEST_cgarchi_bar_fusion_parallel, thread_limit_is_clamped_to_the_cap)
{
	{
		const ForcedThreads forced(1);
		EXPECT_EQ(barFusionThreadLimit(), 1u);
	}
	{
		const ForcedThreads forced(1000);
		EXPECT_EQ(barFusionThreadLimit(), kBarFusionMaxThreads);
	}
	const unsigned int byDefault = barFusionThreadLimit();
	EXPECT_GE(byDefault, 1u);
	EXPECT_LE(byDefault, kBarFusionMaxThreads);
}

// Une limite de L fils : jamais plus de L taches simultanees, et au moins
// deux des que L >= 2 (le travail est reellement reparti).
TEST(TEST_cgarchi_bar_fusion_parallel, a_call_never_exceeds_the_thread_limit)
{
	for (const unsigned int threads : {1u, 2u, 4u})
	{
		const ForcedThreads forced(threads);
		Overlap o;
		barFusionParallelFor(12, [&](size_t) { o.run(2ms); });
		EXPECT_LE(o.peak.load(), (int)threads);
		if (threads >= 2)
			EXPECT_GE(o.peak.load(), 2);
		else
			EXPECT_EQ(o.peak.load(), 1);
	}
}

TEST(TEST_cgarchi_bar_fusion_parallel, default_limit_never_exceeds_the_cap)
{
	Overlap o;
	barFusionParallelFor(24, [&](size_t) { o.run(2ms); });
	EXPECT_LE(o.peak.load(), (int)kBarFusionMaxThreads);
	EXPECT_LE(o.peak.load(), (int)barFusionThreadLimit());
}

// Deux appels simultanes se partagent le budget de L - 1 fils auxiliaires :
// au plus (L - 1) + 2 taches simultanees, et non 2 L.
TEST(TEST_cgarchi_bar_fusion_parallel, simultaneous_calls_share_one_budget)
{
	const unsigned int threads = 3;
	const ForcedThreads forced(threads);
	Overlap o;
	std::atomic<int> started{0};
	auto call = [&]() {
		++started;
		while (started.load() < 2) std::this_thread::yield();
		barFusionParallelFor(12, [&](size_t) { o.run(3ms); });
	};
	std::thread a(call), b(call);
	a.join();
	b.join();
	EXPECT_LE(o.peak.load(), (int)(threads - 1 + 2));
}

// Les taches 2 et 9 levent ; la 9 la premiere dans le temps. L'exception
// relancee est celle de la tache 2, comme en sequentiel, et les taches qui
// la precedent ont toutes tourne.
TEST(TEST_cgarchi_bar_fusion_parallel, the_lowest_failing_index_is_rethrown)
{
	for (const unsigned int threads : {1u, 2u, kBarFusionMaxThreads})
	{
		const ForcedThreads forced(threads);
		std::vector<std::atomic<int>> ran(16);
		std::string what;
		try
		{
			barFusionParallelFor(16, [&](size_t i) {
				if (i == 2)
				{
					std::this_thread::sleep_for(20ms);
					throw std::runtime_error("task 2");
				}
				if (i == 9)
					throw std::runtime_error("task 9");
				++ran[i];
			});
		}
		catch (const std::runtime_error &e)
		{
			what = e.what();
		}
		EXPECT_EQ(what, "task 2") << "fils " << threads;
		EXPECT_EQ(ran[0].load(), 1);
		EXPECT_EQ(ran[1].load(), 1);
	}
}

// Niveaux 2D, issue de l'imbrication et coque fusionnee : identiques a
// l'octet sur 1, 2 et 8 fils. La grille d'ouvertures passe par la
// reparation : le chemin de reprise de la chaine d'imbrication est couvert.
TEST(TEST_cgarchi_bar_fusion_parallel, thread_count_does_not_change_the_fused_shell)
{
	for (const bool grid : {false, true})
	{
		const FusionCase c = grid ? gridOfBars() : closeBars();
		int repairedOrDropped = 0;
		for (const Profile2D &profile : {rollBarProfile(5.0), keelBarProfile(5.0), ogeeBarProfile(5.0)})
		{
			std::vector<std::vector<BarFusionLevel>> levels;
			std::vector<BarFusionReport> reports;
			std::vector<FusedShell> shells;
			for (const unsigned int threads : {1u, 2u, 8u})
			{
				const ForcedThreads forced(threads);
				BarFusionReport report;
				levels.push_back(buildBarFusionLevels(c.bars, profile, &report));
				reports.push_back(report);
				shells.push_back(fuse(c, profile));
			}
			ASSERT_FALSE(shells[0].result.failed);
			ASSERT_GE(levels[0].size(), 5u);
			repairedOrDropped += reports[0].repairedLevels + reports[0].droppedLevels;
			for (size_t t = 1; t < shells.size(); ++t)
			{
				ASSERT_EQ(levels[t].size(), levels[0].size());
				for (size_t k = 0; k < levels[0].size(); ++k)
				{
					EXPECT_EQ(levels[t][k].height, levels[0][k].height);
					ASSERT_EQ(levels[t][k].region.size(), levels[0][k].region.size());
					for (size_t r = 0; r < levels[0][k].region.size(); ++r)
					{
						ASSERT_EQ(levels[t][k].region[r].size(), levels[0][k].region[r].size());
						for (size_t q = 0; q < levels[0][k].region[r].size(); ++q)
						{
							EXPECT_EQ(levels[t][k].region[r][q].x, levels[0][k].region[r][q].x);
							EXPECT_EQ(levels[t][k].region[r][q].y, levels[0][k].region[r][q].y);
						}
					}
				}
				EXPECT_EQ(reports[t].repairedLevels, reports[0].repairedLevels);
				EXPECT_EQ(reports[t].droppedLevels, reports[0].droppedLevels);
				EXPECT_EQ(shells[t].vertices, shells[0].vertices);
				EXPECT_EQ(shells[t].triangles, shells[0].triangles);
				EXPECT_EQ(shells[t].result.levels, shells[0].result.levels);
				EXPECT_EQ(shells[t].result.skippedLevels, shells[0].result.skippedLevels);
				EXPECT_EQ(shells[t].result.nodeInsertions, shells[0].result.nodeInsertions);
				EXPECT_EQ(shells[t].result.delaunayComplete, shells[0].result.delaunayComplete);
			}
		}
		if (grid)
			EXPECT_GT(repairedOrDropped, 0);
	}
}

// Fenetre par defaut et Recursion 2 corps 0, profils Roll, Keel et Ogee :
// maillage identique a l'octet sur 1 et 8 fils.
TEST(TEST_cgarchi_bar_fusion_parallel, thread_count_does_not_change_the_gothic_window)
{
	for (const int profile : {2, 3, 4})
		for (const bool rec2 : {false, true})
		{
			std::vector<std::vector<float>> vertices;
			std::vector<std::vector<unsigned int>> triangles;
			for (const unsigned int threads : {1u, 8u})
			{
				const ForcedThreads forced(threads);
				ParameterizedGothicWindow w;
				setParam(w, "Profile", profile);
				if (rec2)
				{
					setParam(w, "Recursion", 2);
					setParam(w, "Body height", 0);
				}
				w.Regenerate();
				std::unique_ptr<Mesh> m(w.TakeMesh());
				ASSERT_TRUE(m);
				ASSERT_GT(m->GetNFaces(), 0u);
				vertices.push_back(m->GetVertices());
				triangles.push_back(m->GetTriangles());
			}
			EXPECT_EQ(vertices[1], vertices[0]) << "profil " << profile << (rec2 ? " rec2" : "");
			EXPECT_EQ(triangles[1], triangles[0]) << "profil " << profile << (rec2 ? " rec2" : "");
		}
}

// Versement dans l'ordre de l'assemblage : fond (z = 0), plan zTop, parois,
// puis anneaux par niveau croissant et capot. Chaque etape ne cree que des
// sommets de sa cote la plus haute : numerotes dans l'ordre de leur
// creation, les sommets ont des cotes croissantes au sens large, quel que
// soit le nombre de fils.
TEST(TEST_cgarchi_bar_fusion_parallel, fused_shell_vertices_are_numbered_by_increasing_height)
{
	for (const FusionCase &c : {closeBars(), gridOfBars()})
		for (const Profile2D &profile : {rollBarProfile(5.0), keelBarProfile(5.0), ogeeBarProfile(5.0)})
			for (const unsigned int threads : {1u, 8u})
			{
				const ForcedThreads forced(threads);
				const FusedShell s = fuse(c, profile);
				ASSERT_FALSE(s.result.failed);
				ASSERT_GE(s.result.levels, 3);
				size_t descents = 0;
				for (size_t i = 5; i < s.vertices.size(); i += 3)
					if (s.vertices[i] < s.vertices[i - 3])
						++descents;
				EXPECT_EQ(descents, 0u) << "fils " << threads;
				EXPECT_EQ(s.vertices[2], 0.0f);
				EXPECT_GT(s.vertices.back(), 20.0f);
			}
}

// Etape calculee d'avance : get () rend la valeur, ou relance l'exception
// de l'etape, a chaque lecture ; run () ne laisse rien echapper, et une
// etape en echec jamais lue ne leve pas.
TEST(TEST_cgarchi_bar_fusion_parallel, deferred_step_rethrows_its_exception_when_read)
{
	BarFusionDeferred<int> ok;
	ok.run([] { return 7; });
	EXPECT_FALSE(ok.error);
	EXPECT_EQ(ok.get(), 7);

	BarFusionDeferred<int> failed;
	EXPECT_NO_THROW(failed.run([]() -> int { throw std::runtime_error("step"); }));
	ASSERT_TRUE(failed.error);
	for (int read = 0; read < 2; ++read)
	{
		std::string what;
		try
		{
			failed.get();
		}
		catch (const std::runtime_error &e)
		{
			what = e.what();
		}
		EXPECT_EQ(what, "step");
	}

	EXPECT_NO_THROW({
		BarFusionDeferred<int> unread;
		unread.run([]() -> int { throw std::runtime_error("never read"); });
	});
}
