#include <gtest/gtest.h>

#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cgarchi/classical_krepis.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"

// ===========================================================================
//  Krepis : degres places puis mailles
// ===========================================================================
//
// Reglages d'essai : SyntheticKrepisSettings, valeurs synthetiques hors des
// points fixes (non exactes en binaire), qui ne decrivent aucun monument. Les
// debords different en X et en Y, et d'un degre a l'autre, pour qu'une
// permutation se voie ; la variante "debords egaux" montre ce qu'un jeu
// symetrique laisserait passer.
//
// Variantes aux bornes d'interface : 63 degres, toutes les longueurs a
// kKrepisMaxLength ("bornes hautes") ou a FLT_MIN ("bornes basses" : FLT_MIN
// n'est absorbe a aucune cote, ses multiples jusqu'a 64 sont exacts en float).
//
// Tolerances : egalite au bit pres la ou le contrat la promet. Placement
// (double) : 1e-12 fois la plus grande grandeur cumulee (stylobateTop pour les
// cotes, demi-cotes du degre le plus bas pour les debords). Une hauteur ou un
// debord se lit comme la difference de deux sommes cumulees, dont l'erreur
// d'arrondi croit avec la somme et non avec la difference. Le maillage est en float : ses cotes se
// comparent exactement aux cotes arrondies en float, et le volume d'un degre a
// celui de sa boite arrondie en float, a 1e-9 pres en relatif.

using namespace cgarchi::classical;

namespace {

constexpr int kVerticesPerStep = 16;
constexpr int kFacesPerStep = 12;

KrepisSettings SyntheticKrepisSettings ()
{
	KrepisSettings s;
	s.stylobateHalfWidth = 3.7;
	s.stylobateHalfDepth = 4.3;
	s.stylobateHeight = 0.37;
	s.lowerSteps = {{0.29, 0.43, 0.23}, {0.31, 0.41, 0.19}};
	return s;
}

std::vector<std::pair<std::string, KrepisSettings>> Variants ()
{
	std::vector<std::pair<std::string, KrepisSettings>> out;
	out.emplace_back ("synthetique", SyntheticKrepisSettings ());

	KrepisSettings alone = SyntheticKrepisSettings ();
	alone.lowerSteps.clear ();
	out.emplace_back ("stylobate seul", alone);

	KrepisSettings bound = SyntheticKrepisSettings ();
	bound.lowerSteps.clear ();
	for (int k = 0; k < kKrepisMaxLowerSteps; ++k)
		bound.lowerSteps.push_back ({0.17 + 0.003 * k, 0.043 + 0.001 * k, 0.029 + 0.0007 * k});
	out.emplace_back ("63 degres", bound);

	KrepisSettings equal = SyntheticKrepisSettings ();
	for (KrepisStep &step : equal.lowerSteps)
		step.outsetY = step.outsetX;
	out.emplace_back ("debords egaux", equal);

	for (const auto &[label, length] : {std::pair<const char *, double> {"bornes hautes", kKrepisMaxLength},
	                                    std::pair<const char *, double> {"bornes basses", static_cast<double> (FLT_MIN)}})
	{
		KrepisSettings s;
		s.stylobateHalfWidth = s.stylobateHalfDepth = s.stylobateHeight = length;
		s.lowerSteps.assign (kKrepisMaxLowerSteps, KrepisStep {length, length, length});
		out.emplace_back (label, s);
	}
	return out;
}

std::unique_ptr<Mesh> Build (const KrepisSettings &s, KrepisLayout *layout)
{
	std::unique_ptr<Mesh> m = BuildKrepisMesh (s, layout);
	EXPECT_NE (m, nullptr);
	return m;
}

float VertexZ (const Mesh &m, unsigned v)
{
	float p[3];
	m.GetVertex (v, p);
	return p[2];
}

// Tranche [first, first + count) des faces, avec leurs sommets, recopiee dans
// un Mesh independant.
Mesh Slice (const Mesh &m, unsigned firstVertex, unsigned vertexCount, unsigned firstFace, unsigned faceCount)
{
	std::vector<float> vertices;
	for (unsigned v = firstVertex; v < firstVertex + vertexCount; ++v)
	{
		float p[3];
		m.GetVertex (v, p);
		vertices.insert (vertices.end (), p, p + 3);
	}
	std::vector<unsigned> faces;
	for (unsigned f = firstFace; f < firstFace + faceCount; ++f)
		for (unsigned k = 0; k < 3; ++k)
			faces.push_back (static_cast<unsigned> (m.GetFaceVertex (f, k)) - firstVertex);
	Mesh out;
	out.SetVertices (vertexCount, vertices.data ());
	out.SetFaces (faceCount, 3, faces.data ());
	return out;
}

} // namespace

