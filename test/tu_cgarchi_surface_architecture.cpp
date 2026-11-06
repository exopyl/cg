#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "../src/cgmesh/cgmesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include <cgarchi/surface_architecture.h>   // CreateBlock, CreateArch, sampleArc*, Rosace, ArcBrise

using namespace cgarchi;   // fichier de test de cgarchi seul

namespace
{

void polygon2_tesselation_export (Polygon2 *pol, const char *filename)
{
	// tesselation
	unsigned int nVertices, nFaces;
	float *pVertices = NULL;
	unsigned int *pFaces = NULL;
	pol->tesselate (&pVertices, &nVertices, &pFaces, &nFaces);

	// export
	Mesh *m = new Mesh ();
	m->SetVertices (nVertices, pVertices);
	m->SetFaces (nFaces, 3, pFaces, NULL);
	m->save ((char*)filename);

	// cleaning
	free (pVertices);
	free (pFaces);
	delete m;
}

struct Box { float lo[3], hi[3]; };

Box MeshBox (const Mesh &m)
{
	Box b;
	for (int k = 0; k < 3; ++k)
	{
		b.lo[k] = (std::numeric_limits<float>::max) ();
		b.hi[k] = -(std::numeric_limits<float>::max) ();
	}
	for (unsigned v = 0; v < m.GetNVertices (); ++v)
	{
		float p[3];
		m.GetVertex (v, p);
		for (int k = 0; k < 3; ++k)
		{
			b.lo[k] = std::fmin (b.lo[k], p[k]);
			b.hi[k] = std::fmax (b.hi[k], p[k]);
		}
	}
	return b;
}

// Coque fermee et orientee, faces de toute arite, sur les indices bruts :
// chaque arete orientee (a, b) apparait une fois, et son opposee (b, a) aussi.
// Rend le nombre d'aretes orientees fautives (0 = coque fermee orientee).
std::size_t OpenOrInconsistentEdges (const Mesh &m)
{
	std::map<std::pair<unsigned, unsigned>, int> directed;
	for (unsigned f = 0; f < m.GetNFaces (); ++f)
	{
		const unsigned n = static_cast<unsigned> (m.GetFaceNVertices (f));
		for (unsigned k = 0; k < n; ++k)
		{
			const unsigned a = static_cast<unsigned> (m.GetFaceVertex (f, k));
			const unsigned b = static_cast<unsigned> (m.GetFaceVertex (f, (k + 1) % n));
			++directed[{a, b}];
		}
	}
	std::size_t bad = 0;
	for (const auto &[edge, count] : directed)
	{
		const auto opposite = directed.find ({edge.second, edge.first});
		if (count != 1 || opposite == directed.end () || opposite->second != 1)
			++bad;
	}
	return bad;
}

// Volume signe d'une coque, faces en eventail depuis leur premier sommet.
double SignedVolume (const Mesh &m)
{
	double volume = 0.0;
	for (unsigned f = 0; f < m.GetNFaces (); ++f)
	{
		const unsigned n = static_cast<unsigned> (m.GetFaceNVertices (f));
		float p0[3];
		m.GetVertex (static_cast<unsigned> (m.GetFaceVertex (f, 0)), p0);
		for (unsigned k = 1; k + 1 < n; ++k)
		{
			float p1[3], p2[3];
			m.GetVertex (static_cast<unsigned> (m.GetFaceVertex (f, k)), p1);
			m.GetVertex (static_cast<unsigned> (m.GetFaceVertex (f, k + 1)), p2);
			volume += (static_cast<double> (p0[0]) * (static_cast<double> (p1[1]) * p2[2] - static_cast<double> (p1[2]) * p2[1])
			         - static_cast<double> (p0[1]) * (static_cast<double> (p1[0]) * p2[2] - static_cast<double> (p1[2]) * p2[0])
			         + static_cast<double> (p0[2]) * (static_cast<double> (p1[0]) * p2[1] - static_cast<double> (p1[1]) * p2[0])) / 6.0;
		}
	}
	return volume;
}

void ExpectUnitTangentsAlongTheIndices (const ArcSamples &s)
{
	ASSERT_EQ (s.points.size (), s.tangents.size ());
	for (std::size_t i = 0; i < s.tangents.size (); ++i)
		EXPECT_NEAR (s.tangents[i].getLength (), 1.f, 1e-5f) << i;
	// Deux echantillons : la corde joint les pieds, perpendiculaire a la tangente.
	if (s.points.size () < 3)
		return;
	for (std::size_t i = 0; i + 1 < s.points.size (); ++i)
	{
		const Vector2f d = s.points[i + 1] - s.points[i];
		if (d.getLength () < 1e-6f)
			continue;   // echantillon repete (sommet de l'arc brise, npts pair)
		EXPECT_GT (s.tangents[i].DotProduct (d), 0.f) << i;
	}
}

// Formule de l'ancien create_arc_brise pour npts pair, oracle de la
// caracterisation de sampleArcBrise.
std::vector<Vector2f> LegacyArcBrise (float a, float b, unsigned npts)
{
	std::vector<Vector2f> p (npts);
	const unsigned hnpts = npts/2;
	const Vector2f C (static_cast<float> ((a*a - b*b)/(2.*a)), 0.f);
	const float r = a-C[0];
	const float alpha_max = std::atan (-b/C[0]);
	for (unsigned i=0; i<hnpts; i++)
	{
		const float alpha = alpha_max * static_cast<float> (i) / static_cast<float> (hnpts - 1);
		p[i] = Vector2f (C[0]+r*std::cos (alpha), C[1]+r*std::sin (alpha));
		p[npts-1-i] = Vector2f (-p[i][0], p[i][1]);
	}
	return p;
}

Profile2D SquareMoulure ()
{
	Profile2D p;
	p.points = {Vector2d (0., 0.), Vector2d (1., 0.), Vector2d (1., 1.), Vector2d (0., 1.)};
	return p;
}

ArcSamples StraightCurve (unsigned n)
{
	ArcSamples c;
	for (unsigned i = 0; i < n; ++i)
	{
		c.points.push_back (Vector2f (0.f, static_cast<float> (i)));
		c.tangents.push_back (Vector2f (0.f, 1.f));
	}
	return c;
}

} // namespace

