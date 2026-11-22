#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <cgarchi/classical_acanthus.h>
#include <cgarchi/classical_acanthus_detail.h>
#include <cgarchi/classical_acanthus_template.h>
#include <cgarchi/classical_lathe.h>
#include <cgarchi/classical_mesh.h>
#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_profile.h>
#include <cgarchi/parameterized_classical.h>
#include "../src/cgmesh/mesh.h"
#include "acanthus_test_support.h"

using namespace cgarchi;   // fichier de test de cgarchi seul
using namespace cgarchi::classical;

// Mesures de l'ornement corinthien par rang (F, E, C, D, V, H, G) et par niveau de detail (Light, Heavy) : comptes,
// durees, jeu au noyau, debords de la borne du rang et de l'abaque des donnees, couverture de la corbeille, jours.
// Cotes en parties, h depuis le pied du chapiteau.

namespace
{

constexpr double kPi = 3.14159265358979323846;
constexpr int kRanks = 7;
const char *const kRankNames[kRanks] = {"F", "E", "C", "D", "V", "H", "G"};
const CapitalPieceKind kRankKinds[kRanks] = {CapitalPieceKind::LowerLeaf, CapitalPieceKind::MiddleLeaf, CapitalPieceKind::Cauliculus,
                                             CapitalPieceKind::MinorLeaf, CapitalPieceKind::Volute,     CapitalPieceKind::Helix,
                                             CapitalPieceKind::Fleuron};
// Indices des representantes de chaque rang dans CapitalOrnament::pieces (ordre du registre AC-46) : F, E et C la
// piece 0 du rang ; D les quatre poses des tiges 0 et 1 ; V et H les pieces des tiges 0 et 1 ; G la face 0.
const std::vector<std::size_t> kRepresentatives[kRanks] = {{0}, {8}, {16}, {24, 25, 26, 27}, {40, 41}, {48, 49}, {56}};

const char *LevelName (OrnamentDetail d)
{
	return d == OrnamentDetail::Light ? "Light" : "Heavy";
}

const OrderData &Corinthian ()
{
	return ClassicalOrders ().Get (Order::Corinthian);
}

ColumnProfile CorinthianProfile ()
{
	ColumnOptions options;
	options.order = Order::Corinthian;
	return BuildColumnProfile (Corinthian (), options);
}

struct Ornament
{
	ColumnProfile profile;
	detail::CapitalEnvelope env;
	CapitalOrnament ornament;
};

Ornament MakeOrnament (OrnamentDetail level)
{
	Ornament c;
	c.profile = CorinthianProfile ();
	c.env = detail::ReadCapitalEnvelope (Corinthian (), c.profile);
	AcanthusOptions options;
	options.detail = level;
	c.ornament = BuildCorinthianOrnament (Corinthian (), c.profile, options);
	return c;
}

std::vector<CapitalPiece> PiecesOf (const CapitalOrnament &ornament, CapitalPieceKind kind)
{
	std::vector<CapitalPiece> out;
	for (const CapitalPiece &p : ornament.pieces)
		if (p.kind == kind)
			out.push_back (p);
	return out;
}

// Debord de l'abaque des donnees : max (r - EnvelopeRadius (h, theta)) sur les sommets des pieces du rang (borne
// max (noyau, min (cone, contour de l'abaque)), lue par plage de sommets quel que soit le rang).
double AbacusExcess (const Ornament &c, CapitalPieceKind kind)
{
	double excess = -std::numeric_limits<double>::infinity ();
	for (const CapitalPiece &p : PiecesOf (c.ornament, kind))
		excess = std::max (excess, detail::EnvelopeExcess (*c.ornament.mesh, c.env, p.vertexBegin, p.vertexEnd));
	return excess;
}

// Debord en plan contre l'arete superieure de l'abaque construit : max (r - abacusOutline (theta)) sur les sommets
// [vertexBegin, vertexEnd), en parties.
double OutlineExcess (const Mesh &mesh, const detail::CapitalEnvelope &env, unsigned vertexBegin, unsigned vertexEnd)
{
	double excess = -std::numeric_limits<double>::infinity ();
	for (unsigned v = vertexBegin; v < vertexEnd; ++v)
	{
		float q[3] {};
		mesh.GetVertex (v, q);
		const double x = q[0] / env.scale, y = q[1] / env.scale;
		excess = std::max (excess, std::hypot (x, y) - env.abacusOutline.RadiusAt (std::atan2 (y, x)));
	}
	return excess;
}

// Mediane de 21 mesures (ms), apres un appel de chauffe.
double Median (const std::function<void ()> &f)
{
	constexpr int kRuns = 21;
	f ();
	std::vector<double> ms;
	ms.reserve (kRuns);
	for (int i = 0; i < kRuns; ++i)
	{
		const auto t0 = std::chrono::steady_clock::now ();
		f ();
		ms.push_back (std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now () - t0).count ());
	}
	std::sort (ms.begin (), ms.end ());
	return ms[kRuns / 2];
}