// ---------------------------------------------------------------------------
//  Placement
// ---------------------------------------------------------------------------

TEST (TEST_cgarchi_classical_krepis, the_steps_stack_from_the_ground_to_the_top_of_the_stylobate)
{
	for (const auto &[name, s] : Variants ())
	{
		SCOPED_TRACE (name);
		const KrepisLayout layout = LayoutKrepis (s);
		const std::size_t n = s.lowerSteps.size ();
		ASSERT_EQ (layout.steps.size (), n + 1);
		const double tol = 1e-12 * layout.stylobateTop;

		EXPECT_EQ (layout.steps[n].zBottom, 0.0);
		for (std::size_t k = 0; k < n; ++k)
			EXPECT_EQ (layout.steps[k].zBottom, layout.steps[k + 1].zTop) << k;
		EXPECT_EQ (layout.stylobateTop, layout.steps[0].zTop);

		double sum = s.stylobateHeight;
		EXPECT_NEAR (layout.steps[0].zTop - layout.steps[0].zBottom, s.stylobateHeight, tol);
		for (std::size_t k = 1; k <= n; ++k)
		{
			EXPECT_NEAR (layout.steps[k].zTop - layout.steps[k].zBottom, s.lowerSteps[k - 1].height, tol) << k;
			sum += s.lowerSteps[k - 1].height;
		}
		EXPECT_NEAR (sum, layout.stylobateTop, tol);
	}
}

TEST (TEST_cgarchi_classical_krepis, the_stylobate_is_the_given_rectangle_and_each_lower_step_projects_by_its_outsets)
{
	for (const auto &[name, s] : Variants ())
	{
		SCOPED_TRACE (name);
		const KrepisLayout layout = LayoutKrepis (s);
		const std::size_t n = s.lowerSteps.size ();
		ASSERT_EQ (layout.steps.size (), n + 1);

		EXPECT_EQ (layout.steps[0].halfWidth, s.stylobateHalfWidth);
		EXPECT_EQ (layout.steps[0].halfDepth, s.stylobateHalfDepth);
		const double tol = 1e-12 * (layout.steps[n].halfWidth + layout.steps[n].halfDepth);
		for (std::size_t k = 1; k <= n; ++k)
		{
			EXPECT_NEAR (layout.steps[k].halfWidth - layout.steps[k - 1].halfWidth, s.lowerSteps[k - 1].outsetX, tol) << k;
			EXPECT_NEAR (layout.steps[k].halfDepth - layout.steps[k - 1].halfDepth, s.lowerSteps[k - 1].outsetY, tol) << k;
		}
	}
}

// ---------------------------------------------------------------------------
//  Maillage
// ---------------------------------------------------------------------------

TEST (TEST_cgarchi_classical_krepis, the_mesh_spans_the_lowest_step_and_reaches_the_top_of_the_stylobate)
{
	for (const auto &[name, s] : Variants ())
	{
		SCOPED_TRACE (name);
		KrepisLayout layout;
		const std::unique_ptr<Mesh> m = Build (s, &layout);
		ASSERT_EQ (layout.steps.size (), s.lowerSteps.size () + 1);
		const KrepisBox &lowest = layout.steps.back ();

		MeshValidationOptions options;
		options.checkSelfIntersection = false;
		const MeshValidationReport r = ValidateMesh (*m, options);
		EXPECT_EQ (r.bbMax[0], static_cast<double> (static_cast<float> (lowest.halfWidth)));
		EXPECT_EQ (r.bbMin[0], -static_cast<double> (static_cast<float> (lowest.halfWidth)));
		EXPECT_EQ (r.bbMax[1], static_cast<double> (static_cast<float> (lowest.halfDepth)));
		EXPECT_EQ (r.bbMin[1], -static_cast<double> (static_cast<float> (lowest.halfDepth)));
		EXPECT_EQ (r.bbMin[2], 0.0);
		EXPECT_EQ (r.bbMax[2], static_cast<double> (static_cast<float> (layout.stylobateTop)));
	}
}

