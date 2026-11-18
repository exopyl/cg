#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
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

struct SliceReport
{
	std::size_t slices = 0;
	bool closed = true, contiguous = true;
	double volume = 0.0;
};

// Tranches du comble (pieces 2 et suivantes) : chacune valide, d'une composante, de volume positif ; bornes Y
// contigues au bit pres, par Y croissants.
SliceReport SlicesOf (const std::vector<std::unique_ptr<Mesh>> &pieces)
{
	SliceReport s;
	float previousHigh = 0.0f;
	for (std::size_t k = 2; k < pieces.size (); ++k)
	{
		const Mesh &m = *pieces[k];
		const MeshValidationReport report = ValidateMesh (m);
		s.closed = s.closed && report.IsValid () && report.components == 1u && report.signedVolume > 0.0;
		s.volume += report.signedVolume;
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
	return s;
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

// PP2 : deux et cinq poches. Nombre de tranches (bornes Y distinctes moins une), chaque tranche fermee, contigue a
// la precedente ; somme des volumes = prisme - poches, a 1e-9 relatif ; Lambda inchanges. Avec des poches au contact
// par une face (en X et en Y), les memes proprietes.
TEST (TEST_cgarchi_classical_greek_pediment_pocket, several_pockets_slice_the_roof_along_y)
{
	const Fixture &f = Hephaistos ();
	const double prism = PrismVolume (f);
	ASSERT_GT (prism, 0.0);
	struct Case
	{
		const char *name;
		std::vector<RoofPocket> pockets;
		std::size_t slices;
	};
	const std::vector<Case> cases = {
	    {"deux", {PocketAt (f, -3.0, 3.0, -5.0, 5.0, 0.3), PocketAt (f, 3.5, 5.0, -8.0, 9.0, 0.2)}, 5u},
	    {"cinq",
	     {PocketAt (f, -3.0, 3.0, -5.0, 5.0, 0.3), PocketAt (f, 3.5, 5.0, -8.0, 9.0, 0.2), PocketAt (f, -5.0, -3.5, -8.0, 9.0, 0.2),
	      PocketAt (f, -2.0, 2.0, -12.0, -10.0, 0.25), PocketAt (f, -2.0, 2.0, 10.0, 12.0, 0.25)},
	     9u},
	    {"contact",
	     {PocketAt (f, -3.0, 3.0, -5.0, 5.0, 0.3), PocketAt (f, 3.0, 5.0, -8.0, 9.0, 0.2), PocketAt (f, -5.0, -3.0, -8.0, 9.0, 0.3),
	      PocketAt (f, -2.0, 2.0, -12.0, -8.0, 0.25), PocketAt (f, -2.0, 2.0, -8.0, -5.0, 0.1)},
	     6u},
	};
	const std::vector<std::unique_ptr<Mesh>> plain = f.Build ({});
	for (const Case &c : cases)
	{
		SCOPED_TRACE (c.name);
		const std::vector<std::unique_ptr<Mesh>> pieces = f.Build (c.pockets);
		const SliceReport s = SlicesOf (pieces);
		EXPECT_EQ (s.slices, c.slices);
		EXPECT_TRUE (s.closed);
		EXPECT_TRUE (s.contiguous);
		double expected = prism;
		for (const RoofPocket &p : c.pockets)
			expected -= FloatVolume (p, f);
		EXPECT_NEAR (s.volume, expected, kRelativeVolume * expected);
		for (std::size_t k = 0; k < 2; ++k)
		{
			const Roof a = RoofOf (*plain[k]);
			EXPECT_TRUE (SameAs (RoofOf (*pieces[k]), a.positions, a.faces)) << "Lambda " << k;
		}
	}
}

// PP3 : recouvrement refuse, apres les refus de chaque poche ; poches contigues (contact par une face) acceptees.
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
	EXPECT_EQ (RefusalOf ([&] { f.Build ({a, PocketAt (f, 3.0, 4.0, 5.0, 9.0, 0.3)}); }), "");
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
	// Bornes X confondues dans des tranches differentes : admises.
	EXPECT_EQ (RefusalOf ([&] { f.Build ({a, PocketAt (f, afterX, 4.0, 6.0, 9.0, 0.2)}); }), "");

	const RefinementStations none {};
	const std::vector<std::vector<RoofPocket>> lists = {{a}, {a, PocketAt (f, 3.5, 4.0, 6.0, 9.0, 0.2)}};
	for (const std::vector<RoofPocket> &pockets : lists)
		EXPECT_EQ (RefusalOf ([&] {
			           BuildGreekPedimentPieceMeshes (*f.temple.pediment, *f.temple.entablature, f.layout.peristyle,
			                                          f.layout.entablatureFoot, none, {}, pockets);
		           }),
		           "Fronton : 'roofPocket' ne se combine pas avec les stations du raffinement");
}