double Once (const std::function<void ()> &f)
{
	const auto t0 = std::chrono::steady_clock::now ();
	f ();
	return std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now () - t0).count ();
}

// Dossier de sortie des CSV de l'instrument (CG_ACANTHUS_MEASURES_DIR) ; vide : sortie standard seule.
std::string MeasuresDir ()
{
#if defined (_MSC_VER)
	char *value = nullptr;
	std::size_t length = 0;
	if (_dupenv_s (&value, &length, "CG_ACANTHUS_MEASURES_DIR") != 0 || value == nullptr)
		return {};
	const std::string dir (value);
	std::free (value);
	return dir;
#else
	const char *value = std::getenv ("CG_ACANTHUS_MEASURES_DIR");
	return value ? std::string (value) : std::string ();
#endif
}

// Ornement en parties (x, y, h) : sommets, faces et plages des pieces (kind, faceBegin, faceEnd, vertexBegin, vertexEnd).
void DumpOrnament (const Ornament &c, const std::string &stem)
{
	const Mesh &m = *c.ornament.mesh;
	std::ofstream fv (stem + "_v.csv"), ff (stem + "_f.csv"), fp (stem + "_p.csv");
	fv.precision (9);
	for (unsigned v = 0; v < m.GetNVertices (); ++v)
	{
		float p[3] {};
		m.GetVertex (v, p);
		fv << p[0] / c.env.scale << ',' << p[1] / c.env.scale << ',' << p[2] / c.env.scale - c.env.foot << '\n';
	}
	for (unsigned t = 0; t < m.GetNFaces (); ++t)
		ff << m.GetFaceVertex (t, 0) << ',' << m.GetFaceVertex (t, 1) << ',' << m.GetFaceVertex (t, 2) << '\n';
	for (const CapitalPiece &p : c.ornament.pieces)
		fp << static_cast<int> (p.kind) << ',' << p.faceBegin << ',' << p.faceEnd << ',' << p.vertexBegin << ',' << p.vertexEnd << '\n';
}

} // namespace

