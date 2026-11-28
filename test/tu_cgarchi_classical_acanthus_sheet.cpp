#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cgarchi/classical_acanthus.h>
#include <cgarchi/classical_acanthus_detail.h>
#include <cgarchi/classical_acanthus_template.h>
#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_profile.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "acanthus_test_support.h"

using namespace cgarchi::classical;
using acanthus_test::RadiusGrid;
using acanthus_test::Rasterize;

// ===========================================================================
//  Feuille plaquee d'un gabarit : J1 a J8, J11, J12' (b), J14, J15, J16', J17, J19, J21'
// ===========================================================================

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTolerance = 1e-4;   // p : sommets stockes en float
// J17 : longueur du corps / corps nominal du gabarit, par rang (registre AC-41).
constexpr double kLengthScaleF = 1.006, kLengthScaleE = 1.293;

using Point = std::array<double, 3>;

const detail::CapitalEnvelope &Envelope ()
{
	static const detail::CapitalEnvelope env = [] {
		ColumnOptions options;
		options.order = Order::Corinthian;
		const OrderData &order = ClassicalOrders ().Get (Order::Corinthian);
		return detail::ReadCapitalEnvelope (order, BuildColumnProfile (order, options));
	}();
	return env;
}

struct Case
{
	const char *name;
	CapitalPieceKind kind;
	double theta;
	detail::LeafStyle style;
	OrnamentDetail detail;
	unsigned shellTriangles, shellVertices;   // J1, registre AC-42
	double halfWidth;                         // J17 : hw du rang (p), registre AC-41
	double bodyRatio;                         // J17 : rapport corps / hw du decalque Olive (2,54)
};

// F a pi/8, E a 0 ; comptes du gabarit compile (feuille seule pour F, avec la bande pour E).
const Case kCases[] = {
	{"F Olive Light", CapitalPieceKind::LowerLeaf, kPi / 8.0, detail::LeafStyle::Olive, OrnamentDetail::Light, 1800, 902, 4.85, 0.84},
	{"F Olive Heavy", CapitalPieceKind::LowerLeaf, kPi / 8.0, detail::LeafStyle::Olive, OrnamentDetail::Heavy, 10296, 5150, 4.85, 0.84},
	{"E Olive Light", CapitalPieceKind::MiddleLeaf, 0.0, detail::LeafStyle::Olive, OrnamentDetail::Light, 1912, 958, 5.25, 1.0},
	{"E Olive Heavy", CapitalPieceKind::MiddleLeaf, 0.0, detail::LeafStyle::Olive, OrnamentDetail::Heavy, 11016, 5510, 5.25, 1.0},
};

struct Built
{
	detail::LeafSpec spec;
	detail::LeafPlacement placement;
	const detail::LeafTemplate *tpl = nullptr;
	detail::LeafSheet sheet;
};

Built Build (const Case &c, double reliefScale = 1.0)
{
	Built b;
	b.spec = detail::DefaultLeafSpec (c.detail, c.style);
	b.spec.reliefScale = reliefScale;
	b.placement = detail::DefaultLeafPlacement (c.kind, c.theta);
	b.tpl = &detail::LeafTemplateOf (c.style, c.detail);
	b.sheet = detail::BuildLeafSheet (Envelope (), b.spec, b.placement, *b.tpl);
	return b;
}

Point Vertex (const Mesh &mesh, unsigned v)
{
	float p[3];
	mesh.GetVertex (v, p);
	return {p[0], p[1], p[2]};
}

// Point de S au sommet f d'une face : S = (o (t/2 + Hb) + i (t/2 + H)) / (t + H + Hb), repere en parties.
Point Surface (const Built &b, unsigned f)
{
	const Point o = Vertex (*b.sheet.mesh, f), in = Vertex (*b.sheet.mesh, f + b.sheet.faceVertices);
	const double t = b.spec.thickness, H = b.sheet.planar[f][2], Hb = b.sheet.planar[f][3];
	const double wo = 0.5 * t + Hb, wi = 0.5 * t + H, s = Envelope ().scale;
	Point p {};
	for (std::size_t k = 0; k < 3; ++k)
		p[k] = (o[k] * wo + in[k] * wi) / (wo + wi) / s;
	p[2] -= Envelope ().foot;
	return p;
}

double LeafExcess (const Mesh &mesh)
{
	double worst = -std::numeric_limits<double>::infinity ();
	const double s = Envelope ().scale;
	for (unsigned v = 0; v < mesh.GetNVertices (); ++v)
	{
		const Point p = Vertex (mesh, v);
		const double r = std::hypot (p[0], p[1]) / s, h = p[2] / s - Envelope ().foot;
		worst = std::max (worst, r - Envelope ().LeafBoundRadius (h));
	}
	return worst;
}

double MinAngleDeg (const Point &a, const Point &b, const Point &c)
{
	const auto angle = [] (const Point &o, const Point &p, const Point &q) {
		const Point u {p[0] - o[0], p[1] - o[1], p[2] - o[2]}, w {q[0] - o[0], q[1] - o[1], q[2] - o[2]};
		const Point x {u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]};
		return std::atan2 (std::sqrt (x[0] * x[0] + x[1] * x[1] + x[2] * x[2]), u[0] * w[0] + u[1] * w[1] + u[2] * w[2]);
	};
	return std::min ({angle (a, b, c), angle (b, c, a), angle (c, a, b)}) * 180.0 / kPi;
}

