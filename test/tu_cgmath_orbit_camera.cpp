#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

#include "../src/cgmath/orbit_camera.h"

namespace {

constexpr float PI = 3.14159265358979323846f;

/// Tolerance des invariants geometriques, imposee par l'enonce des tests.
constexpr float TOL = 1e-4f;

/// Graine fixe : les tirages sont les memes a chaque execution, donc un echec
/// est rejouable.
constexpr unsigned int SEED = 20260912u;

TVector3<float> Transform(const TMatrix4<float>& m, const TVector3<float>& p)
{
	const TVector4<float> r = m * TVector4<float>(p, 1.f);
	return TVector3<float>(r.x, r.y, r.z);
}

/// Applique trois rotations tirees au hasard : l'orientation resultante n'a
/// aucune propriete particuliere, ce qui est le but.
void ApplyRandomRotation(OrbitCamera& cam, std::mt19937& rng)
{
	std::uniform_real_distribution<float> axisDist(-1.f, 1.f);
	std::uniform_real_distribution<float> angleDist(-PI, PI);

	for (int k = 0; k < 3; ++k)
	{
		float axis[3];
		float length = 0.f;
		do {
			axis[0] = axisDist(rng);
			axis[1] = axisDist(rng);
			axis[2] = axisDist(rng);
			length = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
		} while (length < 1e-3f);

		cam.Orbit(axis, angleDist(rng));
	}
}

OrbitCamera MakeOffCenterCamera(float distance)
{
	OrbitCamera cam;
	cam.SetPivot(500.f, 0.f, 0.f);
	cam.SetDistance(distance);
	cam.SetFovY(PI / 4.f);
	return cam;
}

/// Projette un point monde en PIXELS, origine en haut a gauche et y vers le
/// bas -- la convention des evenements souris, et celle de `camera project`.
///
/// Ecrite ici et non tiree de la camera : un instrument qui emprunterait la
/// formule qu'il verifie mesurerait l'accord du code avec lui-meme.
void ProjectToPixels(const OrbitCamera& cam, const TVector3<float>& world,
                     int viewportWidth, int viewportHeight,
                     float* pixelX, float* pixelY)
{
	const TVector3<float> eye = Transform(cam.GetViewMatrix(), world);
	const float depth = -eye.z;                 // l'oeil regarde vers -Z
	const float halfH = std::tan(cam.GetFovY() * 0.5f) * depth;
	const float halfW = halfH * static_cast<float>(viewportWidth)
	                          / static_cast<float>(viewportHeight);
	*pixelX = (0.5f + 0.5f * eye.x / halfW) * viewportWidth;
	*pixelY = (0.5f - 0.5f * eye.y / halfH) * viewportHeight;
}

} // namespace

//-----------------------------------------------------------------------------
// 1. Le pivot est sur l'axe de vue, a la distance d
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_orbit_camera, view_matrix_maps_pivot_to_minus_distance)
{
	const OrbitCamera cam = MakeOffCenterCamera(100.f);

	const TVector3<float> viewPivot = Transform(cam.GetViewMatrix(), cam.GetPivot());

	EXPECT_NEAR(viewPivot.x, 0.f, TOL);
	EXPECT_NEAR(viewPivot.y, 0.f, TOL);
	EXPECT_NEAR(viewPivot.z, -100.f, TOL);
}