// K60 : debord de l'abaque par rang (registres AC-2 et AC-78), contre deux contours. (1) Abaque des donnees B, dans la
// borne max (noyau, min (cone, B)) ; (2) arete superieure de l'abaque construit A (CapitalEnvelope::abacusOutline), en
// plan seul. Borne de F, E, D et H : le cone (AC-40) ; de C : le cone et B ; de V : le cone et, en plan, A moins c ; G est
// pose sur l'abaque. Hors de la borne de V, ces debords sont des mesures, pas des bornes : chacun est fige a 0,05 p par
// rang et par niveau ; la borne du rang tient (EnvelopeExcess par piece <= 1e-4).
// Controles de l'instrument : sommets a 17,7 p sur l'axe de face, ou B est a 17,6 p (+0,1 contre B) ; l'anneau lu est
// le carre concave des donnees de A (corne 36, milieu de face 18,64) ; sommets a 40 degres de l'axe de face, 0,1 p
// au-dela de cet anneau (+0,1 contre A).
TEST (TEST_cgarchi_classical_acanthus_measures, the_abacus_excess_is_measured_rank_by_rank)
{
	{
		const ColumnProfile profile = CorinthianProfile ();
		const detail::CapitalEnvelope env = detail::ReadCapitalEnvelope (Corinthian (), profile);
		for (const double h : {26.0, 28.0})
		{
			ASSERT_GT (env.ConeRadius (h), 18.7) << h;
			ASSERT_LT (env.CoreRadius (h), 17.6) << h;
		}
		const auto at = [&] (double r, double theta, double h) {
			return std::array<double, 3> {r * std::cos (theta), r * std::sin (theta), env.foot + h};
		};
		const std::vector<std::array<unsigned, 3>> triangles {{0u, 1u, 2u}};
		const std::vector<std::array<double, 3>> onB {at (17.7, -1e-3, 26.0), at (17.7, 1e-3, 26.0), at (17.7, 0.0, 28.0)};
		EXPECT_NEAR (detail::EnvelopeExcess (*MakeTriangleMesh (onB, triangles), env, 0u, 3u), 0.1, 1e-4);

		const Ring dataA = Ring::MakeConcaveSquare (36.0, 18.64);
		EXPECT_TRUE (env.abacusOutline == dataA);
		const double t = 40.0 * kPi / 180.0, rA = dataA.RadiusAt (t) + 0.1;
		const std::vector<std::array<double, 3>> onA {at (rA, t, 30.0), at (rA, t, 32.0), at (rA - 0.5, t, 31.0)};
		const auto probe = MakeTriangleMesh (onA, triangles);
		EXPECT_NEAR (OutlineExcess (*probe, env, 0u, 3u), 0.1, 1e-4);
	}

	struct Expected
	{
		OrnamentDetail detail;
		double excessB[kRanks], excessA[kRanks];
	};
	//                                           F     E     C      D     V     H     G
	const Expected cases[] = {{OrnamentDetail::Light, {2.99, 9.34, -0.91, 1.68, 2.44, 6.29, 1.98}, {1.87, 8.30, -2.10, 0.63, -0.11, 5.25, 3.47}},
	                          {OrnamentDetail::Heavy, {3.03, 9.40, -0.91, 1.69, 2.44, 6.31, 1.99}, {1.88, 8.36, -2.10, 0.64, -0.11, 5.27, 3.47}}};
	for (const Expected &e : cases)
	{
		SCOPED_TRACE (LevelName (e.detail));
		const Ornament c = MakeOrnament (e.detail);
		std::printf ("%s : K60 debord de l'abaque B / arete superieure de A :", LevelName (e.detail));
		for (int r = 0; r < kRanks; ++r)
		{
			const double excess = AbacusExcess (c, kRankKinds[r]);
			double outline = -std::numeric_limits<double>::infinity ();
			for (const CapitalPiece &p : PiecesOf (c.ornament, kRankKinds[r]))
				outline = std::max (outline, OutlineExcess (*c.ornament.mesh, c.env, p.vertexBegin, p.vertexEnd));
			EXPECT_NEAR (excess, e.excessB[r], 0.05) << kRankNames[r];
			EXPECT_NEAR (outline, e.excessA[r], 0.05) << kRankNames[r];
			std::printf (" %s %+.4f / %+.4f", kRankNames[r], excess, outline);
		}
		std::printf (" p\n");
		double bound = -std::numeric_limits<double>::infinity ();
		for (const CapitalPiece &p : c.ornament.pieces)
			bound = std::max (bound, detail::EnvelopeExcess (*c.ornament.mesh, c.env, p));
		EXPECT_LE (bound, 1e-4);
	}
}