TEST (TEST_cgarchi_classical_krepis, each_step_is_a_closed_box_of_its_own_volume)
{
	for (const auto &[name, s] : Variants ())
	{
		SCOPED_TRACE (name);
		KrepisLayout layout;
		const std::unique_ptr<Mesh> m = Build (s, &layout);
		const unsigned count = static_cast<unsigned> (layout.steps.size ());
		ASSERT_EQ (count, s.lowerSteps.size () + 1);
		ASSERT_EQ (static_cast<unsigned> (m->GetNVertices ()), kVerticesPerStep * count);
		ASSERT_EQ (static_cast<unsigned> (m->GetNFaces ()), kFacesPerStep * count);

		for (unsigned k = 0; k < count; ++k)
		{
			SCOPED_TRACE ("degre " + std::to_string (k));
			const Mesh slice = Slice (*m, kVerticesPerStep * k, kVerticesPerStep, kFacesPerStep * k, kFacesPerStep);
			const MeshValidationReport r = ValidateMesh (slice);
			EXPECT_TRUE (r.IsValid ());
			EXPECT_EQ (r.components, 1u);

			// Tranche k = boite k du placement : l'ordre du maillage est celui de layout.steps.
			const KrepisBox &box = layout.steps[k];
			EXPECT_EQ (r.bbMax[0], static_cast<double> (static_cast<float> (box.halfWidth)));
			EXPECT_EQ (r.bbMin[0], -static_cast<double> (static_cast<float> (box.halfWidth)));
			EXPECT_EQ (r.bbMax[1], static_cast<double> (static_cast<float> (box.halfDepth)));
			EXPECT_EQ (r.bbMin[1], -static_cast<double> (static_cast<float> (box.halfDepth)));
			EXPECT_EQ (r.bbMin[2], static_cast<double> (static_cast<float> (box.zBottom)));
			EXPECT_EQ (r.bbMax[2], static_cast<double> (static_cast<float> (box.zTop)));

			// Volume de la boite arrondie en float, celle que le maillage represente :
			// a la cote z, l'arrondi de la hauteur atteint ulp(z), soit plus de 1e-6 en
			// relatif pour un degre mince haut place.
			const double hw = static_cast<float> (box.halfWidth);
			const double hd = static_cast<float> (box.halfDepth);
			const double h = static_cast<double> (static_cast<float> (box.zTop)) - static_cast<float> (box.zBottom);
			const double volume = 4.0 * hw * hd * h;
			EXPECT_NEAR (r.signedVolume, volume, 1e-9 * volume);
		}
	}
}

