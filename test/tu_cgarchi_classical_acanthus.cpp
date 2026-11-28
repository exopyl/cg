#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <cgarchi/classical_acanthus.h>
#include <cgarchi/classical_acanthus_detail.h>
#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_profile.h>
#include <cgarchi/parameterized_classical.h>
#include "../src/cgmesh/mesh.h"

using namespace cgarchi;   // fichier de test de cgarchi seul
using namespace cgarchi::classical;

// ===========================================================================
//  Ornements du chapiteau corinthien : cotes lues, assemblage, instruments
// ===========================================================================

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr const char *kMinorLeaves = "D · foglie minori";
constexpr const char *kAbacus = "B · abaco";
constexpr const char *kAbacusTalon = "A · cimatio dell'abaco (talon)";

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

// `call` leve std::invalid_argument, et son message contient `fragment`.
void ExpectInvalid (const std::function<void ()> &call, const std::string &fragment)
{
	try
	{
		call ();
		ADD_FAILURE () << "aucune exception, attendu : " << fragment;
	}
	catch (const std::invalid_argument &e)
	{
		EXPECT_NE (std::string (e.what ()).find (fragment), std::string::npos) << e.what ();
	}
}

std::size_t MemberIndex (const ColumnProfile &profile, const std::string &name)
{
	for (std::size_t i = 0; i < profile.members.size (); ++i)
		if (profile.members[i].block == "chapiteau" && profile.members[i].name == name)
			return i;
	ADD_FAILURE () << "membre absent : " << name;
	return 0;
}

using RadiusFn = double (*) (const detail::CapitalEnvelope &, double, double);

// Anneaux de `sides` + 1 sommets (sides si la bande fait le tour) aux hauteurs
// `heights` (parties depuis le pied du chapiteau), au rayon radiusAt (h, theta),
// repere du tour a l'echelle env.scale ; faces de la bande laterale entre
// anneaux consecutifs sur [theta0, theta1].
std::unique_ptr<Mesh> SyntheticBand (const detail::CapitalEnvelope &env, const std::vector<double> &heights, int sides,
                                     double theta0, double theta1, RadiusFn radiusAt)
{
	const bool closed = std::fabs (theta1 - theta0 - 2.0 * kPi) < 1e-12;
	const int columns = closed ? sides : sides + 1;
	const unsigned rings = static_cast<unsigned> (heights.size ());
	const unsigned faces = 2u * static_cast<unsigned> (sides) * (rings - 1);
	auto mesh = std::make_unique<Mesh> (rings * static_cast<unsigned> (columns), faces);
	const double s = env.scale;
	for (unsigned i = 0; i < rings; ++i)
		for (int j = 0; j < columns; ++j)
		{
			const double theta = theta0 + (theta1 - theta0) * j / sides;
			const double r = radiusAt (env, heights[i], theta);
			mesh->SetVertex (i * columns + j, static_cast<float> (s * r * std::cos (theta)),
			                 static_cast<float> (s * r * std::sin (theta)), static_cast<float> (s * (env.foot + heights[i])));
		}
	unsigned f = 0;
	for (unsigned i = 0; i + 1 < rings; ++i)
		for (int j = 0; j < sides; ++j)
		{
			const unsigned a = i * columns + j, b = i * columns + (j + 1) % columns;
			const unsigned c = a + columns, d = b + columns;
			mesh->FaceAt (f++)->SetTriangle (a, b, d);
			mesh->FaceAt (f++)->SetTriangle (a, d, c);
		}
	return mesh;
}

CapitalPiece WholePiece (const Mesh &mesh, CapitalPieceKind kind = CapitalPieceKind::LowerLeaf)
{
	return {kind, 0, static_cast<unsigned> (mesh.GetNFaces ()), 0, static_cast<unsigned> (mesh.GetNVertices ())};
}

std::vector<CapitalPiece> WholeMesh (const Mesh &mesh)
{
	return {WholePiece (mesh)};
}

} // namespace

