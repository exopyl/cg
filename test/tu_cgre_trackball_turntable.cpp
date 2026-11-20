//
// Rotation en platine de Ctrackball (RotationMode::Turntable). Le mode par
// defaut est Trackball : chaque test de la platine la pose explicitement.
//
// Proprietes testees :
//   le mode par defaut est Trackball, et ResetTransformations ne change pas le
//   mode ;
//   T2.1 l'horizon reste droit apres chaque evenement, poles atteints ;
//   T2.2 revenir au pixel d'enfoncement restitue la vue de depart au bit pres,
//        roulis compris ;
//   T2.3 signes et pas : kRotationRadiansPerSide / min(w, h) rad par pixel,
//        l'azimut decroit vers la droite, l'elevation croit vers le bas ;
//   T2.4 la vue en plan exacte est atteignable, et un pixel de retour en
//        redescend ;
//   T2.5 sans centre de rotation, le pivot n'est pas touche ;
//   T2.6 straighten_horizon retire le roulis en gardant direction de vue,
//        pivot et distance ;
//   T2.7 le mode trackball garde sa loi ;
//   une ecriture exterieure en plein glisser (mode, orientation, pivot) n'est
//   ni ecrasee par l'ancrage ni suivie d'un saut.
//
// Aucun contexte OpenGL : les matrices sont calculees par la camera. Les
// comparaisons au bit pres portent sur des instantanes relus par les
// accesseurs dans la meme execution ; toute attente recalculee dans ce fichier
// se compare avec une tolerance.
//

#ifdef CG_HAS_CGRE

#include <gtest/gtest.h>

#include "../src/cgre/examinator_trackball.h"
#include "../src/cgmath/turntable.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>