// Instrument, hors suite (--gtest_also_run_disabled_tests) : tableau rang x niveau. Par rang : pieces, faces et
// sommets ; duree de la construction des representantes (mediane de 21 apres chauffe ; F, E, C et D par
// BuildLeafSheet et BuildCauliculusSheet, conversion en Mesh comprise : majorant) ; jeu au noyau (CoreClearance,
// minimum) ; debord de la borne du rang (EnvelopeExcess par piece) et de l'abaque des donnees (par plage) ; couverture
// angulaire et part visible en elevation de la corbeille, cumulees de F a G (h 1 a 23, |y| <= 10) ; jour minimal des
// representantes aux autres pieces (PieceGap, sommets contre triangles, plafonne a 3 p). Puis les durees globales :
// poses des D (cache chaud), geometrie de l'ornement, ornement en Mesh, colonne (K42), normales de la colonne.
// Controles positifs : comptes du registre AC-29, couvertures K8 et K24, debords de l'abaque de C, D, V et H du
// registre AC-2 (a 0,05 p).
TEST (TEST_cgarchi_classical_acanthus_measures, DISABLED_ornament_measures_by_rank)
{
	struct Reference
	{
		OrnamentDetail detail;
		unsigned faces[kRanks];   // par piece
		double coverageFE, coverageAll, visibleAll;
	};
	const Reference references[] = {
	    {OrnamentDetail::Light, {1800, 1912, 720, 1300, 904, 886, 2164}, 0.7097, 0.8755, 0.1192},
	    {OrnamentDetail::Heavy, {10296, 11016, 3072, 2996, 3600, 3510, 7484}, 0.7116, 0.8801, 0.1165}};
	const double registeredAbacus[kRanks] = {std::nan (""), std::nan (""), -0.91, 1.68, 2.44, 6.3, std::nan ("")};

	const std::string dir = MeasuresDir ();
	std::ostringstream ranks, totals;
	ranks << "niveau,rang,pieces,faces,sommets,duree_representantes_ms,jeu_noyau,debord_borne_rang,debord_abaque,"
	         "couverture_cumulee,visible_cumulee,jour_min\n";
	totals << "niveau,poses_D_chaud_ms,geometrie_ms,ornement_ms,colonne_ms,normales_ms,somme_representantes_ms\n";
	for (const Reference &ref : references)
	{
		const OrnamentDetail level = ref.detail;
		SCOPED_TRACE (LevelName (level));
		const Ornament c = MakeOrnament (level);
		const Mesh &mesh = *c.ornament.mesh;
		const std::vector<CapitalPiece> &P = c.ornament.pieces;
		ASSERT_EQ (P.size (), 60u);
		if (!dir.empty ())
			DumpOrnament (c, dir + "/ornament_" + (level == OrnamentDetail::Light ? "light" : "heavy"));

		const detail::CapitalEnvelope &env = c.env;
		const detail::LeafTemplate &olive = detail::LeafTemplateOf (detail::LeafStyle::Olive, level);
		const detail::LeafTemplate &minor = detail::LeafTemplateOf (detail::LeafStyle::OliveMinor, level);
		const detail::LeafSpec leafSpec = detail::DefaultLeafSpec (level, detail::LeafStyle::Olive);
		const std::array<detail::LeafPlacement, 4> poses = detail::DefaultMinorLeafPlacements (env);
		LatheOptions lathe;
		lathe.radialSegments = 48;
		const std::function<void ()> builders[kRanks] = {
		    [&] {
			    detail::BuildLeafSheet (env, leafSpec,
			                            detail::DefaultLeafPlacement (CapitalPieceKind::LowerLeaf, detail::RankTheta (CapitalPieceKind::LowerLeaf, 0)),
			                            olive);
		    },
		    [&] {
			    detail::BuildLeafSheet (env, leafSpec,
			                            detail::DefaultLeafPlacement (CapitalPieceKind::MiddleLeaf, detail::RankTheta (CapitalPieceKind::MiddleLeaf, 0)),
			                            olive);
		    },
		    [&] { detail::BuildCauliculusSheet (env, detail::DefaultCauliculusSpec (level), detail::RankTheta (CapitalPieceKind::Cauliculus, 0)); },
		    [&] {
			    for (std::size_t i = 0; i < 4; ++i)
				    detail::BuildLeafSheet (env,
				                            detail::DefaultMinorLeafSpec (level, i % 2 == 0 ? detail::MinorLeafSide::Inner : detail::MinorLeafSide::Outer),
				                            poses[i], minor);
		    },
		    [&] {
			    for (int stem = 0; stem < 2; ++stem)
				    detail::BuildVoluteRibbon (env, detail::DefaultVoluteSpec (level), stem);
		    },
		    [&] {
			    for (int stem = 0; stem < 2; ++stem)
				    detail::BuildVoluteRibbon (env, detail::DefaultHelixSpec (level), stem);
		    },
		    [&] { detail::BuildFleuron (env, detail::ReadCapitalTour (c.profile, LatheAngles (c.profile, lathe), env), detail::DefaultFleuronSpec (level)); }};

		double sumRepresentatives = 0.0;
		std::vector<CapitalPiece> prefix;
		for (int r = 0; r < kRanks; ++r)
		{
			const std::vector<CapitalPiece> pieces = PiecesOf (c.ornament, kRankKinds[r]);
			unsigned faces = 0, vertices = 0;
			double clearance = std::numeric_limits<double>::infinity (), bound = -std::numeric_limits<double>::infinity ();
			for (const CapitalPiece &p : pieces)
			{
				faces += p.faceEnd - p.faceBegin;
				vertices += p.vertexEnd - p.vertexBegin;
				clearance = std::min (clearance, detail::CoreClearance (mesh, env, p));
				bound = std::max (bound, detail::EnvelopeExcess (mesh, env, p));
			}
			EXPECT_EQ (faces, ref.faces[r] * static_cast<unsigned> (pieces.size ())) << kRankNames[r];
			const double abacus = AbacusExcess (c, kRankKinds[r]);
			if (!std::isnan (registeredAbacus[r]))
				EXPECT_NEAR (abacus, registeredAbacus[r], 0.05) << kRankNames[r];
			prefix.insert (prefix.end (), pieces.begin (), pieces.end ());
			const double coverage = detail::BellCoverage (mesh, prefix, env, 1.0, 23.0);
			const double visible = detail::VisibleBellFraction (mesh, prefix, env, 10.0, 1.0, 23.0);
			if (r == 1)
				EXPECT_NEAR (coverage, ref.coverageFE, 1e-4);
			if (r == kRanks - 1)
			{
				EXPECT_NEAR (coverage, ref.coverageAll, 1e-4);
				EXPECT_NEAR (visible, ref.visibleAll, 1e-4);
			}
			double gap = 3.0;
			for (const std::size_t rep : kRepresentatives[r])
			{
				EXPECT_EQ (P[rep].kind, kRankKinds[r]) << kRankNames[r];
				for (std::size_t j = 0; j < P.size (); ++j)
					if (j != rep)
						gap = std::min (gap, acanthus_test::PieceGap (mesh, env, P[rep], P[j], 3.0));
			}
			const double duration = Median (builders[r]);
			sumRepresentatives += duration;
			char row[512];
			std::snprintf (row, sizeof row, "%s,%s,%zu,%u,%u,%.3f,%.4f,%+.4f,%+.4f,%.4f,%.4f,%.4f\n", LevelName (level), kRankNames[r],
			               pieces.size (), faces, vertices, duration, clearance, bound, abacus, coverage, visible, gap);
			ranks << row;
			std::printf ("RANG %s", row);
		}
		if (level == OrnamentDetail::Light)
		{
			const std::vector<CapitalPiece> minorLeaves = PiecesOf (c.ornament, CapitalPieceKind::MinorLeaf);
			double inner = -std::numeric_limits<double>::infinity (), outer = inner;
			for (std::size_t i = 0; i < minorLeaves.size (); ++i)
			{
				const double x = detail::EnvelopeExcess (mesh, env, minorLeaves[i].vertexBegin, minorLeaves[i].vertexEnd);
				double &side = i % 2 == 0 ? inner : outer;
				side = std::max (side, x);
			}
			std::printf ("D : debord de l'abaque, D interieur %+.4f p, D exterieur %+.4f p\n", inner, outer);
		}

		ClassicalColumnSettings settings;
		settings.order = static_cast<int> (Order::Corinthian);
		settings.capitalOrnaments = static_cast<int> (level);
		AcanthusOptions options;
		options.detail = level;
		const double posesWarm = Median ([&] { detail::DefaultMinorLeafPlacements (env); });
		const double geometry = Median ([&] { detail::BuildCorinthianOrnamentGeometry (Corinthian (), c.profile, options); });
		const double ornament = Median ([&] { BuildCorinthianOrnament (Corinthian (), c.profile, options); });
		const double column = Median ([&] { BuildClassicalColumnMesh (settings); });
		const std::unique_ptr<Mesh> built = BuildClassicalColumnMesh (settings);
		const double normals = Median ([&] { built->ComputeNormals (); });
		char row[256];
		std::snprintf (row, sizeof row, "%s,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n", LevelName (level), posesWarm, geometry, ornament, column,
		               normals, sumRepresentatives);
		totals << row;
		std::printf ("TOTAL %s", row);
	}
	if (!dir.empty ())
	{
		std::ofstream byRank (dir + "/measures_by_rank.csv");
		byRank << ranks.str ();
		std::ofstream global (dir + "/measures_totals.csv");
		global << totals.str ();
	}
}