// ===========================================================================
//  CreateBlock, CreateArch, CreateArch2
// ===========================================================================

TEST(TEST_cgarchi_surface_architecture, block)
{
	const std::unique_ptr<Mesh> block = CreateBlock ();
	ASSERT_NE(block, nullptr);
	EXPECT_EQ(block->GetNVertices (), 24u);
	EXPECT_EQ(block->GetNFaces (), 26u);

	const Box b = MeshBox (*block);
	const float half[3] = {1.61803399f/2.f, 1.f/2.f, 1.f/2.f};   // largeur, profondeur, hauteur
	for (int k = 0; k < 3; ++k)
	{
		EXPECT_NEAR(b.lo[k], -half[k], 1e-6f) << k;
		EXPECT_NEAR(b.hi[k],  half[k], 1e-6f) << k;
	}

	EXPECT_EQ(OpenOrInconsistentEdges (*block), 0u);
	EXPECT_GT(SignedVolume (*block), 0.0);
	// le chanfrein retire de la matiere a la boite
	EXPECT_LT(SignedVolume (*block), 1.61803399 * 1. * 1.);
}

TEST(TEST_cgarchi_surface_architecture, block_follows_its_dimensions)
{
	const std::unique_ptr<Mesh> block = CreateBlock (4.f, 2.f, 3.f, 0.f);
	const Box b = MeshBox (*block);
	EXPECT_NEAR(b.hi[0] - b.lo[0], 4.f, 1e-6f);
	EXPECT_NEAR(b.hi[1] - b.lo[1], 3.f, 1e-6f);
	EXPECT_NEAR(b.hi[2] - b.lo[2], 2.f, 1e-6f);
	// sans chanfrein, le bloc est le pave plein
	EXPECT_NEAR(SignedVolume (*block), 4. * 3. * 2., 1e-5);
}

TEST(TEST_cgarchi_surface_architecture, wall)
{
	const float width = 1.61803399f;
	const float depth = 1.f;
	const float height = 1.f;
	const float bevel = 0.05f;

	Mesh wall;
	for (unsigned int i=0; i<10; i++)
	{
		for (unsigned int j=0; j<5; j++)
		{
			const std::unique_ptr<Mesh> b = CreateBlock (width, height, depth, bevel);
			b->translate (j*width, 0.f, i*height);
			if (i%2)
				b->translate (width/2.f, 0.f, 0.f);
			wall.Append (b.get ());
		}
	}
	EXPECT_EQ(wall.GetNVertices (), 50u * 24u);
	EXPECT_EQ(wall.GetNFaces (), 50u * 26u);

	const Box b = MeshBox (wall);
	EXPECT_NEAR(b.lo[0], -width/2.f, 1e-5f);
	EXPECT_NEAR(b.hi[0], 4.f*width + width/2.f + width/2.f, 1e-5f);
	EXPECT_NEAR(b.lo[1], -depth/2.f, 1e-6f);
	EXPECT_NEAR(b.hi[1],  depth/2.f, 1e-6f);
	EXPECT_NEAR(b.lo[2], -height/2.f, 1e-6f);
	EXPECT_NEAR(b.hi[2], 9.f*height + height/2.f, 1e-5f);

	// 50 coques fermees juxtaposees : chaque bloc garde ses propres sommets
	EXPECT_EQ(OpenOrInconsistentEdges (wall), 0u);
	const std::unique_ptr<Mesh> one = CreateBlock (width, height, depth, bevel);
	EXPECT_NEAR(SignedVolume (wall), 50. * SignedVolume (*one), 1e-3);
}