double OutlineDistance (const std::vector<std::array<double, 2>> &outline, double u, double y)
{
	double best = std::numeric_limits<double>::infinity ();
	for (std::size_t k = 0; k < outline.size (); ++k)
	{
		const auto &a = outline[k], &b = outline[(k + 1) % outline.size ()];
		const double dx = b[0] - a[0], dy = b[1] - a[1], l2 = dx * dx + dy * dy;
		const double t = l2 > 0.0 ? std::clamp (((u - a[0]) * dx + (y - a[1]) * dy) / l2, 0.0, 1.0) : 0.0;
		best = std::min (best, std::hypot (u - a[0] - t * dx, y - a[1] - t * dy));
	}
	return best;
}

// Fichier ouvert en ecriture, nullptr en cas d'echec.
std::FILE *OpenForWriting (const char *path)
{
#if defined (_MSC_VER)
	std::FILE *f = nullptr;
	return fopen_s (&f, path, "w") == 0 ? f : nullptr;
#else
	return std::fopen (path, "w");
#endif
}

// Sommet de l'axe (X = 0) d'une feuille : y du gabarit, rayon et hauteur de la face exterieure (p), epaisseur
// de la lame (distance des deux faces, p).
struct AxisSample
{
	double y, r, h, blade;
};

// Sommets de l'axe par y croissant.
std::vector<AxisSample> AxisOf (const Built &b)
{
	const double s = Envelope ().scale;
	std::vector<AxisSample> axis;
	for (unsigned f = 0; f < b.sheet.faceVertices; ++f)
		if (b.sheet.planar[f][0] == 0.0)
		{
			const Point o = Vertex (*b.sheet.mesh, f), in = Vertex (*b.sheet.mesh, f + b.sheet.faceVertices);
			axis.push_back ({b.sheet.planar[f][1], std::hypot (o[0], o[1]) / s, o[2] / s - Envelope ().foot,
			                 std::sqrt ((o[0] - in[0]) * (o[0] - in[0]) + (o[1] - in[1]) * (o[1] - in[1]) + (o[2] - in[2]) * (o[2] - in[2])) / s});
		}
	std::sort (axis.begin (), axis.end (), [] (const AxisSample &a, const AxisSample &c) { return a.y < c.y; });
	return axis;
}

// Rayon de la face exterieure et epaisseur de la lame a la hauteur h, interpolees sur l'axe, de bas en haut
// jusqu'au sommet le plus haut de la face (avant que le bec ne redescende). NaN si h n'y est pas atteint.
std::array<double, 2> AxisAt (const std::vector<AxisSample> &axis, double h)
{
	std::size_t top = 0;
	for (std::size_t k = 1; k < axis.size (); ++k)
		if (axis[k].h > axis[top].h)
			top = k;
	for (std::size_t k = 1; k <= top; ++k)
		if (axis[k - 1].h <= h && h <= axis[k].h && axis[k].h > axis[k - 1].h)
		{
			const double w = (h - axis[k - 1].h) / (axis[k].h - axis[k - 1].h);
			return {axis[k - 1].r + w * (axis[k].r - axis[k - 1].r), axis[k - 1].blade + w * (axis[k].blade - axis[k - 1].blade)};
		}
	return {std::nan (""), std::nan ("")};
}

bool ThrowsWith (const std::function<void ()> &f, const std::string &fragment)
{
	try
	{
		f ();
	}
	catch (const std::invalid_argument &e)
	{
		if (std::string (e.what ()).find (fragment) != std::string::npos)
			return true;
		ADD_FAILURE () << "message sans « " << fragment << " » : " << e.what ();
		return false;
	}
	ADD_FAILURE () << "aucun refus, « " << fragment << " » attendu";
	return false;
}

} // namespace

TEST (TEST_cgarchi_classical_acanthus_sheet, the_sheet_is_a_sound_shell)
{
	for (const Case &c : kCases)
	{
		SCOPED_TRACE (c.name);
		const Built b = Build (c);
		const Mesh &mesh = *b.sheet.mesh;
		// J1 : comptes.
		EXPECT_EQ (mesh.GetNFaces (), c.shellTriangles);
		EXPECT_EQ (mesh.GetNVertices (), c.shellVertices);
		// J2 : coque valide, une composante, sans auto-intersection, volume positif.
		MeshValidationOptions one;
		one.expectedComponents = 1;
		const MeshValidationReport r = ValidateMesh (mesh, one);
		EXPECT_TRUE (r.IsValid ());
		EXPECT_FALSE (r.selfIntersectionTruncated);
		EXPECT_GT (r.signedVolume, 0.0);
		for (std::size_t k = 0; k < kMeshDefectCount; ++k)
			EXPECT_EQ (r.counts[k], 0u) << MeshDefectName (static_cast<MeshDefect> (k));
		// J2, par indices et sans soudure : une seule composante. Des sommets doubles au meme point
		// (bande non cousue a la feuille) echappent a la validation, qui soude.
		std::vector<unsigned> parent (mesh.GetNVertices ());
		for (unsigned v = 0; v < parent.size (); ++v)
			parent[v] = v;
		const std::function<unsigned (unsigned)> root = [&] (unsigned v) { return parent[v] == v ? v : parent[v] = root (parent[v]); };
		for (unsigned f = 0; f < mesh.GetNFaces (); ++f)
			for (int k = 1; k < 3; ++k)
				parent[root (static_cast<unsigned> (mesh.GetFaceVertex (f, k)))] = root (static_cast<unsigned> (mesh.GetFaceVertex (f, 0)));
		unsigned components = 0;
		for (unsigned v = 0; v < parent.size (); ++v)
			components += root (v) == v ? 1u : 0u;
		EXPECT_EQ (components, 1u);
	}
}

