#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_profile.h>
#include <cgarchi/parameterized_classical.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "../src/cgmesh/parameterized.h"

using namespace cgarchi;   // fichier de test de cgarchi seul

// ===========================================================================
//  Colonne d'un ordre de Vignole : base, fut, chapiteau
// ===========================================================================
//
// La colonne seule (sans piedestal ni entablement) est construite par
// BuildClassicalColumnMesh a un module non unitaire, pour que l'echelle soit
// verifiee avec la geometrie. Les hauteurs de reference sont celles de la table
// generale de l'ordre (classical_orders.h), en modules.

namespace
{
	constexpr int   kOrderCount = 5;
	constexpr float kModule     = 2.5f;

	const classical::OrderData &OrderOf (int o)
	{
		return classical::ClassicalOrders ().Get (static_cast<classical::Order> (o));
	}
}

TEST (TEST_cgarchi_classical_column, each_order_builds_base_shaft_capital_stacked_to_the_column_height)
{
	for (int o = 0; o < kOrderCount; ++o)
	{
		const classical::OrderData &order = OrderOf (o);
		const double P = order.partsPerModule;
		SCOPED_TRACE (order.key);

		ClassicalColumnSettings s;
		s.order = o;
		s.module = kModule;
		classical::ColumnProfile profile;
		const std::unique_ptr<Mesh> m = BuildClassicalColumnMesh (s, &profile);
		ASSERT_NE (m, nullptr);
		ASSERT_GT (m->GetNVertices (), 0u);
		ASSERT_GT (m->GetNFaces (), 0u);

		// hauteur totale = base + fut + chapiteau, en modules
		EXPECT_NEAR (order.general.base_M + order.general.shaft_M + order.general.capital_M,
		             order.general.column_M, 1e-9);

		const MeshValidationReport r = ValidateMesh (*m);
		EXPECT_EQ (r.Count (MeshDefect::NonFiniteVertex), 0u);
		EXPECT_NEAR (r.bbMin[2], 0.0, 1e-5);
		EXPECT_NEAR (r.bbMax[2] - r.bbMin[2], order.general.column_M * kModule, 1e-4 * kModule);

		// emprise : au moins le diametre inferieur du fut, centree sur l'axe
		EXPECT_GE (r.bbMax[0] - r.bbMin[0], order.general.lowerDiameter_p / P * kModule - 1e-4);
		EXPECT_GE (r.bbMax[1] - r.bbMin[1], order.general.lowerDiameter_p / P * kModule - 1e-4);
		EXPECT_NEAR (r.bbMax[0] + r.bbMin[0], 0.0, 1e-4 * kModule);
		EXPECT_NEAR (r.bbMax[1] + r.bbMin[1], 0.0, 1e-4 * kModule);

		// profil en parties de module, de la meme hauteur que la colonne
		EXPECT_NEAR (profile.height, order.general.column_M * P, 1e-6 * P);

		// empilement sans chevauchement : membres de bas en haut, contigus ou disjoints
		ASSERT_FALSE (profile.members.empty ());
		for (std::size_t k = 0; k < profile.members.size (); ++k)
		{
			EXPECT_LE (profile.members[k].y0, profile.members[k].y1) << k;
			if (k > 0)
				EXPECT_GE (profile.members[k].y0, profile.members[k - 1].y1 - 1e-9) << k;
		}

		// les trois parties, dans l'ordre base < fut < chapiteau
		EXPECT_LT (profile.shaft.foot, profile.shaft.top);
		bool hasBase = false, hasCapital = false;
		for (const classical::ProfileMember &member : profile.members)
		{
			if (member.block == "base")
			{
				hasBase = true;
				EXPECT_LE (member.y1, profile.shaft.foot + 1e-9) << member.name;
			}
			else if (member.block == "chapiteau")
			{
				hasCapital = true;
				EXPECT_GE (member.y0, profile.shaft.top - 1e-9) << member.name;
			}
		}
		EXPECT_TRUE (hasBase);
		EXPECT_TRUE (hasCapital);
		EXPECT_NEAR (profile.members.front ().y0, 0.0, 1e-9);
		EXPECT_NEAR (profile.members.back ().y1, profile.height, 1e-6 * P);
	}
}

TEST (TEST_cgarchi_classical_column, the_column_mesh_scales_with_the_module)
{
	for (int o = 0; o < kOrderCount; ++o)
	{
		SCOPED_TRACE (OrderOf (o).key);
		ClassicalColumnSettings unit;
		unit.order = o;
		ClassicalColumnSettings scaled = unit;
		scaled.module = kModule;

		const std::unique_ptr<Mesh> a = BuildClassicalColumnMesh (unit);
		const std::unique_ptr<Mesh> b = BuildClassicalColumnMesh (scaled);
		ASSERT_EQ (a->GetNVertices (), b->GetNVertices ());
		ASSERT_EQ (a->GetNFaces (), b->GetNFaces ());

		double worst = 0.0;
		for (unsigned v = 0; v < a->GetNVertices (); ++v)
		{
			float pa[3], pb[3];
			a->GetVertex (v, pa);
			b->GetVertex (v, pb);
			for (int k = 0; k < 3; ++k)
				worst = std::fmax (worst, std::fabs (static_cast<double> (pa[k]) * kModule - pb[k]));
		}
		EXPECT_LT (worst, 1e-4 * kModule);
	}
}