namespace
{

constexpr int    kWidth  = 1000;
constexpr int    kHeight = 600;   // min(w, h) = 600 : un demi-cote vaut 300 px
constexpr double kPi     = 3.14159265358979323846;
constexpr double kDeg    = kPi / 180.0;

constexpr float kPivot[3]   = { 1234.567f, -890.123f, 5447.66f };
constexpr float kRadius     = 20000.f;
constexpr float kDistance   = 5947.66f;

using Mode = Ctrackball::RotationMode;

// Camera cadree sur kPivot, orientation par defaut (-30 deg, 20 deg), mode par
// defaut.
void SetupFramed (Ctrackball& tb)
{
	tb.set_dimensions (kWidth, kHeight);
	tb.set_scene_spheres (kPivot, kRadius, kRadius);
	tb.ResetTransformations ();
	tb.set_pivot (kPivot[0], kPivot[1], kPivot[2]);
	tb.set_zoom (-kDistance);
}

// SetupFramed, puis mode platine. L'orientation par defaut est sans roulis :
// le redressement du passage en platine la laisse telle quelle.
void SetupFramedTurntable (Ctrackball& tb)
{
	SetupFramed (tb);
	tb.set_rotation_mode (Mode::Turntable);
}

const TMatrix4<float>& Rotation (const Ctrackball& tb)
{
	return tb.camera ().GetRotation ();
}

double Wrap (double a)
{
	return std::remainder (a, 2.0 * kPi);
}

// Platine avec roulis, calculee en double puis stockee en float.
TMatrix4<float> RolledTurntable (double az, double el, double rho)
{
	const double ca = std::cos (az), sa = std::sin (az);
	double ce, se;
	if (el >= kPi / 2)       { ce = 0.0; se = 1.0; }
	else if (el <= -kPi / 2) { ce = 0.0; se = -1.0; }
	else                     { ce = std::cos (el); se = std::sin (el); }
	const double r0[3] = { -sa, ca, 0.0 };
	const double r1[3] = { -se * ca, -se * sa, ce };
	const double r3[3] = { ce * ca, ce * sa, se };
	const double c = std::cos (rho), s = std::sin (rho);

	TMatrix4<float> r;
	for (int i = 0; i < 3; ++i)
	{
		r.at (0, i) = (float)(c * r0[i] + s * r1[i]);
		r.at (1, i) = (float)(-s * r0[i] + c * r1[i]);
		r.at (2, i) = (float)r3[i];
	}
	return r;
}

// Angle signe de la ligne 0 de `before` vers celle de `after`, autour de la
// direction de vue de `before` : rotation de l'image autour de son centre.
double ImageRotation (const TMatrix4<float>& before, const TMatrix4<float>& after)
{
	double r0[3], s0[3], r3[3];
	for (int i = 0; i < 3; ++i)
	{
		r0[i] = before.at (0, i);
		s0[i] = after.at (0, i);
		r3[i] = before.at (2, i);
	}
	const double c[3] = { r0[1] * s0[2] - r0[2] * s0[1],
	                      r0[2] * s0[0] - r0[0] * s0[2],
	                      r0[0] * s0[1] - r0[1] * s0[0] };
	return std::atan2 (c[0] * r3[0] + c[1] * r3[1] + c[2] * r3[2],
	                   r0[0] * s0[0] + r0[1] * s0[1] + r0[2] * s0[2]);
}

// Glisser gauche en ligne droite, `steps` evenements.
void Drag (Ctrackball& tb, int x0, int y0, int dx, int dy, int steps)
{
	tb.mouse_press (LEFT_BUTTON, PRESSED, x0, y0);
	for (int i = 1; i <= steps; ++i)
		tb.mouse_move (x0 + dx * i / steps, y0 + dy * i / steps);
	tb.mouse_press (LEFT_BUTTON, RELEASED, x0 + dx, y0 + dy);
}

// Orientation avec roulis produite par le mode trackball : deux glissers
// orthogonaux depuis l'orientation par defaut.
TMatrix4<float> TrackballRolledRotation ()
{
	Ctrackball tb;
	SetupFramed (tb);
	tb.set_rotation_mode (Mode::Trackball);
	Drag (tb, kWidth / 2, kHeight / 2, 200, 0, 20);
	Drag (tb, kWidth / 2, kHeight / 2, 0, 200, 20);
	return Rotation (tb);
}

// Orientation avec roulis produite par le mode trackball PRES DU POLE : les
// deux glissers de TrackballRolledRotation, puis un glisser depuis le centre le
// long de la projection ecran de -Z (le haut du monde vient vers soi), pas a
// pas, jusqu'a une elevation d'au moins 89,7 deg. Les produits de Rodrigues et
// la reorthonormalisation y laissent leur erreur absolue, que le pole amplifie
// (reserve R-bruit).
TMatrix4<float> TrackballRolledNearPole ()
{
	Ctrackball tb;
	SetupFramed (tb);
	tb.set_rotation_mode (Mode::Trackball);
	Drag (tb, kWidth / 2, kHeight / 2, 200, 0, 20);
	Drag (tb, kWidth / 2, kHeight / 2, 0, 200, 20);

	// +Z du monde a l'ecran (x vers la droite, y vers le HAUT).
	const double ux = Rotation (tb).at (0, 2), uy = Rotation (tb).at (1, 2);
	const double n = std::hypot (ux, uy);
	const int x0 = kWidth / 2, y0 = kHeight / 2;
	int x = x0, y = y0;
	tb.mouse_press (LEFT_BUTTON, PRESSED, x0, y0);
	for (int i = 1; i <= 2000 && TurntableFromRotation (Rotation (tb)).elevation < 89.7 * kDeg; ++i)
	{
		x = x0 - (int)std::lround (0.25 * i * ux / n);
		y = y0 + (int)std::lround (0.25 * i * uy / n);
		tb.mouse_move (x, y);
	}
	tb.mouse_press (LEFT_BUTTON, RELEASED, x, y);
	return Rotation (tb);
}

} // namespace

// Le mode par defaut est Trackball, a la construction comme apres
// ResetTransformations, qui repose l'orientation et non le mode.
TEST (CgreTrackballTurntable, DefaultModeIsTrackball)
{
	Ctrackball tb;
	EXPECT_EQ (tb.get_rotation_mode (), Mode::Trackball) << "construction";
	tb.ResetTransformations ();
	EXPECT_EQ (tb.get_rotation_mode (), Mode::Trackball) << "ResetTransformations";
}