//-----------------------------------------------------------------------------
// 2. La propriete tient QUELLE QUE SOIT la rotation
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_orbit_camera, pivot_stays_on_view_axis_under_random_rotations)
{
	std::mt19937 rng(SEED);

	for (int i = 0; i < 20; ++i)
	{
		OrbitCamera cam = MakeOffCenterCamera(100.f);
		const TVector3<float> eyeBefore = cam.GetEyePosition();

		ApplyRandomRotation(cam, rng);

		// La rotation doit reellement avoir bouge l'oeil, sinon l'invariant
		// serait verifie sur un cas trivial.
		const TVector3<float> eyeAfter = cam.GetEyePosition();
		ASSERT_GT((eyeAfter - eyeBefore).getLength(), 1e-2f) << "iteration " << i;

		const TVector3<float> viewPivot = Transform(cam.GetViewMatrix(), cam.GetPivot());

		EXPECT_NEAR(viewPivot.x, 0.f, TOL) << "iteration " << i;
		EXPECT_NEAR(viewPivot.y, 0.f, TOL) << "iteration " << i;
		EXPECT_NEAR(viewPivot.z, -100.f, TOL) << "iteration " << i;
	}
}

//-----------------------------------------------------------------------------
// 3. Le pan deplace le pivot, et le nouveau pivot reste sur l'axe de vue
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_orbit_camera, panned_pivot_stays_on_view_axis_after_rotation)
{
	std::mt19937 rng(SEED);

	OrbitCamera cam = MakeOffCenterCamera(100.f);
	const TVector3<float> pivotBefore = cam.GetPivot();

	cam.PanScreen(37.f, -21.f, 600);

	const TVector3<float> pivotAfter = cam.GetPivot();
	ASSERT_GT((pivotAfter - pivotBefore).getLength(), 1e-3f);

	// Loi attendue : k = 2.d.tan(fovY/2)/H, deplacement (-k.dx, +k.dy, 0) en
	// espace oeil. Rotation identite ici, donc le pivot se deplace de ce
	// vecteur exactement.
	const float k = 2.f * 100.f * std::tan((PI / 4.f) * 0.5f) / 600.f;
	EXPECT_NEAR(pivotAfter.x - pivotBefore.x, -k * 37.f, TOL);
	EXPECT_NEAR(pivotAfter.y - pivotBefore.y, k * -21.f, TOL);
	EXPECT_NEAR(pivotAfter.z - pivotBefore.z, 0.f, TOL);

	ApplyRandomRotation(cam, rng);

	const TVector3<float> viewPivot = Transform(cam.GetViewMatrix(), cam.GetPivot());
	EXPECT_NEAR(viewPivot.x, 0.f, TOL);
	EXPECT_NEAR(viewPivot.y, 0.f, TOL);
	EXPECT_NEAR(viewPivot.z, -100.f, TOL);
}

//-----------------------------------------------------------------------------
// 3bis. LE critere du pan : le point sous le curseur y reste, A TOUTE DISTANCE
//-----------------------------------------------------------------------------
//
// C'est la propriete que la loi doit rendre, et elle discrimine plus que la
// verification algebrique du test 3 : une constante recalibree la satisferait
// a une distance et la manquerait a toutes les autres. Le facteur 10 sur la
// distance est donc la partie qui mord -- l'ancienne loi (increment en unites
// MONDE, independant de d, du champ et de l'echelle du modele) y echoue d'un
// facteur 10 supplementaire.
//
// Le point suivi est le pivot LUI-MEME avant pan : il est dans le plan de
// profondeur ou la loi est exacte, et il reste fixe dans le monde pendant que
// le pivot, lui, se deplace.

TEST(TEST_cgmath_orbit_camera, pan_keeps_the_point_under_the_cursor)
{
	const int W = 395, H = 349;             // le viewport des mesures du chantier
	const float dragX = 100.f, dragY = 60.f;

	std::mt19937 rng(SEED);

	// Distance de cadrage d'un cube de cote 100, puis dix fois cette distance.
	const float framingDistance = 271.564f;
	const float distances[2] = { framingDistance, 10.f * framingDistance };

	for (int orientation = 0; orientation < 4; ++orientation)
	{
		for (int k = 0; k < 2; ++k)
		{
			OrbitCamera cam = MakeOffCenterCamera(distances[k]);
			if (orientation > 0)
				ApplyRandomRotation(cam, rng);

			// Point monde fixe, pris dans le plan de profondeur du pivot.
			const TVector3<float> tracked = cam.GetPivot();

			float x0 = 0.f, y0 = 0.f, x1 = 0.f, y1 = 0.f;
			ProjectToPixels(cam, tracked, W, H, &x0, &y0);
			cam.PanScreen(dragX, dragY, H);
			ProjectToPixels(cam, tracked, W, H, &x1, &y1);

			EXPECT_NEAR(x1 - x0, dragX, 2.f)
				<< "orientation " << orientation << ", d = " << distances[k];
			EXPECT_NEAR(y1 - y0, dragY, 2.f)
				<< "orientation " << orientation << ", d = " << distances[k];

			// Le pan ne dollie pas : la distance oeil-pivot est intacte.
			EXPECT_NEAR(cam.GetDistance(), distances[k], distances[k] * 1e-5f)
				<< "orientation " << orientation;
		}
	}
}