TEST(TEST_cgarchi_surface_architecture, arch)
{
	const std::unique_ptr<Mesh> arch = CreateArch ();
	ASSERT_NE(arch, nullptr);
	EXPECT_EQ(arch->GetNVertices (), 94u);
	EXPECT_EQ(arch->GetNFaces (), 133u);

	// piles de 2 x 2 de part et d'autre d'une porte de 6, hauteur 6
	const Box b = MeshBox (*arch);
	EXPECT_NEAR(b.lo[0], -5.f, 1e-6f);
	EXPECT_NEAR(b.hi[0],  5.f, 1e-6f);
	EXPECT_NEAR(b.lo[1], -1.f, 1e-6f);
	EXPECT_NEAR(b.hi[1],  1.f, 1e-6f);
	EXPECT_NEAR(b.lo[2],  0.f, 1e-6f);
	EXPECT_NEAR(b.hi[2],  6.f, 1e-6f);

	EXPECT_EQ(OpenOrInconsistentEdges (*arch), 0u);
	// pave plein 10 x 2 x 6, moins la baie : rectangle 6 x 2 et demi-disque de rayon 3
	const double opening = (6. * 2. + 0.5 * 3.14159265358979 * 3. * 3.) * 2.;
	EXPECT_NEAR(std::fabs (SignedVolume (*arch)), 10. * 2. * 6. - opening, 0.05);

	arch->save ((char*)"architecture_arch.obj");
}

TEST(TEST_cgarchi_surface_architecture, arch2_is_a_circle_of_360_vertices)
{
	const std::unique_ptr<Mesh> arch2 = CreateArch2 ();
	ASSERT_NE(arch2, nullptr);
	EXPECT_EQ(arch2->GetNVertices (), 360u);
	EXPECT_EQ(arch2->GetNFaces (), 0u);
	for (unsigned v = 0; v < arch2->GetNVertices (); ++v)
	{
		float p[3];
		arch2->GetVertex (v, p);
		EXPECT_NEAR(std::hypot (p[0], p[1]), 4.f, 1e-5f) << v;
		EXPECT_EQ(p[2], 0.f) << v;
	}
}

// CreateArch2 ne construit aucune face (debt_cgarchi.md, plan 1 phase 3) :
// desactive tant que la forme visee n'est pas definie.
TEST(TEST_cgarchi_surface_architecture, DISABLED_arch2_has_faces)
{
	const std::unique_ptr<Mesh> arch2 = CreateArch2 ();
	EXPECT_GT(arch2->GetNFaces (), 0u);
}

// ===========================================================================
//  Rosace, ArcBrise
// ===========================================================================

TEST(TEST_cgarchi_surface_architecture, rosace)
{
	auto r = std::make_unique<Rosace> ();
	r->SetNFoils (6);
	std::unique_ptr<Polygon2> pol_rosace (r->Generate ());
	ASSERT_NE(pol_rosace, nullptr);

	// Characterization: pins the `center`-based first foil point across the
	// vec2 -> TVector2 migration. With 6 foils, resolution 20: 120 points, and
	// the first point is (center_x, 0) = ((1-1/3)cos(30deg), 0).
	EXPECT_EQ(pol_rosace->get_n_points(0), 120);
	float* rpts = pol_rosace->get_points(0);
	EXPECT_NEAR(rpts[0], 0.57735f, 1e-3f);
	EXPECT_NEAR(rpts[1], 0.0f, 1e-3f);

	// Lobes tangents au cercle unite : rayon maximal 1, atteint par un echantillon.
	float rmax = 0.f;
	for (int i = 0; i < pol_rosace->get_n_points (0); ++i)
		rmax = std::fmax (rmax, std::hypot (rpts[2*i], rpts[2*i+1]));
	EXPECT_NEAR(rmax, 1.f, 1e-5f);

	polygon2_tesselation_export (pol_rosace.get (), "architecture_rosace.obj");
}

TEST(TEST_cgarchi_surface_architecture, arc_brise)
{
	auto pArcBrise = std::make_unique<ArcBrise> ();
	pArcBrise->SetPrincipalArc (0., 660., 660.);
	pArcBrise->SetSecondArc (0., 200., 200.);
	std::unique_ptr<Polygon2> pol (pArcBrise->Generate ());
	ASSERT_NE(pol, nullptr);
	// Characterization: 3*npts points (npts=32), and the first arc point comes
	// from the arc's centre C -> (m_fWidth/2, m_fAltitude) = (330, 0).
	EXPECT_EQ(pol->get_n_points(0), 96);
	float* apts = pol->get_points(0);
	EXPECT_NEAR(apts[0], 330.f, 1e-2f);
	EXPECT_NEAR(apts[1], 0.f, 1e-2f);

	// L'arc principal suit point a point la formule historique.
	const std::vector<Vector2f> legacy = LegacyArcBrise (330.f, 660.f, 32);
	for (unsigned i = 0; i < 32; ++i)
	{
		EXPECT_NEAR(apts[2*i],   legacy[i][0], 1e-6f * std::fmax (1.f, std::fabs (legacy[i][0]))) << i;
		EXPECT_NEAR(apts[2*i+1], legacy[i][1], 1e-6f * std::fmax (1.f, std::fabs (legacy[i][1]))) << i;
	}
	// rosace centrale : second contour de 32 points
	ASSERT_EQ(pol->get_n_contours (), 2);
	EXPECT_EQ(pol->get_n_points (1), 32);
}