// ResetTransformations garde le mode choisi.
TEST (CgreTrackballTurntable, ResetTransformationsKeepsChosenMode)
{
	for (const Mode chosen : { Mode::Turntable, Mode::Trackball })
	{
		Ctrackball tb;
		tb.set_rotation_mode (chosen);
		tb.ResetTransformations ();
		EXPECT_EQ (tb.get_rotation_mode (), chosen)
			<< (chosen == Mode::Turntable ? "platine" : "trackball");
	}
}

// T2.1. Apres CHAQUE mouse_move, la ligne 0 est horizontale au bit pres et la
// verticale du monde est vers le haut de l'image.
TEST (CgreTrackballTurntable, HorizonStaysLevel)
{
	Ctrackball tb;
	SetupFramedTurntable (tb);

	std::mt19937 rng (20261119u);
	std::uniform_int_distribution<int> pressX (0, kWidth), pressY (0, kHeight);
	std::uniform_int_distribution<int> amplitude (-3000, 3000);
	std::uniform_int_distribution<int> stepCount (1, 8);

	int events = 0, northPole = 0, southPole = 0;
	for (int drag = 0; drag < 1000; ++drag)
	{
		const int x0 = pressX (rng), y0 = pressY (rng);
		tb.mouse_press (LEFT_BUTTON, PRESSED, x0, y0);
		const int n = stepCount (rng);
		int x = x0, y = y0;
		for (int i = 0; i < n; ++i)
		{
			x = x0 + amplitude (rng);
			y = y0 + amplitude (rng);
			tb.mouse_move (x, y);
			++events;
			const TMatrix4<float>& r = Rotation (tb);
			ASSERT_EQ (r.at (0, 2), 0.f) << "glisser " << drag << ", pas " << i;
			ASSERT_GE (r.at (1, 2), 0.f) << "glisser " << drag << ", pas " << i;
			if (r.at (2, 2) == 1.f)  ++northPole;
			if (r.at (2, 2) == -1.f) ++southPole;
		}
		tb.mouse_press (LEFT_BUTTON, RELEASED, x, y);
	}
	std::cout << "[ TURNTABLE ] " << events << " evenements, pole nord atteint " << northPole
	          << " fois, pole sud " << southPole << " fois\n";
	EXPECT_GT (northPole, 0);
	EXPECT_GT (southPole, 0);
}

// T2.2. Depart avec roulis (issu du mode trackball), 50 deplacements hors
// borne, puis retour au pixel d'enfoncement : la modelview est celle d'avant
// le geste, au bit pres.
TEST (CgreTrackballTurntable, ReturnToPressRestoresViewExactly)
{
	const TMatrix4<float> rolled = TrackballRolledRotation ();
	const double roll = RollOfRotation (rolled);
	ASSERT_GT (std::fabs (roll), 1e-3) << "le depart doit avoir du roulis";

	Ctrackball tb;
	SetupFramedTurntable (tb);
	tb.set_rotation (&rolled.at (0, 0));
	ASSERT_EQ (tb.get_rotation_mode (), Mode::Turntable);

	float before[16];
	tb.get_modelview_matrix (before);

	std::mt19937 rng (7u);
	std::uniform_int_distribution<int> offset (-40, 40);
	const int x0 = 600, y0 = 250;
	tb.mouse_press (LEFT_BUTTON, PRESSED, x0, y0);
	for (int i = 0; i < 50; ++i)
	{
		int dx = offset (rng), dy = offset (rng);
		if (dx == 0 && dy == 0)
			dx = 1;
		tb.mouse_move (x0 + dx, y0 + dy);
		const double el = TurntableFromRotation (Rotation (tb)).elevation;
		ASSERT_LT (std::fabs (el), kPi / 2 - 1e-3) << "la borne ne doit pas etre touchee";
	}
	tb.mouse_move (x0, y0);

	float after[16];
	tb.get_modelview_matrix (after);
	for (int i = 0; i < 16; ++i)
		EXPECT_EQ (after[i], before[i]) << "element " << i;
	EXPECT_NEAR (RollOfRotation (Rotation (tb)), roll, 0.0) << "roulis restitue";
	tb.mouse_press (LEFT_BUTTON, RELEASED, x0, y0);
}