// Instruments du premier appel d'un processus (cache des poses des D vide, gabarits a charger) : chacun se lance
// seul, un processus par mesure (--gtest_filter sur ce seul test).
TEST (TEST_cgarchi_classical_acanthus_measures, DISABLED_cold_column_light)
{
	ClassicalColumnSettings s;
	s.order = static_cast<int> (Order::Corinthian);
	s.capitalOrnaments = static_cast<int> (OrnamentDetail::Light);
	const double cold = Once ([&] { BuildClassicalColumnMesh (s); });
	const double warm = Median ([&] { BuildClassicalColumnMesh (s); });
	std::printf ("FROID colonne Light : premier appel %.3f ms, cache chaud %.3f ms\n", cold, warm);
}

TEST (TEST_cgarchi_classical_acanthus_measures, DISABLED_cold_column_heavy)
{
	ClassicalColumnSettings s;
	s.order = static_cast<int> (Order::Corinthian);
	s.capitalOrnaments = static_cast<int> (OrnamentDetail::Heavy);
	const double cold = Once ([&] { BuildClassicalColumnMesh (s); });
	const double warm = Median ([&] { BuildClassicalColumnMesh (s); });
	std::printf ("FROID colonne Heavy : premier appel %.3f ms, cache chaud %.3f ms\n", cold, warm);
}