TEST(TEST_cgarchi_surface_architecture, arc_brise_refuses_a_second_arc_wider_than_half_the_first)
{
	ArcBrise arc;
	arc.SetPrincipalArc (0., 660., 660.);
	arc.SetSecondArc (0., 400., 400.);
	EXPECT_EQ(arc.Generate (), nullptr);
}

TEST(TEST_cgarchi_surface_architecture, arc_brise_refuses_an_arc_that_is_not_pointed)
{
	ArcBrise arc;
	arc.SetPrincipalArc (0., 660., 200.);   // fleche 200 < demi-ouverture 330
	arc.SetSecondArc (0., 200., 200.);
	EXPECT_THROW(arc.Generate (), std::invalid_argument);
}

// ===========================================================================
//  sampleArc* : contrat commun
// ===========================================================================

TEST(TEST_cgarchi_surface_architecture, arcs_with_no_sample_are_empty)
{
	for (const ArcSamples &s : {sampleArcBrise (1.f, 2.f, 0, 0.f), sampleArcAccolade (5.f, 3.f, .3f, 0),
	                            sampleArcAnseDePanier (1.f, .3f, 0), sampleArcRampant (1.f, .6f, 0)})
	{
		EXPECT_TRUE(s.points.empty ());
		EXPECT_TRUE(s.tangents.empty ());
	}
}

TEST(TEST_cgarchi_surface_architecture, arcs_with_one_sample_are_refused)
{
	EXPECT_THROW(sampleArcBrise (1.f, 2.f, 1, 0.f), std::invalid_argument);
	EXPECT_THROW(sampleArcAccolade (5.f, 3.f, .3f, 1), std::invalid_argument);
	EXPECT_THROW(sampleArcAnseDePanier (1.f, .3f, 1), std::invalid_argument);
	EXPECT_THROW(sampleArcRampant (1.f, .6f, 1), std::invalid_argument);
}

TEST(TEST_cgarchi_surface_architecture, arcs_give_exactly_npts_unit_tangents_along_the_indices)
{
	for (unsigned npts : {2u, 3u, 4u, 7u, 32u, 33u})
	{
		SCOPED_TRACE (npts);
		const ArcSamples arcs[] = {sampleArcBrise (330.f, 660.f, npts, 0.f), sampleArcAccolade (5.f, 3.f, .3f, npts),
		                           sampleArcAnseDePanier (2.f, .3f, npts), sampleArcRampant (1.f, .6f, npts)};
		for (const ArcSamples &s : arcs)
		{
			EXPECT_EQ(s.points.size (), npts);
			ExpectUnitTangentsAlongTheIndices (s);
		}
	}
}

// ===========================================================================
//  sampleArcBrise
// ===========================================================================

TEST(TEST_cgarchi_surface_architecture, arc_brise_samples_match_the_legacy_formula_for_even_npts)
{
	for (unsigned npts : {4u, 32u, 96u})
	{
		const ArcSamples s = sampleArcBrise (330.f, 660.f, npts, 0.f);
		const std::vector<Vector2f> legacy = LegacyArcBrise (330.f, 660.f, npts);
		for (unsigned i = 0; i < npts; ++i)
		{
			EXPECT_NEAR(s.points[i][0], legacy[i][0], 1e-6f * std::fmax (1.f, std::fabs (legacy[i][0]))) << npts << ' ' << i;
			EXPECT_NEAR(s.points[i][1], legacy[i][1], 1e-6f * std::fmax (1.f, std::fabs (legacy[i][1]))) << npts << ' ' << i;
		}
	}
}

TEST(TEST_cgarchi_surface_architecture, arc_brise_is_symmetric_with_its_apex_twice_for_even_npts)
{
	const unsigned npts = 32;
	const ArcSamples s = sampleArcBrise (100.f, 250.f, npts, 10.f);
	EXPECT_NEAR(s.points.front ()[0], 110.f, 1e-4f);
	EXPECT_NEAR(s.points.front ()[1], 0.f, 1e-4f);
	EXPECT_NEAR(s.points.back ()[0], -110.f, 1e-4f);
	for (unsigned i = 0; i < npts; ++i)
	{
		EXPECT_FLOAT_EQ(s.points[npts-1-i][0], -s.points[i][0]) << i;
		EXPECT_FLOAT_EQ(s.points[npts-1-i][1],  s.points[i][1]) << i;
	}
	for (unsigned i : {npts/2 - 1, npts/2})
	{
		EXPECT_NEAR(s.points[i][0], 0.f, 1e-3f) << i;
		EXPECT_NEAR(s.points[i][1], 260.f, 1e-3f) << i;
	}
}