// Reference de non-regression : colonne seule, module 1, sans volutes (une
// coque), resolution par defaut. Comptes exacts, volume a 1e-6 pres en relatif.
// References MSVC Release.
TEST (TEST_cgarchi_classical_column, reference_columns_keep_their_counts_and_volume)
{
	struct Reference { unsigned vertices, faces; double volume; };
	const Reference reference[kOrderCount] = {
		{3358,  6712, 44.244948832821507},   // toscan
		{4034,  8064, 51.295813704420709},   // dorique
		{6434, 12864, 60.919843383000597},   // ionique
		{6826, 13648, 65.726641230866946},   // corinthien
		{7162, 14320, 65.916156135073592},   // composite
	};

	for (int o = 0; o < kOrderCount; ++o)
	{
		SCOPED_TRACE (OrderOf (o).key);
		ClassicalColumnSettings s;
		s.order = o;
		s.volutes = false;
		const std::unique_ptr<Mesh> m = BuildClassicalColumnMesh (s);
		const MeshValidationReport r = ValidateMesh (*m);

		EXPECT_TRUE (r.IsValid ());
		EXPECT_EQ (r.components, 1u);
		EXPECT_EQ (m->GetNVertices (), reference[o].vertices);
		EXPECT_EQ (m->GetNFaces (), reference[o].faces);
		EXPECT_NEAR (r.signedVolume, reference[o].volume, 1e-6 * std::fabs (reference[o].volume));
	}
}

// ===========================================================================
//  Surface de parametres et equivalence adaptateur / fonction pure
// ===========================================================================
//
// Les noms des parametres sont les cles des documents cggraph et des liens
// profonds de maker : noms, types, bornes, choix, valeurs par defaut et ordre
// de declaration sont figes ici. La colonne parametree, reglee par ses
// Parameter ou par SetSettings, doit construire pour tout reglage exactement le
// maillage de BuildClassicalColumnMesh.

namespace
{
	struct PinnedParameter
	{
		std::string              name;
		Parameter::Type          type;
		std::vector<std::string> choices;   // ENUM
		double                   lo, hi;    // INT et FLOAT ; 0 sinon
		double                   value;     // valeur par defaut (BOOL : 0 ou 1)
	};

	PinnedParameter Pin (const Parameter &p)
	{
		PinnedParameter r {p.GetName (), p.GetType (), p.GetChoices (), 0.0, 0.0, 0.0};
		switch (p.GetType ())
		{
		case Parameter::INT:
			r.lo = p.GetMinInt ();
			r.hi = p.GetMaxInt ();
			r.value = p.GetInt ();
			break;
		case Parameter::FLOAT:
			r.lo = p.GetMinFloat ();
			r.hi = p.GetMaxFloat ();
			r.value = p.GetFloat ();
			break;
		case Parameter::BOOL:
			r.value = p.GetBool () ? 1.0 : 0.0;
			break;
		case Parameter::ENUM:
			r.value = p.GetInt ();
			break;
		case Parameter::STRING:
			break;
		}
		return r;
	}

	void ExpectPinned (const std::vector<Parameter> &actual, const std::vector<PinnedParameter> &expected)
	{
		ASSERT_EQ (actual.size (), expected.size ());
		for (std::size_t k = 0; k < expected.size (); ++k)
		{
			const PinnedParameter a = Pin (actual[k]);
			const PinnedParameter &e = expected[k];
			SCOPED_TRACE (e.name);
			EXPECT_EQ (a.name, e.name) << k;
			EXPECT_EQ (a.type, e.type);
			EXPECT_EQ (a.choices, e.choices);
			EXPECT_EQ (a.lo, e.lo);
			EXPECT_EQ (a.hi, e.hi);
			EXPECT_EQ (a.value, e.value);
		}
	}

	const std::vector<std::string> kOrderChoices {"Tuscan", "Doric", "Ionic", "Corinthian", "Composite"};
	const std::vector<std::string> kCorniceChoices {"Dentils", "Mutules"};
	const std::vector<std::string> kDoricFluteChoices {"Segment (A)", "Semicircle (B)"};

	// Vrai si les deux maillages ont les memes sommets au bit pres et les memes
	// faces ; sinon `why` dit le premier ecart.
	bool SameMesh (const Mesh &a, const Mesh &b, std::string &why)
	{
		if (a.GetNVertices () != b.GetNVertices () || a.GetNFaces () != b.GetNFaces ())
		{
			why = "comptes " + std::to_string (a.GetNVertices ()) + "/" + std::to_string (a.GetNFaces ()) + " contre " +
			      std::to_string (b.GetNVertices ()) + "/" + std::to_string (b.GetNFaces ());
			return false;
		}
		for (unsigned v = 0; v < a.GetNVertices (); ++v)
		{
			float pa[3], pb[3];
			a.GetVertex (v, pa);
			b.GetVertex (v, pb);
			if (std::memcmp (pa, pb, sizeof pa) != 0)
			{
				why = "sommet " + std::to_string (v);
				return false;
			}
		}
		for (unsigned f = 0; f < a.GetNFaces (); ++f)
		{
			const unsigned n = static_cast<unsigned> (a.GetFaceNVertices (f));
			bool same = n == static_cast<unsigned> (b.GetFaceNVertices (f));
			for (unsigned k = 0; same && k < n; ++k)
				same = a.GetFaceVertex (f, k) == b.GetFaceVertex (f, k);
			if (!same)
			{
				why = "face " + std::to_string (f);
				return false;
			}
		}
		return true;
	}

