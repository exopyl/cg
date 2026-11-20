#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cgarchi/classical_greek_pediment.h>
#include <cgarchi/classical_greek_pediment_detail.h>
#include <cgarchi/classical_monument.h>
#include <cgarchi/classical_monument_data.h>
#include <cgarchi/classical_temple.h>
#include <cgarchi/classical_temple_refinement.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "greek_doric_test_support.h"

// ===========================================================================
//  Poche du comble (lot C2a, registre CE-14)
// ===========================================================================
//
// Sans poche, le comble est compare au bit pres (memcmp) a des references
// relevees sur la construction anterieure a la poche : temple synthetique
// (SyntheticTempleWithPediment) et temple d'Hephaistos (document embarque).
// Avec la poche de l'Hephaisteion, valeurs d'essai dans le repere du temple
// (x en facade, y en profondeur) : x = +-(3,115 + 0,514), y de -7,1 a +5,6,
// plafond 8,073 - 7,694 au-dessus de la base du comble.
//
// Contraction FMA : les sommets de la poche sont des copies des bornes et de
// zBase (une somme) ; le volume attendu est calcule sur les coordonnees float
// lues sur le maillage, a 1e-9 pres relatif.

using namespace cgarchi::classical;

namespace {

constexpr double kRelativeVolume = 1e-9;

struct Roof
{
	std::vector<float> positions;
	std::vector<unsigned> faces;
};

Roof RoofOf (const Mesh &m)
{
	Roof r;
	for (unsigned i = 0; i < m.GetNVertices (); ++i)
	{
		float q[3];
		m.GetVertex (i, q);
		r.positions.insert (r.positions.end (), q, q + 3);
	}
	for (unsigned f = 0; f < m.GetNFaces (); ++f)
		for (unsigned j = 0; j < 3; ++j)
			r.faces.push_back (m.GetFaceVertex (f, j));
	return r;
}

bool SameAs (const Roof &r, const std::vector<float> &positions, const std::vector<unsigned> &faces)
{
	return r.positions.size () == positions.size () && r.faces.size () == faces.size () &&
	       std::memcmp (r.positions.data (), positions.data (), positions.size () * sizeof (float)) == 0 &&
	       std::memcmp (r.faces.data (), faces.data (), faces.size () * sizeof (unsigned)) == 0;
}

const std::vector<unsigned> kPrismFaces = {3, 4, 1, 3, 1, 0, 4, 5, 2, 4, 2, 1, 5, 3, 0, 5, 0, 2, 0, 1, 2, 3, 5, 4};

const std::vector<float> kSyntheticRoof = {
    -0x1.b33334p+1f, -0x1.e147aep+1f, 0x1.f7ae16p+2f, 0x1.b33334p+1f, -0x1.e147aep+1f, 0x1.f7ae16p+2f,
    0.0f,            -0x1.e147aep+1f, 0x1.170a3ep+3f, -0x1.b33334p+1f, 0x1.e147aep+1f, 0x1.f7ae16p+2f,
    0x1.b33334p+1f,  0x1.e147aep+1f,  0x1.f7ae16p+2f, 0.0f,            0x1.e147aep+1f, 0x1.170a3ep+3f};

const std::vector<float> kHephaistosRoof = {
    -0x1.cf2284p+2f, -0x1.f89eb8p+3f, 0x1.23f506p+3f, 0x1.cf2284p+2f, -0x1.f89eb8p+3f, 0x1.23f506p+3f,
    0.0f,            -0x1.f89eb8p+3f, 0x1.5ca662p+3f, -0x1.cf2284p+2f, 0x1.f89eb8p+3f, 0x1.23f506p+3f,
    0x1.cf2284p+2f,  0x1.f89eb8p+3f,  0x1.23f506p+3f, 0.0f,            0x1.f89eb8p+3f, 0x1.5ca662p+3f};

struct Fixture
{
	TempleSettings temple;
	TempleLayout layout;
	double zBase = 0.0, xTop = 0.0, yTympanum = 0.0;

	explicit Fixture (TempleSettings t) : temple (std::move (t)), layout (LayoutTemple (temple))
	{
		zBase = layout.entablatureFoot + GreekEntablatureBandBounds (*temple.entablature).back ();
		yTympanum = layout.peristyle.halfDepth + temple.pediment->tympanumFace;
		const detail::CheckedEntablature e = detail::CheckEntablature (*temple.entablature);
		xTop = detail::PedimentFeet (detail::CheckPediment (*temple.pediment, e)).back ();
	}

	std::vector<std::unique_ptr<Mesh>> Build (const std::vector<RoofPocket> &pockets) const
	{
		return BuildGreekPedimentPieceMeshes (*temple.pediment, *temple.entablature, layout.peristyle, layout.entablatureFoot, {},
		                                      pockets);
	}
};

const Fixture &Hephaistos ()
{
	static const Fixture f (MonumentTempleSettings (HephaistosMonument ()));
	return f;
}

RoofPocket HephaistosPocket (const Fixture &f)
{
	const double wall = 3.115 + 0.514;
	return {-wall, wall, -7.1, 5.6, f.zBase + (8.073 - 7.694)};
}

std::string RefusalOf (const std::function<void ()> &call)
{
	try
	{
		call ();
	}
	catch (const std::invalid_argument &e)
	{
		return e.what ();
	}
	return "";
}

bool Starts (const std::string &s, const std::string &prefix)
{
	return s.rfind (prefix, 0) == 0;
}

} // namespace

