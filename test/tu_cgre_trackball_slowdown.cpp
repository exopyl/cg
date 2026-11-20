//
// Rotation ralentie de pres (Ctrackball::begin_left_rotation).
//
// Au debut d'un glisser gauche, le pas angulaire est multiplie par
// s = clamp(t / d, kSlowMin, 1), t etant la distance de l'oeil a la surface
// touchee par le rayon du centre de l'image (a defaut, l'entree dans la boite
// de scene), d la distance au centre de rotation. Proprietes testees :
//   1. s = t/d borne, plancher compris ;
//   2. secours par la boite, oeil hors et dans la boite ;
//   3. s = 1 sans touche, et s propre a chaque geste ;
//   4. ni le centre de rotation ni la vue ne changent au debut du geste ;
//   5. s est fige pendant le geste ;
//   6. en platine, le retour au pixel d'enfoncement restitue la vue au bit
//      pres avec s < 1 ;
//   7. s s'applique dans les deux modes ;
//   8. le centre de rotation et la distance sont invariants pendant le
//      glisser, y compris a d/t = 11,9 ;
//   9. choix de t (ChooseSlowdownScene) : seuls les modeles a faces donnent
//      une surface ; un impact au-dessus du plan de coupe est ecarte, et la
//      boite de secours est limitee au demi-espace visible z <= z_c.
//
// Configuration principale : le toit du temple d'Hephaistos tel que sinaia le
// cadre. Sphere de cadrage centree en (0, 0, 5447,66), rayon 19 218,5 mm,
// boite x +-7 713,5, y +-16 738,5, z de 0 a 10 895,32, viewport 1147 x 581, vue
// de dessus, pivot au centre de scene. Aucun contexte OpenGL : les matrices
// sont calculees par la camera. Les comparaisons au bit pres portent sur des
// instantanes relus par les accesseurs ; toute attente recalculee dans ce
// fichier se compare avec une tolerance.
//

#ifdef CG_HAS_CGRE

#include <gtest/gtest.h>

#include "../src/cgre/examinator_trackball.h"
#include "../src/cgmath/turntable.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