	// Regle la colonne par ses Parameter, sous leur nom d'affichage, comme le
	// font cggraph et maker ; rend le nombre de parametres reconnus.
	int SetByParameters (cgarchi::ParameterizedClassicalColumn &column, const ClassicalColumnSettings &s)
	{
		int recognised = 0;
		std::vector<Parameter> ps = column.GetParameters ();
		for (Parameter &p : ps)
		{
			const std::string &n = p.GetName ();
			if (n == "Order") p.SetInt (s.order);
			else if (n == "Module") p.SetFloat (s.module);
			else if (n == "Pedestal") p.SetBool (s.pedestal);
			else if (n == "Entablature") p.SetBool (s.entablature);
			else if (n == "Doric cornice") p.SetInt (s.doricCornice);
			else if (n == "Ornament envelope") p.SetBool (s.envelope);
			else if (n == "Volutes") p.SetBool (s.volutes);
			else if (n == "Flutes") p.SetBool (s.flutes);
			else if (n == "Doric flutes") p.SetInt (s.doricFlutes);
			else if (n == "Radial segments") p.SetInt (s.radialSegments);
			else if (n == "Curve segments") p.SetInt (s.curveSegments);
			else continue;
			++recognised;
		}
		return recognised;
	}

	ClassicalColumnSettings WithOrder (int order)
	{
		ClassicalColumnSettings s;
		s.order = order;
		return s;
	}

	void ExpectSameSettings (const ClassicalColumnSettings &a, const ClassicalColumnSettings &e)
	{
		EXPECT_EQ (a.order, e.order);
		EXPECT_EQ (a.module, e.module);
		EXPECT_EQ (a.pedestal, e.pedestal);
		EXPECT_EQ (a.entablature, e.entablature);
		EXPECT_EQ (a.doricCornice, e.doricCornice);
		EXPECT_EQ (a.envelope, e.envelope);
		EXPECT_EQ (a.volutes, e.volutes);
		EXPECT_EQ (a.flutes, e.flutes);
		EXPECT_EQ (a.doricFlutes, e.doricFlutes);
		EXPECT_EQ (a.radialSegments, e.radialSegments);
		EXPECT_EQ (a.curveSegments, e.curveSegments);
	}

	// Valeur d'un Parameter lue sous son nom d'affichage, convertie en double.
	double ParameterValue (std::vector<Parameter> &ps, const std::string &name)
	{
		for (Parameter &p : ps)
			if (p.GetName () == name)
				return p.GetType () == Parameter::FLOAT ? p.GetFloat () : p.GetInt ();
		ADD_FAILURE () << "parametre absent : " << name;
		return 0.0;
	}

	void SetParameterValue (std::vector<Parameter> &ps, const std::string &name, double value)
	{
		for (Parameter &p : ps)
			if (p.GetName () == name)
			{
				if (p.GetType () == Parameter::FLOAT)
					p.SetFloat (static_cast<float> (value));
				else
					p.SetInt (static_cast<int> (value));
				return;
			}
		ADD_FAILURE () << "parametre absent : " << name;
	}
}

TEST (TEST_cgarchi_classical_column, the_parameter_surface_of_the_classical_shapes_is_pinned)
{
	cgarchi::ParameterizedClassicalColumn column;
	ExpectPinned (column.GetParameters (), {
		{"Order",             Parameter::ENUM,  kOrderChoices,      0.0,   0.0,    0.0},
		{"Module",            Parameter::FLOAT, {},                 0.1f,  10.f,   1.0},
		{"Pedestal",          Parameter::BOOL,  {},                 0.0,   0.0,    0.0},
		{"Entablature",       Parameter::BOOL,  {},                 0.0,   0.0,    0.0},
		{"Doric cornice",     Parameter::ENUM,  kCorniceChoices,    0.0,   0.0,    0.0},
		{"Ornament envelope", Parameter::BOOL,  {},                 0.0,   0.0,    0.0},
		{"Volutes",           Parameter::BOOL,  {},                 0.0,   0.0,    1.0},
		{"Flutes",            Parameter::BOOL,  {},                 0.0,   0.0,    0.0},
		{"Doric flutes",      Parameter::ENUM,  kDoricFluteChoices, 0.0,   0.0,    0.0},
		{"Radial segments",   Parameter::INT,   {},                 8.0,   256.0,  48.0},
		{"Curve segments",    Parameter::INT,   {},                 2.0,   32.0,   6.0},
	});

	cgarchi::ParameterizedClassicalColonnade colonnade;
	ExpectPinned (colonnade.GetParameters (), {
		{"Order",             Parameter::ENUM,  kOrderChoices,      0.0,   0.0,    0.0},
		{"Module",            Parameter::FLOAT, {},                 0.1f,  10.f,   1.0},
		{"Columns",           Parameter::INT,   {},                 1.0,   12.0,   4.0},
		{"Pedestal",          Parameter::BOOL,  {},                 0.0,   0.0,    0.0},
		{"Doric cornice",     Parameter::ENUM,  kCorniceChoices,    0.0,   0.0,    0.0},
		{"Ornaments",         Parameter::BOOL,  {},                 0.0,   0.0,    1.0},
		{"Ornament envelope", Parameter::BOOL,  {},                 0.0,   0.0,    0.0},
		{"Volutes",           Parameter::BOOL,  {},                 0.0,   0.0,    1.0},
		{"Flutes",            Parameter::BOOL,  {},                 0.0,   0.0,    0.0},
		{"Doric flutes",      Parameter::ENUM,  kDoricFluteChoices, 0.0,   0.0,    0.0},
		{"Radial segments",   Parameter::INT,   {},                 8.0,   256.0,  48.0},
		{"Curve segments",    Parameter::INT,   {},                 2.0,   32.0,   6.0},
	});
}