// T2.3. Depuis (-30 deg, 20 deg), un demi-cote de glisser vaut 1 rad.
TEST (CgreTrackballTurntable, SignsAndRate)
{
	const int half = (std::min) (kWidth, kHeight) / 2;
	{
		Ctrackball tb;
		SetupFramedTurntable (tb);
		const TurntableAngles a0 = TurntableFromRotation (Rotation (tb));
		Drag (tb, kWidth / 2, kHeight / 2, half, 0, 1);
		const TurntableAngles a1 = TurntableFromRotation (Rotation (tb));
		EXPECT_NEAR (Wrap (a1.azimuth - a0.azimuth), -1.0, 1e-6) << "vers la droite";
		EXPECT_NEAR (a1.elevation - a0.elevation, 0.0, 1e-6);
	}
	{
		Ctrackball tb;
		SetupFramedTurntable (tb);
		const TurntableAngles a0 = TurntableFromRotation (Rotation (tb));
		Drag (tb, kWidth / 2, kHeight / 2, 0, half, 1);
		const TurntableAngles a1 = TurntableFromRotation (Rotation (tb));
		EXPECT_NEAR (Wrap (a1.azimuth - a0.azimuth), 0.0, 1e-6);
		EXPECT_NEAR (a1.elevation - a0.elevation, 1.0, 1e-6) << "vers le bas";
	}
}

// T2.4. La borne de +90 deg est atteinte exactement, et le premier pixel de
// retour en redescend.
TEST (CgreTrackballTurntable, ElevationBoundIsReachableAndResponsive)
{
	Ctrackball tb;
	SetupFramedTurntable (tb);
	const int x0 = kWidth / 2, y0 = kHeight / 2;
	tb.mouse_press (LEFT_BUTTON, PRESSED, x0, y0);
	tb.mouse_move (x0, y0 + 10000);
	EXPECT_EQ (Rotation (tb).at (2, 0), 0.f);
	EXPECT_EQ (Rotation (tb).at (2, 1), 0.f);
	EXPECT_EQ (Rotation (tb).at (2, 2), 1.f);

	tb.mouse_move (x0, y0 + 9999);
	EXPECT_LT (TurntableFromRotation (Rotation (tb)).elevation, kTurntableMaxElevation);
	tb.mouse_press (LEFT_BUTTON, RELEASED, x0, y0 + 9999);
}

// T2.5. Sans centre de rotation, seule R change : le pivot reste identique au
// bit pres, pendant et apres le geste.
TEST (CgreTrackballTurntable, PivotUntouchedWithoutCenter)
{
	Ctrackball tb;
	SetupFramedTurntable (tb);
	const TVector3<float> pivot = tb.camera ().GetPivot ();
	const float d = tb.camera ().GetDistance ();

	const int x0 = 300, y0 = 400;
	tb.mouse_press (LEFT_BUTTON, PRESSED, x0, y0);
	for (int i = 1; i <= 40; ++i)
	{
		tb.mouse_move (x0 + 7 * i, y0 - 3 * i);
		const TVector3<float>& c = tb.camera ().GetPivot ();
		ASSERT_EQ (c.x, pivot.x) << "pas " << i;
		ASSERT_EQ (c.y, pivot.y) << "pas " << i;
		ASSERT_EQ (c.z, pivot.z) << "pas " << i;
	}
	tb.mouse_press (LEFT_BUTTON, RELEASED, x0 + 280, y0 - 120);
	EXPECT_EQ (tb.camera ().GetPivot ().x, pivot.x);
	EXPECT_EQ (tb.camera ().GetPivot ().y, pivot.y);
	EXPECT_EQ (tb.camera ().GetPivot ().z, pivot.z);
	EXPECT_EQ (tb.camera ().GetDistance (), d);
}