// Cotes lues contre les valeurs du JSON (cinq_ordres.json, membres d'ornement
// du chapiteau corinthien, saillies depuis distance_axe = 15).
TEST (TEST_cgarchi_classical_acanthus, the_envelope_is_read_from_the_capital_members)
{
	const detail::CapitalEnvelope env = detail::ReadCapitalEnvelope (Corinthian (), CorinthianProfile ());
	EXPECT_EQ (env.bands.size (), 6u);   // F tige, F retombee, E tige, E retombee, D, C
	EXPECT_DOUBLE_EQ (env.axisDistance, 15.0);
	EXPECT_NEAR (env.BandBottom (), 0.0, 1e-12);
	EXPECT_NEAR (env.BandTop (), 36.0, 1e-12);

	// Enveloppe aux jonctions des membres : 126/43, 585/86, 348/43, 1029/86,
	// 570/43, 644/43, 792/43.
	const double cone[][2] = {{0.0, 126.0 / 43.0},  {9.0, 585.0 / 86.0},  {12.0, 348.0 / 43.0}, {21.0, 1029.0 / 86.0},
	                          {24.0, 570.0 / 43.0}, {28.0, 644.0 / 43.0}, {36.0, 792.0 / 43.0}};
	for (const auto &c : cone)
		EXPECT_NEAR (env.ConeRadius (c[0]), 15.0 + c[1], 1e-12) << c[0];
	// Noyau : 0 de F a E, 0 -> 1 au rang D, 1 -> 11/2 aux caulicoles C ; un
	// point par bande.
	const double core[][2] = {{4.5, 0.0}, {10.5, 0.0}, {16.5, 0.0}, {22.5, 0.0}, {26.0, 0.5}, {32.0, 3.25}, {36.0, 5.5}};
	for (const auto &c : core)
		EXPECT_NEAR (env.CoreRadius (c[0]), 15.0 + c[1], 1e-12) << c[0];
	EXPECT_NEAR (env.CoreRadius (36.0), 20.5, 1e-12);

	// Regle de Vignole : 15 + 2,5 + (37/86) (h + 1), du pied du chapiteau au
	// haut des caulicoles.
	double worst = 0.0;
	for (int k = 0; k < 100; ++k)
	{
		const double h = 36.0 * k / 99.0;
		const double rule = 15.0 + 2.5 + (37.0 / 86.0) * (h + 1.0);
		worst = std::max (worst, std::fabs (env.ConeRadius (h) - rule));
		EXPECT_NEAR (env.ConeRadius (h), rule, 1e-12) << h;
	}
	std::cout << "[ mesure ] cone lu contre la regle : ecart maximal " << worst << " p sur 100 hauteurs\n";
}