//-----------------------------------------------------------------------------
// 4. La position d'oeil est bien l'origine de l'espace vue
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_orbit_camera, eye_position_maps_to_view_origin)
{
	std::mt19937 rng(SEED);

	OrbitCamera cam = MakeOffCenterCamera(100.f);
	ApplyRandomRotation(cam, rng);

	const TVector3<float> viewEye = Transform(cam.GetViewMatrix(), cam.GetEyePosition());

	EXPECT_NEAR(viewEye.x, 0.f, TOL);
	EXPECT_NEAR(viewEye.y, 0.f, TOL);
	EXPECT_NEAR(viewEye.z, 0.f, TOL);
}

//-----------------------------------------------------------------------------
// 5. Les plans de coupe encadrent une sphere de scene disjointe du pivot
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_orbit_camera, clip_planes_enclose_off_center_scene_sphere)
{
	const float sceneCenter[3] = { 0.f, 0.f, 0.f };
	const float sceneRadius = 271.f;

	for (int i = 0; i < 20; ++i)
	{
		OrbitCamera cam = MakeOffCenterCamera(157.f);

		const float azimuth = 2.f * PI * static_cast<float>(i) / 20.f;
		const float axis[3] = { 0.f, 1.f, 0.f };
		cam.Orbit(axis, azimuth);

		const TVector3<float> eye = cam.GetEyePosition();
		const float distToScene = std::sqrt(eye.x * eye.x + eye.y * eye.y + eye.z * eye.z);

		float zNear = 0.f, zFar = 0.f;
		cam.ClipPlanes(sceneCenter, sceneRadius, &zNear, &zFar);

		EXPECT_GT(zNear, 0.f) << "azimut " << i;
		EXPECT_LT(zNear, distToScene - sceneRadius) << "azimut " << i;
		EXPECT_GT(zFar, distToScene + sceneRadius) << "azimut " << i;
	}
}

//-----------------------------------------------------------------------------
// 6. Derive de l'orthonormalite sur 10^4 rotations incrementales
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_orbit_camera, rotation_stays_orthonormal_after_10k_orbits)
{
	std::mt19937 rng(SEED);
	std::uniform_real_distribution<float> axisDist(-1.f, 1.f);
	std::uniform_real_distribution<float> angleDist(-0.1f, 0.1f);

	OrbitCamera cam;
	for (int i = 0; i < 10000; ++i)
	{
		const float axis[3] = { axisDist(rng), axisDist(rng), axisDist(rng) };
		cam.Orbit(axis, angleDist(rng));
	}

	const TMatrix4<float>& R = cam.GetRotation();

	// ||R.R^t - I||_inf sur la partie 3x3.
	float maxDeviation = 0.f;
	for (int i = 0; i < 3; ++i)
		for (int j = 0; j < 3; ++j)
		{
			float dot = 0.f;
			for (int k = 0; k < 3; ++k)
				dot += R.at(i, k) * R.at(j, k);
			const float expected = (i == j) ? 1.f : 0.f;
			maxDeviation = std::max(maxDeviation, std::fabs(dot - expected));
		}

	const float det =
		  R.at(0, 0) * (R.at(1, 1) * R.at(2, 2) - R.at(1, 2) * R.at(2, 1))
		- R.at(0, 1) * (R.at(1, 0) * R.at(2, 2) - R.at(1, 2) * R.at(2, 0))
		+ R.at(0, 2) * (R.at(1, 0) * R.at(2, 1) - R.at(1, 1) * R.at(2, 0));

	std::cout << "[ DRIFT    ] ||R.R^t - I||_inf = " << maxDeviation
	          << " ; det(R) - 1 = " << (det - 1.f) << '\n';

	EXPECT_LT(maxDeviation, 1e-5f);
	EXPECT_LT(std::fabs(det - 1.f), 1e-4f);
}