// T2.6. straighten_horizon : horizon droit, direction de vue conservee a 1e-6,
// pivot et distance identiques au bit pres, image tournee de -roulis (1e-6
// rad ; 1e-4 rad pour le depart produit par le trackball, reserve R-bruit) ;
// au pole exact, R inchangee a 1e-6.
TEST (CgreTrackballTurntable, StraightenHorizon)
{
	struct Start { const char* name; TMatrix4<float> r; double rotationTol; bool pole; double minElevationDeg; };
	const Start starts[] = {
		{ "roulis 25 a el 30",     RolledTurntable (0.7, 30.0 * kDeg, 25.0 * kDeg),  1e-6, false, -90.0 },
		{ "roulis 90 a el 89,9",   RolledTurntable (0.7, 89.9 * kDeg, 90.0 * kDeg),  1e-6, false, 89.0 },
		{ "pole exact, az 1",      RotationFromTurntable (1.0, kTurntableMaxElevation), 1e-6, true, 89.0 },
		{ "issu du trackball",     TrackballRolledRotation (),                       1e-4, false, -90.0 },
		{ "trackball pres du pole", TrackballRolledNearPole (),                      1e-4, false, 89.7 },
	};

	for (const Start& s : starts)
	{
		Ctrackball tb;
		SetupFramed (tb);
		tb.set_rotation (&s.r.at (0, 0));
		const TMatrix4<float> before = Rotation (tb);
		const double roll = RollOfRotation (before);
		if (!s.pole)
			ASSERT_GT (std::fabs (roll), 1e-3) << s.name << " : le depart doit avoir du roulis";
		ASSERT_GE (TurntableFromRotation (before).elevation, s.minElevationDeg * kDeg) << s.name;
		const TVector3<float> pivot = tb.camera ().GetPivot ();
		const float d = tb.camera ().GetDistance ();

		tb.straighten_horizon ();
		const TMatrix4<float> after = Rotation (tb);

		EXPECT_EQ (after.at (0, 2), 0.f) << s.name;
		EXPECT_GE (after.at (1, 2), 0.f) << s.name;
		EXPECT_EQ (RollOfRotation (after), 0.0) << s.name;
		for (int i = 0; i < 3; ++i)
			EXPECT_NEAR (after.at (2, i), before.at (2, i), 1e-6) << s.name << ", r3[" << i << "]";
		EXPECT_EQ (tb.camera ().GetPivot ().x, pivot.x) << s.name;
		EXPECT_EQ (tb.camera ().GetPivot ().y, pivot.y) << s.name;
		EXPECT_EQ (tb.camera ().GetPivot ().z, pivot.z) << s.name;
		EXPECT_EQ (tb.camera ().GetDistance (), d) << s.name;

		const double rotation = ImageRotation (before, after);
		if (s.pole)
		{
			for (int row = 0; row < 3; ++row)
				for (int col = 0; col < 3; ++col)
					EXPECT_NEAR (after.at (row, col), before.at (row, col), 1e-6) << s.name;
		}
		else
			EXPECT_LE (std::fabs (Wrap (rotation + roll)), s.rotationTol) << s.name;

		std::cout << "[ TURNTABLE ] " << s.name << " : el "
		          << TurntableFromRotation (before).elevation / kDeg << " deg, roulis " << roll / kDeg
		          << " deg, rotation d'image " << rotation / kDeg << " deg, ecart "
		          << std::fabs (Wrap (rotation + roll)) << " rad\n";
	}
}