TEST (TEST_cgarchi_classical_acanthus_sheet, the_sheet_follows_its_template_and_carrier)
{
	for (const Case &c : kCases)
	{
		SCOPED_TRACE (c.name);
		const Built b = Build (c);
		const detail::LeafTemplate &tpl = *b.tpl;
		const bool withStem = c.kind == CapitalPieceKind::MiddleLeaf;
		const unsigned nv = withStem ? static_cast<unsigned> (tpl.vertices.size ()) : tpl.leafVertices;

		// J3 : le bord de la feuille (u = X / hw du registre) est sur le contour dessine, et chaque point
		// du contour est un sommet.
		std::set<unsigned> rim;
		for (const detail::LeafTemplateEdge &e : tpl.leafRim.edges)
			rim.insert ({e[0], e[1]});
		double rimGap = 0.0;
		for (const unsigned v : rim)
			rimGap = std::max (rimGap, OutlineDistance (tpl.outline, b.sheet.planar[v][0] / c.halfWidth, b.sheet.planar[v][1]));
		EXPECT_LE (rimGap, 2e-6);
		for (const auto &q : tpl.outline)
		{
			double nearest = std::numeric_limits<double>::infinity ();
			for (const unsigned v : rim)
				nearest = std::min (nearest, std::hypot (b.sheet.planar[v][0] / c.halfWidth - q[0], b.sheet.planar[v][1] - q[1]));
			EXPECT_LE (nearest, 2e-6) << q[0] << ", " << q[1];
		}

		// J4 : crete de S sur l'axe.
		double crest = -std::numeric_limits<double>::infinity ();
		for (unsigned f = 0; f < b.sheet.faceVertices; ++f)
			if (b.sheet.planar[f][0] == 0.0)
				crest = std::max (crest, Surface (b, f)[2]);
		EXPECT_NEAR (crest, b.placement.hTop, 0.05);

		// J5 : jeu avec la corbeille.
		const double clearance = detail::CoreClearance (*b.sheet.mesh, Envelope ());
		EXPECT_GE (clearance, -kTolerance);

		// J16' : contact des deux faces avec la borne des feuilles.
		const double contact = LeafExcess (*b.sheet.mesh);
		EXPECT_LE (contact, 0.0);
		EXPECT_GE (contact, -0.35);

		// J17 : X = u hw du registre (arc lateral de S) ; rapports de longueur.
		double xGap = 0.0;
		for (unsigned f = 0; f < nv; ++f)
		{
			const Point p = Surface (b, f);
			double phi = std::atan2 (p[1], p[0]) - c.theta;
			phi = std::remainder (phi, 2.0 * kPi);
			xGap = std::max (xGap, std::fabs (std::hypot (p[0], p[1]) * phi - tpl.vertices[f].u * c.halfWidth));
		}
		EXPECT_LE (xGap, 1e-3);
		const detail::LeafBend bend (Envelope (), b.spec, b.placement, tpl);
		const double body = bend.BodyLength () / c.halfWidth / 2.54;
		EXPECT_GE (body, c.bodyRatio - 0.10);
		EXPECT_LE (body, c.bodyRatio + 0.10);
		const double lengthScale = c.kind == CapitalPieceKind::LowerLeaf ? kLengthScaleF : kLengthScaleE;
		const double curl = bend.BendLength () / (lengthScale * tpl.nominalCurl);
		EXPECT_GE (curl, 0.85);
		EXPECT_LE (curl, 1.15);

		// J19 : bande de tige presente si et seulement si hStem < hFoot, pied a hStem, jeu c.
		EXPECT_EQ (b.sheet.stemVertices > 0, withStem);
		double foot = std::numeric_limits<double>::infinity (), bandClearance = std::numeric_limits<double>::infinity ();
		for (unsigned f = 0; f < b.sheet.faceVertices; ++f)
			if (b.sheet.planar[f][1] < 0.0)
			{
				foot = std::min (foot, Surface (b, f)[2]);
				const Point q = Vertex (*b.sheet.mesh, f + b.sheet.faceVertices);
				const double h = q[2] / Envelope ().scale - Envelope ().foot;
				bandClearance = std::min (bandClearance, std::hypot (q[0], q[1]) / Envelope ().scale - Envelope ().CoreRadius (h));
			}
		if (withStem)
		{
			EXPECT_NEAR (foot, b.placement.hStem, 0.05);
			EXPECT_GE (bandClearance, b.spec.clearance - kTolerance);
		}
		// Plancher (registre AC-49) : la feuille repose sur l'astragale, aucun sommet sous h = c.
		double lowest = std::numeric_limits<double>::infinity ();
		for (unsigned v = 0; v < b.sheet.mesh->GetNVertices (); ++v)
			lowest = std::min (lowest, Vertex (*b.sheet.mesh, v)[2] / Envelope ().scale - Envelope ().foot);
		EXPECT_GE (lowest, b.spec.clearance - kTolerance);

		std::printf ("%-14s crete %.3f | jeu %+.4f | contact %+.3f | X %.1e | corps/hw %.3f | bec %.3f | bande %u sommets, pied %.3f | "
		             "plus bas sommet %.4f\n",
		             c.name, crest, clearance, contact, xGap, body, curl, b.sheet.stemVertices, withStem ? foot : 0.0, lowest);
	}
}