TEST(TEST_cgarchi_surface_architecture, arc_brise_with_two_samples_gives_the_feet)
{
	const ArcSamples s = sampleArcBrise (1.f, 2.f, 2, 0.5f);
	ASSERT_EQ(s.points.size (), 2u);
	EXPECT_FLOAT_EQ(s.points[0][0],  1.5f);
	EXPECT_FLOAT_EQ(s.points[0][1],  0.f);
	EXPECT_FLOAT_EQ(s.points[1][0], -1.5f);
	EXPECT_FLOAT_EQ(s.points[1][1],  0.f);
}

TEST(TEST_cgarchi_surface_architecture, arc_brise_with_odd_npts_has_its_apex_in_the_middle)
{
	for (unsigned npts : {3u, 9u, 33u})
	{
		const ArcSamples s = sampleArcBrise (1.f, 2.f, npts, 0.f);
		const unsigned mid = npts/2;
		EXPECT_FLOAT_EQ(s.points[mid][0], 0.f) << npts;
		EXPECT_FLOAT_EQ(s.points[mid][1], 2.f) << npts;
		EXPECT_FLOAT_EQ(s.tangents[mid][0], -1.f) << npts;
		EXPECT_FLOAT_EQ(s.tangents[mid][1], 0.f) << npts;
		// le sommet n'est pas repete
		EXPECT_GT(s.points[mid-1][0], 0.f) << npts;
		EXPECT_LT(s.points[mid+1][0], 0.f) << npts;
	}
}

TEST(TEST_cgarchi_surface_architecture, arc_brise_refuses_degenerate_parameters)
{
	const float nan = std::numeric_limits<float>::quiet_NaN ();
	const float inf = std::numeric_limits<float>::infinity ();
	EXPECT_THROW(sampleArcBrise (0.f, 2.f, 8, 0.f), std::invalid_argument);    // a = 0
	EXPECT_THROW(sampleArcBrise (1.f, 0.f, 8, 0.f), std::invalid_argument);    // b = 0
	EXPECT_THROW(sampleArcBrise (1.f, 1.f, 8, 0.f), std::invalid_argument);    // b = a : plein cintre
	EXPECT_THROW(sampleArcBrise (2.f, 1.f, 8, 0.f), std::invalid_argument);    // b < a
	EXPECT_THROW(sampleArcBrise (1.f, 2.f, 8, -1.f), std::invalid_argument);   // a + offset = 0
	EXPECT_THROW(sampleArcBrise (nan, 2.f, 8, 0.f), std::invalid_argument);
	EXPECT_THROW(sampleArcBrise (1.f, inf, 8, 0.f), std::invalid_argument);
	EXPECT_THROW(sampleArcBrise (1.f, 2.f, 8, nan), std::invalid_argument);
}

// ===========================================================================
//  sampleArcAccolade, sampleArcRampant : parametres ignores (D12)
// ===========================================================================

// Defauts figes (debt_cgarchi.md) : construction a a = 5, b = 3, e = 0,3 quels
// que soient les parametres ; le premier arc ne part pas du pied A (5, 0) ; le
// second arc est parcouru du sommet B (0, 3) vers le point de raccord E (1,5 ; 2,1).
TEST(TEST_cgarchi_surface_architecture, arc_accolade_ignores_its_parameters_and_is_not_a_continuous_curve)
{
	const unsigned npts = 16;
	const ArcSamples a = sampleArcAccolade (5.f, 3.f, .3f, npts);
	const ArcSamples b = sampleArcAccolade (1.f, 7.f, .9f, npts);
	ASSERT_EQ(a.points.size (), b.points.size ());
	for (std::size_t i = 0; i < a.points.size (); ++i)
	{
		EXPECT_EQ(a.points[i][0], b.points[i][0]) << i;
		EXPECT_EQ(a.points[i][1], b.points[i][1]) << i;
	}
	EXPECT_LT(a.points.front ()[1], -0.1f);                 // sous la ligne des pieds
	EXPECT_NEAR(a.points[npts/2 - 1][0], 1.5f, 1e-4f);      // fin du premier arc : E
	EXPECT_NEAR(a.points[npts/2 - 1][1], 2.1f, 1e-4f);
	EXPECT_NEAR(a.points[npts/2][0], 0.f, 1e-4f);           // debut du second : B
	EXPECT_NEAR(a.points[npts/2][1], 3.f, 1e-4f);
	EXPECT_NEAR(a.points.back ()[0], 1.5f, 1e-4f);          // fin du second : E
	EXPECT_NEAR(a.points.back ()[1], 2.1f, 1e-4f);
}