TEST (TEST_cgarchi_classical_greek_pediment_pocket, without_pocket_the_roof_is_the_reference_to_the_bit)
{
	const Fixture synthetic (cgarchi_test::SyntheticTempleWithPediment ());
	for (const Fixture *f : {&synthetic, &Hephaistos ()})
	{
		const std::vector<float> &reference = f == &synthetic ? kSyntheticRoof : kHephaistosRoof;
		const std::vector<std::unique_ptr<Mesh>> plain = BuildGreekPedimentPieceMeshes (
		    *f->temple.pediment, *f->temple.entablature, f->layout.peristyle, f->layout.entablatureFoot);
		const std::vector<std::unique_ptr<Mesh>> none = f->Build ({});
		ASSERT_EQ (plain.size (), 3u);
		ASSERT_EQ (none.size (), 3u);
		EXPECT_TRUE (SameAs (RoofOf (*plain[2]), reference, kPrismFaces));
		EXPECT_TRUE (SameAs (RoofOf (*none[2]), reference, kPrismFaces));
		for (std::size_t k = 0; k < 2; ++k)
		{
			const Roof a = RoofOf (*plain[k]);
			EXPECT_TRUE (SameAs (RoofOf (*none[k]), a.positions, a.faces)) << "Lambda " << k;
		}
	}
}

TEST (TEST_cgarchi_classical_greek_pediment_pocket, the_hephaisteion_pocket_hollows_the_roof)
{
	const Fixture &f = Hephaistos ();
	const RoofPocket pocket = HephaistosPocket (f);
	const std::vector<std::unique_ptr<Mesh>> plain = f.Build ({});
	const std::vector<std::unique_ptr<Mesh>> hollow = f.Build ({pocket});
	ASSERT_EQ (hollow.size (), 3u);
	for (std::size_t k = 0; k < 2; ++k)
	{
		const Roof a = RoofOf (*plain[k]);
		EXPECT_TRUE (SameAs (RoofOf (*hollow[k]), a.positions, a.faces)) << "Lambda " << k;
	}

	const Mesh &m = *hollow[2];
	EXPECT_EQ (m.GetNVertices (), 14u);
	EXPECT_EQ (m.GetNFaces (), 24u);
	const Roof r = RoofOf (m);
	EXPECT_EQ (std::memcmp (r.positions.data (), kHephaistosRoof.data (), kHephaistosRoof.size () * sizeof (float)), 0)
	    << "les 6 sommets du prisme d'abord, au bit pres";

	const std::array<float, 4> px = {static_cast<float> (pocket.x0), static_cast<float> (pocket.x1),
	                                 static_cast<float> (pocket.x1), static_cast<float> (pocket.x0)};
	const std::array<float, 4> py = {static_cast<float> (pocket.y0), static_cast<float> (pocket.y0),
	                                 static_cast<float> (pocket.y1), static_cast<float> (pocket.y1)};
	const float zLow = static_cast<float> (f.zBase), zHigh = static_cast<float> (pocket.zTop);
	for (unsigned i = 0; i < 8; ++i)
	{
		const float *q = &r.positions[3 * (6 + i)];
		EXPECT_EQ (q[0], px[i % 4]) << i;
		EXPECT_EQ (q[1], py[i % 4]) << i;
		EXPECT_EQ (q[2], i < 4 ? zLow : zHigh) << i;
	}

	const MeshValidationReport report = ValidateMesh (m);
	EXPECT_TRUE (report.IsValid ());
	EXPECT_TRUE (report.selfIntersectionChecked);
	EXPECT_EQ (report.components, 1u);

	const double X = r.positions[3], Y = r.positions[13], zB = r.positions[2], zR = r.positions[8];
	const double prism = X * (zR - zB) * 2.0 * Y;
	const double box = (static_cast<double> (px[1]) - px[0]) * (static_cast<double> (py[2]) - py[0]) *
	                   (static_cast<double> (zHigh) - zLow);
	const double expected = prism - box;
	EXPECT_GT (box, 0.0);
	EXPECT_NEAR (report.signedVolume, expected, kRelativeVolume * expected);
	RecordProperty ("pocket_volume_gap", std::to_string (report.signedVolume - expected));
}