namespace
{

using Mode     = Ctrackball::RotationMode;
using Source   = Ctrackball::SlowdownSource;
using Scene    = Ctrackball::SlowdownScene;

constexpr int    kWidth  = 1147;
constexpr int    kHeight = 581;
constexpr double kPi     = 3.14159265358979323846;

constexpr float kSceneCenter[3] = { 0.f, 0.f, 5447.66f };
constexpr float kFramingRadius  = 19218.5f;
constexpr float kRoofTopZ       = 2.f * 5447.66f;
constexpr float kBoxMin[3]      = { -7713.5f, -16738.5f, 0.f };
constexpr float kBoxMax[3]      = {  7713.5f,  16738.5f, kRoofTopZ };

const float kTopView[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };

// Cadrage de sinaia (set_pivot puis set_zoom), oeil a `eyeAboveRoof` mm
// au-dessus du faite en vue de dessus, ou orientation par defaut si
// `rotation` est nul.
void SetupHephaistos (Ctrackball& tb, float eyeAboveRoof, Mode mode,
                      const float* rotation = kTopView)
{
	tb.set_dimensions (kWidth, kHeight);
	tb.set_scene_spheres (kSceneCenter, kFramingRadius, kFramingRadius);
	if (rotation)
		tb.set_rotation (rotation);
	else
		tb.ResetTransformations ();
	tb.set_pivot (kSceneCenter[0], kSceneCenter[1], kSceneCenter[2]);
	tb.set_zoom (-((kRoofTopZ - kSceneCenter[2]) + eyeAboveRoof));
	tb.set_rotation_mode (mode);
}

// Distance oeil -> faite le long de l'axe de vue, en vue de dessus : ce que
// le picking du centre de l'image rendrait.
float RoofDepth (const Ctrackball& tb)
{
	return tb.camera ().GetEyePosition ().z - kRoofTopZ;
}

Scene SurfaceScene (float depth)
{
	Scene scene;
	scene.surfaceHit   = true;
	scene.surfaceDepth = depth;
	return scene;
}

Scene BoxScene (const float mn[3], const float mx[3])
{
	Scene scene;
	scene.hasBox = true;
	for (int i = 0; i < 3; ++i)
	{
		scene.boxMin[i] = mn[i];
		scene.boxMax[i] = mx[i];
	}
	return scene;
}

// Projection en pixels, origine en haut a gauche, avec les matrices que
// set_camera enverrait a GL ; calcul en double.
void Project (const Ctrackball& tb, const float p[3], double& px, double& py)
{
	float mv[16], pr[16];
	tb.get_modelview_matrix (mv);
	tb.get_projection_matrix (pr);
	double e[4], c[4];
	for (int r = 0; r < 4; ++r)
		e[r] = (double)mv[r] * p[0] + (double)mv[4 + r] * p[1] + (double)mv[8 + r] * p[2] + mv[12 + r];
	for (int r = 0; r < 4; ++r)
		c[r] = (double)pr[r] * e[0] + (double)pr[4 + r] * e[1] + (double)pr[8 + r] * e[2] + (double)pr[12 + r] * e[3];
	px = (c[0] / c[3] + 1.0) * 0.5 * kWidth;
	py = (1.0 - c[1] / c[3]) * 0.5 * kHeight;
}

// Plus grand deplacement ecran de `p` sur `steps` pas horizontaux de 1 px
// depuis le centre du viewport. `scene` non nul : premier temps du geste
// (begin_left_rotation), comme sinaia au franchissement du seuil.
double MaxScreenStepPerPixel (Ctrackball& tb, const float p[3], int steps, const Scene* scene)
{
	const int x0 = kWidth / 2, y0 = kHeight / 2;
	tb.mouse_press (LEFT_BUTTON, PRESSED, x0, y0);
	if (scene)
		tb.begin_left_rotation (*scene);
	double px0, py0;
	Project (tb, p, px0, py0);
	double worst = 0.;
	for (int i = 1; i <= steps; ++i)
	{
		tb.mouse_move (x0 + i, y0);
		double px, py;
		Project (tb, p, px, py);
		worst = (std::max) (worst, std::hypot (px - px0, py - py0));
		px0 = px; py0 = py;
	}
	tb.mouse_press (LEFT_BUTTON, RELEASED, x0 + steps, y0);
	return worst;
}

// Angle de la rotation qui mene de `a` a `b` : ||A - B||_F = 2.sqrt(2).sin(theta/2).
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

double Wrap (double a)
{
	return std::remainder (a, 2.0 * kPi);
}

// Pas angulaire d'un pixel a s = 1.
double PixelStep ()
{
	return (double)Ctrackball::kRotationRadiansPerSide / (double)(std::min) (kWidth, kHeight);
}

void ExpectSamePivotAndDistance (const Ctrackball& tb, const TVector3<float>& pivot, float d,
                                 const char* where)
{
	EXPECT_EQ (tb.camera ().GetPivot ().x, pivot.x) << where;
	EXPECT_EQ (tb.camera ().GetPivot ().y, pivot.y) << where;
	EXPECT_EQ (tb.camera ().GetPivot ().z, pivot.z) << where;
	EXPECT_EQ (tb.camera ().GetDistance (), d)      << where;
}

} // namespace

// 1. s = t/d, borne a [kSlowMin, 1] ; les entrees invalides rendent 1.
TEST (CgreTrackballSlowdown, FactorIsDepthOverDistanceClamped)
{
	const float d = 5947.66f;
	EXPECT_NEAR (Ctrackball::SlowdownFactor (500.f, d), 500.0 / 5947.66, 1e-6);
	EXPECT_NEAR (Ctrackball::SlowdownFactor (0.5f * d, d), 0.5, 1e-7);

	// Plancher.
	EXPECT_EQ (Ctrackball::SlowdownFactor (0.01f * d, d), Ctrackball::kSlowMin);
	EXPECT_EQ (Ctrackball::SlowdownFactor (0.f, d), Ctrackball::kSlowMin);
	EXPECT_GT (Ctrackball::SlowdownFactor (0.03f * d, d), Ctrackball::kSlowMin);

	// Plafond : surface au-dela du centre de rotation.
	EXPECT_EQ (Ctrackball::SlowdownFactor (d, d), 1.f);
	EXPECT_EQ (Ctrackball::SlowdownFactor (2.f * d, d), 1.f);
	EXPECT_EQ (Ctrackball::SlowdownFactor (std::numeric_limits<float>::infinity (), d), 1.f);

	// Entrees invalides.
	EXPECT_EQ (Ctrackball::SlowdownFactor (std::numeric_limits<float>::quiet_NaN (), d), 1.f);
	EXPECT_EQ (Ctrackball::SlowdownFactor (-1.f, d), 1.f);
	EXPECT_EQ (Ctrackball::SlowdownFactor (100.f, 0.f), 1.f);
	EXPECT_EQ (Ctrackball::SlowdownFactor (100.f, std::numeric_limits<float>::infinity ()), 1.f);
}