// Borne = max (noyau, min (cone, abaque)) : abaque B (cornes 32, faces 13/5
// depuis 15), noyau la ou la levre de la corbeille passe la face de l'abaque.
TEST (TEST_cgarchi_classical_acanthus, the_envelope_is_the_abacus_raised_to_the_bell)
{
	using Env = detail::CapitalEnvelope;
	const Env env = detail::ReadCapitalEnvelope (Corinthian (), CorinthianProfile ());
	ASSERT_EQ (env.abacus.shape, RingShape::ConcaveSquare);
	EXPECT_NEAR (env.abacus.RadiusAt (kPi / 4.0), 15.0 + 17.0, 1e-12);       // corne
	EXPECT_NEAR (env.abacus.RadiusAt (0.0), 15.0 + 13.0 / 5.0, 1e-12);       // milieu de face

	EXPECT_NEAR (env.EnvelopeRadius (36.0, 0.0), 20.5, 1e-12);                // noyau
	EXPECT_NEAR (env.EnvelopeRadius (36.0, kPi / 4.0), 32.0, 1e-12);          // min (cone 33,42, 32)
	EXPECT_NEAR (env.EnvelopeRadius (9.0, kPi / 4.0), 15.0 + 585.0 / 86.0, 1e-12);   // min (cone, 32)
	EXPECT_NEAR (env.EnvelopeRadius (9.0, 0.0), 17.6, 1e-12);                 // face de l'abaque
	EXPECT_NEAR (env.EnvelopeRadius (0.0, 0.0), 17.6, 1e-12);

	// Hauteur, au pas de 0,01 p, des laquelle min (cone, abaque) passe sous le
	// noyau dans la direction theta : le noyau y releve la borne.
	auto crossing = [&env] (double theta) {
		for (int k = 0; k <= 3600; ++k)
		{
			const double h = k / 100.0;
			if (std::min (env.ConeRadius (h), env.abacus.RadiusAt (theta)) < env.CoreRadius (h))
				return h;
		}
		return std::numeric_limits<double>::quiet_NaN ();
	};
	EXPECT_NEAR (crossing (0.0), 30.85, 0.005);
	EXPECT_NEAR (crossing (kPi / 8.0), 34.45, 0.005);
	EXPECT_TRUE (std::isnan (crossing (kPi / 4.0)));
	std::cout << "[ mesure ] abaque : " << env.abacus.RadiusAt (0.0) << " p (face), " << env.abacus.RadiusAt (kPi / 8.0)
	          << " p (22,5 deg), " << env.abacus.RadiusAt (kPi / 4.0) << " p (corne) ; noyau a h = 36 : "
	          << env.CoreRadius (36.0) << " p ; noyau au-dessus de l'abaque des h = " << crossing (0.0) << " p (face), "
	          << crossing (kPi / 8.0) << " p (22,5 deg)\n";

	// Sur l'axe de face, au-dessus de 30,85 : une surface a noyau + 0,1 sort de
	// la borne, une surface au noyau n'en sort pas.
	const std::vector<double> top {31.0, 32.0, 33.0, 34.0, 35.0, 36.0};
	const auto above = SyntheticBand (env, top, 8, -0.04, 0.04, [] (const Env &e, double h, double) { return e.CoreRadius (h) + 0.1; });
	const auto onCore = SyntheticBand (env, top, 8, -0.04, 0.04, [] (const Env &e, double h, double) { return e.CoreRadius (h); });
	EXPECT_NEAR (detail::EnvelopeExcess (*above, env), 0.1, 1e-4);
	EXPECT_LE (detail::EnvelopeExcess (*onCore, env), 1e-5);
	EXPECT_GE (detail::EnvelopeExcess (*onCore, env), -1e-5);

	// Le fleuron, pose sur l'abaque, est hors du controle.
	const auto cone = SyntheticBand (env, top, 8, -0.04, 0.04, [] (const Env &e, double h, double) { return e.ConeRadius (h); });
	EXPECT_GT (detail::EnvelopeExcess (*cone, env, WholePiece (*cone, CapitalPieceKind::Cauliculus)), 10.0);
	EXPECT_EQ (detail::EnvelopeExcess (*cone, env, WholePiece (*cone, CapitalPieceKind::Fleuron)),
	           -std::numeric_limits<double>::infinity ());

	// Borne par rang (registre AC-48 et AC-2) : a 22,5 degres, une bande sous le cone de 0,1 p est dans la
	// borne de F, de E et de D (cone seul) et sort de celle de C (abaque vers 19,6 p, sous le cone des que h > 4,2).
	const std::vector<double> low {0.0, 3.0, 6.0, 9.0, 12.0};
	constexpr int kSides = 8;
	const double t0 = kPi / 8.0 - 0.04, t1 = kPi / 8.0 + 0.04;
	const auto underCone = SyntheticBand (env, low, kSides, t0, t1, [] (const Env &e, double h, double) { return e.ConeRadius (h) - 0.1; });
	EXPECT_NEAR (detail::EnvelopeExcess (*underCone, env, WholePiece (*underCone, CapitalPieceKind::LowerLeaf)), -0.1, 1e-4);
	EXPECT_NEAR (detail::EnvelopeExcess (*underCone, env, WholePiece (*underCone, CapitalPieceKind::MiddleLeaf)), -0.1, 1e-4);
	double abacusLow = std::numeric_limits<double>::infinity ();
	for (int j = 0; j <= kSides; ++j)
		abacusLow = std::min (abacusLow, env.abacus.RadiusAt (t0 + (t1 - t0) * j / kSides));
	EXPECT_NEAR (detail::EnvelopeExcess (*underCone, env, WholePiece (*underCone, CapitalPieceKind::MinorLeaf)), -0.1, 1e-4);
	const double minor = detail::EnvelopeExcess (*underCone, env, WholePiece (*underCone, CapitalPieceKind::Cauliculus));
	EXPECT_NEAR (minor, env.ConeRadius (12.0) - 0.1 - abacusLow, 1e-4);
	EXPECT_GT (minor, 3.0);
	EXPECT_NEAR (detail::EnvelopeExcess (*underCone, env), minor, 1e-12);
}