// Refus nommes de la poche, dans l'ordre du contrat ; la poche ecrite en
// (x, y) permutes (x de -7,1 a 5,6 en facade) traverse un pan : son plafond, a
// 0,379 au-dessus de la base, passe au-dessus du pan au droit de |x| = 7,1.
TEST (TEST_cgarchi_classical_greek_pediment_pocket, invalid_pockets_are_refused_naming_the_field)
{
	const Fixture &f = Hephaistos ();
	const RoofPocket good = HephaistosPocket (f);
	auto refusal = [&] (const RoofPocket &p) { return RefusalOf ([&] { f.Build ({p}); }); };
	ASSERT_EQ (refusal (good), "");

	RoofPocket p = good;
	p.y1 = std::numeric_limits<double>::quiet_NaN ();
	EXPECT_TRUE (Starts (refusal (p), "Fronton : 'roofPocket' a une borne non finie")) << refusal (p);

	p = good;
	p.x1 = p.x0;
	EXPECT_TRUE (Starts (refusal (p), "Fronton : 'roofPocket.x' a une emprise vide")) << refusal (p);
	p = good;
	std::swap (p.y0, p.y1);
	EXPECT_TRUE (Starts (refusal (p), "Fronton : 'roofPocket.y' a une emprise vide")) << refusal (p);

	p = good;
	p.zTop = f.zBase;
	EXPECT_TRUE (Starts (refusal (p), "Fronton : 'roofPocket.zTop' doit etre au-dessus")) << refusal (p);
	p.zTop = f.zBase - 0.1;
	EXPECT_TRUE (Starts (refusal (p), "Fronton : 'roofPocket.zTop' doit etre au-dessus")) << refusal (p);

	p = good;
	p.x1 = f.xTop;
	EXPECT_TRUE (Starts (refusal (p), "Fronton : 'roofPocket.x' touche ou traverse un bord de la base")) << refusal (p);
	p = good;
	p.x0 = -f.xTop - 1.0;
	EXPECT_TRUE (Starts (refusal (p), "Fronton : 'roofPocket.x' touche ou traverse un bord de la base")) << refusal (p);

	p = good;
	p.y1 = f.yTympanum;
	EXPECT_TRUE (Starts (refusal (p), "Fronton : 'roofPocket.y' touche ou traverse un tympan")) << refusal (p);
	p = good;
	p.y0 = -f.yTympanum - 1.0;
	EXPECT_TRUE (Starts (refusal (p), "Fronton : 'roofPocket.y' touche ou traverse un tympan")) << refusal (p);

	// En deca du plan du tympan en double, confondue avec lui en float.
	p = good;
	p.y1 = std::nextafter (f.yTympanum, 0.0);
	EXPECT_TRUE (Starts (refusal (p), "Fronton : 'roofPocket' touche le comble une fois les sommets arrondis en float"))
	    << refusal (p);
}

TEST (TEST_cgarchi_classical_greek_pediment_pocket, a_pocket_reaching_a_pan_is_refused)
{
	const Fixture &f = Hephaistos ();
	const RoofPocket good = HephaistosPocket (f);
	const std::string pan = "Fronton : 'roofPocket.zTop' touche ou traverse un pan";

	RoofPocket swapped {good.y0, good.y1, good.x0, good.x1, good.zTop};
	ASSERT_LT (swapped.x0, -7.0);
	ASSERT_GT (swapped.x0, -f.xTop);
	EXPECT_TRUE (Starts (RefusalOf ([&] { f.Build ({swapped}); }), pan)) << RefusalOf ([&] { f.Build ({swapped}); });

	// Plafond monte jusqu'au faite : le coin le plus eloigne de l'axe passe le pan.
	RoofPocket high = good;
	high.zTop = f.zBase + 1.5;
	EXPECT_TRUE (Starts (RefusalOf ([&] { f.Build ({high}); }), pan)) << RefusalOf ([&] { f.Build ({high}); });

	// Un seul cote passe le pan : x1 seul pousse vers le pied.
	RoofPocket side = good;
	side.x1 = f.xTop - 0.01;
	EXPECT_TRUE (Starts (RefusalOf ([&] { f.Build ({side}); }), pan)) << RefusalOf ([&] { f.Build ({side}); });
}

TEST (TEST_cgarchi_classical_greek_pediment_pocket, the_refined_pediment_refuses_a_pocket)
{
	const Fixture &f = Hephaistos ();
	const RefinementStations none {};
	const std::string refusal = RefusalOf ([&] {
		BuildGreekPedimentPieceMeshes (*f.temple.pediment, *f.temple.entablature, f.layout.peristyle, f.layout.entablatureFoot, none,
		                               {}, {HephaistosPocket (f)});
	});
	EXPECT_EQ (refusal, "Fronton : 'roofPocket' ne se combine pas avec les stations du raffinement");
	EXPECT_EQ (RefusalOf ([&] {
		           BuildGreekPedimentPieceMeshes (*f.temple.pediment, *f.temple.entablature, f.layout.peristyle,
		                                          f.layout.entablatureFoot, none, {}, {});
	           }),
	           "");
}

// ===========================================================================
//  Comble a poches multiples (lot C4b, registre CE-34)
// ===========================================================================
//
// Poches d'essai dans le repere du temple d'Hephaistos, sous les pans
// (plafond au plus 0,3 au-dessus de la base du comble, |x| au plus 5) :
// rectangles disjoints, ou au contact par une face. Volumes calcules sur les
// coordonnees float relues sur le maillage (prisme) et sur les bornes
// arrondies en float (poches), a 1e-9 pres relatif.