// 2. Rien de touche : oeil hors de la boite -> t = entree du rayon dans la
// boite ; oeil dans la boite, boite manquee ou derriere l'oeil -> s = 1.
TEST (CgreTrackballSlowdown, SceneBoxFallback)
{
	{
		Ctrackball tb;
		SetupHephaistos (tb, 500.f, Mode::Trackball);
		const Ctrackball::RotationSlowdown r = tb.evaluate_rotation_slowdown (BoxScene (kBoxMin, kBoxMax));
		EXPECT_EQ (r.source, Source::SceneBox) << "oeil au-dessus de la boite";
		EXPECT_NEAR (r.t, 500.f, 0.01f);
		EXPECT_EQ (r.d, tb.camera ().GetDistance ());
		EXPECT_EQ (r.s, Ctrackball::SlowdownFactor (r.t, r.d));
		EXPECT_NEAR (r.s, 500.0 / 5947.66, 1e-5);
	}
	{
		Ctrackball tb;
		SetupHephaistos (tb, -2000.f, Mode::Trackball);   // oeil sous le faite
		ASSERT_LT (tb.camera ().GetEyePosition ().z, kRoofTopZ);
		const Ctrackball::RotationSlowdown r = tb.evaluate_rotation_slowdown (BoxScene (kBoxMin, kBoxMax));
		EXPECT_EQ (r.source, Source::None) << "oeil dans la boite";
		EXPECT_EQ (r.s, 1.f) << "oeil dans la boite";
	}
	{
		Ctrackball tb;
		SetupHephaistos (tb, 500.f, Mode::Trackball);
		const float mn[3] = { 20000.f, -100.f, 0.f }, mx[3] = { 30000.f, 100.f, kRoofTopZ };
		const Ctrackball::RotationSlowdown r = tb.evaluate_rotation_slowdown (BoxScene (mn, mx));
		EXPECT_EQ (r.source, Source::None) << "boite manquee";
		EXPECT_EQ (r.s, 1.f) << "boite manquee";
	}
	{
		Ctrackball tb;
		SetupHephaistos (tb, 500.f, Mode::Trackball);
		const float mn[3] = { -100.f, -100.f, 20000.f }, mx[3] = { 100.f, 100.f, 30000.f };
		const Ctrackball::RotationSlowdown r = tb.evaluate_rotation_slowdown (BoxScene (mn, mx));
		EXPECT_EQ (r.source, Source::None) << "boite derriere l'oeil";
		EXPECT_EQ (r.s, 1.f) << "boite derriere l'oeil";
	}
	{
		// La surface prime sur la boite.
		Ctrackball tb;
		SetupHephaistos (tb, 500.f, Mode::Trackball);
		Scene scene = BoxScene (kBoxMin, kBoxMax);
		scene.surfaceHit   = true;
		scene.surfaceDepth = 2000.f;
		const Ctrackball::RotationSlowdown r = tb.evaluate_rotation_slowdown (scene);
		EXPECT_EQ (r.source, Source::Surface);
		EXPECT_EQ (r.t, 2000.f);
	}
}