TEST (TEST_cgarchi_classical_krepis, the_only_overlaps_of_the_krepis_are_its_step_contacts)
{
	for (const auto &[name, s] : Variants ())
	{
		SCOPED_TRACE (name);
		KrepisLayout layout;
		const std::unique_ptr<Mesh> m = Build (s, &layout);
		const std::size_t count = layout.steps.size ();
		ASSERT_EQ (count, s.lowerSteps.size () + 1);

		MeshValidationOptions options;
		options.maxRecordsPerKind = 100000;
		const MeshValidationReport r = ValidateMesh (*m, options);
		EXPECT_EQ (r.Count (MeshDefect::BoundaryEdge), 0u);
		EXPECT_EQ (r.Count (MeshDefect::NonManifoldEdge), 0u);
		EXPECT_EQ (r.Count (MeshDefect::NonManifoldVertex), 0u);
		EXPECT_EQ (r.Count (MeshDefect::InconsistentOrientation), 0u);
		EXPECT_EQ (r.Count (MeshDefect::InvertedShell), 0u);
		EXPECT_EQ (r.Count (MeshDefect::DegenerateFace), 0u);
		EXPECT_EQ (r.Count (MeshDefect::DuplicateFace), 0u);
		EXPECT_EQ (r.components, count);
		EXPECT_TRUE (r.selfIntersectionChecked);
		EXPECT_FALSE (r.selfIntersectionTruncated);

		// Chaque recouvrement reunit une face du degre k (k >= 1) et une face du
		// degre k - 1 pose dessus, toutes deux a la cote du contact.
		std::vector<std::size_t> perContact (count, 0);
		std::size_t records = 0;
		for (const MeshDefectRecord &rec : r.records)
		{
			if (rec.kind != MeshDefect::SelfIntersection)
				continue;
			++records;
			EXPECT_EQ (rec.faces.size (), 2u);
			if (rec.faces.size () != 2u)
				continue;
			const std::size_t a = rec.faces[0] / kFacesPerStep;
			const std::size_t b = rec.faces[1] / kFacesPerStep;
			const std::size_t lower = a > b ? a : b;
			const std::size_t upper = a > b ? b : a;
			EXPECT_EQ (lower, upper + 1) << rec.faces[0] << " " << rec.faces[1];
			if (lower != upper + 1 || lower >= count)
				continue;
			++perContact[lower];
			const float zContact = static_cast<float> (layout.steps[lower].zTop);
			for (unsigned f : rec.faces)
				for (unsigned k = 0; k < 3; ++k)
					EXPECT_EQ (VertexZ (*m, static_cast<unsigned> (m->GetFaceVertex (f, k))), zContact) << f;
		}
		EXPECT_EQ (records, r.Count (MeshDefect::SelfIntersection));
		RecordProperty ("self_intersections_" + std::to_string (count - 1) + "_lower_steps", static_cast<int> (records));
		for (std::size_t k = 1; k < count; ++k)
			EXPECT_GT (perContact[k], 0u) << "contact " << k;
		if (count == 1)
			EXPECT_EQ (r.Count (MeshDefect::SelfIntersection), 0u);
	}
}

// ---------------------------------------------------------------------------
//  Domaines
// ---------------------------------------------------------------------------