// Chaque cas part d'un reglage `base` ou le parametre a un effet, et le fait
// varier hors defaut. Garde : la variation change le maillage de la fonction
// pure, sinon le cas ne prouverait rien. Les valeurs restent dans les bornes,
// pour que le bornage de Regenerate soit l'identite.
TEST (TEST_cgarchi_classical_column, the_parameterized_column_builds_what_the_pure_builder_builds)
{
	struct Case
	{
		const char             *parameter;
		ClassicalColumnSettings base;
		void                  (*vary) (ClassicalColumnSettings &);
	};
	ClassicalColumnSettings doricEntablature = WithOrder (1);
	doricEntablature.entablature = true;
	ClassicalColumnSettings compositePedestal = WithOrder (4);
	compositePedestal.pedestal = true;
	ClassicalColumnSettings doricFluted = WithOrder (1);
	doricFluted.flutes = true;

	const Case cases[] = {
		{"(defaut)",          {},                nullptr},
		{"Order",             {},                [] (ClassicalColumnSettings &s) { s.order = 2; }},
		{"Module",            {},                [] (ClassicalColumnSettings &s) { s.module = 2.5f; }},
		{"Pedestal",          {},                [] (ClassicalColumnSettings &s) { s.pedestal = true; }},
		{"Entablature",       {},                [] (ClassicalColumnSettings &s) { s.entablature = true; }},
		{"Doric cornice",     doricEntablature,  [] (ClassicalColumnSettings &s) { s.doricCornice = 1; }},
		{"Ornament envelope", compositePedestal, [] (ClassicalColumnSettings &s) { s.envelope = true; }},
		{"Volutes",           WithOrder (2),     [] (ClassicalColumnSettings &s) { s.volutes = false; }},
		{"Flutes",            WithOrder (2),     [] (ClassicalColumnSettings &s) { s.flutes = true; }},
		{"Doric flutes",      doricFluted,       [] (ClassicalColumnSettings &s) { s.doricFlutes = 1; }},
		{"Radial segments",   {},                [] (ClassicalColumnSettings &s) { s.radialSegments = 64; }},
		{"Curve segments",    {},                [] (ClassicalColumnSettings &s) { s.curveSegments = 7; }},
	};

	for (const Case &c : cases)
	{
		SCOPED_TRACE (c.parameter);
		ClassicalColumnSettings s = c.base;
		std::string why;
		if (c.vary)
		{
			c.vary (s);
			EXPECT_FALSE (SameMesh (*BuildClassicalColumnMesh (c.base), *BuildClassicalColumnMesh (s), why))
				<< "le cas ne fait pas varier le maillage";
		}

		classical::ColumnProfile profile;
		std::vector<classical::PlanConflict> conflicts;
		const std::unique_ptr<Mesh> pure = BuildClassicalColumnMesh (s, &profile, &conflicts);

		cgarchi::ParameterizedClassicalColumn column;
		ASSERT_EQ (SetByParameters (column, s), 11);
		column.Regenerate ();
		ASSERT_TRUE (column.LastError ().empty ()) << column.LastError ();
		ASSERT_NE (column.GetMesh (), nullptr);

		why.clear ();
		EXPECT_TRUE (SameMesh (*column.GetMesh (), *pure, why)) << why;
		EXPECT_EQ (column.Profile ().height, profile.height);
		EXPECT_EQ (column.Profile ().members.size (), profile.members.size ());
		EXPECT_EQ (column.PlanConflicts ().size (), conflicts.size ());

		cgarchi::ParameterizedClassicalColumn bySettings;
		bySettings.SetSettings (s);
		bySettings.Regenerate ();
		ASSERT_TRUE (bySettings.LastError ().empty ()) << bySettings.LastError ();
		why.clear ();
		EXPECT_TRUE (SameMesh (*bySettings.GetMesh (), *pure, why)) << "SetSettings : " << why;
		EXPECT_EQ (bySettings.Profile ().height, profile.height);
		EXPECT_EQ (bySettings.Profile ().members.size (), profile.members.size ());
		EXPECT_EQ (bySettings.PlanConflicts ().size (), conflicts.size ());
		ExpectSameSettings (bySettings.Settings (), s);
	}
}

// Domaine de `order` : [0, kOrderCount). Hors domaine, la construction leve
// std::invalid_argument sans rien ecrire dans `*profile` ni `*conflicts` ;
// controle positif aux deux bornes du domaine.
TEST (TEST_cgarchi_classical_column, an_order_outside_the_five_throws_before_writing_the_outputs)
{
	for (int order : {-1, classical::kOrderCount})
	{
		SCOPED_TRACE (order);
		classical::ColumnProfile profile;
		profile.height = -1.0;
		std::vector<classical::PlanConflict> conflicts (1);
		conflicts[0].y = -1.0;

		EXPECT_THROW (BuildClassicalColumnMesh (WithOrder (order), &profile, &conflicts), std::invalid_argument);
		EXPECT_EQ (profile.height, -1.0);
		EXPECT_TRUE (profile.members.empty ());
		ASSERT_EQ (conflicts.size (), 1u);
		EXPECT_EQ (conflicts[0].y, -1.0);
	}
	for (int order : {0, classical::kOrderCount - 1})
	{
		SCOPED_TRACE (order);
		classical::ColumnProfile profile;
		EXPECT_NO_THROW (BuildClassicalColumnMesh (WithOrder (order), &profile));
		EXPECT_GT (profile.height, 0.0);
	}
}