// 3. Sans surface ni boite, s = 1 ; s est propre au geste : un nouvel
// enfoncement le remet a 1, le relachement le conserve pour l'instrumentation.
TEST (CgreTrackballSlowdown, NoHitGivesOneAndFactorIsPerGesture)
{
	Ctrackball tb;
	SetupHephaistos (tb, 500.f, Mode::Trackball);

	tb.mouse_press (LEFT_BUTTON, PRESSED, kWidth / 2, kHeight / 2);
	const float slowed = tb.begin_left_rotation (SurfaceScene (RoofDepth (tb))).s;
	ASSERT_LT (slowed, 1.f);
	tb.mouse_move (kWidth / 2 + 5, kHeight / 2);
	tb.mouse_press (LEFT_BUTTON, RELEASED, kWidth / 2 + 5, kHeight / 2);
	EXPECT_EQ (tb.get_rotation_slowdown (), slowed) << "conserve apres le relachement";

	tb.mouse_press (LEFT_BUTTON, PRESSED, kWidth / 2, kHeight / 2);
	EXPECT_EQ (tb.get_rotation_slowdown (), 1.f) << "nouvel enfoncement";
	const Ctrackball::RotationSlowdown r = tb.begin_left_rotation (Scene ());
	EXPECT_EQ (r.source, Source::None);
	EXPECT_EQ (r.t, 0.f);
	EXPECT_EQ (r.s, 1.f);
	EXPECT_EQ (tb.get_rotation_slowdown (), 1.f);
	tb.mouse_press (LEFT_BUTTON, RELEASED, kWidth / 2, kHeight / 2);
}

// 4. Le premier temps du geste ne touche ni au centre de rotation ni a la vue :
// pivot, distance et modelview identiques au bit pres a l'instantane d'avant.
TEST (CgreTrackballSlowdown, PivotAndViewUnchangedAtGestureStart)
{
	for (const float* rotation : { kTopView, (const float*)nullptr })
		for (const Mode mode : { Mode::Trackball, Mode::Turntable })
		{
			Ctrackball tb;
			SetupHephaistos (tb, 500.f, mode, rotation);
			const TVector3<float> pivot = tb.camera ().GetPivot ();
			const float d = tb.camera ().GetDistance ();
			float before[16];
			tb.get_modelview_matrix (before);

			tb.mouse_press (LEFT_BUTTON, PRESSED, kWidth / 2, kHeight / 2);
			const float t = (rotation == kTopView) ? RoofDepth (tb) : 0.3f * d;
			ASSERT_LT (tb.evaluate_rotation_slowdown (SurfaceScene (t)).s, 1.f);
			ASSERT_LT (tb.begin_left_rotation (SurfaceScene (t)).s, 1.f);

			const char* where = (rotation == kTopView) ? "vue de dessus" : "vue par defaut";
			ExpectSamePivotAndDistance (tb, pivot, d, where);
			float after[16];
			tb.get_modelview_matrix (after);
			for (int i = 0; i < 16; ++i)
				EXPECT_EQ (after[i], before[i]) << where << ", element " << i;
			tb.mouse_press (LEFT_BUTTON, RELEASED, kWidth / 2, kHeight / 2);
		}
}

// 5. s est fige pendant le geste : ni la molette, ni une ecriture du pivot
// (qui reancre le geste), ni les deplacements ne le changent. En platine, le
// pas en azimut reste s.k par pixel avant et apres ces ecritures.
TEST (CgreTrackballSlowdown, FactorFrozenDuringGesture)
{
	Ctrackball tb;
	SetupHephaistos (tb, 500.f, Mode::Turntable, nullptr);
	const float d = tb.camera ().GetDistance ();
	const int x0 = kWidth / 2, y0 = kHeight / 2;

	tb.mouse_press (LEFT_BUTTON, PRESSED, x0, y0);
	const float s = tb.begin_left_rotation (SurfaceScene (0.25f * d)).s;
	ASSERT_EQ (s, 0.25f);

	const double az0 = TurntableFromRotation (tb.camera ().GetRotation ()).azimuth;
	tb.mouse_move (x0 + 10, y0);
	const double az1 = TurntableFromRotation (tb.camera ().GetRotation ()).azimuth;
	EXPECT_NEAR (Wrap (az1 - az0), -0.25 * PixelStep () * 10.0, 1e-6) << "avant les ecritures";

	tb.zoom_step (3.f);
	tb.set_pivot (100.f, -200.f, 3000.f);
	EXPECT_EQ (tb.get_rotation_slowdown (), s);

	const double az2 = TurntableFromRotation (tb.camera ().GetRotation ()).azimuth;
	tb.mouse_move (x0 + 20, y0);
	const double az3 = TurntableFromRotation (tb.camera ().GetRotation ()).azimuth;
	EXPECT_NEAR (Wrap (az3 - az2), -0.25 * PixelStep () * 10.0, 1e-6) << "apres les ecritures";
	EXPECT_EQ (tb.get_rotation_slowdown (), s);
	tb.mouse_press (LEFT_BUTTON, RELEASED, x0 + 20, y0);
}