namespace {

// Comble a une poche (HephaistosPocket), reference figee au bit pres : les 8 sommets de la poche apres les 6 du prisme
// (kHephaistosRoof), puis les 24 triangles, dans l'ordre du contrat de BuildGreekPedimentPieceMeshes (CE-14).
const std::vector<float> kOnePocketRoofTail = {
    -0x1.d08312p+1f, -0x1.c66666p+2f, 0x1.23f506p+3f, 0x1.d08312p+1f, -0x1.c66666p+2f, 0x1.23f506p+3f,
    0x1.d08312p+1f,  0x1.666666p+2f,  0x1.23f506p+3f, -0x1.d08312p+1f, 0x1.666666p+2f,  0x1.23f506p+3f,
    -0x1.d08312p+1f, -0x1.c66666p+2f, 0x1.3015cap+3f, 0x1.d08312p+1f, -0x1.c66666p+2f, 0x1.3015cap+3f,
    0x1.d08312p+1f,  0x1.666666p+2f,  0x1.3015cap+3f, -0x1.d08312p+1f, 0x1.666666p+2f,  0x1.3015cap+3f};
const std::vector<unsigned> kOnePocketRoofFaces = {
    4, 5, 2, 4, 2, 1, 5, 3, 0, 5, 0, 2,   0, 1, 2, 3, 5, 4, 0, 6, 7, 0, 7, 1,
    1, 7, 8, 1, 8, 4, 4, 8, 9, 4, 9, 3,   3, 9, 6, 3, 6, 0, 6, 10, 11, 6, 11, 7,
    7, 11, 12, 7, 12, 8, 8, 12, 13, 8, 13, 9, 9, 13, 10, 9, 10, 6, 10, 12, 11, 10, 13, 12};

RoofPocket PocketAt (const Fixture &f, double x0, double x1, double y0, double y1, double rise)
{
	return {x0, x1, y0, y1, f.zBase + rise};
}

double FloatVolume (const RoofPocket &p, const Fixture &f)
{
	auto r = [] (double v) { return static_cast<double> (static_cast<float> (v)); };
	return (r (p.x1) - r (p.x0)) * (r (p.y1) - r (p.y0)) * (r (p.zTop) - r (f.zBase));
}

// Prisme plein du comble, sur ses coordonnees float.
double PrismVolume (const Fixture &f)
{
	const std::vector<std::unique_ptr<Mesh>> plain = f.Build ({});
	const Roof r = RoofOf (*plain[2]);
	const double X = r.positions[3], Y = r.positions[13], zB = r.positions[2], zR = r.positions[8];
	return X * (zR - zB) * 2.0 * Y;
}

using Position = std::array<float, 3>;
using Triangle = std::array<unsigned, 3>;

// Reunion des tranches du comble (pieces 2 et suivantes), sommets confondus par position exacte (les tranches
// voisines calculent leurs sommets communs sur les memes doubles).
struct RoofUnion
{
	std::vector<Position> positions;
	std::vector<Triangle> faces;
};

RoofUnion UnionOf (const std::vector<std::unique_ptr<Mesh>> &pieces)
{
	RoofUnion u;
	std::map<Position, unsigned> index;
	for (std::size_t k = 2; k < pieces.size (); ++k)
	{
		const Mesh &m = *pieces[k];
		std::vector<unsigned> local (m.GetNVertices ());
		for (unsigned v = 0; v < m.GetNVertices (); ++v)
		{
			Position q;
			m.GetVertex (v, q.data ());
			const auto inserted = index.emplace (q, static_cast<unsigned> (u.positions.size ()));
			if (inserted.second)
				u.positions.push_back (q);
			local[v] = inserted.first->second;
		}
		for (unsigned f = 0; f < m.GetNFaces (); ++f)
			u.faces.push_back ({local[static_cast<unsigned> (m.GetFaceVertex (f, 0))],
			                    local[static_cast<unsigned> (m.GetFaceVertex (f, 1))],
			                    local[static_cast<unsigned> (m.GetFaceVertex (f, 2))]});
	}
	return u;
}

// Aire de l'intersection de deux triangles d'un meme plan, en (x, z) : decoupe de Sutherland-Hodgman du premier par
// les trois demi-plans du second, rendu direct.
double OverlapArea (std::vector<std::array<double, 2>> a, std::array<std::array<double, 2>, 3> b)
{
	auto cross = [] (const std::array<double, 2> &o, const std::array<double, 2> &p, const std::array<double, 2> &q) {
		return (p[0] - o[0]) * (q[1] - o[1]) - (p[1] - o[1]) * (q[0] - o[0]);
	};
	if (cross (b[0], b[1], b[2]) < 0.0)
		std::swap (b[1], b[2]);
	for (std::size_t e = 0; e < 3 && !a.empty (); ++e)
	{
		const std::array<double, 2> &p = b[e], &q = b[(e + 1) % 3];
		std::vector<std::array<double, 2>> out;
		for (std::size_t i = 0; i < a.size (); ++i)
		{
			const std::array<double, 2> &s = a[i], &t = a[(i + 1) % a.size ()];
			const double ds = cross (p, q, s), dt = cross (p, q, t);
			if (ds >= 0.0)
				out.push_back (s);
			if ((ds >= 0.0) != (dt >= 0.0))
			{
				const double r = ds / (ds - dt);
				out.push_back ({s[0] + r * (t[0] - s[0]), s[1] + r * (t[1] - s[1])});
			}
		}
		a = std::move (out);
	}
	double area = 0.0;
	for (std::size_t i = 0; i < a.size (); ++i)
		area += a[i][0] * a[(i + 1) % a.size ()][1] - a[(i + 1) % a.size ()][0] * a[i][1];
	return std::fabs (area) / 2.0;
}

// Releve du comble a poches : tranches (nombre, bornes Y contigues au bit pres par Y croissants) et leur reunion.
// `closed` : chaque arete non orientee de la reunion portee par exactement deux faces, une dans chaque sens, et
// aucune face degeneree. `overlaps` : paires de faces d'un meme plan y = cte (normale +-Y) dont l'intersection a une
// aire non nulle (au-dela de 1e-9 m2). `volume` : volume signe de la reunion, en double sur les positions float.
struct RoofReport
{
	std::size_t slices = 0;
	bool contiguous = true, closed = true;
	std::size_t degenerate = 0, unpairedEdges = 0, overlaps = 0, cutFaces = 0;
	double volume = 0.0;
	MeshValidationReport validation;
};

RoofReport RoofReportOf (const std::vector<std::unique_ptr<Mesh>> &pieces)
{
	RoofReport s;
	float previousHigh = 0.0f;
	for (std::size_t k = 2; k < pieces.size (); ++k)
	{
		const Mesh &m = *pieces[k];
		float low = std::numeric_limits<float>::max (), high = std::numeric_limits<float>::lowest ();
		for (unsigned v = 0; v < m.GetNVertices (); ++v)
		{
			float q[3];
			m.GetVertex (v, q);
			low = std::min (low, q[1]);
			high = std::max (high, q[1]);
		}
		if (k > 2)
			s.contiguous = s.contiguous && low == previousHigh;
		previousHigh = high;
		++s.slices;
	}

	const RoofUnion u = UnionOf (pieces);
	std::map<std::pair<unsigned, unsigned>, std::size_t> directed;
	std::map<float, std::vector<std::size_t>> byPlane;
	for (std::size_t f = 0; f < u.faces.size (); ++f)
	{
		const Triangle &t = u.faces[f];
		const Position &a = u.positions[t[0]], &b = u.positions[t[1]], &c = u.positions[t[2]];
		const double ux = static_cast<double> (b[0]) - a[0], uy = static_cast<double> (b[1]) - a[1], uz = static_cast<double> (b[2]) - a[2];
		const double vx = static_cast<double> (c[0]) - a[0], vy = static_cast<double> (c[1]) - a[1], vz = static_cast<double> (c[2]) - a[2];
		const double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
		if (t[0] == t[1] || t[1] == t[2] || t[2] == t[0] || (nx == 0.0 && ny == 0.0 && nz == 0.0))
			++s.degenerate;
		s.volume += (a[0] * (static_cast<double> (b[1]) * c[2] - static_cast<double> (b[2]) * c[1]) +
		             a[1] * (static_cast<double> (b[2]) * c[0] - static_cast<double> (b[0]) * c[2]) +
		             a[2] * (static_cast<double> (b[0]) * c[1] - static_cast<double> (b[1]) * c[0])) /
		            6.0;
		for (std::size_t e = 0; e < 3; ++e)
			++directed[{t[e], t[(e + 1) % 3]}];
		if (a[1] == b[1] && b[1] == c[1])
			byPlane[a[1]].push_back (f);
	}
	for (const auto &d : directed)
	{
		const auto back = directed.find ({d.first.second, d.first.first});
		if (d.second != 1 || back == directed.end () || back->second != 1)
			++s.unpairedEdges;
	}
	for (const auto &plane : byPlane)
	{
		const std::vector<std::size_t> &faces = plane.second;
		s.cutFaces += faces.size ();
		auto xz = [&u] (unsigned v) { return std::array<double, 2> {u.positions[v][0], u.positions[v][2]}; };
		for (std::size_t i = 0; i < faces.size (); ++i)
			for (std::size_t j = i + 1; j < faces.size (); ++j)
			{
				const Triangle &p = u.faces[faces[i]], &q = u.faces[faces[j]];
				if (OverlapArea ({xz (p[0]), xz (p[1]), xz (p[2])}, {xz (q[0]), xz (q[1]), xz (q[2])}) > 1e-9)
					++s.overlaps;
			}
	}
	s.closed = s.degenerate == 0 && s.unpairedEdges == 0;

	std::vector<float> vertices;
	for (const Position &q : u.positions)
		vertices.insert (vertices.end (), q.begin (), q.end ());
	std::vector<unsigned> faces;
	for (const Triangle &t : u.faces)
		faces.insert (faces.end (), t.begin (), t.end ());
	Mesh m;
	m.SetVertices (static_cast<unsigned> (u.positions.size ()), vertices.data ());
	m.SetFaces (static_cast<unsigned> (u.faces.size ()), 3, faces.data ());
	s.validation = ValidateMesh (m);
	return s;
}

// Temple d'Hephaistos complet du document : reglages de BuildMonumentMesh, toutes parties construites.
const Fixture &CompleteHephaistos ()
{
	static const Fixture f ([] {
		const MonumentData &d = HephaistosMonument ();
		TempleSettings s = MonumentTempleSettings (d);
		s.cella = MonumentCellaSettings (d);
		s.cella->porches = MonumentPorchSettings (d);
		s.interior = MonumentInteriorSettings (d);
		s.epicranitis = MonumentEpicranitisSettings (d);
		s.porchExtension = MonumentPorchExtensionSettings (d);
		s.eastCeiling = MonumentEastCeilingSettings (d);
		s.flankCeilings = MonumentFlankCeilingSettings (d);
		return s;
	}());
	return f;
}

// Poches du temple dans l'ordre du temple : interieur, plafond est (pteron est, porche ouest), flancs.
std::vector<RoofPocket> TemplePockets (const Fixture &f)
{
	std::vector<RoofPocket> pockets;
	if (f.layout.interior)
		pockets.push_back (f.layout.interior->pocket);
	if (f.layout.eastCeiling)
		pockets.insert (pockets.end (), {f.layout.eastCeiling->eastPocket, f.layout.eastCeiling->westPocket});
	if (f.layout.flankCeilings)
		pockets.insert (pockets.end (), f.layout.flankCeilings->pockets.begin (), f.layout.flankCeilings->pockets.end ());
	return pockets;
}

} // namespace