// npts impair : les npts - 1 premiers echantillons sont ceux de npts - 1 (meme
// pas sur chaque arc), le dernier est l'extremite E du second arc (D12).
TEST(TEST_cgarchi_surface_architecture, arc_accolade_with_odd_npts_keeps_the_samples_of_npts_minus_one)
{
	for (unsigned npts : {5u, 9u, 33u})
	{
		SCOPED_TRACE (npts);
		const ArcSamples odd = sampleArcAccolade (5.f, 3.f, .3f, npts);
		const ArcSamples even = sampleArcAccolade (5.f, 3.f, .3f, npts - 1);
		ASSERT_EQ(odd.points.size (), npts);
		for (unsigned i = 0; i + 1 < npts; ++i)
		{
			EXPECT_EQ(odd.points[i][0], even.points[i][0]) << i;
			EXPECT_EQ(odd.points[i][1], even.points[i][1]) << i;
			EXPECT_EQ(odd.tangents[i][0], even.tangents[i][0]) << i;
			EXPECT_EQ(odd.tangents[i][1], even.tangents[i][1]) << i;
		}
		EXPECT_NEAR(odd.points[npts/2][0], 0.f, 1e-4f);         // debut du second arc : B
		EXPECT_NEAR(odd.points[npts/2][1], 3.f, 1e-4f);
		EXPECT_NEAR(odd.points[npts - 2][0], 1.5f, 1e-4f);      // fin du second arc : E
		EXPECT_NEAR(odd.points[npts - 2][1], 2.1f, 1e-4f);
		EXPECT_NEAR(odd.points.back ()[0], 1.5f, 1e-4f);        // E repete
		EXPECT_NEAR(odd.points.back ()[1], 2.1f, 1e-4f);
	}
}

TEST(TEST_cgarchi_surface_architecture, arc_rampant_ignores_its_parameters_and_stops_short_of_its_end)
{
	const ArcSamples a = sampleArcRampant (1.f, .6f, 16);
	const ArcSamples b = sampleArcRampant (3.f, .1f, 16);
	ASSERT_EQ(a.points.size (), b.points.size ());
	for (std::size_t i = 0; i < a.points.size (); ++i)
	{
		EXPECT_EQ(a.points[i][0], b.points[i][0]) << i;
		EXPECT_EQ(a.points[i][1], b.points[i][1]) << i;
	}
	// premier echantillon : le pied gauche (-1, 0)
	EXPECT_NEAR(a.points.front ()[0], -1.f, 1e-5f);
	EXPECT_NEAR(a.points.front ()[1], 0.f, 1e-5f);
	// l'extremite du second arc, (0,4 ; 0,6), n'est pas atteinte (debt_cgarchi.md)
	EXPECT_LT(a.points.back ()[0], 0.4f - 1e-3f);
	EXPECT_GT(a.points.back ()[1], 0.6f + 1e-3f);
}

// ===========================================================================
//  sampleArcAnseDePanier
// ===========================================================================

TEST(TEST_cgarchi_surface_architecture, anse_de_panier_with_e_zero_is_a_semicircle)
{
	const ArcSamples s = sampleArcAnseDePanier (2.f, 0.f, 17);
	for (std::size_t i = 0; i < s.points.size (); ++i)
		EXPECT_NEAR(s.points[i].getLength (), 2.f, 1e-5f) << i;
	EXPECT_NEAR(s.points.front ()[0], 2.f, 1e-5f);
	EXPECT_NEAR(s.points.back ()[0], -2.f, 1e-4f);
	EXPECT_NEAR(s.points[8][1], 2.f, 1e-5f);
}

TEST(TEST_cgarchi_surface_architecture, anse_de_panier_spans_its_opening_and_is_lowered)
{
	const ArcSamples s = sampleArcAnseDePanier (2.f, .3f, 31);
	EXPECT_NEAR(s.points.front ()[0], 2.f, 1e-5f);
	EXPECT_NEAR(s.points.front ()[1], 0.f, 1e-5f);
	EXPECT_NEAR(s.points.back ()[0], -2.f, 1e-4f);
	EXPECT_NEAR(s.points.back ()[1], 0.f, 1e-4f);
	// trois centres : la fleche est sous la demi-ouverture
	float top = 0.f;
	for (const Vector2f &p : s.points)
		top = std::fmax (top, p[1]);
	EXPECT_GT(top, 0.f);
	EXPECT_LT(top, 2.f);
}