// 6. Platine, s < 1 : depart avec roulis (issu du trackball), 50 deplacements
// hors borne, retour au pixel d'enfoncement -> modelview d'avant le geste au
// bit pres.
TEST (CgreTrackballSlowdown, ReturnToPressIsExactWithSlowdown)
{
	TMatrix4<float> rolled;
	{
		Ctrackball free;
		SetupHephaistos (free, 500.f, Mode::Trackball, nullptr);
		for (const int axis : { 0, 1 })
		{
			free.mouse_press (LEFT_BUTTON, PRESSED, kWidth / 2, kHeight / 2);
			for (int i = 1; i <= 20; ++i)
				free.mouse_move (kWidth / 2 + (axis == 0 ? 10 * i : 0), kHeight / 2 + (axis == 1 ? 10 * i : 0));
			free.mouse_press (LEFT_BUTTON, RELEASED, kWidth / 2, kHeight / 2);
		}
		rolled = free.camera ().GetRotation ();
	}
	ASSERT_GT (std::fabs (RollOfRotation (rolled)), 1e-3) << "le depart doit avoir du roulis";

	Ctrackball tb;
	SetupHephaistos (tb, 500.f, Mode::Turntable, nullptr);
	tb.set_rotation (&rolled.at (0, 0));
	float before[16];
	tb.get_modelview_matrix (before);

	const int x0 = 600, y0 = 250;
	tb.mouse_press (LEFT_BUTTON, PRESSED, x0, y0);
	const float s = tb.begin_left_rotation (SurfaceScene (0.1f * tb.camera ().GetDistance ())).s;
	ASSERT_LT (s, 0.11f);
	ASSERT_GT (s, Ctrackball::kSlowMin);

	std::mt19937 rng (20261120u);
	std::uniform_int_distribution<int> offset (-40, 40);
	for (int i = 0; i < 50; ++i)
	{
		int dx = offset (rng), dy = offset (rng);
		if (dx == 0 && dy == 0)
			dx = 1;
		tb.mouse_move (x0 + dx, y0 + dy);
		ASSERT_LT (std::fabs (TurntableFromRotation (tb.camera ().GetRotation ()).elevation), kPi / 2 - 1e-3);
	}
	tb.mouse_move (x0, y0);

	float after[16];
	tb.get_modelview_matrix (after);
	for (int i = 0; i < 16; ++i)
		EXPECT_EQ (after[i], before[i]) << "element " << i;
	tb.mouse_press (LEFT_BUTTON, RELEASED, x0, y0);
}

// 7. s multiplie l'angle par pixel en platine ET en trackball.
TEST (CgreTrackballSlowdown, FactorAppliedInBothModes)
{
	{
		// Platine : un demi-cote de glisser vaut s rad.
		Ctrackball tb;
		SetupHephaistos (tb, 500.f, Mode::Turntable, nullptr);
		const int half = (std::min) (kWidth, kHeight) / 2;
		const double az0 = TurntableFromRotation (tb.camera ().GetRotation ()).azimuth;
		tb.mouse_press (LEFT_BUTTON, PRESSED, kWidth / 2, kHeight / 2);
		ASSERT_EQ (tb.begin_left_rotation (SurfaceScene (0.25f * tb.camera ().GetDistance ())).s, 0.25f);
		tb.mouse_move (kWidth / 2 + half, kHeight / 2);
		tb.mouse_press (LEFT_BUTTON, RELEASED, kWidth / 2 + half, kHeight / 2);
		const double az1 = TurntableFromRotation (tb.camera ().GetRotation ()).azimuth;
		EXPECT_NEAR (Wrap (az1 - az0), -0.25 * PixelStep () * half, 1e-6) << "platine";
	}
	{
		// Trackball : meme glisser a s = 1 et a s = 0,25, angles dans le rapport s.
		double angle[2] = { 0.0, 0.0 };
		for (int k = 0; k < 2; ++k)
		{
			Ctrackball tb;
			SetupHephaistos (tb, 500.f, Mode::Trackball, nullptr);
			const TMatrix4<float> r0 = tb.camera ().GetRotation ();
			tb.mouse_press (LEFT_BUTTON, PRESSED, kWidth / 2, kHeight / 2);
			if (k == 1)
				ASSERT_EQ (tb.begin_left_rotation (SurfaceScene (0.25f * tb.camera ().GetDistance ())).s, 0.25f);
			tb.mouse_move (kWidth / 2 + 10, kHeight / 2);
			tb.mouse_press (LEFT_BUTTON, RELEASED, kWidth / 2 + 10, kHeight / 2);
			angle[k] = AngleBetween (r0, tb.camera ().GetRotation ());
		}
		ASSERT_GT (angle[0], 0.01);
		EXPECT_NEAR (angle[1] / angle[0], 0.25, 1e-3) << "trackball";
	}
}