//-----------------------------------------------------------------------------
// 7. BoundingSphereOfBox : la formule partagee par les DEUX spheres du cadrage
//-----------------------------------------------------------------------------

TEST(TEST_cgmath_orbit_camera, bounding_sphere_of_box_is_centred_on_the_box)
{
	// Boite quelconque, ni centree sur l'origine ni cubique : c'est le cas que
	// l'ancienne formule -- « plus petite sphere centree sur l'ORIGINE » --
	// traitait faux, et le seul ou les deux se distinguent.
	const float mn[3] = { -225.f, -145.f, -3.05f };
	const float mx[3] = {  500.05f, 155.f, 0.05f };

	const BoundingSphere s = BoundingSphereOfBox(mn, mx);

	EXPECT_NEAR(s.center[0], 137.525f, 1e-3f);
	EXPECT_NEAR(s.center[1],   5.000f, 1e-3f);
	EXPECT_NEAR(s.center[2],  -1.500f, 1e-3f);

	const float expected = 0.5f * std::sqrt(725.05f * 725.05f + 300.f * 300.f + 3.1f * 3.1f);
	EXPECT_NEAR(s.radius, expected, 1e-2f);

	// La sphere contient REELLEMENT les huit coins : c'est la propriete dont
	// depend le critere near/far, pas la valeur du rayon.
	for (int i = 0; i < 8; ++i)
	{
		const float corner[3] = { (i & 1) ? mx[0] : mn[0],
		                          (i & 2) ? mx[1] : mn[1],
		                          (i & 4) ? mx[2] : mn[2] };
		const float dx = corner[0] - s.center[0];
		const float dy = corner[1] - s.center[1];
		const float dz = corner[2] - s.center[2];
		EXPECT_LE(std::sqrt(dx * dx + dy * dy + dz * dz), s.radius * (1.f + 1e-5f))
			<< "coin " << i;
	}
}

TEST(TEST_cgmath_orbit_camera, bounding_sphere_of_a_point_has_zero_radius)
{
	const float p[3] = { 7.f, -2.f, 13.f };

	const BoundingSphere s = BoundingSphereOfBox(p, p);

	EXPECT_FLOAT_EQ(s.radius, 0.f);
	EXPECT_FLOAT_EQ(s.center[0], p[0]);
	EXPECT_FLOAT_EQ(s.center[1], p[1]);
	EXPECT_FLOAT_EQ(s.center[2], p[2]);
}

//-----------------------------------------------------------------------------
// 8. Recentrer la sphere de profondeur RESSERRE les plans de coupe
//-----------------------------------------------------------------------------
//
// Meme scene, deux descriptions : la sphere centree sur l'ORIGINE que posait
// l'etape 4, et la sphere centree sur la SCENE qu'introduit l'etape 5. Les deux
// contiennent la scene, donc les deux sont valides -- mais la seconde doit
// ouvrir une plage de profondeur strictement plus etroite, sinon le changement
// n'achete rien.