// Une garde par refus de ReadCapitalEnvelope et de BuildCorinthianOrnament,
// chacune vue par un fragment de son message.
TEST (TEST_cgarchi_classical_acanthus, the_capital_envelope_rejects_what_it_cannot_read)
{
	const ColumnProfile profile = CorinthianProfile ();
	ExpectInvalid ([&] { detail::ReadCapitalEnvelope (Corinthian (), profile, 0.0); }, "scale fini et > 0");
	ExpectInvalid ([&] { detail::ReadCapitalEnvelope (Corinthian (), profile, std::numeric_limits<double>::infinity ()); },
	               "scale fini et > 0");

	const OrderData &tuscan = ClassicalOrders ().Get (Order::Tuscan);
	ExpectInvalid ([&] { detail::ReadCapitalEnvelope (tuscan, BuildColumnProfile (tuscan, ColumnOptions {})); },
	               "aucun membre d'ornement");

	ColumnProfile missing = profile;
	missing.members.erase (missing.members.begin () + static_cast<std::ptrdiff_t> (MemberIndex (profile, kMinorLeaves)));
	ExpectInvalid ([&] { detail::ReadCapitalEnvelope (Corinthian (), missing); },
	               std::string ("ReadCapitalEnvelope : membre absent du profil : ") + kMinorLeaves);

	ColumnProfile twice = profile;
	twice.members.push_back (profile.members[MemberIndex (profile, kMinorLeaves)]);
	ExpectInvalid ([&] { detail::ReadCapitalEnvelope (Corinthian (), twice); },
	               std::string ("membre en double dans le profil : ") + kMinorLeaves);

	ColumnProfile flat = profile;
	flat.members[MemberIndex (profile, kMinorLeaves)].y1 = flat.members[MemberIndex (profile, kMinorLeaves)].y0;
	ExpectInvalid ([&] { detail::ReadCapitalEnvelope (Corinthian (), flat); },
	               std::string ("membre de hauteur nulle : ") + kMinorLeaves);

	ColumnProfile gap = profile;
	gap.members[MemberIndex (profile, kMinorLeaves)].y0 += 0.5;
	ExpectInvalid ([&] { detail::ReadCapitalEnvelope (Corinthian (), gap); }, "membres d'ornement non contigus");

	ColumnProfile noAbacus = profile;
	noAbacus.members.erase (noAbacus.members.begin () + static_cast<std::ptrdiff_t> (MemberIndex (profile, kAbacus)));
	ExpectInvalid ([&] { detail::ReadCapitalEnvelope (Corinthian (), noAbacus); },
	               std::string ("membre absent du profil : ") + kAbacus);

	const int abacusIndex = static_cast<int> (MemberIndex (profile, kAbacus));
	const auto abacusVertex = std::find_if (profile.vertices.begin (), profile.vertices.end (),
	                                        [&] (const ProfileVertex &v) { return v.member == abacusIndex; });
	ASSERT_NE (abacusVertex, profile.vertices.end ());
	ColumnProfile varying = profile;
	ProfileVertex other = *abacusVertex;
	other.ring.radius += 1.0;
	varying.vertices.insert (varying.vertices.begin () + (abacusVertex - profile.vertices.begin ()) + 1, other);
	ExpectInvalid ([&] { detail::ReadCapitalEnvelope (Corinthian (), varying); }, "anneau de l'abaque variable");

	ColumnProfile bare = profile;
	for (ProfileVertex &v : bare.vertices)
		if (v.member == abacusIndex)
			v.member = -1;
	ExpectInvalid ([&] { detail::ReadCapitalEnvelope (Corinthian (), bare); }, "abaque sans sommet");

	ColumnProfile noTalon = profile;
	noTalon.members.erase (noTalon.members.begin () + static_cast<std::ptrdiff_t> (MemberIndex (profile, kAbacusTalon)));
	ExpectInvalid ([&] { detail::ReadCapitalEnvelope (Corinthian (), noTalon); },
	               std::string ("membre absent du profil : ") + kAbacusTalon);

	const int talonIndex = static_cast<int> (MemberIndex (profile, kAbacusTalon));
	ColumnProfile bareTalon = profile;
	for (ProfileVertex &v : bareTalon.vertices)
		if (v.member == talonIndex)
			v.member = -1;
	ExpectInvalid ([&] { detail::ReadCapitalEnvelope (Corinthian (), bareTalon); }, "talon de l'abaque sans sommet");

	// BuildCorinthianOrnament : profil valide meme a None, bornes des options.
	AcanthusOptions none;
	none.detail = OrnamentDetail::None;
	ExpectInvalid ([&] { BuildCorinthianOrnament (Corinthian (), missing, none); }, "membre absent du profil");
	AcanthusOptions bad;
	bad.detail = static_cast<OrnamentDetail> (7);
	ExpectInvalid ([&] { BuildCorinthianOrnament (Corinthian (), profile, bad); }, "detail hors de l'enumeration");
	bad = AcanthusOptions {};
	bad.scale = 0.0;
	ExpectInvalid ([&] { BuildCorinthianOrnament (Corinthian (), profile, bad); }, "options invalides");
	bad.scale = std::numeric_limits<double>::infinity ();
	ExpectInvalid ([&] { BuildCorinthianOrnament (Corinthian (), profile, bad); }, "scale non fini ou <= 0");
	ExpectInvalid ([&] { detail::RankTheta (CapitalPieceKind::MinorLeaf, 0); }, "F, E ou tige de caulicole attendu");
	ExpectInvalid ([&] { detail::RankTheta (CapitalPieceKind::LowerLeaf, detail::kLeavesPerRank); }, "hors de [0, 8)");
	ExpectInvalid ([&] { detail::RankTheta (CapitalPieceKind::MiddleLeaf, -1); }, "k -1");
	const OrderData &ionic = ClassicalOrders ().Get (Order::Ionic);
	ColumnOptions ionicOptions;
	ionicOptions.order = Order::Ionic;
	ExpectInvalid ([&] { BuildCorinthianOrnament (ionic, BuildColumnProfile (ionic, ionicOptions)); },
	               "ordre corinthien requis");
}