TEST (TEST_cgarchi_classical_acanthus_sheet, the_relief_stays_under_the_bound_when_exaggerated)
{
	// J6 : relief exagere la ou il fait face a la borne, plafond bilateral compris. H quadruple sur le
	// corps, y dans [0,3 ; 0,8] : pres du pied, de la jonction de la bande et du depart du bec, 4 H passe
	// le rayon de courbure du meridien (garde de courbure), sans rapport avec la borne. Hb decuple : il
	// s'annule sur le contour, et triple il ne depasse guere le Hb de reference qui cale le bec ; le
	// plafond ne serait pas sollicite.
	for (const Case &c : kCases)
	{
		SCOPED_TRACE (c.name);
		detail::LeafTemplate exaggerated = detail::LeafTemplateOf (c.style, c.detail);
		for (detail::LeafTemplateVertex &v : exaggerated.vertices)
		{
			if (v.y >= 0.3 && v.y <= 0.8)
				v.H *= 4.0;
			v.Hb *= 10.0;
		}
		const detail::LeafSpec spec = detail::DefaultLeafSpec (c.detail, c.style);
		const detail::LeafSheet sheet = detail::BuildLeafSheet (Envelope (), spec, detail::DefaultLeafPlacement (c.kind, c.theta),
		                                                       exaggerated);
		const double excess = LeafExcess (*sheet.mesh);
		EXPECT_LE (excess, kTolerance);
		// Le plafond a mordu : au moins un sommet plafonne sur H et un sur Hb (demi-feuille droite).
		const unsigned nv = c.kind == CapitalPieceKind::MiddleLeaf ? static_cast<unsigned> (exaggerated.vertices.size ()) : exaggerated.leafVertices;
		unsigned cappedH = 0, cappedHb = 0;
		for (unsigned f = 0; f < nv; ++f)
		{
			cappedH += sheet.planar[f][2] < spec.reliefScale * exaggerated.vertices[f].H - 1e-9 ? 1u : 0u;
			cappedHb += sheet.planar[f][3] < spec.reliefScale * exaggerated.vertices[f].Hb - 1e-9 ? 1u : 0u;
		}
		EXPECT_GE (cappedH, 1u);
		EXPECT_GE (cappedHb, 1u);
		std::printf ("%-14s J6 debord max (relief exagere) %+.3f p | plafonnes H %u, Hb %u\n", c.name, excess, cappedH, cappedHb);
	}
}

// Garde de pli : sur une surface S plane (deux triangles, n_S = +z), une face decalee dont un sommet
// passe de l'autre cote de l'arete opposee est refusee, au nom de sa face ; la face simplement
// decalee est acceptee (controle positif).
TEST (TEST_cgarchi_classical_acanthus_sheet, the_fold_guard_refuses_a_folded_face)
{
	using P3 = std::array<double, 3>;
	const std::vector<P3> surface {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {0.0, 1.0, 0.0}};
	const std::vector<std::array<unsigned, 3>> triangles {{0, 1, 2}, {0, 2, 3}};
	const std::vector<P3> normals (4, P3 {0.0, 0.0, 1.0});
	const std::vector<std::array<double, 4>> planar (4, std::array<double, 4> {0.0, 0.0, 0.0, 0.0});
	std::vector<P3> face = surface;
	for (P3 &p : face)
		p[2] += 0.3;
	EXPECT_NO_THROW (detail::CheckFaceFold (face, surface, triangles, normals, planar, "exterieure"));
	face[2] = {-0.5, -0.5, 0.3};
	EXPECT_TRUE (ThrowsWith ([&] { detail::CheckFaceFold (face, surface, triangles, normals, planar, "exterieure"); },
	                         "pli de la face exterieure : triangle 0"));
	EXPECT_TRUE (ThrowsWith ([&] { detail::CheckFaceFold (face, surface, triangles, normals, planar, "interieure"); },
	                         "pli de la face interieure : triangle 0"));
}

TEST (TEST_cgarchi_classical_acanthus_sheet, the_faces_keep_their_angles)
{
	// J11 : angle 3D minimal de la face exterieure, et sur le bec (y > 1).
	for (const Case &c : kCases)
	{
		SCOPED_TRACE (c.name);
		const Built b = Build (c);
		const Mesh &mesh = *b.sheet.mesh;
		double all = 180.0, bec = 180.0;
		unsigned worst = 0;
		for (unsigned k = 0; k < b.sheet.faceTriangles; ++k)
		{
			const unsigned i = static_cast<unsigned> (mesh.GetFaceVertex (k, 0)), j = static_cast<unsigned> (mesh.GetFaceVertex (k, 1)),
			               l = static_cast<unsigned> (mesh.GetFaceVertex (k, 2));
			const double a = MinAngleDeg (Vertex (mesh, i), Vertex (mesh, j), Vertex (mesh, l));
			if (a < all)
			{
				all = a;
				worst = k;
			}
			if (i < b.sheet.faceVertices && b.sheet.planar[i][1] > 1.0 && b.sheet.planar[j % b.sheet.faceVertices][1] > 1.0)
				bec = std::min (bec, a);
		}
		const double floor = c.detail == OrnamentDetail::Light ? 1.0 : 3.0;
		EXPECT_GE (all, floor);
		EXPECT_GE (bec, floor);
		double yMin = std::numeric_limits<double>::infinity (), yMax = -yMin;
		for (int e = 0; e < 3; ++e)
		{
			const double y = b.sheet.planar[static_cast<unsigned> (mesh.GetFaceVertex (worst, e)) % b.sheet.faceVertices][1];
			yMin = std::min (yMin, y);
			yMax = std::max (yMax, y);
		}
		std::printf ("%-14s angle 3D minimal %.2f deg (region %d, y %.3f a %.3f), bec %.2f deg\n", c.name, all,
		             b.sheet.faceRegion[worst], yMin, yMax, bec);
	}
}

