#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <cgarchi/classical_greek_pediment.h>
#include <cgarchi/classical_temple.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "greek_doric_test_support.h"

// ===========================================================================
//  Fronton grec : corniches rampantes en Lambda et comble triangulaire (lot P1)
// ===========================================================================
//
// Reglages d'essai : SyntheticTempleWithPediment (greek_doric_test_support.h),
// valeurs synthetiques qui ne decrivent aucun monument (D3-4). Pieces construites
// sur l'anneau de ce temple : demi-distances 2,68 et 3,31, couronnement 0,85,
// donc X_e = 3,53 ; montee du haut 0,85 ; pente 1 : 4 ; nu du tympan 0,45 ;
// section de 14 sommets, T = 0,22. D'ou s = hypot (4, 1) / 4, H_b = 0,85 - T s
// = 0,62323, pieds de 2,49292 (h = 0) a 3,4 (h = T).
//
// Contraction FMA (GCC gnu++17 -O2 contracte a b + c) : les montees
// H_b + h s et les pieds r (H_b + h s) sont contractables ; elles ne se
// comparent ici qu'a kTol. Les egalites au bit pres ne portent que sur des
// sommes (plans du tympan, zBase, X_e) ou sur des valeurs de la bibliotheque
// relues par deux pieces.

using namespace cgarchi::classical;
using cgarchi_test::SyntheticPedimentSettings;
using cgarchi_test::SyntheticTempleWithPediment;