// K1, K15, K45 : 8 F, 8 E, 8 tiges C, 16 D, 8 V, 8 H puis 4 G (registre AC-46) ; F, E, C ou D : une coque ; V ou H : deux
// (ruban, oeil) ; G : deux (rosace, tige) ; comptes de la feuille (AC-29),
// de la tige (2 n_phi n_z triangles, n_phi n_z + 2 sommets : 24 x 15 en Light, 48 x 32 en Heavy) et de la feuille D
// (gabarit OliveMinor sans bande : 1 300 / 2 996 triangles), pieces contigues.
TEST (TEST_cgarchi_classical_acanthus, the_ornament_is_one_mesh_tiled_by_its_pieces)
{
	struct Counts
	{
		OrnamentDetail detail;
		unsigned faces, vertices;       // ornement
		unsigned fFaces, fVertices;     // coque F
		unsigned eFaces, eVertices;     // coque E (feuille et bande)
		unsigned cFaces, cVertices;     // tige C
		unsigned dFaces, dVertices;     // feuille D
		unsigned vFaces, vVertices;     // volute (ruban et oeil)
		unsigned hFaces, hVertices;     // helice (ruban et oeil)
		unsigned gFaces, gVertices;     // fleuron (rosace et tige)
	};
	const Counts cases[] = {{OrnamentDetail::Light, 79232, 39776, 1800, 902, 1912, 958, 720, 362, 1300, 652, 904, 456, 886, 447, 2164, 1086},
	                        {OrnamentDetail::Heavy, 329824, 165072, 10296, 5150, 11016, 5510, 3072, 1538, 2996, 1500, 3600, 1804, 3510, 1759, 7484, 3746}};
	for (const Counts &c : cases)
	{
		SCOPED_TRACE (c.detail == OrnamentDetail::Light ? "Light" : "Heavy");
		AcanthusOptions options;
		options.detail = c.detail;
		const CapitalOrnament ornament = BuildCorinthianOrnament (Corinthian (), CorinthianProfile (), options);
		ASSERT_NE (ornament.mesh, nullptr);
		const Mesh &mesh = *ornament.mesh;
		EXPECT_EQ (mesh.GetNFaces (), static_cast<int> (c.faces));
		EXPECT_EQ (mesh.GetNVertices (), static_cast<int> (c.vertices));
		ASSERT_EQ (ornament.pieces.size (), 60u);

		unsigned face = 0, vertex = 0;
		for (std::size_t i = 0; i < ornament.pieces.size (); ++i)
		{
			const CapitalPiece &piece = ornament.pieces[i];
			const std::size_t rank = i < 24u ? i / 8u : (i < 40u ? 3u : (i < 48u ? 4u : (i < 56u ? 5u : 6u)));
			const CapitalPieceKind kinds[] = {CapitalPieceKind::LowerLeaf, CapitalPieceKind::MiddleLeaf, CapitalPieceKind::Cauliculus,
			                                  CapitalPieceKind::MinorLeaf, CapitalPieceKind::Volute,    CapitalPieceKind::Helix,
			                                  CapitalPieceKind::Fleuron};
			const unsigned faces[] = {c.fFaces, c.eFaces, c.cFaces, c.dFaces, c.vFaces, c.hFaces, c.gFaces},
			               vertices[] = {c.fVertices, c.eVertices, c.cVertices, c.dVertices, c.vVertices, c.hVertices, c.gVertices};
			EXPECT_EQ (piece.kind, kinds[rank]) << i;
			EXPECT_EQ (piece.faceBegin, face);
			EXPECT_EQ (piece.vertexBegin, vertex);
			EXPECT_EQ (piece.faceEnd - piece.faceBegin, faces[rank]) << i;
			EXPECT_EQ (piece.vertexEnd - piece.vertexBegin, vertices[rank]) << i;
			for (unsigned f = piece.faceBegin; f < piece.faceEnd; ++f)
				for (unsigned k = 0; k < 3; ++k)
				{
					const unsigned v = static_cast<unsigned> (mesh.GetFaceVertex (f, k));
					ASSERT_TRUE (piece.vertexBegin <= v && v < piece.vertexEnd) << f;
				}
			face = piece.faceEnd;
			vertex = piece.vertexEnd;
		}
		EXPECT_EQ (face, static_cast<unsigned> (mesh.GetNFaces ()));
		EXPECT_EQ (vertex, static_cast<unsigned> (mesh.GetNVertices ()));
	}

	AcanthusOptions none;
	none.detail = OrnamentDetail::None;
	const CapitalOrnament empty = BuildCorinthianOrnament (Corinthian (), CorinthianProfile (), none);
	ASSERT_NE (empty.mesh, nullptr);
	EXPECT_EQ (empty.mesh->GetNFaces (), 0);
	EXPECT_TRUE (empty.pieces.empty ());
}