TEST (TEST_cgarchi_classical_acanthus_sheet, the_steps_and_layers_hold_on_the_leaf_alone)
{
	// J7 et J8, sur la partie feuille seule : la bande de tige (triangles >= leafTriangles, sommets
	// y < 0) est exclue, et seuls les sommets de la demi-feuille droite comptent.
	for (const Case &c : kCases)
	{
		SCOPED_TRACE (c.name);
		const Built b = Build (c);
		const detail::LeafTemplate &tpl = *b.tpl;
		const auto &P = b.sheet.planar;
		std::vector<std::set<int>> regions (tpl.leafVertices);
		std::vector<std::set<unsigned>> ring (tpl.leafVertices);

		for (unsigned k = 0; k < tpl.leafTriangles; ++k)
		{
			const auto &t = tpl.triangles[k];
			for (std::size_t e = 0; e < 3; ++e)
			{
				regions[t[e]].insert (tpl.triangleRegion[k]);
				ring[t[e]].insert ({t[(e + 1) % 3], t[(e + 2) % 3]});

			}
		}
		// J7 : marche = sommet de plusieurs regions ; sa hauteur est la plus grande chute de H vers un voisin
		// qui n'appartient pas a la region du sommet.
		unsigned steps = 0, tall = 0;
		for (unsigned v = 0; v < tpl.leafVertices; ++v)
		{
			if (regions[v].size () < 2)
				continue;
			const int own = tpl.vertices[v].region;
			double drop = 0.0;
			for (const unsigned w : ring[v])
				if (!regions[w].count (own))
					drop = std::max (drop, P[v][2] - P[w][2]);
			++steps;
			tall += drop >= 0.15 ? 1u : 0u;
		}
		ASSERT_GT (steps, 0u);
		const double tallShare = static_cast<double> (tall) / steps;
		EXPECT_GE (tallShare, 0.80);
		// J8 : a un sommet de marche entre deux lobes de couches differentes, ses voisins propres a la couche
		// haute sont plus hauts (H moyen) que ses voisins propres a la couche basse.
		const auto isLobe = [&] (int r) { return tpl.regions[static_cast<std::size_t> (r)].cup > 0.0; };
		unsigned pairs = 0, ordered = 0;
		for (unsigned v = 0; v < tpl.leafVertices; ++v)
		{
			int hi = -1, lo = -1;
			for (const int r : regions[v])
				if (isLobe (r))
				{
					const double layer = tpl.regions[static_cast<std::size_t> (r)].layer;
					if (hi < 0 || layer > tpl.regions[static_cast<std::size_t> (hi)].layer)
						hi = r;
					if (lo < 0 || layer < tpl.regions[static_cast<std::size_t> (lo)].layer)
						lo = r;
				}
			if (hi < 0 || tpl.regions[static_cast<std::size_t> (hi)].layer == tpl.regions[static_cast<std::size_t> (lo)].layer)
				continue;
			double sumHi = 0.0, sumLo = 0.0;
			int nHi = 0, nLo = 0;
			for (const unsigned w : ring[v])
			{
				if (regions[w].size () != 1)
					continue;
				if (regions[w].count (hi))
				{
					sumHi += P[w][2];
					++nHi;
				}
				else if (regions[w].count (lo))
				{
					sumLo += P[w][2];
					++nLo;
				}
			}
			if (nHi == 0 || nLo == 0)
				continue;
			++pairs;
			ordered += sumHi / nHi > sumLo / nLo ? 1u : 0u;
		}
		ASSERT_GT (pairs, 0u);
		const double orderedShare = static_cast<double> (ordered) / pairs;
		EXPECT_GE (orderedShare, 0.80);
		std::printf ("%-14s J7 marches >= 0,15 p : %u / %u (%.2f) | J8 couches ordonnees : %u / %u (%.2f)\n", c.name, tall, steps,
		             tallShare, ordered, pairs, orderedShare);
	}
}