// 8. Le centre de rotation et la distance sont invariants pendant le glisser,
// y compris a d/t = 11,9 : identiques au bit pres a chaque deplacement et
// apres le relachement, dans les deux modes.
TEST (CgreTrackballSlowdown, PivotStaysPutAfterFirstDrag)
{
	for (const Mode mode : { Mode::Trackball, Mode::Turntable })
	{
		const char* where = (mode == Mode::Trackball) ? "trackball" : "platine";
		Ctrackball tb;
		SetupHephaistos (tb, 500.f, mode);
		const TVector3<float> pivot = tb.camera ().GetPivot ();
		const float d = tb.camera ().GetDistance ();
		ASSERT_GT (d / RoofDepth (tb), 11.f);

		const int x0 = kWidth / 2, y0 = kHeight / 2;
		tb.mouse_press (LEFT_BUTTON, PRESSED, x0, y0);
		ASSERT_EQ (tb.begin_left_rotation (SurfaceScene (RoofDepth (tb))).source, Source::Surface);
		for (int i = 1; i <= 20; ++i)
		{
			tb.mouse_move (x0 + i, y0 + i / 2);
			ExpectSamePivotAndDistance (tb, pivot, d, where);
		}
		tb.mouse_press (LEFT_BUTTON, RELEASED, x0 + 20, y0 + 10);
		ExpectSamePivotAndDistance (tb, pivot, d, where);
	}
}

// 9a. Un modele sans faces (nuage, lignes) ne donne pas de surface : son
// impact est l'entree dans sa boite, 0 des que l'oeil y est.
TEST (CgreTrackballSlowdown, SceneIgnoresModelsWithoutFaces)
{
	using Impact = Ctrackball::SlowdownImpact;
	const Ctrackball::SlowdownClip noClip;
	{
		// Nuage seul, oeil dans sa boite : s = 1.
		Ctrackball tb;
		SetupHephaistos (tb, 500.f, Mode::Trackball);
		const float eyeZ = tb.camera ().GetEyePosition ().z;
		const float mn[3] = { -1000.f, -1000.f, kRoofTopZ }, mx[3] = { 1000.f, 1000.f, kRoofTopZ + 1000.f };
		ASSERT_GT (eyeZ, mn[2]);
		ASSERT_LT (eyeZ, mx[2]);
		Impact cloud;
		cloud.t = 0.f; cloud.z = eyeZ; cloud.surface = false;

		const Ctrackball::SlowdownSceneChoice choice =
			Ctrackball::ChooseSlowdownScene ({ cloud }, true, mn, mx, noClip);
		EXPECT_FALSE (choice.scene.surfaceHit);
		EXPECT_EQ (choice.impact, -1);
		EXPECT_FALSE (choice.clippedHit);
		const Ctrackball::RotationSlowdown r = tb.evaluate_rotation_slowdown (choice.scene);
		EXPECT_EQ (r.source, Source::None) << "nuage seul, oeil dans sa boite";
		EXPECT_EQ (r.s, 1.f) << "nuage seul, oeil dans sa boite";
	}
	{
		// Nuage devant le toit : t est celui du toit.
		Ctrackball tb;
		SetupHephaistos (tb, 500.f, Mode::Trackball);
		Impact cloud, roof;
		cloud.t = 0.f;            cloud.z = tb.camera ().GetEyePosition ().z; cloud.surface = false;
		roof.t  = RoofDepth (tb); roof.z  = kRoofTopZ;                        roof.surface  = true;

		const Ctrackball::SlowdownSceneChoice choice =
			Ctrackball::ChooseSlowdownScene ({ cloud, roof }, true, kBoxMin, kBoxMax, noClip);
		EXPECT_EQ (choice.impact, 1);
		const Ctrackball::RotationSlowdown r = tb.evaluate_rotation_slowdown (choice.scene);
		EXPECT_EQ (r.source, Source::Surface) << "nuage devant le toit";
		EXPECT_EQ (r.t, RoofDepth (tb)) << "nuage devant le toit";
		EXPECT_NEAR (r.s, 500.0 / 5947.66, 1e-5);
	}
}