// « Capital ornaments » choisit le niveau, « Volutes » decoche coupe tout.
TEST (TEST_cgarchi_classical_acanthus, capital_ornaments_and_volutes_carry_the_corinthian_ornament)
{
	ClassicalColumnSettings s;
	s.order = static_cast<int> (Order::Corinthian);
	EXPECT_EQ (s.capitalOrnaments, static_cast<int> (OrnamentDetail::None));
	const std::unique_ptr<Mesh> byDefault = BuildClassicalColumnMesh (s);
	EXPECT_EQ (byDefault->GetNFaces (), 13648);
	EXPECT_EQ (byDefault->GetNVertices (), 6826);

	struct Expected
	{
		OrnamentDetail detail;
		int faces, vertices;   // ornement (K1)
	};
	for (const Expected &e : {Expected {OrnamentDetail::Light, 79232, 39776}, Expected {OrnamentDetail::Heavy, 329824, 165072}})
	{
		s.capitalOrnaments = static_cast<int> (e.detail);
		s.volutes = true;
		const std::unique_ptr<Mesh> with = BuildClassicalColumnMesh (s);
		EXPECT_EQ (with->GetNFaces (), 13648 + e.faces);
		EXPECT_EQ (with->GetNVertices (), 6826 + e.vertices);
		s.volutes = false;
		EXPECT_EQ (BuildClassicalColumnMesh (s)->GetNFaces (), 13648);
	}
	// K14 : le defaut Light est porte par les formes neuves ; la structure reste a None.
	EXPECT_EQ (ClassicalColumnSettings ().capitalOrnaments, static_cast<int> (OrnamentDetail::None));
	ParameterizedClassicalColumn column;
	EXPECT_EQ (column.Settings ().capitalOrnaments, static_cast<int> (OrnamentDetail::Light));
	ParameterizedClassicalColonnade colonnade;
	int colonnadeOrnaments = -1;
	for (const Parameter &p : colonnade.GetParameters ())
		if (p.GetName () == "Capital ornaments")
			colonnadeOrnaments = p.GetInt ();
	EXPECT_EQ (colonnadeOrnaments, static_cast<int> (OrnamentDetail::Light));

	s.capitalOrnaments = kCapitalOrnamentsCount;
	ExpectInvalid ([&] { BuildClassicalColumnMesh (s); }, "capitalOrnaments 3");
	s.capitalOrnaments = -1;
	ExpectInvalid ([&] { BuildClassicalColumnMesh (s); }, "capitalOrnaments -1");
}