TEST (TEST_cgarchi_classical_acanthus_sheet, the_sheet_guards_are_seen)
{
	const Case &c = kCases[1];
	const detail::LeafSpec spec = detail::DefaultLeafSpec (c.detail, c.style);
	const detail::LeafPlacement placement = detail::DefaultLeafPlacement (c.kind, c.theta);
	const detail::LeafTemplate &tpl = detail::LeafTemplateOf (c.style, c.detail);
	detail::CapitalEnvelope broken = Envelope ();
	broken.bands.clear ();
	EXPECT_TRUE (ThrowsWith ([&] { detail::BuildLeafSheet (broken, spec, placement, tpl); }, "enveloppe hors domaine"));
	EXPECT_TRUE (ThrowsWith ([&] { detail::BuildLeafSheet (Envelope (), spec, placement, detail::LeafTemplateOf (c.style, OrnamentDetail::Light)); },
	                         "gabarit d'un autre style ou d'un autre niveau"));
	const auto with = [&] (const std::function<void (detail::LeafSpec &)> &change) {
		return [&, change] {
			detail::LeafSpec s = spec;
			change (s);
			detail::BuildLeafSheet (Envelope (), s, placement, tpl);
		};
	};
	EXPECT_TRUE (ThrowsWith (with ([] (detail::LeafSpec &s) { s.reliefShare = 0.0; }), "reliefShare dans ]0, 1]"));
	EXPECT_TRUE (ThrowsWith (with ([] (detail::LeafSpec &s) { s.reliefScale = -1.0; }), "reliefScale fini et >= 0"));
	// Sur la face exterieure, la garde de courbure (2 t + H) precede la garde de pli (t/2 + H) : un relief
	// qui plierait la face est refuse par elle.
	EXPECT_TRUE (ThrowsWith (with ([] (detail::LeafSpec &s) { s.reliefScale = 3.0; }), "rayon de courbure"));
	detail::LeafTemplate negative = tpl;
	negative.vertices[3].H = -0.1;
	EXPECT_TRUE (ThrowsWith ([&] { detail::BuildLeafSheet (Envelope (), spec, placement, negative); }, "relief negatif au sommet 3"));
	negative = tpl;
	negative.vertices[5].Hb = -0.1;
	EXPECT_TRUE (ThrowsWith ([&] { detail::BuildLeafSheet (Envelope (), spec, placement, negative); }, "relief negatif au sommet 5"));
	// Pied : F posee a h = 0 plonge sa face exterieure dans l'astragale ; E dont la bande part de 0 y pose son mur.
	detail::LeafPlacement grounded = placement;
	grounded.hStem = grounded.hFoot = 0.0;
	EXPECT_TRUE (ThrowsWith ([&] { detail::BuildLeafSheet (Envelope (), spec, grounded, tpl); }, "pied sous le plancher du chapiteau"));
	detail::LeafPlacement groundedE = detail::DefaultLeafPlacement (CapitalPieceKind::MiddleLeaf, 0.0);
	groundedE.hStem = 0.0;
	EXPECT_TRUE (ThrowsWith ([&] { detail::BuildLeafSheet (Envelope (), spec, groundedE, tpl); }, "pied sous le plancher du chapiteau"));
}

// Revers plafonne cote noyau : Hb = 1 sur le corps de F (y dans [0,1 ; 0,5]), la ou S longe la corbeille,
// est ramene a la place au-dessus de CoreRadius + clearance ; la face interieure garde le jeu.
TEST (TEST_cgarchi_classical_acanthus_sheet, the_back_relief_stops_at_the_core)
{
	const Case &c = kCases[0];
	detail::LeafTemplate back = detail::LeafTemplateOf (c.style, c.detail);
	for (detail::LeafTemplateVertex &v : back.vertices)
		if (v.y >= 0.1 && v.y <= 0.5)
			v.Hb = 1.0;
	const detail::LeafSpec spec = detail::DefaultLeafSpec (c.detail, c.style);
	const detail::LeafSheet sheet = detail::BuildLeafSheet (Envelope (), spec, detail::DefaultLeafPlacement (c.kind, c.theta), back);
	EXPECT_GE (detail::CoreClearance (*sheet.mesh, Envelope ()), spec.clearance - kTolerance);
	unsigned capped = 0, raised = 0;
	for (unsigned f = 0; f < back.leafVertices; ++f)
		if (back.vertices[f].Hb == 1.0)
		{
			capped += sheet.planar[f][3] < 1.0 - 1e-9 ? 1u : 0u;
			raised += sheet.planar[f][3] > 0.05 ? 1u : 0u;
		}
	EXPECT_GE (capped, 1u);
	EXPECT_GE (raised, 1u);
	std::printf ("Hb = 1 sur le corps de F : %u sommets plafonnes cote noyau, %u au-dessus de 0,05 p, jeu %.4f p\n", capped, raised,
	             detail::CoreClearance (*sheet.mesh, Envelope ()));
}

// J12' (b) : demi-largeur en arc de S dans la bande de crete (|y - y_crete| <= 0,06), r (phi - theta) lu sur le
// maillage, dans [0,45 ; 0,80] hw du rang (pl. XXVI : capuchon de E de 4 +- 1 p). L'arc lateral vaut |X| = |u| hw :
// le test mesure la largeur du gabarit et le placement, non la forme pliee de la calotte.
TEST (TEST_cgarchi_classical_acanthus_sheet, the_cap_keeps_its_width_at_the_crest)
{
	for (const Case &c : kCases)
	{
		SCOPED_TRACE (c.name);
		const Built b = Build (c);
		const double crest = detail::LeafBend (Envelope (), b.spec, b.placement, *b.tpl).CrestY ();
		double width = 0.0;
		unsigned samples = 0;
		for (unsigned f = 0; f < b.sheet.faceVertices; ++f)
		{
			if (std::fabs (b.sheet.planar[f][1] - crest) > 0.06)
				continue;
			const Point p = Surface (b, f);
			width = std::max (width, std::fabs (std::hypot (p[0], p[1]) * std::remainder (std::atan2 (p[1], p[0]) - c.theta, 2.0 * kPi)));
			++samples;
		}
		ASSERT_GT (samples, 2u);
		EXPECT_GE (width / c.halfWidth, 0.45);
		EXPECT_LE (width / c.halfWidth, 0.80);
		std::printf ("%-14s J12' (b) demi-largeur a la crete %.3f p = %.3f hw (%u sommets)\n", c.name, width, width / c.halfWidth, samples);
	}
}