// PP1 : une poche, le comble a poche au bit pres sur la reference figee (sommets et faces).
TEST (TEST_cgarchi_classical_greek_pediment_pocket, one_pocket_gives_the_pinned_reference_roof)
{
	const Fixture &f = Hephaistos ();
	const std::vector<std::unique_ptr<Mesh>> pieces = f.Build ({HephaistosPocket (f)});
	ASSERT_EQ (pieces.size (), 3u);
	const Roof r = RoofOf (*pieces[2]);
	ASSERT_EQ (r.positions.size (), 3u * 14u);
	ASSERT_EQ (r.faces.size (), 3u * 24u);
	std::vector<float> reference (kHephaistosRoof);
	reference.insert (reference.end (), kOnePocketRoofTail.begin (), kOnePocketRoofTail.end ());
	const bool same = SameAs (r, reference, kOnePocketRoofFaces);
	EXPECT_TRUE (same);
	if (!same)
	{
		for (std::size_t k = 3 * 6; k < r.positions.size (); ++k)
			std::printf ("%a%s", static_cast<double> (r.positions[k]), (k + 1) % 3 == 0 ? "\n" : ", ");
		for (std::size_t k = 0; k < r.faces.size (); ++k)
			std::printf ("%u%s", r.faces[k], (k + 1) % 12 == 0 ? "\n" : ", ");
	}
}