// 9b. Plan de coupe { 0, 0, -1, z_c } : un impact au-dessus (z > z_c) est
// ecarte, la premiere surface visible derriere est retenue, et la boite de
// secours est limitee a z <= z_c ; une coupe sous la boite la vide.
TEST (CgreTrackballSlowdown, SceneBoxCutByClippingPlane)
{
	using Impact = Ctrackball::SlowdownImpact;
	const auto clipAt = [] (float z)
	{
		Ctrackball::SlowdownClip clip;
		clip.active = true;
		clip.z      = z;
		return clip;
	};
	const auto roofImpact = [] (const Ctrackball& tb)
	{
		Impact roof;
		roof.t = RoofDepth (tb); roof.z = kRoofTopZ; roof.surface = true;
		return roof;
	};

	{
		// Coupe a z_c = 5 000, oeil a 500 mm au-dessus du faite : toit coupe,
		// entree dans la boite coupee a eye.z - 5 000 > d, donc s = 1.
		Ctrackball tb;
		SetupHephaistos (tb, 500.f, Mode::Trackball);
		const Ctrackball::SlowdownSceneChoice choice =
			Ctrackball::ChooseSlowdownScene ({ roofImpact (tb) }, true, kBoxMin, kBoxMax, clipAt (5000.f));
		EXPECT_TRUE (choice.clippedHit);
		EXPECT_FALSE (choice.scene.surfaceHit);
		ASSERT_TRUE (choice.scene.hasBox);
		EXPECT_EQ (choice.scene.boxMax[2], 5000.f);
		const Ctrackball::RotationSlowdown r = tb.evaluate_rotation_slowdown (choice.scene);
		EXPECT_EQ (r.source, Source::SceneBox);
		EXPECT_NEAR (r.t, tb.camera ().GetEyePosition ().z - 5000.f, 0.01f);
		EXPECT_GT (r.t, r.d);
		EXPECT_EQ (r.s, 1.f) << "coupe a 5 000";
	}
	{
		// Coupe 200 mm sous le faite : t = 700, distance d'entree au plan.
		Ctrackball tb;
		SetupHephaistos (tb, 500.f, Mode::Trackball);
		const float zc = kRoofTopZ - 200.f;
		const Ctrackball::SlowdownSceneChoice choice =
			Ctrackball::ChooseSlowdownScene ({ roofImpact (tb) }, true, kBoxMin, kBoxMax, clipAt (zc));
		const Ctrackball::RotationSlowdown r = tb.evaluate_rotation_slowdown (choice.scene);
		EXPECT_EQ (r.source, Source::SceneBox);
		EXPECT_NEAR (r.t, 700.f, 0.01f);
		EXPECT_NEAR (r.s, 700.0 / 5947.66, 1e-5) << "coupe 200 mm sous le faite";
	}
	{
		// Coupe sous la boite : plus de boite, s = 1.
		Ctrackball tb;
		SetupHephaistos (tb, 500.f, Mode::Trackball);
		const Ctrackball::SlowdownSceneChoice choice =
			Ctrackball::ChooseSlowdownScene ({ roofImpact (tb) }, true, kBoxMin, kBoxMax, clipAt (-10.f));
		EXPECT_TRUE (choice.clippedHit);
		EXPECT_FALSE (choice.scene.hasBox);
		const Ctrackball::RotationSlowdown r = tb.evaluate_rotation_slowdown (choice.scene);
		EXPECT_EQ (r.source, Source::None);
		EXPECT_EQ (r.s, 1.f) << "coupe sous la boite";
	}
	{
		// Toit coupe, sol visible d'un autre modele derriere : le sol est retenu.
		Ctrackball tb;
		SetupHephaistos (tb, 500.f, Mode::Trackball);
		Impact floor;
		floor.t = tb.camera ().GetEyePosition ().z; floor.z = 0.f; floor.surface = true;
		const Ctrackball::SlowdownSceneChoice choice =
			Ctrackball::ChooseSlowdownScene ({ roofImpact (tb), floor }, true, kBoxMin, kBoxMax, clipAt (5000.f));
		EXPECT_TRUE (choice.clippedHit);
		EXPECT_EQ (choice.impact, 1);
		EXPECT_EQ (choice.scene.surfaceDepth, floor.t);
	}
	{
		// Coupe au-dessus du toit, ou inactive : rien n'est ecarte, boite intacte.
		for (const bool active : { true, false })
		{
			Ctrackball tb;
			SetupHephaistos (tb, 500.f, Mode::Trackball);
			Ctrackball::SlowdownClip clip = clipAt (active ? 20000.f : 5000.f);
			clip.active = active;
			const Ctrackball::SlowdownSceneChoice choice =
				Ctrackball::ChooseSlowdownScene ({ roofImpact (tb) }, true, kBoxMin, kBoxMax, clip);
			EXPECT_FALSE (choice.clippedHit);
			EXPECT_EQ (choice.impact, 0);
			EXPECT_EQ (choice.scene.boxMax[2], kBoxMax[2]);
		}
	}
}