// Instruments sur des surfaces de rayon connu : chacun vu vert et vu rouge.
TEST (TEST_cgarchi_classical_acanthus, the_instruments_read_known_surfaces)
{
	using Env = detail::CapitalEnvelope;
	const Env env = detail::ReadCapitalEnvelope (Corinthian (), CorinthianProfile ());
	const std::vector<double> heights {0.0, 4.5, 9.0, 12.0, 21.0, 24.0, 28.0, 32.0, 36.0};

	const auto offCore = SyntheticBand (env, heights, 64, 0.0, 2.0 * kPi,
	                                    [] (const Env &e, double h, double) { return e.CoreRadius (h) + 0.25; });
	const auto inCore = SyntheticBand (env, heights, 64, 0.0, 2.0 * kPi,
	                                   [] (const Env &e, double h, double) { return e.CoreRadius (h) - 0.5; });
	EXPECT_NEAR (detail::CoreClearance (*offCore, env), 0.25, 1e-4);
	EXPECT_NEAR (detail::CoreClearance (*inCore, env), -0.5, 1e-4);

	const auto inside = SyntheticBand (env, heights, 64, 0.0, 2.0 * kPi,
	                                   [] (const Env &e, double h, double t) { return e.EnvelopeRadius (h, t) - 0.1; });
	const auto cone = SyntheticBand (env, heights, 64, 0.0, 2.0 * kPi,
	                                 [] (const Env &e, double h, double) { return e.ConeRadius (h); });
	EXPECT_NEAR (detail::EnvelopeExcess (*inside, env), -0.1, 1e-4);
	// Le cone de revolution sort de la borne, au plus sur l'axe de face (colonne
	// theta = 0 de la bande).
	double coneExcess = -std::numeric_limits<double>::infinity ();
	for (const double h : heights)
		coneExcess = std::max (coneExcess, env.ConeRadius (h) - env.EnvelopeRadius (h, 0.0));
	EXPECT_NEAR (coneExcess, env.ConeRadius (32.0) - env.CoreRadius (32.0), 1e-12);   // h = 32 : noyau 18,25
	EXPECT_NEAR (detail::EnvelopeExcess (*cone, env), coneExcess, 1e-4);

	EXPECT_EQ (detail::CoreClearance (*offCore, env, 3, 3), std::numeric_limits<double>::infinity ());
	EXPECT_EQ (detail::EnvelopeExcess (*offCore, env, 3, 3), -std::numeric_limits<double>::infinity ());
	ExpectInvalid ([&] { detail::CoreClearance (*offCore, env, 0, offCore->GetNVertices () + 1); }, "plage de sommets");

	// Echelle : memes surfaces a 0,01 unite par partie, et l'ornement a 2.
	const Env small = detail::ReadCapitalEnvelope (Corinthian (), CorinthianProfile (), 0.01);
	const auto smallOffCore = SyntheticBand (small, heights, 64, 0.0, 2.0 * kPi,
	                                         [] (const Env &e, double h, double) { return e.CoreRadius (h) + 0.25; });
	EXPECT_NEAR (detail::CoreClearance (*smallOffCore, small), 0.25, 1e-4);
	AcanthusOptions doubled;
	doubled.scale = 2.0;
	const CapitalOrnament big = BuildCorinthianOrnament (Corinthian (), CorinthianProfile (), doubled);
	const Env bigEnv = detail::ReadCapitalEnvelope (Corinthian (), CorinthianProfile (), 2.0);
	// jeu c des feuilles (registre AC-12) ; les fleurons, poses sur l'abaque, sont bornes par le tour (K49')
	ASSERT_EQ (big.pieces.back ().kind, CapitalPieceKind::Fleuron);
	EXPECT_NEAR (detail::CoreClearance (*big.mesh, bigEnv, 0u, big.pieces[big.pieces.size () - static_cast<std::size_t> (detail::kFleurons)].vertexBegin), 0.05, 1e-4);

	const auto full = SyntheticBand (env, {1.0, 23.0}, 90, -kPi, kPi,
	                                 [] (const Env &e, double h, double) { return e.CoreRadius (h) + 1.0; });
	const auto half = SyntheticBand (env, {1.0, 23.0}, 45, 0.0, kPi,
	                                 [] (const Env &e, double h, double) { return e.CoreRadius (h) + 1.0; });
	const auto lower = SyntheticBand (env, {1.0, 12.0}, 90, -kPi, kPi,
	                                  [] (const Env &e, double h, double) { return e.CoreRadius (h) + 1.0; });
	// A cheval sur la couture theta = pi : faces deroulees d'un seul tenant.
	const auto seam = SyntheticBand (env, {1.0, 23.0}, 45, 0.75 * kPi, 1.25 * kPi,
	                                 [] (const Env &e, double h, double) { return e.CoreRadius (h) + 1.0; });
	EXPECT_DOUBLE_EQ (detail::BellCoverage (*full, WholeMesh (*full), env, 1.0, 23.0), 1.0);
	EXPECT_DOUBLE_EQ (detail::BellCoverage (*half, WholeMesh (*half), env, 1.0, 23.0), 0.5);
	EXPECT_DOUBLE_EQ (detail::BellCoverage (*lower, WholeMesh (*lower), env, 1.0, 23.0), 0.5);
	EXPECT_DOUBLE_EQ (detail::BellCoverage (*seam, WholeMesh (*seam), env, 1.0, 23.0), 0.25);
	EXPECT_DOUBLE_EQ (detail::BellCoverage (*full, {}, env, 1.0, 23.0), 0.0);
	ExpectInvalid ([&] { detail::BellCoverage (*full, WholeMesh (*full), env, 23.0, 1.0); }, "bande vide");
	CapitalPiece beyond = WholePiece (*full);
	++beyond.faceEnd;
	ExpectInvalid ([&] { detail::BellCoverage (*full, {beyond}, env, 1.0, 23.0); }, "plage de faces");
	Mesh quad (4, 1);
	quad.FaceAt (0)->SetQuad (0, 1, 2, 3);
	ExpectInvalid ([&] { detail::BellCoverage (quad, WholeMesh (quad), env, 1.0, 23.0); }, "non triangulaire");

	// VisibleBellFraction (K24') : elevation de face, |y| <= 10 p, h dans [1, 23]. La bande pleine cache toute la
	// fenetre ; la demi-bande y >= 0 en cache la moitie ; une bande derriere la corbeille (x < 0) ne cache rien ;
	// une bande sous la corbeille (rayon noyau - 0,5) non plus.
	const auto back = SyntheticBand (env, {1.0, 23.0}, 45, 0.5 * kPi + 0.01, 1.5 * kPi - 0.01,
	                                 [] (const Env &e, double h, double) { return e.CoreRadius (h) + 1.0; });
	const auto under = SyntheticBand (env, {1.0, 23.0}, 90, -kPi, kPi,
	                                  [] (const Env &e, double h, double) { return e.CoreRadius (h) - 0.5; });
	EXPECT_DOUBLE_EQ (detail::VisibleBellFraction (*full, WholeMesh (*full), env, 10.0, 1.0, 23.0), 0.0);
	EXPECT_NEAR (detail::VisibleBellFraction (*half, WholeMesh (*half), env, 10.0, 1.0, 23.0), 0.5, 0.0025);
	EXPECT_DOUBLE_EQ (detail::VisibleBellFraction (*back, WholeMesh (*back), env, 10.0, 1.0, 23.0), 1.0);
	EXPECT_DOUBLE_EQ (detail::VisibleBellFraction (*under, WholeMesh (*under), env, 10.0, 1.0, 23.0), 1.0);
	EXPECT_DOUBLE_EQ (detail::VisibleBellFraction (*full, {}, env, 10.0, 1.0, 23.0), 1.0);
	EXPECT_NEAR (detail::VisibleBellFraction (*lower, WholeMesh (*lower), env, 10.0, 1.0, 23.0), 0.5, 0.0025);
	ExpectInvalid ([&] { detail::VisibleBellFraction (*full, WholeMesh (*full), env, 10.0, 23.0, 1.0); }, "fenetre vide");
	ExpectInvalid ([&] { detail::VisibleBellFraction (*full, WholeMesh (*full), env, 10.0, 1.0, 23.0, 0.0); }, "cellule");
	ExpectInvalid ([&] { detail::VisibleBellFraction (*full, WholeMesh (*full), env, 15.0, 1.0, 23.0); }, "plus large que la corbeille");
	ExpectInvalid ([&] { detail::VisibleBellFraction (*full, {beyond}, env, 10.0, 1.0, 23.0); }, "plage de faces");
	ExpectInvalid ([&] { detail::VisibleBellFraction (quad, WholeMesh (quad), env, 10.0, 1.0, 23.0); }, "non triangulaire");
}