TEST(TEST_cgmath_orbit_camera, recentring_the_depth_sphere_narrows_the_clip_range)
{
	// Cube de 0,1 unite centre en (500, 0, 0) : le cas ou l'excentricite domine.
	const float mn[3] = { 499.95f, -0.05f, -0.05f };
	const float mx[3] = { 500.05f,  0.05f,  0.05f };

	const BoundingSphere centred = BoundingSphereOfBox(mn, mx);

	// Ancienne forme : centre a l'origine, rayon = plus grande distance a
	// l'origine sur les axes.
	const float originCenter[3] = { 0.f, 0.f, 0.f };
	const float originRadius =
		std::sqrt(mx[0] * mx[0] + mx[1] * mx[1] + mx[2] * mx[2]);

	OrbitCamera cam = MakeOffCenterCamera(0.271564f);

	float nearOld = 0.f, farOld = 0.f, nearNew = 0.f, farNew = 0.f;
	cam.ClipPlanes(originCenter, originRadius, &nearOld, &farOld);
	cam.ClipPlanes(centred.center, centred.radius, &nearNew, &farNew);

	EXPECT_LT(farNew, farOld);

	// C'est le RAPPORT near/far qui mesure la resolution du tampon de
	// profondeur, pas la valeur absolue de zNear : recentrer la sphere REDUIT
	// zNear ici (0,168 contre 1,100), et l'ameliore pourtant d'un facteur 446.
	// Enoncer le critere sur zNear seul le ferait echouer a tort.
	EXPECT_GT(nearNew / farNew, nearOld / farOld);

	// L'ancienne forme depensait tout le rapport far/near a couvrir du vide :
	// zNear y tombait sur le plancher zFar/1000.
	EXPECT_NEAR(nearOld / farOld, 0.001f, 1e-6f);
	EXPECT_GT(nearNew / farNew, 0.001f);

	// Et la nouvelle encadre toujours la scene, ce qui est la seule chose qui
	// rende le resserrement legitime.
	const TVector3<float> eye = cam.GetEyePosition();
	const float dx = eye.x - centred.center[0];
	const float dy = eye.y - centred.center[1];
	const float dz = eye.z - centred.center[2];
	const float distToScene = std::sqrt(dx * dx + dy * dy + dz * dz);
	EXPECT_LT(nearNew, distToScene - centred.radius);
	EXPECT_GT(farNew,  distToScene + centred.radius);
}

//-----------------------------------------------------------------------------
// 9. LE critere de l'etape 7 : l'ALLER-RETOUR projection / deprojection
//-----------------------------------------------------------------------------
//
// Projeter un point monde donne un pixel ; deprojeter ce pixel doit rendre un
// rayon qui REPASSE PAR LE POINT. C'est plus fort qu'une verification
// algebrique de RayFromNdc : une erreur de repere, de ratio d'aspect ou de
// demi-champ survit a l'une et pas a l'autre.
//
// La projection de reference est celle du haut de ce fichier, ecrite
// independamment de la camera. Emprunter la formule qu'on verifie mesurerait
// l'accord du code avec lui-meme.
//
// Le pivot est EXCENTRE et une des configurations subit un pan : ce sont les
// situations que les etapes 4 et 6 rendent possibles, et celles ou l'ancienne
// formulation ratait le plus largement.