TEST (TEST_cgarchi_classical_krepis, invalid_settings_are_refused_naming_the_field)
{
	using Edit = std::function<void (KrepisSettings &)>;
	struct Case { std::string field; Edit edit; };
	std::vector<Case> cases;

	struct Field { std::string name; std::function<double &(KrepisSettings &)> get; };
	const std::vector<Field> fields = {
		{"stylobateHalfWidth", [] (KrepisSettings &s) -> double & { return s.stylobateHalfWidth; }},
		{"stylobateHalfDepth", [] (KrepisSettings &s) -> double & { return s.stylobateHalfDepth; }},
		{"stylobateHeight", [] (KrepisSettings &s) -> double & { return s.stylobateHeight; }},
		{"lowerSteps[0].height", [] (KrepisSettings &s) -> double & { return s.lowerSteps[0].height; }},
		{"lowerSteps[0].outsetX", [] (KrepisSettings &s) -> double & { return s.lowerSteps[0].outsetX; }},
		{"lowerSteps[0].outsetY", [] (KrepisSettings &s) -> double & { return s.lowerSteps[0].outsetY; }},
		{"lowerSteps[1].height", [] (KrepisSettings &s) -> double & { return s.lowerSteps[1].height; }},
		{"lowerSteps[1].outsetX", [] (KrepisSettings &s) -> double & { return s.lowerSteps[1].outsetX; }},
		{"lowerSteps[1].outsetY", [] (KrepisSettings &s) -> double & { return s.lowerSteps[1].outsetY; }},
	};
	const double inf = std::numeric_limits<double>::infinity ();
	// 1e-39 est sous-normal en float (< FLT_MIN) ; 1e31 depasse kKrepisMaxLength.
	const std::vector<double> refused = {0.0, -1.0, std::numeric_limits<double>::quiet_NaN (), inf, -inf, 1e-39, 1e31};
	for (const Field &f : fields)
		for (double v : refused)
			cases.push_back ({f.name, [f, v] (KrepisSettings &s) { f.get (s) = v; }});

	cases.push_back ({"lowerSteps", [] (KrepisSettings &s) {
		                  s.lowerSteps.assign (kKrepisMaxLowerSteps + 1, KrepisStep {0.29, 0.43, 0.23});
	                  }});
	// Hauteur absorbee a la cote 1e9 (pas du float : 64).
	cases.push_back ({"lowerSteps[0].height", [] (KrepisSettings &s) { s.lowerSteps[1].height = 1e9; }});
	cases.push_back ({"stylobateHeight", [] (KrepisSettings &s) { s.lowerSteps[0].height = 1e9; }});
	// Debord absorbe au demi-cote 1e9.
	cases.push_back ({"lowerSteps[0].outsetX", [] (KrepisSettings &s) { s.stylobateHalfWidth = 1e9; }});
	cases.push_back ({"lowerSteps[0].outsetY", [] (KrepisSettings &s) { s.stylobateHalfDepth = 1e9; }});
	// Le premier debord (128) passe, le second est absorbe au demi-cote 1e9.
	cases.push_back ({"lowerSteps[1].outsetX", [] (KrepisSettings &s) {
		                  s.stylobateHalfWidth = 1e9 - 100.0;
		                  s.lowerSteps[0].outsetX = 128.0;
		                  s.lowerSteps[1].outsetX = 1e-3;
	                  }});

	for (const Case &c : cases)
	{
		KrepisSettings s = SyntheticKrepisSettings ();
		c.edit (s);
		const std::string quoted = "'" + c.field + "'";
		auto expectRefused = [&] (const char *entry, const std::function<void ()> &call) {
			try
			{
				call ();
				ADD_FAILURE () << c.field << " : accepte par " << entry;
				return false;
			}
			catch (const std::invalid_argument &e)
			{
				EXPECT_NE (std::string (e.what ()).find (quoted), std::string::npos) << entry << " : " << e.what ();
				return true;
			}
		};
		// Des reglages que la validation laisse passer ne vont pas au maillage :
		// extrudeToMesh n'est pas sur pour une coordonnee non finie.
		if (!expectRefused ("ValidateKrepisSettings", [&] { ValidateKrepisSettings (s); }))
			continue;
		expectRefused ("LayoutKrepis", [&] { LayoutKrepis (s); });

		KrepisLayout layout;
		layout.stylobateTop = -1.0;
		expectRefused ("BuildKrepisMesh", [&] { BuildKrepisMesh (s, &layout); });
		EXPECT_EQ (layout.stylobateTop, -1.0) << c.field << " : placement ecrit avant le refus";
		EXPECT_TRUE (layout.steps.empty ()) << c.field << " : placement ecrit avant le refus";
	}
}

// ---------------------------------------------------------------------------
//  Determinisme
// ---------------------------------------------------------------------------

TEST (TEST_cgarchi_classical_krepis, two_identical_builds_are_equal_bit_for_bit)
{
	for (const auto &[name, s] : Variants ())
	{
		SCOPED_TRACE (name);
		KrepisLayout la, lb;
		const std::unique_ptr<Mesh> a = Build (s, &la);
		const std::unique_ptr<Mesh> b = Build (s, &lb);
		ASSERT_EQ (la.steps.size (), lb.steps.size ());
		EXPECT_EQ (std::memcmp (la.steps.data (), lb.steps.data (), la.steps.size () * sizeof (KrepisBox)), 0);
		EXPECT_EQ (std::memcmp (&la.stylobateTop, &lb.stylobateTop, sizeof (double)), 0);
		ASSERT_EQ (a->GetNVertices (), b->GetNVertices ());
		ASSERT_EQ (a->GetNFaces (), b->GetNFaces ());
		for (unsigned v = 0; v < static_cast<unsigned> (a->GetNVertices ()); ++v)
		{
			float va[3], vb[3];
			a->GetVertex (v, va);
			b->GetVertex (v, vb);
			ASSERT_EQ (std::memcmp (va, vb, sizeof va), 0) << v;
		}
		for (unsigned f = 0; f < static_cast<unsigned> (a->GetNFaces ()); ++f)
			for (unsigned k = 0; k < 3; ++k)
				ASSERT_EQ (a->GetFaceVertex (f, k), b->GetFaceVertex (f, k)) << f;
	}
}
