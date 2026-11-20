//
// Panoramique ralenti de pres (Ctrackball::begin_pan).
//
// Le glisser du bouton du milieu multiplie son pas de translation par le
// facteur s de la rotation, evalue par la meme regle sur la meme scene au
// premier temps du geste et fige jusqu'au relachement. pan_screen deplace le
// pivot d'un pas calcule au plan du pivot : un point a la profondeur t glisse
// de s.d/t px par px de curseur, soit 1 pour kSlowMin.d <= t <= d.
// Proprietes testees :
//   1. de pres, le point vise suit le curseur a 1 px par px ; sans s, il
//      glisse de d/t ;
//   2. au plancher, s.d/t ; au-dela du pivot, s = 1 ;
//   3. s est celui de evaluate_rotation_slowdown, pour les trois sources ;
//   4. s est propre au geste du milieu : remis a 1 a l'enfoncement, conserve
//      au relachement, fige pendant le geste, independant du geste gauche ;
//   5. la molette et le dolly gardent leur pas.
//
// Configuration : celle de tu_cgre_trackball_slowdown.cpp, le toit du temple
// d'Hephaistos tel que sinaia le cadre, vue de dessus, viewport 1147 x 581.
// Les comparaisons au bit pres portent sur des instantanes relus par les
// accesseurs ; toute attente recalculee dans ce fichier se compare avec une
// tolerance.
//

#ifdef CG_HAS_CGRE

#include <gtest/gtest.h>