// Cas d'essai des tranches, sur leur temple : nombre de tranches et de faces dans les plans y = cte (coupes et
// tympans), mesures. « decales » : poches de tranches voisines au contact par une coupe, se recouvrant en partie en
// X, de plafonds differents ; les parois et plafonds d'une tranche y sont coupes par les bornes et les plafonds de la
// voisine. « temple » : les cinq poches du temple d'Hephaistos complet.
struct SliceCase
{
	const char *name;
	const Fixture *fixture;
	std::vector<RoofPocket> pockets;
	std::size_t slices, cutFaces;
};

std::vector<SliceCase> SliceCases ()
{
	const Fixture &f = Hephaistos (), &complete = CompleteHephaistos ();
	return {
	    {"deux", &f, {PocketAt (f, -3.0, 3.0, -5.0, 5.0, 0.3), PocketAt (f, 3.5, 5.0, -8.0, 9.0, 0.2)}, 5u, 10u},
	    {"cinq",
	     &f,
	     {PocketAt (f, -3.0, 3.0, -5.0, 5.0, 0.3), PocketAt (f, 3.5, 5.0, -8.0, 9.0, 0.2), PocketAt (f, -5.0, -3.5, -8.0, 9.0, 0.2),
	      PocketAt (f, -2.0, 2.0, -12.0, -10.0, 0.25), PocketAt (f, -2.0, 2.0, 10.0, 12.0, 0.25)},
	     9u,
	     22u},
	    {"contact",
	     &f,
	     {PocketAt (f, -3.0, 3.0, -5.0, 5.0, 0.3), PocketAt (f, 3.0, 5.0, -8.0, 9.0, 0.2), PocketAt (f, -5.0, -3.0, -8.0, 9.0, 0.3),
	      PocketAt (f, -2.0, 2.0, -12.0, -8.0, 0.25), PocketAt (f, -2.0, 2.0, -8.0, -5.0, 0.1)},
	     6u,
	     26u},
	    {"decales",
	     &f,
	     {PocketAt (f, -3.0, 1.0, -5.0, 0.0, 0.3), PocketAt (f, -1.0, 3.0, 0.0, 5.0, 0.2), PocketAt (f, 2.0, 4.0, -5.0, 0.0, 0.1),
	      PocketAt (f, -4.0, -2.0, 5.0, 8.0, 0.25)},
	     5u,
	     24u},
	    {"temple", &complete, TemplePockets (complete), 9u, 22u},
	};
}