TEST(TEST_cgmath_orbit_camera, screen_ray_passes_through_the_projected_point)
{
	const int W = 395, H = 349;
	const float aspect = static_cast<float>(W) / static_cast<float>(H);

	const float distances[3] = { 271.564f, 2000.f, 3978.67f };

	float worstRelative = 0.f;
	int samples = 0;

	for (int k = 0; k < 3; ++k)
	{
		for (int panned = 0; panned < 2; ++panned)
		{
			OrbitCamera cam = MakeOffCenterCamera(distances[k]);
			if (panned)
				cam.PanScreen(120.f, -70.f, H);

			// Points suivis : le pivot et huit points a 0,4.d autour de lui --
			// assez loin du centre pour que les bords du champ comptent.
			const TVector3<float> c = cam.GetPivot();
			const float r = 0.4f * distances[k] / std::sqrt(3.f);
			std::vector<TVector3<float>> points;
			points.push_back(c);
			for (int sx = -1; sx <= 1; sx += 2)
				for (int sy = -1; sy <= 1; sy += 2)
					for (int sz = -1; sz <= 1; sz += 2)
						points.push_back(TVector3<float>(c.x + sx * r, c.y + sy * r, c.z + sz * r));

			for (int azStep = 0; azStep < 12; ++azStep)
			{
				// Balayage d'azimut autour de Y, la convention du harnais.
				const float angle = static_cast<float>(azStep) * (PI / 6.f);
				OrbitCamera view = cam;
				const float rot[16] = {
					std::cos(angle), 0.f, -std::sin(angle), 0.f,
					0.f,             1.f,  0.f,             0.f,
					std::sin(angle), 0.f,  std::cos(angle), 0.f,
					0.f,             0.f,  0.f,             1.f
				};
				view.SetRotation(rot);

				for (const TVector3<float>& p : points)
				{
					float px = 0.f, py = 0.f;
					ProjectToPixels(view, p, W, H, &px, &py);

					// Pixels -> NDC, convention de `camera project` : origine en
					// haut a gauche, y vers le bas.
					const float ndcX = 2.f * px / static_cast<float>(W) - 1.f;
					const float ndcY = 1.f - 2.f * py / static_cast<float>(H);

					float origin[3], dir[3];
					view.RayFromNdc(ndcX, ndcY, aspect, origin, dir);

					// Distance point-droite.
					const float wx = p.x - origin[0];
					const float wy = p.y - origin[1];
					const float wz = p.z - origin[2];
					const float t = wx * dir[0] + wy * dir[1] + wz * dir[2];
					const float ex = wx - t * dir[0];
					const float ey = wy - t * dir[1];
					const float ez = wz - t * dir[2];
					const float err = std::sqrt(ex * ex + ey * ey + ez * ez);

					EXPECT_LT(err, 1e-3f * distances[k])
						<< "d = " << distances[k] << ", panned = " << panned
						<< ", azStep = " << azStep;

					worstRelative = std::max(worstRelative, err / distances[k]);
					++samples;
				}
			}
		}
	}

	EXPECT_EQ(samples, 3 * 2 * 12 * 9);
	std::cout << "[          ] aller-retour : " << samples << " echantillons, ecart max "
	          << worstRelative << " . d (budget 1e-3)" << std::endl;
}

//-----------------------------------------------------------------------------
// 10. Le rayon du centre de l'image part de l'oeil et passe par le pivot
//-----------------------------------------------------------------------------
//
// Deux proprietes que le test 9 ne distingue pas d'une reparametrisation :
// l'origine est bien l'OEIL -- et non le plan near, dont la position depend de
// la scene -- et le rayon central vise le pivot, quelle que soit l'orientation.

TEST(TEST_cgmath_orbit_camera, centre_ray_starts_at_the_eye_and_aims_at_the_pivot)
{
	std::mt19937 rng(SEED);

	for (int k = 0; k < 8; ++k)
	{
		OrbitCamera cam = MakeOffCenterCamera(157.f);
		ApplyRandomRotation(cam, rng);

		float origin[3], dir[3];
		cam.RayFromNdc(0.f, 0.f, 395.f / 349.f, origin, dir);

		const TVector3<float> eye = cam.GetEyePosition();
		EXPECT_NEAR(origin[0], eye.x, TOL);
		EXPECT_NEAR(origin[1], eye.y, TOL);
		EXPECT_NEAR(origin[2], eye.z, TOL);

		// Le pivot est a la distance d dans la direction du rayon.
		const TVector3<float> c = cam.GetPivot();
		EXPECT_NEAR(origin[0] + cam.GetDistance() * dir[0], c.x, 1e-2f);
		EXPECT_NEAR(origin[1] + cam.GetDistance() * dir[1], c.y, 1e-2f);
		EXPECT_NEAR(origin[2] + cam.GetDistance() * dir[2], c.z, 1e-2f);
	}
}