// Bornage de Regenerate, partage par la colonne et la colonnade : chaque champ
// borne est pousse sous et au-dessus de ses bornes, et relu apres Regenerate.
// Les champs non bornes sont inchanges. Deux reglages par classe, pour que la
// borne haute des segments ne se cumule pas avec celle de l'ordre et des
// colonnes (cout du maillage).
TEST (TEST_cgarchi_classical_column, the_settings_are_clamped_to_the_bounds_of_their_parameters)
{
	struct Case { ClassicalColumnSettings in, out; };
	ClassicalColumnSettings highEnums, highEnumsClamped, highSegments, highSegmentsClamped;
	highEnums.order = 9;          highEnumsClamped.order = classical::kOrderCount - 1;
	highEnums.doricCornice = 5;   highEnumsClamped.doricCornice = 1;
	highEnums.doricFlutes = 5;    highEnumsClamped.doricFlutes = 1;
	highEnums.module = 50.f;      highEnumsClamped.module = 10.f;
	highEnums.radialSegments = 2; highEnumsClamped.radialSegments = 8;
	highEnums.curveSegments = 0;  highEnumsClamped.curveSegments = 2;
	highEnums.pedestal = highEnumsClamped.pedestal = true;
	highEnums.flutes = highEnumsClamped.flutes = true;

	highSegments.order = -3;             highSegmentsClamped.order = 0;
	highSegments.doricCornice = -1;      highSegmentsClamped.doricCornice = 0;
	highSegments.doricFlutes = -1;       highSegmentsClamped.doricFlutes = 0;
	highSegments.module = 0.01f;         highSegmentsClamped.module = 0.1f;
	highSegments.radialSegments = 1000;  highSegmentsClamped.radialSegments = 256;
	highSegments.curveSegments = 99;     highSegmentsClamped.curveSegments = 32;
	highSegments.volutes = highSegmentsClamped.volutes = false;
	highSegments.envelope = highSegmentsClamped.envelope = true;

	const Case cases[] = {{highEnums, highEnumsClamped}, {highSegments, highSegmentsClamped}};
	const int columns[] = {99, -5};
	const int columnsClamped[] = {12, 1};
	for (int k = 0; k < 2; ++k)
	{
		SCOPED_TRACE (k);
		const Case &c = cases[k];

		cgarchi::ParameterizedClassicalColumn column;
		column.SetSettings (c.in);
		column.Regenerate ();
		ExpectSameSettings (column.Settings (), c.out);
		EXPECT_TRUE (column.LastError ().empty ()) << column.LastError ();

		cgarchi::ParameterizedClassicalColonnade colonnade;
		std::vector<Parameter> ps = colonnade.GetParameters ();
		SetParameterValue (ps, "Order", c.in.order);
		SetParameterValue (ps, "Module", c.in.module);
		SetParameterValue (ps, "Columns", columns[k]);
		SetParameterValue (ps, "Doric cornice", c.in.doricCornice);
		SetParameterValue (ps, "Doric flutes", c.in.doricFlutes);
		SetParameterValue (ps, "Radial segments", c.in.radialSegments);
		SetParameterValue (ps, "Curve segments", c.in.curveSegments);
		colonnade.Regenerate ();
		EXPECT_TRUE (colonnade.LastError ().empty ()) << colonnade.LastError ();
		ps = colonnade.GetParameters ();
		EXPECT_EQ (ParameterValue (ps, "Order"), c.out.order);
		EXPECT_EQ (ParameterValue (ps, "Module"), static_cast<double> (c.out.module));
		EXPECT_EQ (ParameterValue (ps, "Columns"), columnsClamped[k]);
		EXPECT_EQ (ParameterValue (ps, "Doric cornice"), c.out.doricCornice);
		EXPECT_EQ (ParameterValue (ps, "Doric flutes"), c.out.doricFlutes);
		EXPECT_EQ (ParameterValue (ps, "Radial segments"), c.out.radialSegments);
		EXPECT_EQ (ParameterValue (ps, "Curve segments"), c.out.curveSegments);
	}
}

// ===========================================================================
//  Cotes d'une colonne sans maillage (ComputeClassicalColumnLevels)
// ===========================================================================
//
// Chaque cote est le produit y du profil fois l'echelle du tour, calcule en
// double avec la meme echelle que la cote du sommet ; le sommet est ensuite
// arrondi en float, a un demi-ULP pres, soit au plus 2^-24 |z|. La plus haute colonne (composite, avec
// piedestal et entablement) ne depasse pas 32 modules : l'arrondi reste sous
// 1,9e-6 module. La tolerance de LevelTolerance, 1e-5 module, garde une marge de
// 5 sur cet arrondi, et suit l'echelle de la colonne comme celle-ci suit le
// module (the_column_mesh_scales_with_the_module).

namespace
{
	// Decalage du cas negatif, en parties de module : au-dessus de la
	// tolerance (1,2e-4 a 1,8e-4 partie selon l'ordre), sous la moitie de
	// l'ecart minimal entre une cote et une autre cote de sommet (0,0145 partie,
	// toscan, shaftTop, mesure sur la grille de LevelCase, dont les segments sont
	// fixes). Une partie entiere ne convient pas : les membres ont des hauteurs
	// entieres en parties, et la cote decalee tombe sur le membre voisin.
	constexpr double kLevelShift = 0.005;