// J14 : symetrie par le plan meridien theta. Chaque sommet hors de l'axe (X, y) a son miroir (-X, y), de meme
// relief applique, et les deux faces du miroir sont les reflets de celles du sommet.
TEST (TEST_cgarchi_classical_acanthus_sheet, the_sheet_is_mirror_symmetric)
{
	for (const Case &c : kCases)
	{
		SCOPED_TRACE (c.name);
		const Built b = Build (c);
		const Mesh &mesh = *b.sheet.mesh;
		const unsigned n = b.sheet.faceVertices;
		std::map<std::pair<double, double>, unsigned> left;
		unsigned axis = 0;
		for (unsigned f = 0; f < n; ++f)
		{
			if (b.sheet.planar[f][0] < 0.0)
				left[{-b.sheet.planar[f][0], b.sheet.planar[f][1]}] = f;
			axis += b.sheet.planar[f][0] == 0.0 ? 1u : 0u;
		}
		const double c2 = std::cos (2.0 * c.theta), s2 = std::sin (2.0 * c.theta), scale = Envelope ().scale;
		const auto reflect = [&] (const Point &p) { return Point {c2 * p[0] + s2 * p[1], s2 * p[0] - c2 * p[1], p[2]}; };
		unsigned pairs = 0;
		double gap = 0.0, reliefGap = 0.0;
		for (unsigned f = 0; f < n; ++f)
		{
			if (!(b.sheet.planar[f][0] > 0.0))
				continue;
			const auto it = left.find ({b.sheet.planar[f][0], b.sheet.planar[f][1]});
			ASSERT_NE (it, left.end ()) << "sommet " << f << " sans miroir";
			const unsigned m = it->second;
			++pairs;
			for (const unsigned layer : {0u, n})
			{
				const Point p = reflect (Vertex (mesh, f + layer)), q = Vertex (mesh, m + layer);
				gap = std::max (gap, std::hypot (p[0] - q[0], p[1] - q[1], p[2] - q[2]) / scale);
			}
			reliefGap = std::max ({reliefGap, std::fabs (b.sheet.planar[f][2] - b.sheet.planar[m][2]),
			                       std::fabs (b.sheet.planar[f][3] - b.sheet.planar[m][3])});
		}
		EXPECT_EQ (2 * pairs + axis, n);
		EXPECT_GT (pairs, 0u);
		EXPECT_LE (gap, kTolerance);
		EXPECT_LE (reliefGap, 1e-9);
	}
}

// J15 : le revers garde son relief sur le bec. Sur l'axe, de y = 1,25 a 1,9 (avant que le gabarit ne l'annule a
// la pointe), Hb applique >= 0,2 p.
TEST (TEST_cgarchi_classical_acanthus_sheet, the_back_keeps_its_relief_on_the_cap)
{
	for (const Case &c : kCases)
	{
		SCOPED_TRACE (c.name);
		const Built b = Build (c);
		double lowest = std::numeric_limits<double>::infinity ();
		unsigned samples = 0;
		for (unsigned f = 0; f < b.sheet.faceVertices; ++f)
			if (b.sheet.planar[f][0] == 0.0 && b.sheet.planar[f][1] >= 1.25 && b.sheet.planar[f][1] <= 1.9)
			{
				lowest = std::min (lowest, b.sheet.planar[f][3]);
				++samples;
			}
		ASSERT_GT (samples, 2u);
		EXPECT_GE (lowest, 0.2);
		std::printf ("%-14s J15 Hb minimal sur l'axe du bec %.3f p (%u sommets)\n", c.name, lowest, samples);
	}
}