//-----------------------------------------------------------------------------
// 11. Les axes de l'ecran ne sont ni inverses ni echanges
//-----------------------------------------------------------------------------
//
// Une convention de y inversee traverserait le test 9 si la projection de
// reference portait la meme erreur. Ici le sens est ancre sur R directement :
// ndcX > 0 doit pencher vers la DROITE de l'ecran (1re ligne de R), ndcY > 0
// vers le HAUT (2e ligne). Et le ratio d'aspect ne doit agir que sur x.

TEST(TEST_cgmath_orbit_camera, screen_axes_keep_their_orientation)
{
	std::mt19937 rng(SEED);
	OrbitCamera cam = MakeOffCenterCamera(500.f);
	ApplyRandomRotation(cam, rng);

	const TMatrix4<float>& R = cam.GetRotation();
	const TVector3<float> right(R.at(0, 0), R.at(0, 1), R.at(0, 2));
	const TVector3<float> up   (R.at(1, 0), R.at(1, 1), R.at(1, 2));

	const float aspect = 395.f / 349.f;
	const float tanHalf = std::tan(cam.GetFovY() * 0.5f);

	float o[3], d[3];

	cam.RayFromNdc(1.f, 0.f, aspect, o, d);
	const float alongRight = d[0] * right.x + d[1] * right.y + d[2] * right.z;
	EXPECT_GT(alongRight, 0.f);
	// tan de l'angle horizontal = aspect . tan(fovY/2) : c'est la seule place ou
	// le ratio d'aspect entre.
	const float forwardR = -(d[0] * R.at(2, 0) + d[1] * R.at(2, 1) + d[2] * R.at(2, 2));
	EXPECT_NEAR(alongRight / forwardR, aspect * tanHalf, 1e-4f);

	cam.RayFromNdc(0.f, 1.f, aspect, o, d);
	const float alongUp = d[0] * up.x + d[1] * up.y + d[2] * up.z;
	EXPECT_GT(alongUp, 0.f);
	const float forwardU = -(d[0] * R.at(2, 0) + d[1] * R.at(2, 1) + d[2] * R.at(2, 2));
	EXPECT_NEAR(alongUp / forwardU, tanHalf, 1e-4f);

	// Et la direction est normalisee.
	EXPECT_NEAR(std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]), 1.f, 1e-5f);
}

//-----------------------------------------------------------------------------
// 11. Cadrer UNE cible d'une scene qui en compte deux vise CETTE cible
//-----------------------------------------------------------------------------
//
// L'arithmetique sur laquelle repose le recentrage sur la selection (touche F,
// `camera pivot model N`). Deux modeles identiques, A a l'origine et B a 500 mm.
// Le critere naif -- « apres cadrage sur B, la projection du centre de B est au
// centre du viewport » -- ne peut PAS echouer : poser le pivot sur le centre de
// B le met au centre par construction (test 1). C'est donc le centre de A qui
// est mesure ici, et la distance qui est comparee a celle de l'AGREGAT : une
// implementation qui cadrerait toujours la scene entiere passerait le premier
// critere et echouerait ces deux-la.