// Volume attendu, independant du maillage du comble a poches : prisme plein moins les boites des poches.
double ExpectedVolume (const Fixture &f, const std::vector<RoofPocket> &pockets)
{
	double expected = PrismVolume (f);
	for (const RoofPocket &p : pockets)
		expected -= FloatVolume (p, f);
	return expected;
}

// PP2 : nombre de tranches (bornes Y distinctes moins une), contigues par Y croissants ; Lambda inchanges ; reunion
// des tranches fermee (aretes appariees, sans face degeneree), valide, d'une composante, de volume prisme - poches a
// 1e-9 relatif. Avec des poches au contact par une face (en X et en Y), decalees d'une tranche a l'autre, et celles du
// temple complet, les memes proprietes.
TEST (TEST_cgarchi_classical_greek_pediment_pocket, several_pockets_slice_the_roof_along_y)
{
	const std::vector<SliceCase> cases = SliceCases ();
	ASSERT_EQ (cases.back ().pockets.size (), 5u);
	for (const SliceCase &c : cases)
	{
		SCOPED_TRACE (c.name);
		const Fixture &f = *c.fixture;
		ASSERT_GT (PrismVolume (f), 0.0);
		const std::vector<std::unique_ptr<Mesh>> plain = f.Build ({});
		const std::vector<std::unique_ptr<Mesh>> pieces = f.Build (c.pockets);
		const RoofReport s = RoofReportOf (pieces);
		EXPECT_EQ (s.slices, c.slices);
		EXPECT_TRUE (s.contiguous);
		EXPECT_TRUE (s.closed) << s.degenerate << " faces degenerees, " << s.unpairedEdges << " aretes non appariees";
		EXPECT_TRUE (s.validation.IsValid ());
		EXPECT_EQ (s.validation.components, 1u);
		const double expected = ExpectedVolume (f, c.pockets);
		EXPECT_NEAR (s.volume, expected, kRelativeVolume * expected);
		EXPECT_NEAR (s.validation.signedVolume, expected, kRelativeVolume * expected);
		for (std::size_t k = 0; k < 2; ++k)
		{
			const Roof a = RoofOf (*plain[k]);
			EXPECT_TRUE (SameAs (RoofOf (*pieces[k]), a.positions, a.faces)) << "Lambda " << k;
		}
	}
}

// PP5 : aucune face interne aux coupes. Dans un plan y = cte, les faces des deux tranches voisines ne se recouvrent
// jamais (chaque bout ne couvre que la difference des deux sections) ; aux tympans, la section entiere. Sur les cas
// de PP2, dont le comble du temple d'Hephaistos : aucun recouvrement, nombre de faces de ces plans fige.
TEST (TEST_cgarchi_classical_greek_pediment_pocket, slices_leave_no_internal_face_at_their_cuts)
{
	for (const SliceCase &c : SliceCases ())
	{
		SCOPED_TRACE (c.name);
		const RoofReport s = RoofReportOf (c.fixture->Build (c.pockets));
		EXPECT_EQ (s.overlaps, 0u);
		EXPECT_EQ (s.cutFaces, c.cutFaces);
	}
}

// PP3 : recouvrement refuse, apres les refus de chaque poche ; poches contigues (contact par une face) acceptees,
// contact par une arete seule refuse (PP4).
TEST (TEST_cgarchi_classical_greek_pediment_pocket, overlapping_pockets_are_refused_and_contiguous_ones_accepted)
{
	const Fixture &f = Hephaistos ();
	const RoofPocket a = PocketAt (f, -3.0, 3.0, -5.0, 5.0, 0.3);
	const RoofPocket far = PocketAt (f, -2.0, 2.0, 10.0, 12.0, 0.25);
	const RoofPocket b = PocketAt (f, 2.0, 4.0, 4.0, 9.0, 0.2);
	EXPECT_EQ (RefusalOf ([&] { f.Build ({a, far, b}); }), "Fronton : 'roofPocket' : les poches 0 et 2 se recouvrent");
	EXPECT_EQ (RefusalOf ([&] { f.Build ({far, a, a}); }), "Fronton : 'roofPocket' : les poches 1 et 2 se recouvrent");
	// Une poche fautive est refusee avant le recouvrement.
	RoofPocket bad = b;
	bad.zTop = f.zBase;
	EXPECT_TRUE (Starts (RefusalOf ([&] { f.Build ({a, bad}); }), "Fronton : 'roofPocket.zTop' doit etre au-dessus"));

	EXPECT_EQ (RefusalOf ([&] { f.Build ({a, PocketAt (f, 3.0, 4.0, -5.0, 5.0, 0.2)}); }), "");
	EXPECT_EQ (RefusalOf ([&] { f.Build ({a, PocketAt (f, -3.0, 3.0, 5.0, 9.0, 0.3)}); }), "");
	EXPECT_EQ (RefusalOf ([&] { f.Build ({a, PocketAt (f, 3.0, 4.0, 5.0, 9.0, 0.3)}); }),
	           "Fronton : 'roofPocket' : deux poches de tranches voisines ne se touchent que par une arete");
}