namespace {

// Tolerance des cotes recalculees ici, contractables dans la bibliotheque.
constexpr double kTol = 1e-6;

struct Pediment
{
	TempleSettings temple;
	TempleLayout layout;
	GreekPedimentSettings settings;
	std::vector<std::unique_ptr<Mesh>> pieces;   // Lambda Front, Lambda Back, comble
	// Sommes du repere, au bit pres (aucun produit) ; s et H_b recalcules, a kTol.
	double xEave = 0.0, yTympanum = 0.0, zBase = 0.0, scale = 0.0, baseRise = 0.0;
};

Pediment BuildSynthetic ()
{
	Pediment p;
	p.temple = SyntheticTempleWithPediment ();
	p.layout = LayoutTemple (p.temple);
	p.settings = *p.temple.pediment;
	const GreekEntablatureSettings &e = *p.temple.entablature;
	p.pieces = BuildGreekPedimentPieceMeshes (p.settings, e, p.layout.peristyle, p.layout.entablatureFoot);
	p.xEave = p.layout.peristyle.halfWidth + e.bands.back ().topProjection;
	p.yTympanum = p.layout.peristyle.halfDepth + p.settings.tympanumFace;
	p.zBase = p.layout.entablatureFoot + GreekEntablatureBandBounds (e).back ();
	p.scale = std::hypot (p.settings.rakingRun, 1.0) / p.settings.rakingRun;
	p.baseRise = p.settings.apexRise - p.settings.rakingSection.back ().z * p.scale;
	return p;
}

std::array<float, 3> Vertex (const Mesh &m, unsigned i)
{
	float q[3];
	m.GetVertex (i, q);
	return {q[0], q[1], q[2]};
}

// ulp float de [8, 16) : majorant de celui des coordonnees du jeu (|x| < 4,
// |y| < 4, z < 10).
double FloatUlp ()
{
	return static_cast<double> (std::nextafter (8.0f, 9.0f) - 8.0f);
}

double SectionArea (const std::vector<RingSectionPoint> &s)
{
	double twice = 0.0;
	for (std::size_t i = 0; i < s.size (); ++i)
	{
		const RingSectionPoint &a = s[i], &b = s[(i + 1) % s.size ()];
		twice += a.u * b.z - b.u * a.z;
	}
	return 0.5 * twice;
}

// Integrale de h sur la section (moment par rapport a h = 0).
double SectionMomentH (const std::vector<RingSectionPoint> &s)
{
	double sum = 0.0;
	for (std::size_t i = 0; i < s.size (); ++i)
	{
		const RingSectionPoint &a = s[i], &b = s[(i + 1) % s.size ()];
		sum += (a.u * b.z - b.u * a.z) * (a.z + b.z);
	}
	return sum / 6.0;
}

// Sommet de section d'un sommet du Lambda : y au bit pres (somme) ; au faite
// (x = 0), z a kTol de zBase + H_b + h s ; au pied (z = zBase), |x| a kTol de
// r (H_b + h s). -1 si aucun ou plusieurs.
int SectionIndex (const Pediment &p, const std::array<float, 3> &q, bool back)
{
	const std::vector<RingSectionPoint> &s = p.settings.rakingSection;
	const bool atRidge = q[0] == 0.0f;
	int found = -1;
	for (std::size_t k = 0; k < s.size (); ++k)
	{
		const double depth = p.layout.peristyle.halfDepth + (p.settings.tympanumFace + s[k].u);
		const float y = static_cast<float> (back ? depth : -depth);
		const double rise = p.baseRise + s[k].z * p.scale;
		const bool on = atRidge ? std::fabs (q[2] - (p.zBase + rise)) < kTol
		                        : std::fabs (std::fabs (q[0]) - p.settings.rakingRun * rise) < kTol;
		if (q[1] == y && on)
		{
			if (found >= 0)
				return -1;
			found = static_cast<int> (k);
		}
	}
	return found;
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

}  // namespace

// PG1 : chaque face de pied est la section, triangulee par decoupe d'oreilles,
// posee dans le plan z = float (zBase) : 12 triangles par bras, normale
// sortante -z, aire de meme signe pour tous dans (u_t + u_k, r (H_b + h_k s)),
// aire totale r s A a 1e-9 relatif ; ses sommets sont les 14 sommets de la
// section.
TEST (TEST_cgarchi_classical_greek_pediment, the_foot_face_is_the_section_capped_by_ear_clipping_on_the_cornice)
{
	const Pediment p = BuildSynthetic ();
	const std::vector<RingSectionPoint> &s = p.settings.rakingSection;
	const double expected = SectionArea (s) * p.settings.rakingRun * p.scale;
	const float zLow = static_cast<float> (p.zBase);
	for (const bool back : {false, true})
	{
		const Mesh &m = *p.pieces[back ? 1 : 0];
		for (const float side : {1.0f, -1.0f})
		{
			SCOPED_TRACE (std::string (back ? "Back" : "Front") + (side > 0.0f ? ", bras droit" : ", bras gauche"));
			std::size_t triangles = 0, positive = 0, negative = 0, downward = 0;
			std::set<int> covered;
			double area = 0.0;
			for (unsigned f = 0; f < m.GetNFaces (); ++f)
			{
				std::array<std::array<float, 3>, 3> q;
				bool cap = true;
				for (unsigned j = 0; j < 3; ++j)
				{
					q[j] = Vertex (m, static_cast<unsigned> (m.GetFaceVertex (f, j)));
					cap = cap && q[j][2] == zLow && q[j][0] * side > 0.0f;
				}
				if (!cap)
					continue;
				++triangles;
				std::array<int, 3> k {};
				for (unsigned j = 0; j < 3; ++j)
				{
					k[j] = SectionIndex (p, q[j], back);
					ASSERT_GE (k[j], 0) << "sommet de pied hors de la section, face " << f;
					covered.insert (k[j]);
				}
				auto U = [&] (int i) { return p.settings.tympanumFace + s[static_cast<std::size_t> (i)].u; };
				auto X = [&] (int i) { return p.settings.rakingRun * (p.baseRise + s[static_cast<std::size_t> (i)].z * p.scale); };
				const double twice = (U (k[1]) - U (k[0])) * (X (k[2]) - X (k[0])) - (U (k[2]) - U (k[0])) * (X (k[1]) - X (k[0]));
				(twice > 0.0 ? positive : negative) += 1;
				area += 0.5 * std::fabs (twice);
				const double nz = (static_cast<double> (q[1][0]) - q[0][0]) * (static_cast<double> (q[2][1]) - q[0][1]) -
				                  (static_cast<double> (q[1][1]) - q[0][1]) * (static_cast<double> (q[2][0]) - q[0][0]);
				if (nz < 0.0)
					++downward;
			}
			EXPECT_EQ (triangles, s.size () - 2);
			EXPECT_TRUE (positive == triangles || negative == triangles)
				<< positive << " triangles d'un sens, " << negative << " de l'autre";
			EXPECT_EQ (downward, triangles) << "normales sortantes -z";
			EXPECT_EQ (covered.size (), s.size ());
			EXPECT_NEAR (area, expected, 1e-9 * expected);
		}
	}
	EXPECT_NEAR (expected, 0.258312567445, 1e-9);
	RecordProperty ("cap_triangles", static_cast<int> (s.size () - 2));
}

// PG2 : une corniche rampante est un Lambda ferme de 3n = 42 sommets et
// 6n - 4 = 80 faces, valide, d'une composante ; volume de la formule
// 2 hypot (r, 1) (H_b A + s M_h), M_h l'integrale de h sur la section, egal a
// 0,392224114656 a 1e-9 relatif, et celui du maillage a l'aire fois l'ulp float
// pres ; chaque sommet de pied a float (zFoot + z11) au bit pres, aucun sommet
// dessous ; pieds dans [r H_b, r H_t] a kTol, aucun au-dela de float (X_e) ;
// faite a x = 0 ; dos a y = -float (3,31 + 0,45), au bit pres.
TEST (TEST_cgarchi_classical_greek_pediment, a_raking_cornice_is_a_closed_lambda_cut_horizontally_on_the_cornice)
{
	const Pediment p = BuildSynthetic ();
	const std::vector<RingSectionPoint> &s = p.settings.rakingSection;
	ASSERT_EQ (p.pieces.size (), 3u);
	const Mesh &m = *p.pieces[0];
	EXPECT_EQ (m.GetNVertices (), 3 * s.size ());
	EXPECT_EQ (m.GetNFaces (), 6 * s.size () - 4);
	EXPECT_EQ (m.GetNVertices (), 42u);
	EXPECT_EQ (m.GetNFaces (), 80u);
	const MeshValidationReport r = ValidateMesh (m);
	EXPECT_TRUE (r.IsValid ());
	EXPECT_EQ (r.components, 1u);
	const double formula =
		2.0 * std::hypot (p.settings.rakingRun, 1.0) * (p.baseRise * SectionArea (s) + p.scale * SectionMomentH (s));
	EXPECT_NEAR (formula, 0.392224114656, 1e-9 * 0.392224114656);
	EXPECT_GT (r.signedVolume, 0.0);
	EXPECT_NEAR (r.signedVolume, formula, r.area * FloatUlp ());
	RecordProperty ("lambda_volume_gap", std::to_string (r.signedVolume - formula));

	const float zLow = static_cast<float> (p.zBase), xMax = static_cast<float> (p.xEave);
	const float yBack = -static_cast<float> (p.yTympanum);
	const double r0 = p.settings.rakingRun * p.baseRise, r1 = p.settings.rakingRun * p.settings.apexRise;
	std::size_t ridge = 0, feet = 0, backs = 0;
	for (unsigned i = 0; i < m.GetNVertices (); ++i)
	{
		const std::array<float, 3> q = Vertex (m, i);
		EXPECT_GE (q[2], zLow) << i;
		EXPECT_LE (std::fabs (q[0]), xMax) << i;
		const bool atRidge = q[0] == 0.0f;
		EXPECT_TRUE (atRidge || q[2] == zLow) << "sommet " << i << " ni au faite ni au pied";
		const int k = SectionIndex (p, q, false);
		ASSERT_GE (k, 0) << "sommet " << i << " hors de la section";
		(atRidge ? ridge : feet) += 1;
		if (!atRidge)
		{
			EXPECT_GE (std::fabs (q[0]), r0 - kTol) << i;
			EXPECT_LE (std::fabs (q[0]), r1 + kTol) << i;
		}
		if (s[static_cast<std::size_t> (k)].u == 0.0)
		{
			EXPECT_EQ (q[1], yBack) << i;
			++backs;
		}
	}
	EXPECT_EQ (ridge, s.size ());
	EXPECT_EQ (feet, 2 * s.size ());
	EXPECT_EQ (backs, 6u);
	RecordProperty ("lambda_faces", static_cast<int> (m.GetNFaces ()));
}

// PG3 : le comble est un prisme triangulaire de 6 sommets et 8 faces, valide,
// entre les deux tympans ; volume de la formule r H_t^2 2 Y_t egal a
// 21,7328 a 1e-9 relatif, celui du maillage a l'aire fois l'ulp pres ; abouts a
// y = -+float (3,31 + 0,45), base a float (zFoot + z11), au bit pres ; ses six
// sommets egaux bit pour bit aux sommets T (pieds et faite) des deux Lambda.
TEST (TEST_cgarchi_classical_greek_pediment, the_roof_is_a_triangular_prism_on_the_tops_of_the_two_lambdas)
{
	const Pediment p = BuildSynthetic ();
	const Mesh &m = *p.pieces[2];
	EXPECT_EQ (m.GetNVertices (), 6u);
	EXPECT_EQ (m.GetNFaces (), 8u);
	const MeshValidationReport r = ValidateMesh (m);
	EXPECT_TRUE (r.IsValid ());
	EXPECT_EQ (r.components, 1u);
	const double formula = p.settings.rakingRun * p.settings.apexRise * p.settings.apexRise * 2.0 * p.yTympanum;
	EXPECT_NEAR (formula, 21.7328, 1e-9 * 21.7328);
	EXPECT_GT (r.signedVolume, 0.0);
	EXPECT_NEAR (r.signedVolume, formula, r.area * FloatUlp ());

	const float yT = static_cast<float> (p.yTympanum), zLow = static_cast<float> (p.zBase);
	std::size_t front = 0, bottom = 0;
	for (unsigned i = 0; i < m.GetNVertices (); ++i)
	{
		const std::array<float, 3> q = Vertex (m, i);
		EXPECT_EQ (std::fabs (q[1]), yT) << i;
		front += q[1] < 0.0f ? 1 : 0;
		bottom += q[2] == zLow ? 1 : 0;
		EXPECT_TRUE ((q[2] == zLow) != (q[0] == 0.0f)) << "sommet " << i << " ni a la base hors de l'axe, ni au faite";
		// Sommet T du Lambda de sa facade : le plus haut sur (x, y), ou un pied du dos.
		const Mesh &lambda = *p.pieces[q[1] < 0.0f ? 0 : 1];
		bool found = false;
		for (unsigned j = 0; j < lambda.GetNVertices () && !found; ++j)
		{
			const std::array<float, 3> v = Vertex (lambda, j);
			found = v[0] == q[0] && v[1] == q[1] && v[2] == q[2];
		}
		EXPECT_TRUE (found) << "sommet " << i << " du comble absent du Lambda";
	}
	EXPECT_EQ (front, 3u);
	EXPECT_EQ (bottom, 4u);
	RecordProperty ("roof_volume_gap", std::to_string (r.signedVolume - formula));
}

// PG4 : refus par champ, prefixe "Fronton : ", dans l'ordre du contrat ; une
// marche de 1e-9 est acceptee par la validation et refusee au maillage ; une
// pente trop douce, dont le pied du haut passe l'avant du couronnement, est
// acceptee par la validation et refusee au maillage.
TEST (TEST_cgarchi_classical_greek_pediment, invalid_pediment_settings_are_refused_naming_the_field)
{
	const TempleSettings t = SyntheticTempleWithPediment ();
	const GreekEntablatureSettings &e = *t.entablature;
	const double nan = std::numeric_limits<double>::quiet_NaN ();
	const double inf = std::numeric_limits<double>::infinity ();
	auto expectRefused = [&] (const GreekPedimentSettings &q, const std::string &field, const std::string &rule,
	                          const std::string &what) {
		const std::string refusal = RefusalOf ([&] { ValidateGreekPedimentSettings (q, e); });
		EXPECT_EQ (refusal.rfind ("Fronton : '" + field + "'", 0), 0u) << what << " : '" << refusal << "'";
		EXPECT_NE (refusal.find (rule), std::string::npos) << what << " : '" << refusal << "'";
	};
	auto expectAccepted = [&] (const GreekPedimentSettings &q, const std::string &what) {
		EXPECT_EQ (RefusalOf ([&] { ValidateGreekPedimentSettings (q, e); }), "") << what;
	};
	expectAccepted (SyntheticPedimentSettings (), "jeu synthetique");

	for (const double h : {0.0, -0.1, nan})
	{
		GreekPedimentSettings q = SyntheticPedimentSettings ();
		q.apexRise = h;
		q.rakingRun = nan;
		expectRefused (q, "pediment.apexRise", "fini", "montee " + std::to_string (h) + ", avant la pente");
	}
	for (const double r : {0.0, -4.0, nan, inf})
	{
		GreekPedimentSettings q = SyntheticPedimentSettings ();
		q.rakingRun = r;
		q.tympanumFace = nan;
		expectRefused (q, "pediment.rakingRun", "fini", "pente 1 : " + std::to_string (r) + ", avant le tympan");
	}
	for (const double u : {nan, -0.44, 0.85})
	{
		GreekPedimentSettings q = SyntheticPedimentSettings ();
		q.tympanumFace = u;
		expectRefused (q, "pediment.tympanumFace", "couronnement", "nu du tympan " + std::to_string (u));
	}
	for (const double u : {0.849, -0.439})
	{
		GreekPedimentSettings q = SyntheticPedimentSettings ();
		q.tympanumFace = u;
		expectAccepted (q, "nu du tympan " + std::to_string (u));
	}

	auto withSection = [] (const std::function<void (std::vector<RingSectionPoint> &)> &edit) {
		GreekPedimentSettings q = SyntheticPedimentSettings ();
		edit (q.rakingSection);
		return q;
	};
	const std::string section = "pediment.rakingSection";
	expectRefused (withSection ([] (auto &s) { s = {{0.0, 0.0}, {0.0, 0.22}}; }), section, "au moins 3", "deux sommets");
	expectRefused (withSection ([&] (auto &s) { s[5].z = nan; }), section + "[5]", "fini", "sommet NaN");
	expectRefused (withSection ([] (auto &s) { s[0] = {0.0, 0.01}; }), section, "commencer", "premier (0 ; 0,01)");
	expectRefused (withSection ([] (auto &s) { s.back () = {0.01, 0.22}; }), section, "finir", "dernier (0,01 ; 0,22)");
	expectRefused (withSection ([] (auto &s) { s.back () = {0.0, 0.21}; }), section, "finir", "dernier (0 ; 0,21), h max 0,22");
	expectRefused (withSection ([] (auto &s) { s[7] = {0.0, 0.1}; }), section, "sommet 7", "sommet (0 ; 0,1) en milieu");
	expectRefused (withSection ([] (auto &s) { s[7] = {0.33, -0.01}; }), section, "sommet 7", "h -0,01");
	expectRefused (withSection ([] (auto &s) { s.insert (s.begin () + 6, s[5]); }), section, "consecutifs", "doublon consecutif");
	expectRefused (withSection ([] (auto &s) { std::swap (s[5], s[6]); }), section, "simple", "sommets 5 et 6 permutes");

	// H_b = apexRise - T s : refuse a 0,2 (T s = 0,2268), accepte a 0,25.
	{
		GreekPedimentSettings low = SyntheticPedimentSettings ();
		low.apexRise = 0.2;
		expectRefused (low, "pediment.apexRise", "T s", "montee 0,2 sous la hauteur verticale de la corniche");
		low.apexRise = 0.25;
		expectAccepted (low, "montee 0,25");
	}

	// L'entablement est valide d'abord.
	GreekEntablatureSettings bad = e;
	bad.bands.clear ();
	GreekPedimentSettings q = SyntheticPedimentSettings ();
	q.apexRise = 0.0;
	const std::string first = RefusalOf ([&] { ValidateGreekPedimentSettings (q, bad); });
	EXPECT_EQ (first.rfind ("Entablement : ", 0), 0u) << first;

	// Pente trop douce : r H_t passe X_e = 3,53 (r = 4,2 : 3,57), acceptee par la
	// validation, refusee au maillage ; r = 4,15 (3,5275) acceptee.
	const TempleLayout l = LayoutTemple (t);
	{
		GreekPedimentSettings gentle = SyntheticPedimentSettings ();
		gentle.rakingRun = 4.2;
		expectAccepted (gentle, "pente 1 : 4,2");
		const std::string far = RefusalOf ([&] { BuildGreekPedimentPieceMeshes (gentle, e, l.peristyle, l.entablatureFoot); });
		EXPECT_EQ (far.rfind ("Fronton : 'pediment.rakingRun'", 0), 0u) << far;
		EXPECT_NE (far.find ("avant du couronnement"), std::string::npos) << far;
		gentle.rakingRun = 4.15;
		EXPECT_EQ (RefusalOf ([&] { BuildGreekPedimentPieceMeshes (gentle, e, l.peristyle, l.entablatureFoot); }), "");
	}

	// Dessous dans l'axe a 1e-9 au-dessus de z11 : H_b > 0 accepte par la
	// validation, faite confondu en float avec le dessus de l'anneau, refuse au
	// maillage (paroi en lame).
	{
		GreekPedimentSettings blade = SyntheticPedimentSettings ();
		const double s = std::hypot (blade.rakingRun, 1.0) / blade.rakingRun;
		blade.apexRise = blade.rakingSection.back ().z * s + 1e-9;
		expectAccepted (blade, "H_b de 1e-9");
		const std::string thin = RefusalOf ([&] { BuildGreekPedimentPieceMeshes (blade, e, l.peristyle, l.entablatureFoot); });
		EXPECT_EQ (thin.rfind ("Fronton : 'pediment.apexRise'", 0), 0u) << thin;
		EXPECT_NE (thin.find ("float"), std::string::npos) << thin;
	}

	// Marche de 1e-9 : deux sommets voisins distincts en double, confondus en float.
	const GreekPedimentSettings step = withSection ([] (auto &s) {
		s.insert (s.begin () + 4, RingSectionPoint {0.05 + 1e-9, 0.02});
		s[5].u = 0.05 + 1e-9;
	});
	expectAccepted (step, "marche de 1e-9");
	const std::string mesh = RefusalOf ([&] { BuildGreekPedimentPieceMeshes (step, e, l.peristyle, l.entablatureFoot); });
	EXPECT_EQ (mesh.rfind ("Fronton : sommets confondus en float", 0), 0u) << mesh;

	// Demi-distances et cote du pied finies.
	PeristyleLayout open = l.peristyle;
	open.halfDepth = nan;
	const std::string depth = RefusalOf ([&] { BuildGreekPedimentPieceMeshes (SyntheticPedimentSettings (), e, open, l.entablatureFoot); });
	EXPECT_EQ (depth.rfind ("Fronton : demi-distance", 0), 0u) << depth;
	const std::string foot = RefusalOf ([&] { BuildGreekPedimentPieceMeshes (SyntheticPedimentSettings (), e, l.peristyle, nan); });
	EXPECT_EQ (foot.rfind ("Fronton : demi-distance", 0), 0u) << foot;

	// Avant du couronnement et plan du tympan du cote positif de leurs axes.
	for (const double hw : {-1.0, -0.85})
	{
		PeristyleLayout narrow = l.peristyle;
		narrow.halfWidth = hw;
		const std::string refusal = RefusalOf ([&] { BuildGreekPedimentPieceMeshes (SyntheticPedimentSettings (), e, narrow, l.entablatureFoot); });
		EXPECT_EQ (refusal.rfind ("Fronton : 'peristyle.halfWidth'", 0), 0u) << "halfWidth " << hw << " : '" << refusal << "'";
	}
	PeristyleLayout behind = l.peristyle;
	behind.halfDepth = -1.0;
	const std::string tympanum = RefusalOf ([&] { BuildGreekPedimentPieceMeshes (SyntheticPedimentSettings (), e, behind, l.entablatureFoot); });
	EXPECT_EQ (tympanum.rfind ("Fronton : 'peristyle.halfDepth'", 0), 0u) << tympanum;

	// Section dont l'aire s'annule en double (produits sous le plus petit
	// double) : acceptee par la validation, refusee au maillage par la garde float.
	for (const double a : {1e-162, 1e-200})
	{
		GreekPedimentSettings tiny = SyntheticPedimentSettings ();
		tiny.rakingSection = {{0.0, 0.0}, {a, 0.0}, {a, a}, {0.0, a}};
		expectAccepted (tiny, "section de cote " + std::to_string (a));
		const std::string collapse = RefusalOf ([&] { BuildGreekPedimentPieceMeshes (tiny, e, l.peristyle, l.entablatureFoot); });
		EXPECT_EQ (collapse.rfind ("Fronton : sommets confondus en float", 0), 0u) << a << " : '" << collapse << "'";
	}
}

// PG5 : le Lambda Back est le miroir du Front : memes sommets, y oppose au bit
// pres, faces retournees ; volumes positifs et egaux.
TEST (TEST_cgarchi_classical_greek_pediment, the_back_pediment_mirrors_the_front_one)
{
	const Pediment p = BuildSynthetic ();
	const Mesh &front = *p.pieces[0], &back = *p.pieces[1];
	ASSERT_EQ (front.GetNVertices (), back.GetNVertices ());
	ASSERT_EQ (front.GetNFaces (), back.GetNFaces ());
	for (unsigned i = 0; i < front.GetNVertices (); ++i)
	{
		const std::array<float, 3> a = Vertex (front, i), b = Vertex (back, i);
		EXPECT_TRUE (a[0] == b[0] && a[1] == -b[1] && a[2] == b[2] && std::signbit (a[1]) != std::signbit (b[1])) << i;
	}
	for (unsigned f = 0; f < front.GetNFaces (); ++f)
		EXPECT_TRUE (back.GetFaceVertex (f, 0) == front.GetFaceVertex (f, 0) && back.GetFaceVertex (f, 1) == front.GetFaceVertex (f, 2) &&
		             back.GetFaceVertex (f, 2) == front.GetFaceVertex (f, 1))
			<< f;
	const MeshValidationReport rf = ValidateMesh (front), rb = ValidateMesh (back);
	EXPECT_TRUE (rb.IsValid ());
	EXPECT_GT (rf.signedVolume, 0.0);
	EXPECT_GT (rb.signedVolume, 0.0);
	EXPECT_NEAR (rb.signedVolume, rf.signedVolume, rf.area * FloatUlp ());
}