// Mesure : a 50 cm du faite, centre au milieu de la scene, le faite balaie
// l'ecran de plus de 10 px par px sans ralentissement, de moins de 2,5 px
// (cot(22,5 deg).(1 - t/d) = 2,21) avec. Mode trackball epingle : en vue de
// dessus, pole de la platine, un glisser horizontal tourne autour de l'axe de
// vue et le faite ne bougerait pas, quel que soit s.
TEST (CgreTrackballSlowdown, AimedPointSlowedNearSurface)
{
	const float roof[3] = { 0.f, 0.f, kRoofTopZ };

	Ctrackball control;
	SetupHephaistos (control, 500.f, Mode::Trackball);
	const double without = MaxScreenStepPerPixel (control, roof, 20, nullptr);
	EXPECT_GT (without, 10.0);

	Ctrackball tb;
	SetupHephaistos (tb, 500.f, Mode::Trackball);
	const Scene scene = SurfaceScene (RoofDepth (tb));
	const double with = MaxScreenStepPerPixel (tb, roof, 20, &scene);
	float zNear = 0.f, zFar = 0.f;
	tb.get_clip_planes (&zNear, &zFar);
	std::cout << "[ SLOWDOWN ] 50 cm du faite : t " << scene.surfaceDepth << " d " << tb.camera ().GetDistance ()
	          << " s " << tb.get_rotation_slowdown () << " ; faite, px ecran par px souris : sans "
	          << without << ", avec " << with << " ; near " << zNear << "\n";
	EXPECT_LT (with, 2.5);
	EXPECT_GT (with, 1.0);
}

// Le plancher kSlowMin ne mord qu'au plus pres du toit : avec le centre au
// milieu d'Hephaistos, sous kSlowMin.5447,66/(1 - kSlowMin) = 111 mm du faite.
// Le tableau imprime fixe la justification de kSlowMin (plan, D13).
TEST (CgreTrackballSlowdown, FloorBitesOnlyCloseToRoof)
{
	for (const float h : { 29.f, 50.f, 100.f, 120.f, 192.f, 500.f, 1000.f, 2000.f, 5000.f })
	{
		Ctrackball tb;
		SetupHephaistos (tb, h, Mode::Trackball);
		const Ctrackball::RotationSlowdown r = tb.evaluate_rotation_slowdown (SurfaceScene (RoofDepth (tb)));
		float zNear = 0.f, zFar = 0.f;
		tb.get_clip_planes (&zNear, &zFar);
		const double radPerPx = r.s * PixelStep ();
		std::cout << "[ SLOWDOWN ] h " << h << " mm : t " << r.t << " d " << r.d << " s " << r.s
		          << " ; " << radPerPx * 180.0 / kPi << " deg/px, quart de tour " << (kPi / 2) / radPerPx
		          << " px ; near " << zNear << "\n";
		if (h <= 100.f)
			EXPECT_EQ (r.s, Ctrackball::kSlowMin) << "h " << h;
		else
			EXPECT_GT (r.s, Ctrackball::kSlowMin) << "h " << h;
	}
}

#endif // CG_HAS_CGRE