// PP4 : bornes de deux poches distinctes en double et confondues en float, en Y (toutes tranches) et en X (meme
// tranche) ; plafonds de deux poches au contact confondus en float ; les stations du raffinement refusent toute
// liste non vide.
TEST (TEST_cgarchi_classical_greek_pediment_pocket, pocket_bounds_merged_in_float_and_stations_are_refused)
{
	const Fixture &f = Hephaistos ();
	const RoofPocket a = PocketAt (f, -3.0, 3.0, -5.0, 5.0, 0.3);
	const std::string merged = "Fronton : sommets confondus en float : bornes de deux poches";
	const double afterY = std::nextafter (5.0, 6.0), afterX = std::nextafter (3.0, 4.0);
	ASSERT_EQ (static_cast<float> (afterY), 5.0f);
	ASSERT_EQ (static_cast<float> (afterX), 3.0f);
	EXPECT_EQ (RefusalOf ([&] { f.Build ({a, PocketAt (f, 3.5, 4.0, afterY, 9.0, 0.2)}); }), merged);
	EXPECT_EQ (RefusalOf ([&] { f.Build ({a, PocketAt (f, afterX, 4.0, -5.0, 5.0, 0.2)}); }), merged);
	// Poches au contact par une face, plafonds distincts en double et confondus en float.
	const double zA = f.zBase + 0.3, zB = std::nextafter (zA, zA + 1.0);
	ASSERT_EQ (static_cast<float> (zA), static_cast<float> (zB));
	EXPECT_EQ (RefusalOf ([&] { f.Build ({a, {3.0, 4.0, -5.0, 5.0, zB}}); }),
	           "Fronton : sommets confondus en float : plafonds de deux poches au contact");
	// Bornes X confondues dans des tranches separees par une autre : admises ; dans deux tranches voisines, elles se
	// rencontrent dans leur plan de coupe : refusees, comme deux plafonds distincts en double, confondus en float, au
	// droit d'une meme colonne de part et d'autre de la coupe.
	EXPECT_EQ (RefusalOf ([&] { f.Build ({a, PocketAt (f, afterX, 4.0, 6.0, 9.0, 0.2)}); }), "");
	EXPECT_EQ (RefusalOf ([&] { f.Build ({a, PocketAt (f, afterX, 4.0, 5.0, 9.0, 0.2)}); }), merged);
	EXPECT_EQ (RefusalOf ([&] { f.Build ({a, {-1.0, 1.0, 5.0, 9.0, zB}}); }),
	           "Fronton : sommets confondus en float : plafonds de deux poches au contact");
	EXPECT_EQ (RefusalOf ([&] { f.Build ({a, {-1.0, 1.0, 5.0, 9.0, zA}}); }), "");
	// Poches de tranches voisines au contact par une arete verticale seule (coins en damier dans le plan de coupe),
	// dans les deux sens (plafonds egaux : PP3) : refusees ; les emprises se recouvrant en X :
	// admises.
	const std::string edge = "Fronton : 'roofPocket' : deux poches de tranches voisines ne se touchent que par une arete";
	EXPECT_EQ (RefusalOf ([&] { f.Build ({PocketAt (f, -2.0, 0.0, -2.0, 2.0, 0.3), PocketAt (f, 0.0, 1.0, -3.0, -2.0, 0.2)}); }),
	           edge);
	EXPECT_EQ (RefusalOf ([&] { f.Build ({PocketAt (f, 0.0, 1.0, -2.0, 2.0, 0.2), PocketAt (f, -2.0, 0.0, -3.0, -2.0, 0.3)}); }),
	           edge);
	EXPECT_EQ (RefusalOf ([&] { f.Build ({PocketAt (f, -2.0, 0.0, -2.0, 2.0, 0.3), PocketAt (f, -0.5, 1.0, -3.0, -2.0, 0.2)}); }),
	           "");

	const RefinementStations none {};
	const std::vector<std::vector<RoofPocket>> lists = {{a}, {a, PocketAt (f, 3.5, 4.0, 6.0, 9.0, 0.2)}};
	for (const std::vector<RoofPocket> &pockets : lists)
		EXPECT_EQ (RefusalOf ([&] {
			           BuildGreekPedimentPieceMeshes (*f.temple.pediment, *f.temple.entablature, f.layout.peristyle,
			                                          f.layout.entablatureFoot, none, {}, pockets);
		           }),
		           "Fronton : 'roofPocket' ne se combine pas avec les stations du raffinement");
}