// T2.7. En mode trackball, la loi de la sphere virtuelle est conservee : R
// suit, a 1e-6 pres, une replique de la loi (Bell/Shoemake, angle de la corde,
// axe produit vectoriel, OrbitCamera::Orbit) calculee dans ce fichier.
TEST (CgreTrackballTurntable, TrackballModeUnchanged)
{
	Ctrackball tb;
	SetupFramed (tb);
	tb.set_rotation_mode (Mode::Trackball);

	OrbitCamera reference;
	reference.SetRotation (&Rotation (tb).at (0, 0));

	const auto toSphere = [] (int x, int y, float v[3])
	{
		const double side = (std::min) (kWidth, kHeight);
		v[0] = (float)((2.0 * x - kWidth) / side);
		v[1] = (float)((kHeight - 2.0 * y) / side);
		const double d2 = v[0] * v[0] + v[1] * v[1];
		v[2] = (float)((d2 <= 0.5) ? std::sqrt (1.0 - d2) : 0.5 / std::sqrt (d2));
		const double a = 1.0 / std::sqrt (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
		for (int i = 0; i < 3; ++i)
			v[i] = (float)(v[i] * a);
	};

	int x = 700, y = 200;
	float last[3];
	toSphere (x, y, last);
	tb.mouse_press (LEFT_BUTTON, PRESSED, x, y);
	for (int i = 0; i < 20; ++i)
	{
		if (i < 10) ++x; else ++y;
		tb.mouse_move (x, y);

		float cur[3];
		toSphere (x, y, cur);
		const double dx = cur[0] - last[0], dy = cur[1] - last[1], dz = cur[2] - last[2];
		const double halfChord = (std::min) (1.0, 0.5 * std::sqrt (dx * dx + dy * dy + dz * dz));
		const float axis[3] = { last[1] * cur[2] - last[2] * cur[1],
		                        last[2] * cur[0] - last[0] * cur[2],
		                        last[0] * cur[1] - last[1] * cur[0] };
		reference.Orbit (axis, (float)(2.0 * std::asin (halfChord)));
		for (int k = 0; k < 3; ++k)
			last[k] = cur[k];
	}
	tb.mouse_press (LEFT_BUTTON, RELEASED, x, y);

	double worst = 0.0;
	for (int row = 0; row < 3; ++row)
		for (int col = 0; col < 3; ++col)
		{
			const double e = std::fabs ((double)Rotation (tb).at (row, col)
			                          - (double)reference.GetRotation ().at (row, col));
			EXPECT_LE (e, 1e-6) << "ligne " << row << " colonne " << col;
			worst = (std::max) (worst, e);
		}
	std::cout << "[ TURNTABLE ] trackball : ecart max a la replique " << worst << "\n";
}

// set_rotation_mode : passer en platine redresse ; reposer le mode courant ne
// touche pas a R.
TEST (CgreTrackballTurntable, SwitchingToTurntableStraightens)
{
	const TMatrix4<float> rolled = TrackballRolledRotation ();

	Ctrackball tb;
	SetupFramedTurntable (tb);
	tb.set_rotation (&rolled.at (0, 0));
	tb.set_rotation_mode (Mode::Turntable);
	EXPECT_EQ (Rotation (tb).at (0, 2), rolled.at (0, 2)) << "mode inchange : R inchangee";

	tb.set_rotation_mode (Mode::Trackball);
	EXPECT_EQ (Rotation (tb).at (0, 2), rolled.at (0, 2)) << "passage en trackball : R inchangee";
	tb.set_rotation_mode (Mode::Turntable);
	EXPECT_EQ (Rotation (tb).at (0, 2), 0.f) << "passage en platine : horizon redresse";
}

namespace
{

// Angle de la rotation qui mene de `a` a `b` : ||A - B||_F = 2.sqrt(2).sin(theta/2).
// Forme precise aux petits angles, contrairement a acos de la trace.
double AngleBetween (const TMatrix4<float>& a, const TMatrix4<float>& b)
{
	double sum = 0.0;
	for (int row = 0; row < 3; ++row)
		for (int col = 0; col < 3; ++col)
		{
			const double e = (double)a.at (row, col) - (double)b.at (row, col);
			sum += e * e;
		}
	return 2.0 * std::asin ((std::min) (1.0, std::sqrt (sum) / (2.0 * std::sqrt (2.0))));
}

// Pas angulaire d'un pixel, kRotationRadiansPerSide / min(w, h).
double PixelStep ()
{
	return (double)Ctrackball::kRotationRadiansPerSide / (double)(std::min) (kWidth, kHeight);
}

} // namespace

// Ecriture exterieure en plein glisser : changement de mode dans les deux sens.
// Le pas qui suit tourne d'au plus un pixel de pas angulaire (marge de 1 %,
// la sphere du trackball etant un peu plus raide hors de son centre) : le
// geste reprend du dernier pixel, pas du pixel d'enfoncement.
TEST (CgreTrackballTurntable, ModeChangeMidGestureDoesNotJump)
{
	const double bound = 1.01 * PixelStep ();
	const int x0 = kWidth / 2, y0 = kHeight / 2;

	for (const Mode from : { Mode::Trackball, Mode::Turntable })
	{
		const Mode to = (from == Mode::Trackball) ? Mode::Turntable : Mode::Trackball;
		Ctrackball tb;
		SetupFramed (tb);
		tb.set_rotation_mode (from);

		tb.mouse_press (LEFT_BUTTON, PRESSED, x0, y0);
		for (int i = 1; i <= 20; ++i)
			tb.mouse_move (x0 + i, y0);
		tb.set_rotation_mode (to);
		const TMatrix4<float> afterSwitch = Rotation (tb);

		tb.mouse_move (x0 + 21, y0);
		const double step = AngleBetween (afterSwitch, Rotation (tb));
		EXPECT_LE (step, bound) << (from == Mode::Trackball ? "trackball -> platine" : "platine -> trackball");
		EXPECT_GT (step, 0.0);
		tb.mouse_press (LEFT_BUTTON, RELEASED, x0 + 21, y0);
	}
}

// set_rotation et ResetTransformations en plein glisser : l'orientation ecrite
// tient, le pas suivant ne s'en ecarte que d'un pixel de pas angulaire.
TEST (CgreTrackballTurntable, RotationWrittenMidGestureIsKept)
{
	const double bound = 1.01 * PixelStep ();
	const int x0 = kWidth / 2, y0 = kHeight / 2;

	for (int writer = 0; writer < 2; ++writer)
	{
		Ctrackball tb;
		SetupFramedTurntable (tb);
		tb.mouse_press (LEFT_BUTTON, PRESSED, x0, y0);
		for (int i = 1; i <= 30; ++i)
			tb.mouse_move (x0 + i, y0 + i / 3);

		if (writer == 0)
		{
			const TMatrix4<float> target = RotationFromTurntable (1.0, 0.5);
			tb.set_rotation (&target.at (0, 0));
		}
		else
			tb.ResetTransformations ();
		const TMatrix4<float> written = Rotation (tb);

		tb.mouse_move (x0 + 31, y0 + 10);
		EXPECT_LE (AngleBetween (written, Rotation (tb)), bound)
			<< (writer == 0 ? "set_rotation" : "ResetTransformations");
		tb.mouse_press (LEFT_BUTTON, RELEASED, x0 + 31, y0 + 10);
	}
}

// set_pivot en plein glisser, puis retour au pixel d'enfoncement : le pivot
// ecrit reste, l'ancien n'est pas restitue.
TEST (CgreTrackballTurntable, PivotWrittenMidGestureIsNotUndone)
{
	const int x0 = kWidth / 2, y0 = kHeight / 2;
	Ctrackball tb;
	SetupFramedTurntable (tb);
	tb.mouse_press (LEFT_BUTTON, PRESSED, x0, y0);
	tb.mouse_move (x0 + 30, y0 + 10);

	tb.set_pivot (-100.f, 250.f, 3000.f);
	const TVector3<float> written = tb.camera ().GetPivot ();

	tb.mouse_move (x0, y0);
	EXPECT_EQ (tb.camera ().GetPivot ().x, written.x);
	EXPECT_EQ (tb.camera ().GetPivot ().y, written.y);
	EXPECT_EQ (tb.camera ().GetPivot ().z, written.z);
	tb.mouse_press (LEFT_BUTTON, RELEASED, x0, y0);
}

#endif // CG_HAS_CGRE