// J21' : E (theta = 0) passe derriere F (theta = +-pi/8). Sur la grille (theta, h), aux noeuds couverts par la
// face exterieure de E et par la face interieure d'une F, rayon de F - c - rayon de E >= 0,05 p.
TEST (TEST_cgarchi_classical_acanthus_sheet, the_middle_leaf_passes_behind_the_lower_leaves)
{
	for (const OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
	{
		SCOPED_TRACE (level == OrnamentDetail::Light ? "Light" : "Heavy");
		const Case e {"E", CapitalPieceKind::MiddleLeaf, 0.0, detail::LeafStyle::Olive, level, 0, 0, 5.25, 1.0};
		const Built be = Build (e);
		RadiusGrid outerE, innerF;
		Rasterize (outerE, *be.sheet.mesh, Envelope (), 0, be.sheet.faceTriangles, true);
		for (const double theta : {kPi / 8.0, -kPi / 8.0})
		{
			const Case f {"F", CapitalPieceKind::LowerLeaf, theta, detail::LeafStyle::Olive, level, 0, 0, 4.85, 0.84};
			const Built bf = Build (f);
			Rasterize (innerF, *bf.sheet.mesh, Envelope (), bf.sheet.faceTriangles, 2 * bf.sheet.faceTriangles, false);
		}
		double margin = std::numeric_limits<double>::infinity (), atTheta = 0.0, atH = 0.0;
		unsigned common = 0;
		for (int j = 0; j < RadiusGrid::kNH; ++j)
			for (int i = 0; i < RadiusGrid::kNTheta; ++i)
			{
				const double re = outerE.At (i, j), rf = innerF.At (i, j);
				if (std::isnan (re) || std::isnan (rf))
					continue;
				++common;
				if (rf - be.spec.clearance - re < margin)
				{
					margin = rf - be.spec.clearance - re;
					atTheta = RadiusGrid::kTheta0 + i * RadiusGrid::kDTheta;
					atH = j * RadiusGrid::kDH;
				}
			}
		ASSERT_GT (common, 0u);
		EXPECT_GE (margin, 0.05);
		std::printf ("J21' %s : marge minimale %+.3f p a theta %+.2f deg, h %.2f (%u noeuds communs)\n",
		             level == OrnamentDetail::Light ? "Light" : "Heavy", margin, atTheta, atH, common);
	}
}

// J20 et J20b, instrument sans seuil : sur l'axe, distance de la face exterieure au noyau de la corbeille, et
// epaisseur de la lame, aux hauteurs cotees de la pl. XXV, contre les cotes gravees (registre AC-44).
TEST (TEST_cgarchi_classical_acanthus_sheet, DISABLED_blade_against_plate_XXV)
{
	struct Target
	{
		double h, face;
	};
	const std::vector<Target> targetsF {{1.7, 1.1}, {3.3, 1.1}, {6.6, 1.3}, {9.0, 1.7}};
	const std::vector<Target> targetsE {{12.0, 1.80}, {13.3, 2.08}, {15.0, 2.43}, {17.2, 3.08}, {19.0, 3.74}};
	for (const Case &c : kCases)
	{
		const bool isF = c.kind == CapitalPieceKind::LowerLeaf;
		const Built b = Build (c);
		const std::vector<AxisSample> axis = AxisOf (b);
		std::printf ("%-14s J20b lame gravee %s\n", c.name, isF ? "0,74 a 0,88 p" : "1,06 a 1,45 p");
		for (const Target &t : isF ? targetsF : targetsE)
		{
			const std::array<double, 2> at = AxisAt (axis, t.h);
			const double face = at[0] - Envelope ().CoreRadius (t.h);
			std::printf ("    h %5.2f : J20 face - noyau %.3f p (gravee %.2f, ecart %+.3f) | J20b lame %.3f p\n", t.h, face, t.face,
			             face - t.face, at[1]);
		}
	}
}

// Duree de BuildLeafSheet (gabarit), F et E en Light et Heavy : mediane de 21 constructions.
TEST (TEST_cgarchi_classical_acanthus_sheet, DISABLED_leaf_build_time)
{
	constexpr int kRuns = 21;
	for (const Case &c : kCases)
	{
		const detail::LeafSpec spec = detail::DefaultLeafSpec (c.detail, c.style);
		const detail::LeafPlacement placement = detail::DefaultLeafPlacement (c.kind, c.theta);
		const detail::LeafTemplate &tpl = detail::LeafTemplateOf (c.style, c.detail);
		std::vector<double> ms;
		for (int i = 0; i < kRuns; ++i)
		{
			const auto t0 = std::chrono::steady_clock::now ();
			const detail::LeafSheet sheet = detail::BuildLeafSheet (Envelope (), spec, placement, tpl);
			const auto t1 = std::chrono::steady_clock::now ();
			ASSERT_GT (sheet.mesh->GetNFaces (), 0u);
			ms.push_back (std::chrono::duration<double, std::milli> (t1 - t0).count ());
		}
		std::sort (ms.begin (), ms.end ());
		std::printf ("%-14s BuildLeafSheet : mediane %.3f ms sur %d (min %.3f, max %.3f)\n", c.name, ms[kRuns / 2], kRuns, ms.front (),
		             ms.back ());
	}
}

// Vidage des feuilles plaquees pour les planches : s4_<cas>.mesh (en-tete, sommets en parties depuis le
// pied du chapiteau, triangles et region, -1 pour un mur) et s4_corbeille.csv (h, noyau, cone).
TEST (TEST_cgarchi_classical_acanthus_sheet, DISABLED_dump_sheets)
{
	for (const Case &c : kCases)
	{
		const Built b = Build (c);
		std::string name = std::string ("s4_") + c.name + ".mesh";
		std::replace (name.begin (), name.end (), ' ', '_');
		std::FILE *f = OpenForWriting (name.c_str ());
		ASSERT_NE (f, nullptr);
		const Mesh &mesh = *b.sheet.mesh;
		const double s = Envelope ().scale;
		std::fprintf (f, "%u %u %u %u %.9f\n", mesh.GetNVertices (), mesh.GetNFaces (), b.sheet.faceVertices, b.sheet.faceTriangles,
		              c.theta);
		for (unsigned v = 0; v < mesh.GetNVertices (); ++v)
		{
			const Point p = Vertex (mesh, v);
			std::fprintf (f, "%.6f %.6f %.6f\n", p[0] / s, p[1] / s, p[2] / s - Envelope ().foot);
		}
		for (unsigned k = 0; k < mesh.GetNFaces (); ++k)
			std::fprintf (f, "%d %d %d %d\n", mesh.GetFaceVertex (k, 0), mesh.GetFaceVertex (k, 1), mesh.GetFaceVertex (k, 2),
			              b.sheet.faceRegion[k]);
		std::fclose (f);
	}
	std::FILE *f = OpenForWriting ("s4_corbeille.csv");
	ASSERT_NE (f, nullptr);
	std::fprintf (f, "h,core,cone\n");
	for (int i = 0; i <= 400; ++i)
	{
		const double h = 36.0 * i / 400;
		std::fprintf (f, "%.4f,%.6f,%.6f\n", h, Envelope ().CoreRadius (h), Envelope ().ConeRadius (h));
	}
	std::fclose (f);
}