#include "../src/cgre/examinator_trackball.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace
{

using Mode   = Ctrackball::RotationMode;
using Source = Ctrackball::SlowdownSource;
using Scene  = Ctrackball::SlowdownScene;

constexpr int kWidth  = 1147;
constexpr int kHeight = 581;

constexpr float kSceneCenter[3] = { 0.f, 0.f, 5447.66f };
constexpr float kFramingRadius  = 19218.5f;
constexpr float kRoofTopZ       = 2.f * 5447.66f;
constexpr float kBoxMin[3]      = { -7713.5f, -16738.5f, 0.f };
constexpr float kBoxMax[3]      = {  7713.5f,  16738.5f, kRoofTopZ };

const float kTopView[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };

// Cadrage de sinaia, vue de dessus, oeil a `eyeAboveRoof` mm au-dessus du
// faite, pivot au centre de la scene.
void SetupHephaistos (Ctrackball& tb, float eyeAboveRoof)
{
	tb.set_dimensions (kWidth, kHeight);
	tb.set_scene_spheres (kSceneCenter, kFramingRadius, kFramingRadius);
	tb.set_rotation (kTopView);
	tb.set_pivot (kSceneCenter[0], kSceneCenter[1], kSceneCenter[2]);
	tb.set_zoom (-((kRoofTopZ - kSceneCenter[2]) + eyeAboveRoof));
	tb.set_rotation_mode (Mode::Trackball);
}

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

Scene BoxScene ()
{
	Scene scene;
	scene.hasBox = true;
	for (int i = 0; i < 3; ++i)
	{
		scene.boxMin[i] = kBoxMin[i];
		scene.boxMax[i] = kBoxMax[i];
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

struct PanTrace
{
	double minStep = 1e30;   // deplacement ecran signe par px de curseur, le
	double maxStep = -1e30;  // long du glisser : min et max sur les pas
	double total   = 0.0;    // deplacement ecran total, le long du glisser
	double drift   = 0.0;    // plus grand ecart en travers du glisser
};

enum class DragAxis { X, Y };

// Glisser du milieu de `steps` pas de 1 px depuis le centre du viewport, vers
// la droite (X) ou vers le bas (Y). `scene` non nul : premier temps du geste
// (begin_pan), comme sinaia au franchissement du seuil.
PanTrace DragMiddle (Ctrackball& tb, const float p[3], int steps, const Scene* scene,
                     DragAxis axis = DragAxis::X)
{
	const int x0 = kWidth / 2, y0 = kHeight / 2;
	const int ux = (axis == DragAxis::X) ? 1 : 0;
	const int uy = 1 - ux;
	tb.mouse_press (MIDDLE_BUTTON, PRESSED, x0, y0);
	if (scene)
		tb.begin_pan (*scene);
	double px0, py0;
	Project (tb, p, px0, py0);
	const double startX = px0, startY = py0;
	PanTrace trace;
	for (int i = 1; i <= steps; ++i)
	{
		tb.mouse_move (x0 + ux * i, y0 + uy * i);
		double px, py;
		Project (tb, p, px, py);
		const double along  = ux ? px - px0 : py - py0;
		const double across = ux ? py - startY : px - startX;
		trace.minStep = (std::min) (trace.minStep, along);
		trace.maxStep = (std::max) (trace.maxStep, along);
		trace.drift   = (std::max) (trace.drift, std::fabs (across));
		px0 = px; py0 = py;
	}
	trace.total = ux ? px0 - startX : py0 - startY;
	tb.mouse_press (MIDDLE_BUTTON, RELEASED, x0 + ux * steps, y0 + uy * steps);
	return trace;
}

} // namespace

// 1. A 50 cm du faite, pivot a 5,9 m : sans s, le faite glisse de d/t = 11,9 px
// par px ; avec s, il suit le curseur a 1 px par px, en x comme en y.
TEST (CgreTrackballPan, SurfaceFollowsCursorNearSurface)
{
	const float roof[3] = { 0.f, 0.f, kRoofTopZ };

	Ctrackball control;
	SetupHephaistos (control, 500.f);
	const double ratio = control.camera ().GetDistance () / RoofDepth (control);
	const PanTrace without = DragMiddle (control, roof, 20, nullptr);
	EXPECT_NEAR (without.minStep, ratio, 0.05);
	EXPECT_NEAR (without.maxStep, ratio, 0.05);

	Ctrackball tb;
	SetupHephaistos (tb, 500.f);
	const Scene scene = SurfaceScene (RoofDepth (tb));
	const PanTrace with = DragMiddle (tb, roof, 100, &scene);
	std::cout << "[ PAN ] 50 cm du faite : t " << scene.surfaceDepth << " d " << tb.camera ().GetDistance ()
	          << " s " << tb.get_pan_slowdown () << " ; faite, px ecran par px de curseur : sans "
	          << without.minStep << " a " << without.maxStep << ", avec " << with.minStep << " a "
	          << with.maxStep << " ; 100 px de curseur -> " << with.total << " px, derive y "
	          << with.drift << "\n";
	EXPECT_LT (tb.get_pan_slowdown (), 1.f);
	EXPECT_NEAR (with.minStep, 1.0, 0.01);
	EXPECT_NEAR (with.maxStep, 1.0, 0.01);
	EXPECT_NEAR (with.total, 100.0, 0.5);
	EXPECT_LT (with.drift, 0.01);

	// Meme facteur sur l'axe vertical : glisser vers le bas.
	Ctrackball vert;
	SetupHephaistos (vert, 500.f);
	const PanTrace down = DragMiddle (vert, roof, 100, &scene, DragAxis::Y);
	std::cout << "[ PAN ] glisser vertical : s " << vert.get_pan_slowdown () << " ; faite, px ecran par px de curseur "
	          << down.minStep << " a " << down.maxStep << " ; 100 px de curseur -> " << down.total
	          << " px, derive x " << down.drift << "\n";
	EXPECT_LT (vert.get_pan_slowdown (), 1.f);
	EXPECT_NEAR (down.minStep, 1.0, 0.01);
	EXPECT_NEAR (down.maxStep, 1.0, 0.01);
	EXPECT_NEAR (down.total, 100.0, 0.5);
	EXPECT_LT (down.drift, 0.01);
}

// 2. Au plancher (h = 50 mm, s = kSlowMin), le faite glisse de kSlowMin.d/t ;
// une surface au-dela du pivot laisse s = 1, et le point vise glisse de d/t < 1.
TEST (CgreTrackballPan, FloorAndFarSurface)
{
	const float roof[3] = { 0.f, 0.f, kRoofTopZ };
	{
		Ctrackball tb;
		SetupHephaistos (tb, 50.f);
		const Scene scene = SurfaceScene (RoofDepth (tb));
		const PanTrace trace = DragMiddle (tb, roof, 20, &scene);
		ASSERT_EQ (tb.get_pan_slowdown (), Ctrackball::kSlowMin);
		const double expected = Ctrackball::kSlowMin * tb.camera ().GetDistance () / RoofDepth (tb);
		std::cout << "[ PAN ] 50 mm du faite : s " << tb.get_pan_slowdown () << " ; faite " << trace.minStep
		          << " a " << trace.maxStep << " px par px (attendu " << expected << ")\n";
		EXPECT_NEAR (trace.minStep, expected, 0.02);
		EXPECT_NEAR (trace.maxStep, expected, 0.02);
	}
	{
		// Sol de la scene, sous le pivot : t > d.
		Ctrackball tb;
		SetupHephaistos (tb, 500.f);
		const float ground[3] = { 0.f, 0.f, 0.f };
		const float t = tb.camera ().GetEyePosition ().z;
		ASSERT_GT (t, tb.camera ().GetDistance ());
		const Scene scene = SurfaceScene (t);
		const PanTrace trace = DragMiddle (tb, ground, 20, &scene);
		EXPECT_EQ (tb.get_pan_slowdown (), 1.f);
		const double expected = tb.camera ().GetDistance () / (double)t;
		EXPECT_NEAR (trace.minStep, expected, 0.01);
		EXPECT_NEAR (trace.maxStep, expected, 0.01);
	}
}

// 3. Meme regle que la rotation : surface, boite de secours, rien.
TEST (CgreTrackballPan, FactorFollowsRotationRule)
{
	struct Case { const char* name; Scene scene; Source source; };
	Ctrackball probe;
	SetupHephaistos (probe, 500.f);
	const Case cases[] = {
		{ "surface", SurfaceScene (RoofDepth (probe)), Source::Surface },
		{ "boite",   BoxScene (),                      Source::SceneBox },
		{ "rien",    Scene (),                         Source::None },
	};
	for (const Case& c : cases)
	{
		Ctrackball tb;
		SetupHephaistos (tb, 500.f);
		const Ctrackball::RotationSlowdown expected = tb.evaluate_rotation_slowdown (c.scene);
		ASSERT_EQ (expected.source, c.source) << c.name;
		tb.mouse_press (MIDDLE_BUTTON, PRESSED, kWidth / 2, kHeight / 2);
		const Ctrackball::RotationSlowdown r = tb.begin_pan (c.scene);
		EXPECT_EQ (r.source, expected.source) << c.name;
		EXPECT_EQ (r.t, expected.t) << c.name;
		EXPECT_EQ (r.d, expected.d) << c.name;
		EXPECT_EQ (r.s, expected.s) << c.name;
		EXPECT_EQ (tb.get_pan_slowdown (), expected.s) << c.name;
		tb.mouse_press (MIDDLE_BUTTON, RELEASED, kWidth / 2, kHeight / 2);
	}
}

// 4. s est propre au geste du milieu.
TEST (CgreTrackballPan, FactorIsPerMiddleGesture)
{
	Ctrackball tb;
	SetupHephaistos (tb, 500.f);
	const int x0 = kWidth / 2, y0 = kHeight / 2;
	EXPECT_EQ (tb.get_pan_slowdown (), 1.f) << "construction";

	tb.mouse_press (MIDDLE_BUTTON, PRESSED, x0, y0);
	const float s = tb.begin_pan (SurfaceScene (RoofDepth (tb))).s;
	ASSERT_LT (s, 1.f);
	tb.mouse_move (x0 + 5, y0);
	tb.zoom_step (2.f);
	tb.set_pivot (100.f, -200.f, 3000.f);
	EXPECT_EQ (tb.get_pan_slowdown (), s) << "fige pendant le geste";
	tb.mouse_move (x0 + 10, y0);
	tb.mouse_press (MIDDLE_BUTTON, RELEASED, x0 + 10, y0);
	EXPECT_EQ (tb.get_pan_slowdown (), s) << "conserve apres le relachement";

	// Chaque geste remet son propre facteur a 1, jamais celui de l'autre. Le
	// facteur de rotation est fige par une surface a mi-distance du pivot :
	// t/d = 0,5.
	tb.mouse_press (LEFT_BUTTON, PRESSED, x0, y0);
	EXPECT_EQ (tb.get_pan_slowdown (), s) << "enfoncement gauche";
	const float sr = tb.begin_left_rotation (SurfaceScene (0.5f * tb.camera ().GetDistance ())).s;
	ASSERT_LT (sr, 1.f);
	EXPECT_EQ (tb.get_pan_slowdown (), s) << "rotation figee";
	tb.mouse_press (LEFT_BUTTON, RELEASED, x0, y0);
	EXPECT_EQ (tb.get_pan_slowdown (), s) << "relachement gauche";

	// Un nouvel enfoncement du milieu remet le facteur du panoramique a 1 et
	// laisse celui de la rotation : sans begin_pan, le pas est brut.
	tb.mouse_press (MIDDLE_BUTTON, PRESSED, x0, y0);
	EXPECT_EQ (tb.get_pan_slowdown (), 1.f) << "nouvel enfoncement du milieu";
	EXPECT_EQ (tb.get_rotation_slowdown (), sr) << "enfoncement du milieu";
	Ctrackball raw;
	SetupHephaistos (raw, 500.f);
	raw.set_pivot (tb.camera ().GetPivot ().x, tb.camera ().GetPivot ().y, tb.camera ().GetPivot ().z);
	raw.set_zoom (tb.get_zoom ());
	tb.mouse_move (x0 + 7, y0 - 3);
	raw.pan_screen (7.f, -3.f);
	EXPECT_EQ (tb.camera ().GetPivot ().x, raw.camera ().GetPivot ().x);
	EXPECT_EQ (tb.camera ().GetPivot ().y, raw.camera ().GetPivot ().y);
	EXPECT_EQ (tb.camera ().GetPivot ().z, raw.camera ().GetPivot ().z);
	tb.mouse_press (MIDDLE_BUTTON, RELEASED, x0 + 7, y0 - 3);
	EXPECT_EQ (tb.get_rotation_slowdown (), sr) << "relachement du milieu";
}

// 5. Molette et dolly : leur pas ne lit pas le facteur du panoramique.
TEST (CgreTrackballPan, WheelAndDollyUnchanged)
{
	Ctrackball tb;
	SetupHephaistos (tb, 500.f);
	const int x0 = kWidth / 2, y0 = kHeight / 2;
	tb.mouse_press (MIDDLE_BUTTON, PRESSED, x0, y0);
	ASSERT_LT (tb.begin_pan (SurfaceScene (RoofDepth (tb))).s, 0.1f);
	tb.mouse_move (x0 + 3, y0);

	const double d0 = tb.camera ().GetDistance ();
	tb.zoom_step (3.f);
	const double d1 = tb.camera ().GetDistance ();
	EXPECT_NEAR (d1 / d0, std::exp (-0.3), 1e-6) << "molette pendant le geste";
	tb.mouse_press (MIDDLE_BUTTON, RELEASED, x0 + 3, y0);

	tb.zoom_step (-3.f);
	const double d2 = tb.camera ().GetDistance ();
	EXPECT_NEAR (d2 / d1, std::exp (0.3), 1e-6) << "molette apres le geste";

	tb.mouse_press (RIGHT_BUTTON, PRESSED, x0, y0);
	tb.mouse_move (x0, y0 + 58);
	tb.mouse_press (RIGHT_BUTTON, RELEASED, x0, y0 + 58);
	const double d3 = tb.camera ().GetDistance ();
	EXPECT_NEAR (d3 / d2, std::exp (-2.0 * 58.0 / kHeight), 1e-6) << "dolly";
}

#endif // CG_HAS_CGRE