TEST (TEST_cgarchi_classical_acanthus_measures, DISABLED_cold_minor_leaf_poses)
{
	const ColumnProfile profile = CorinthianProfile ();
	const detail::CapitalEnvelope env = detail::ReadCapitalEnvelope (Corinthian (), profile);
	const double cold = Once ([&] { detail::DefaultMinorLeafPlacements (env); });
	const double warm = Median ([&] { detail::DefaultMinorLeafPlacements (env); });
	std::printf ("FROID poses des D : premier appel %.3f ms, cache chaud %.3f ms\n", cold, warm);
}

// Instrument, hors suite : empreinte FNV-1a 64 bits de chaque rang (octets des sommets float, puis indices des faces, de
// toutes les pieces du rang), Light et Heavy, a comparer entre deux etats du code construits par le meme compilateur.
TEST (TEST_cgarchi_classical_acanthus_measures, DISABLED_rank_fingerprints)
{
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
	{
		const Ornament c = MakeOrnament (level);
		const Mesh &m = *c.ornament.mesh;
		std::printf ("EMPREINTE %s", LevelName (level));
		for (int r = 0; r < kRanks; ++r)
		{
			std::uint64_t h = 1469598103934665603ULL;
			const auto add = [&h] (const void *data, std::size_t n) {
				const unsigned char *b = static_cast<const unsigned char *> (data);
				for (std::size_t i = 0; i < n; ++i)
					h = (h ^ b[i]) * 1099511628211ULL;
			};
			for (const CapitalPiece &p : PiecesOf (c.ornament, kRankKinds[r]))
			{
				for (unsigned v = p.vertexBegin; v < p.vertexEnd; ++v)
				{
					float q[3] {};
					m.GetVertex (v, q);
					add (q, sizeof q);
				}
				for (unsigned f = p.faceBegin; f < p.faceEnd; ++f)
					for (unsigned k = 0; k < 3; ++k)
					{
						const int index = m.GetFaceVertex (f, k);
						add (&index, sizeof index);
					}
			}
			std::printf (" %s=%016llx", kRankNames[r], static_cast<unsigned long long> (h));
		}
		std::printf ("\n");
	}
}