	double LevelTolerance (const ClassicalColumnSettings &s)
	{
		return 1e-5 * s.module;
	}

	struct NamedLevel
	{
		const char *name;
		double      z;
	};

	std::vector<NamedLevel> NamedLevels (const ClassicalColumnLevels &l)
	{
		std::vector<NamedLevel> r = {{"foot", l.foot},
		                             {"shaftFoot", l.shaftFoot},
		                             {"shaftTop", l.shaftTop},
		                             {"capitalBottom", l.capitalBottom},
		                             {"capitalTop", l.capitalTop},
		                             {"top", l.top}};
		if (l.pedestalTop)
			r.push_back ({"pedestalTop", *l.pedestalTop});
		if (l.entablatureFoot)
			r.push_back ({"entablatureFoot", *l.entablatureFoot});
		return r;
	}

	// Cotes z des sommets, croissantes.
	std::vector<double> VertexHeights (const Mesh &m, unsigned first = 0, unsigned last = ~0u)
	{
		std::vector<double> z;
		last = std::min (last, m.GetNVertices ());
		for (unsigned v = first; v < last; ++v)
		{
			float p[3];
			m.GetVertex (v, p);
			z.push_back (p[2]);
		}
		std::sort (z.begin (), z.end ());
		return z;
	}

	// Ecart de `z` a la cote de sommet la plus proche ; +inf sans sommet.
	double DistanceToNearestHeight (const std::vector<double> &heights, double z)
	{
		const auto it = std::lower_bound (heights.begin (), heights.end (), z);
		double d = HUGE_VAL;
		if (it != heights.end ())
			d = *it - z;
		if (it != heights.begin ())
			d = std::min (d, z - *(it - 1));
		return d;
	}

	// Grille de T5 et T6. Les segments sont fixes ici, et non pris aux valeurs
	// par defaut : l'ecart minimal qui borne kLevelShift depend de curveSegments
	// (echantillons des conges et des moulures).
	ClassicalColumnSettings LevelCase (int order, bool pedestal, bool entablature)
	{
		ClassicalColumnSettings s;
		s.order = order;
		s.module = kModule;
		s.pedestal = pedestal;
		s.entablature = entablature;
		s.radialSegments = 48;
		s.curveSegments = 6;
		s.volutes = true;
		s.flutes = false;
		s.envelope = false;
		return s;
	}

	double SumHeights (const classical::Block *block)
	{
		double h = 0.0;
		if (block)
			for (const classical::Member &m : block->members)
				h += m.h;
		return h;
	}
}

// 5 ordres x (piedestal, entablement) : chaque cote est la cote d'au moins un
// sommet du maillage de BuildClassicalColumnMesh, le pied et le sommet sont ceux
// de la boite. Reglages par defaut sinon (volutes ioniques comprises).
TEST (TEST_cgarchi_classical_column, each_level_is_the_height_of_a_vertex_of_the_column_mesh)
{
	double worst = 0.0;
	for (int o = 0; o < kOrderCount; ++o)
		for (int pedestal = 0; pedestal < 2; ++pedestal)
			for (int entablature = 0; entablature < 2; ++entablature)
			{
				const ClassicalColumnSettings s = LevelCase (o, pedestal != 0, entablature != 0);
				SCOPED_TRACE (OrderOf (o).key + (pedestal ? " piedestal" : "") + (entablature ? " entablement" : ""));
				const ClassicalColumnLevels levels = ComputeClassicalColumnLevels (s);
				const std::unique_ptr<Mesh> m = BuildClassicalColumnMesh (s);
				const std::vector<double> heights = VertexHeights (*m);
				const double tol = LevelTolerance (s);

				EXPECT_EQ (levels.entablatureFoot.has_value (), entablature != 0);
				for (const NamedLevel &level : NamedLevels (levels))
				{
					const double d = DistanceToNearestHeight (heights, level.z);
					EXPECT_LE (d, tol) << level.name << " = " << level.z;
					worst = std::max (worst, d);
				}
				const MeshValidationReport r = ValidateMesh (*m);
				EXPECT_NEAR (levels.foot, r.bbMin[2], tol);
				EXPECT_NEAR (levels.top, r.bbMax[2], tol);
			}
	std::cout << "[ mesure ] ecart maximal cote / sommet : " << worst << " (tolerance " << 1e-5 * kModule << ")\n";
}

// Cas negatif du detecteur ci-dessus : chaque cote decalee de kLevelShift partie,
// vers le haut ou vers le bas, n'est la cote d'aucun sommet des memes maillages.
TEST (TEST_cgarchi_classical_column, a_level_moved_off_its_ring_is_the_height_of_no_vertex)
{
	for (int o = 0; o < kOrderCount; ++o)
		for (int pedestal = 0; pedestal < 2; ++pedestal)
			for (int entablature = 0; entablature < 2; ++entablature)
			{
				const ClassicalColumnSettings s = LevelCase (o, pedestal != 0, entablature != 0);
				SCOPED_TRACE (OrderOf (o).key + (pedestal ? " piedestal" : "") + (entablature ? " entablement" : ""));
				const double part = s.module / OrderOf (o).partsPerModule;
				ASSERT_GT (kLevelShift * part, 4.0 * LevelTolerance (s));
				const std::unique_ptr<Mesh> m = BuildClassicalColumnMesh (s);
				const std::vector<double> heights = VertexHeights (*m);
				for (const NamedLevel &level : NamedLevels (ComputeClassicalColumnLevels (s)))
					for (double shift : {kLevelShift * part, -kLevelShift * part})
						EXPECT_GT (DistanceToNearestHeight (heights, level.z + shift), LevelTolerance (s))
							<< level.name << " " << shift / part << " partie";
			}
}