// Chaque echantillon, d'angle alpha = pi*i/(npts-1), est sur le cercle de la
// portion contenant alpha : pied droit (centre (a*e, 0), rayon a(1-e)) pour
// alpha < pi/3, pied gauche (centre (-a*e, 0)) pour alpha > 2pi/3, centrale
// (centre (0, -a*e*tan(pi/3)), rayon a(1-e) + 2*a*e) entre les deux. Aux
// raccords pi/3 et 2pi/3 les deux cercles voisins se touchent : l'un ou l'autre
// convient. Les npts retenus placent un echantillon hors de sa portion d'indice
// (i < npts/3, i > 2*npts/3) : npts = 2 (indice 1), 5 (indice 3), 32 (indice 21),
// 33 (indice 22).
TEST(TEST_cgarchi_surface_architecture, anse_de_panier_samples_lie_on_the_circle_of_their_angle)
{
	const double a = 2., e = .3;
	const double pi = std::acos (-1.);
	const double r = a*(1. - e);
	const double rc = r + 2.*a*e;
	for (unsigned npts : {2u, 5u, 32u, 33u})
	{
		SCOPED_TRACE (npts);
		const ArcSamples s = sampleArcAnseDePanier (static_cast<float> (a), static_cast<float> (e), npts);
		ASSERT_EQ(s.points.size (), npts);
		for (unsigned i = 0; i < npts; ++i)
		{
			const double alpha = pi * i / (npts - 1);
			const bool lateral = alpha < pi/3. || alpha > 2.*pi/3.;
			const double cx = !lateral ? 0. : (alpha < pi/3. ? a*e : -a*e);
			const double cy = lateral ? 0. : -a*e*std::tan (pi/3.);
			const double radius = lateral ? r : rc;
			EXPECT_NEAR(s.points[i][0], cx + radius*std::cos (alpha), 1e-4) << i;
			EXPECT_NEAR(s.points[i][1], cy + radius*std::sin (alpha), 1e-4) << i;
		}
		EXPECT_NEAR(s.points.back ()[0], -a, 1e-4);
		EXPECT_NEAR(s.points.back ()[1], 0., 1e-4);
	}
}

TEST(TEST_cgarchi_surface_architecture, anse_de_panier_refuses_degenerate_parameters)
{
	const float nan = std::numeric_limits<float>::quiet_NaN ();
	EXPECT_THROW(sampleArcAnseDePanier (0.f, .3f, 8), std::invalid_argument);
	EXPECT_THROW(sampleArcAnseDePanier (-1.f, .3f, 8), std::invalid_argument);
	EXPECT_THROW(sampleArcAnseDePanier (1.f, 1.f, 8), std::invalid_argument);
	EXPECT_THROW(sampleArcAnseDePanier (1.f, -.1f, 8), std::invalid_argument);
	EXPECT_THROW(sampleArcAnseDePanier (nan, .3f, 8), std::invalid_argument);
	EXPECT_THROW(sampleArcAnseDePanier (1.f, nan, 8), std::invalid_argument);
}

// ===========================================================================
//  extrudeMoulureAlongCurve
// ===========================================================================

TEST(TEST_cgarchi_surface_architecture, moulure_sweep_has_n_m_vertices_and_2_m_n_minus_1_triangles)
{
	// npts impair : sommet unique. Un npts pair repete le sommet avec deux
	// tangentes opposees en y, et les deux sections s'y replient l'une sur l'autre.
	const ArcSamples curve = sampleArcBrise (330.f, 660.f, 33, 0.f);
	const Profile2D moulure = rollBarProfile (10., 12);
	Mesh out;
	extrudeMoulureAlongCurve (curve, moulure, out);
	const std::size_t n = curve.points.size ();
	const std::size_t m = moulure.points.size ();
	EXPECT_EQ(out.GetNVertices (), n * m);
	EXPECT_EQ(out.GetNFaces (), 2 * m * (n - 1));

	// tube ouvert sans capots : seules les deux sections d'extremite sont des bords
	MeshValidationOptions options;
	options.checkSelfIntersection = false;
	const MeshValidationReport r = ValidateMesh (out, options);
	EXPECT_EQ(r.Count (MeshDefect::InvalidIndex), 0u);
	EXPECT_EQ(r.Count (MeshDefect::NonFiniteVertex), 0u);
	EXPECT_EQ(r.Count (MeshDefect::InconsistentOrientation), 0u);
	EXPECT_EQ(r.Count (MeshDefect::NonManifoldEdge), 0u);
	EXPECT_EQ(r.Count (MeshDefect::BoundaryEdge), 2 * m);
	EXPECT_EQ(r.components, 1u);
}

TEST(TEST_cgarchi_surface_architecture, moulure_section_is_placed_in_the_curve_frame)
{
	// courbe verticale x = 0 dans le plan (x, 0, y) : T = (0, 0, 1), N = (-1, 0, 0), B = (0, -1, 0)
	const ArcSamples curve = StraightCurve (2);
	Mesh out;
	extrudeMoulureAlongCurve (curve, SquareMoulure (), out);
	ASSERT_EQ(out.GetNVertices (), 8u);
	const float expected[8][3] = {
		{ 0.f,  0.f, 0.f}, { 0.f, -1.f, 0.f}, {-1.f, -1.f, 0.f}, {-1.f,  0.f, 0.f},
		{ 0.f,  0.f, 1.f}, { 0.f, -1.f, 1.f}, {-1.f, -1.f, 1.f}, {-1.f,  0.f, 1.f}};
	for (unsigned v = 0; v < 8; ++v)
	{
		float p[3];
		out.GetVertex (v, p);
		for (int k = 0; k < 3; ++k)
			EXPECT_NEAR(p[k], expected[v][k], 1e-6f) << v << ' ' << k;
	}
}