// Instrument de duree, hors suite : --gtest_also_run_disabled_tests. Mediane et
// minimum de BuildClassicalColumnMesh corinthien, ornements Light.
TEST (TEST_cgarchi_classical_acanthus, DISABLED_corinthian_column_build_time)
{
	ClassicalColumnSettings s;
	s.order = static_cast<int> (Order::Corinthian);
	s.capitalOrnaments = static_cast<int> (OrnamentDetail::Light);
	constexpr int kRuns = 21;
	std::vector<double> ms;
	ms.reserve (kRuns);
	unsigned faces = BuildClassicalColumnMesh (s)->GetNFaces ();
	for (int i = 0; i < kRuns; ++i)
	{
		const auto t0 = std::chrono::steady_clock::now ();
		const std::unique_ptr<Mesh> m = BuildClassicalColumnMesh (s);
		const auto t1 = std::chrono::steady_clock::now ();
		faces = m->GetNFaces ();
		ms.push_back (std::chrono::duration<double, std::milli> (t1 - t0).count ());
	}
	std::sort (ms.begin (), ms.end ());
	std::printf ("BuildClassicalColumnMesh corinthien : %u faces, mediane %.3f ms, min %.3f ms (%d mesures)\n", faces,
	             ms[kRuns / 2], ms.front (), kRuns);
	EXPECT_GT (faces, 0u);
}