// Chaque cote, recomposee depuis les donnees de l'ordre sans passer par le
// profil : blocs du piedestal, de la base (membres hors module compris), de
// fut_haut et du chapiteau, et table generale (colonne, piedestal,
// entablement), en modules. Le dorique est pris avec ses deux corniches.
// L'echelle du tour, module / parties par module, est un quotient float : les
// cotes s'ecartent de la table generale, calculee en double, d'au plus 2^-24 |z|
// en relatif, sous LevelTolerance. Un membre pris pour un autre decale une cote
// d'une demi-partie au moins.
TEST (TEST_cgarchi_classical_column, the_levels_are_the_heights_of_the_order_data)
{
	for (int o = 0; o < kOrderCount; ++o)
		for (int pedestal = 0; pedestal < 2; ++pedestal)
			for (int entablature = 0; entablature < 2; ++entablature)
				for (int cornice = 0; cornice < (o == 1 ? 2 : 1); ++cornice)
				{
					const classical::OrderData &order = OrderOf (o);
					ClassicalColumnSettings s = LevelCase (o, pedestal != 0, entablature != 0);
					s.doricCornice = cornice;
					SCOPED_TRACE (order.key + (pedestal ? " piedestal" : "") + (entablature ? " entablement" : "") +
					              (cornice ? " mutules" : ""));
					const ClassicalColumnLevels l = ComputeClassicalColumnLevels (s);
					const double scale = s.module / order.partsPerModule;
					const double tol = LevelTolerance (s);

					const double pedestalTop = pedestal ? SumHeights (order.FindBlock ("piedestal")) * scale : 0.0;
					if (pedestal)
						EXPECT_NEAR (pedestalTop, order.general.pedestal_M * s.module, tol);
					const double columnTop = pedestalTop + order.general.column_M * s.module;
					const double entablatureHeight = entablature ? order.general.entablature_M * s.module : 0.0;

					EXPECT_EQ (l.foot, 0.0);
					ASSERT_EQ (l.pedestalTop.has_value (), pedestal != 0);
					if (l.pedestalTop)
						EXPECT_NEAR (*l.pedestalTop, pedestalTop, tol);
					EXPECT_NEAR (l.shaftFoot, pedestalTop + SumHeights (order.FindBlock ("base")) * scale, tol);
					EXPECT_NEAR (l.capitalBottom - l.shaftTop, SumHeights (order.FindBlock ("fut_haut")) * scale, tol);
					EXPECT_NEAR (l.capitalTop - l.capitalBottom, SumHeights (order.FindBlock ("chapiteau")) * scale, tol);
					EXPECT_NEAR (l.capitalTop, columnTop, tol);
					EXPECT_NEAR (l.top, columnTop + entablatureHeight, tol);
					ASSERT_EQ (l.entablatureFoot.has_value (), entablature != 0);
					if (l.entablatureFoot)
						EXPECT_EQ (*l.entablatureFoot, l.capitalTop);
					EXPECT_LT (l.foot, l.shaftFoot);
					if (l.pedestalTop)
						EXPECT_LT (*l.pedestalTop, l.shaftFoot);
					EXPECT_LT (l.shaftFoot, l.shaftTop);
					EXPECT_LT (l.shaftTop, l.capitalBottom);
					EXPECT_LT (l.capitalBottom, l.capitalTop);
					EXPECT_LE (l.capitalTop, l.top);
				}
}

// Colonnade : le pied de l'entablement et de ses ornements, soit le plus bas
// des sommets qui suivent les N copies de la colonne, est la cote `top` de la
// colonne seule (la colonnade construit ses colonnes sans entablement).
TEST (TEST_cgarchi_classical_column, the_colonnade_entablature_rests_on_the_top_level_of_its_column)
{
	const int n = 2;
	for (int o = 0; o < kOrderCount; ++o)
		for (int pedestal = 0; pedestal < 2; ++pedestal)
			for (int ornaments = 0; ornaments < 2; ++ornaments)
			{
				SCOPED_TRACE (OrderOf (o).key + (pedestal ? " piedestal" : "") + (ornaments ? " ornements" : ""));
				ParameterizedClassicalColonnade colonnade;
				std::vector<Parameter> ps = colonnade.GetParameters ();
				SetParameterValue (ps, "Order", o);
				SetParameterValue (ps, "Module", kModule);
				SetParameterValue (ps, "Columns", n);
				for (Parameter &p : ps)
				{
					if (p.GetName () == "Pedestal")
						p.SetBool (pedestal != 0);
					if (p.GetName () == "Ornaments")
						p.SetBool (ornaments != 0);
				}
				colonnade.Regenerate ();
				ASSERT_TRUE (colonnade.LastError ().empty ()) << colonnade.LastError ();

				const ClassicalColumnSettings s = LevelCase (o, pedestal != 0, false);
				const ClassicalColumnLevels levels = ComputeClassicalColumnLevels (s);
				const unsigned columnVertices = BuildClassicalColumnMesh (s)->GetNVertices ();
				const Mesh &m = *colonnade.GetMesh ();
				ASSERT_GT (m.GetNVertices (), n * columnVertices);

				const std::vector<double> above = VertexHeights (m, n * columnVertices);
				const std::vector<double> columns = VertexHeights (m, 0, n * columnVertices);
				EXPECT_NEAR (above.front (), levels.top, LevelTolerance (s));
				EXPECT_NEAR (columns.back (), levels.top, LevelTolerance (s));
			}
}