TEST(TEST_cgarchi_surface_architecture, moulure_sweep_refuses_bad_input_and_leaves_out_unchanged)
{
	const float nan = std::numeric_limits<float>::quiet_NaN ();
	Mesh out;
	extrudeMoulureAlongCurve (StraightCurve (3), SquareMoulure (), out);
	const unsigned nv = out.GetNVertices ();
	const unsigned nf = out.GetNFaces ();
	// Toute ecriture de geometrie incremente la revision (mesh.h) : elle voit
	// aussi les refus dont la courbe a le meme n*m que le maillage initial.
	const uint64_t revision = out.GetRevision ();
	std::vector<float> coordinates (3 * nv);
	for (unsigned v = 0; v < nv; ++v)
		out.GetVertex (v, &coordinates[3 * v]);

	Profile2D twoPoints;
	twoPoints.points = {Vector2d (0., 0.), Vector2d (1., 0.)};
	Profile2D nanMoulure = SquareMoulure ();
	nanMoulure.points[2] = Vector2d (std::numeric_limits<double>::quiet_NaN (), 0.);
	ArcSamples mismatch = StraightCurve (3);
	mismatch.tangents.pop_back ();
	ArcSamples nullTangent = StraightCurve (3);
	nullTangent.tangents[1] = Vector2f (0.f, 0.f);
	ArcSamples nanPoint = StraightCurve (3);
	nanPoint.points[1] = Vector2f (nan, 0.f);

	EXPECT_THROW(extrudeMoulureAlongCurve (StraightCurve (3), Profile2D (), out), std::invalid_argument);
	EXPECT_THROW(extrudeMoulureAlongCurve (StraightCurve (3), twoPoints, out), std::invalid_argument);
	EXPECT_THROW(extrudeMoulureAlongCurve (StraightCurve (3), nanMoulure, out), std::invalid_argument);
	EXPECT_THROW(extrudeMoulureAlongCurve (StraightCurve (1), SquareMoulure (), out), std::invalid_argument);
	EXPECT_THROW(extrudeMoulureAlongCurve (ArcSamples (), SquareMoulure (), out), std::invalid_argument);
	EXPECT_THROW(extrudeMoulureAlongCurve (mismatch, SquareMoulure (), out), std::invalid_argument);
	EXPECT_THROW(extrudeMoulureAlongCurve (nullTangent, SquareMoulure (), out), std::invalid_argument);
	EXPECT_THROW(extrudeMoulureAlongCurve (nanPoint, SquareMoulure (), out), std::invalid_argument);

	EXPECT_EQ(out.GetRevision (), revision);
	ASSERT_EQ(out.GetNVertices (), nv);
	EXPECT_EQ(out.GetNFaces (), nf);
	for (unsigned v = 0; v < nv; ++v)
	{
		float p[3];
		out.GetVertex (v, p);
		for (int k = 0; k < 3; ++k)
			EXPECT_EQ(p[k], coordinates[3 * v + k]) << v << ' ' << k;
	}
}

// Meme n*m que le maillage precedent : SetVertices seul garderait les
// attributs par sommet de l'ancienne geometrie.
TEST(TEST_cgarchi_surface_architecture, moulure_sweep_replaces_every_attribute_of_out)
{
	Mesh out;
	extrudeMoulureAlongCurve (StraightCurve (3), SquareMoulure (), out);
	out.ComputeNormals ();
	out.InitVertexColors (1.f, 0.f, 0.f);
	ASSERT_FALSE(out.GetVertexNormals ().empty ());
	ASSERT_FALSE(out.GetVertexColors ().empty ());
	const uint64_t revision = out.GetRevision ();

	ArcSamples shifted = StraightCurve (3);
	for (Vector2f &p : shifted.points)
		p[0] += 5.f;
	extrudeMoulureAlongCurve (shifted, SquareMoulure (), out);

	ASSERT_EQ(out.GetNVertices (), 12u);
	EXPECT_EQ(out.GetNFaces (), 16u);
	EXPECT_TRUE(out.GetVertexNormals ().empty ());
	EXPECT_TRUE(out.GetVertexColors ().empty ());
	EXPECT_GT(out.GetRevision (), revision);   // cle des caches : jamais en arriere
	float p[3];
	out.GetVertex (0, p);
	EXPECT_FLOAT_EQ(p[0], 5.f);
	EXPECT_FLOAT_EQ(p[1], 0.f);
	EXPECT_FLOAT_EQ(p[2], 0.f);
}
