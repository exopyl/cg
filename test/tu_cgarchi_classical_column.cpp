#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <memory>
#include <string>

#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_profile.h>
#include <cgarchi/parameterized_classical.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"

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