TEST(TEST_cgmath_orbit_camera, framing_one_model_targets_it_and_not_the_whole_scene)
{
	// Deux cubes de cote 100 : A centre sur l'origine, B a 500 mm en X.
	const float aMin[3] = { -50.f, -50.f, -50.f };
	const float aMax[3] = {  50.f,  50.f,  50.f };
	const float bMin[3] = { 450.f, -50.f, -50.f };
	const float bMax[3] = { 550.f,  50.f,  50.f };
	// L'agregat des deux, ce que cadrerait une implementation qui ignore la cible.
	const float sMin[3] = { -50.f, -50.f, -50.f };
	const float sMax[3] = { 550.f,  50.f,  50.f };

	const int W = 395, H = 349;

	OrbitCamera cam;
	cam.SetFovY(PI / 4.f);
	cam.Frame(bMin, bMax);

	// (1) Le pivot est le centre de B, et la distance suit SA demi-diagonale.
	const TVector3<float> pivot = cam.GetPivot();
	EXPECT_NEAR(pivot.x, 500.f, TOL);
	EXPECT_NEAR(pivot.y,   0.f, TOL);
	EXPECT_NEAR(pivot.z,   0.f, TOL);

	const float rTarget = BoundingSphereOfBox(bMin, bMax).radius;
	const float expected = rTarget / std::sin(PI / 8.f) * 1.2f;
	std::cout << "[ FRAME    ] r_cible = " << rTarget
	          << " ; d = " << cam.GetDistance() << " ; attendu = " << expected << '\n';
	EXPECT_NEAR(cam.GetDistance(), expected, 1e-3f * expected);

	// (2) La distance de l'AGREGAT est une autre valeur : les deux criteres ne
	// se confondent pas, donc celui du dessus discrimine.
	OrbitCamera whole;
	whole.SetFovY(PI / 4.f);
	whole.Frame(sMin, sMax);
	std::cout << "[ FRAME    ] d(agregat) = " << whole.GetDistance()
	          << " ; rapport = " << (whole.GetDistance() / cam.GetDistance()) << '\n';
	EXPECT_GT(whole.GetDistance(), 3.f * cam.GetDistance());

	// (3) La cible occupe une fraction FRANCHE de l'image : cos(fovY/2)/1,2 pour
	// le diametre de la sphere de cadrage, soit 77,0 % de la hauteur.
	const float halfHeightAtPivot = std::tan(cam.GetFovY() * 0.5f) * cam.GetDistance();
	const float sphereFraction = rTarget / halfHeightAtPivot;
	std::cout << "[ FRAME    ] part de hauteur (sphere) = " << (100.f * sphereFraction) << " %\n";
	EXPECT_NEAR(sphereFraction, std::cos(PI / 8.f) / 1.2f, 1e-4f);

	// (4) LE critere qui mord : le centre de A part LOIN du centre du viewport.
	// Un cadrage sur l'agregat l'y ramenerait a moins d'un quart de largeur.
	float bx = 0.f, by = 0.f, ax = 0.f, ay = 0.f;
	ProjectToPixels(cam, TVector3<float>(500.f, 0.f, 0.f), W, H, &bx, &by);
	ProjectToPixels(cam, TVector3<float>(  0.f, 0.f, 0.f), W, H, &ax, &ay);
	std::cout << "[ FRAME    ] centre B -> (" << bx << ", " << by << ") ; centre A -> ("
	          << ax << ", " << ay << ") ; ecart A = " << std::fabs(ax - 0.5f * W) << " px\n";

	EXPECT_NEAR(bx, 0.5f * W, 1e-2f);
	EXPECT_NEAR(by, 0.5f * H, 1e-2f);
	// Seuil 1,5 largeur : mesure 775,7 px (196 % de W) avec la cible, contre
	// 109 px (28 % de W) si l'on cadrait l'agregat -- les deux ne se touchent pas.
	EXPECT_GT(std::fabs(ax - 0.5f * W), 1.5f * W);

	// Symetrie : cadrer sur A rejette B aussi loin, a la meme distance d'oeil.
	OrbitCamera other;
	other.SetFovY(PI / 4.f);
	other.Frame(aMin, aMax);
	EXPECT_NEAR(other.GetDistance(), cam.GetDistance(), 1e-3f * expected);
	ProjectToPixels(other, TVector3<float>(500.f, 0.f, 0.f), W, H, &bx, &by);
	std::cout << "[ FRAME    ] cadre sur A : centre B -> " << bx << " px\n";
	EXPECT_GT(std::fabs(bx - 0.5f * W), 1.5f * W);
}