// Les cotes ne dependent ni des segments (dans les bornes des Parameter), ni
// des cannelures, de l'enveloppe ou des volutes. Exclusion : les coussinets
// ioniques ne sont pas dans le profil tourne. Sans volutes, chaque sommet de la
// colonne ionique est a la cote d'un sommet du profil ; avec, des sommets n'y
// sont pas, et les cotes ne disent rien de leur etendue.
TEST (TEST_cgarchi_classical_column, the_levels_ignore_segments_and_ornaments_and_exclude_the_ionic_volutes)
{
	auto same = [] (const ClassicalColumnLevels &a, const ClassicalColumnLevels &b) {
		return a.foot == b.foot && a.pedestalTop == b.pedestalTop && a.shaftFoot == b.shaftFoot && a.shaftTop == b.shaftTop &&
		       a.capitalBottom == b.capitalBottom && a.capitalTop == b.capitalTop && a.top == b.top &&
		       a.entablatureFoot == b.entablatureFoot;
	};
	// Chaque variation n'est essayee que sur les ordres ou elle change le
	// maillage, et une garde le verifie (comme dans T2) : cannelures hors du
	// toscan, section B au dorique, enveloppe au corinthien et au composite,
	// volutes a l'ionique.
	struct Variation
	{
		const char *name;
		std::function<void (ClassicalColumnSettings &)> apply;
		std::vector<int> orders;
	};
	const std::vector<int> all = {0, 1, 2, 3, 4};
	const std::vector<int> fluted = {1, 2, 3, 4};
	const Variation variations[] = {
		{"curveSegments 2", [] (ClassicalColumnSettings &s) { s.curveSegments = 2; }, all},
		{"curveSegments 32", [] (ClassicalColumnSettings &s) { s.curveSegments = 32; }, all},
		{"radialSegments 8", [] (ClassicalColumnSettings &s) { s.radialSegments = 8; }, all},
		{"radialSegments 256", [] (ClassicalColumnSettings &s) { s.radialSegments = 256; }, all},
		{"flutes", [] (ClassicalColumnSettings &s) { s.flutes = true; }, fluted},
		{"doricFlutes B", [] (ClassicalColumnSettings &s) { s.flutes = true; s.doricFlutes = 1; }, {1}},
		{"flutes, 256 x 32", [] (ClassicalColumnSettings &s) { s.flutes = true; s.radialSegments = 256; s.curveSegments = 32; }, fluted},
		{"envelope", [] (ClassicalColumnSettings &s) { s.envelope = true; }, {3, 4}},
		{"volutes off", [] (ClassicalColumnSettings &s) { s.volutes = false; }, {2}},
	};
	for (int o = 0; o < kOrderCount; ++o)
	{
		SCOPED_TRACE (OrderOf (o).key);
		const ClassicalColumnSettings base = LevelCase (o, true, true);
		const ClassicalColumnLevels reference = ComputeClassicalColumnLevels (base);
		const std::unique_ptr<Mesh> baseMesh = BuildClassicalColumnMesh (base);
		for (const Variation &v : variations)
		{
			if (std::find (v.orders.begin (), v.orders.end (), o) == v.orders.end ())
				continue;
			ClassicalColumnSettings s = base;
			v.apply (s);
			std::string why;
			EXPECT_FALSE (SameMesh (*BuildClassicalColumnMesh (s), *baseMesh, why)) << "garde : " << v.name;
			EXPECT_TRUE (same (ComputeClassicalColumnLevels (s), reference)) << v.name;
		}
	}

	for (bool volutes : {false, true})
	{
		SCOPED_TRACE (volutes ? "ionique a volutes" : "ionique sans volutes");
		ClassicalColumnSettings s = LevelCase (2, false, false);
		s.volutes = volutes;
		classical::ColumnProfile profile;
		const std::unique_ptr<Mesh> m = BuildClassicalColumnMesh (s, &profile);
		std::vector<double> ringHeights;
		for (const classical::ProfileVertex &v : profile.vertices)
			ringHeights.push_back (v.y * s.module / OrderOf (2).partsPerModule);
		std::sort (ringHeights.begin (), ringHeights.end ());

		std::size_t offProfile = 0;
		for (double z : VertexHeights (*m))
			if (DistanceToNearestHeight (ringHeights, z) > LevelTolerance (s))
				++offProfile;
		if (volutes)
			EXPECT_GT (offProfile, 0u);
		else
			EXPECT_EQ (offProfile, 0u);
	}
}

// Domaine : celui de BuildClassicalColumnMesh.
TEST (TEST_cgarchi_classical_column, the_levels_of_an_order_outside_the_five_throw)
{
	for (int order : {-1, classical::kOrderCount})
		EXPECT_THROW (ComputeClassicalColumnLevels (WithOrder (order)), std::invalid_argument) << order;
	for (int order : {0, classical::kOrderCount - 1})
		EXPECT_NO_THROW (ComputeClassicalColumnLevels (WithOrder (order))) << order;
}
