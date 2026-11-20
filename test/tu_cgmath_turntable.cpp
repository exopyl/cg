//
// Platine Z-up (turntable.h).
//
// Proprietes testees :
//   1. l'orientation par defaut de sinaia est la platine (-30 deg, 20 deg) ;
//   2. l'extraction inverse la construction, poles compris ;
//   3. la construction a un horizon droit exact et le haut en haut ;
//   4. l'elevation est bornee a +-pi/2 ;
//   5. le redressement d'une R avec roulis garde la direction de vue et fait
//      tourner l'image de l'oppose du roulis ; au pole exact, il ne fait rien ;
//      le roulis d'une vue exactement verticale vaut 0.
//
// Les oracles sont calcules en double dans ce fichier, et compares avec une
// tolerance : jamais au bit pres, la contraction FMA pouvant differer entre
// unites de traduction.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#include "../src/cgmath/turntable.h"

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;

/// Ramene un angle dans [-pi, pi].
double Wrap(double a)
{
	return std::remainder(a, 2.0 * kPi);
}

/// Ligne `row` de la partie 3x3, en double.
void Row(const TMatrix4<float>& r, int row, double out[3])
{
	for (int i = 0; i < 3; ++i)
		out[i] = r.at(row, i);
}

double Dot(const double a[3], const double b[3])
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void Cross(const double a[3], const double b[3], double out[3])
{
	out[0] = a[1] * b[2] - a[2] * b[1];
	out[1] = a[2] * b[0] - a[0] * b[2];
	out[2] = a[0] * b[1] - a[1] * b[0];
}

/// Angle signe de la ligne 0 de `before` vers celle de `after`, autour de la
/// direction de vue de `before` : rotation de l'image autour de son centre.
double ImageRotation(const TMatrix4<float>& before, const TMatrix4<float>& after)
{
	double r0[3], s0[3], r3[3], c[3];
	Row(before, 0, r0);
	Row(after, 0, s0);
	Row(before, 2, r3);
	Cross(r0, s0, c);
	return std::atan2(Dot(c, r3), Dot(r0, s0));
}

/// Platine avec roulis : lignes de R_tt(az, el) calculees en double, puis
/// rotation d'angle rho autour de l'axe de vue (lignes 0 et 1), puis stockage
/// en float, comme dans OrbitCamera.
TMatrix4<float> RolledTurntable(double az, double el, double rho)
{
	const double ca = std::cos(az), sa = std::sin(az);
	double ce, se;
	if (el >= kPi / 2)       { ce = 0.0; se = 1.0; }
	else if (el <= -kPi / 2) { ce = 0.0; se = -1.0; }
	else                     { ce = std::cos(el); se = std::sin(el); }

	const double r0[3] = { -sa, ca, 0.0 };
	const double r1[3] = { -se * ca, -se * sa, ce };
	const double r3[3] = { ce * ca, ce * sa, se };
	const double c = std::cos(rho), s = std::sin(rho);

	TMatrix4<float> r;
	for (int i = 0; i < 3; ++i)
	{
		r.at(0, i) = static_cast<float>(c * r0[i] + s * r1[i]);
		r.at(1, i) = static_cast<float>(-s * r0[i] + c * r1[i]);
		r.at(2, i) = static_cast<float>(r3[i]);
	}
	return r;
}

/// Azimuts de ]-pi, pi], pi compris.
std::vector<double> AzimuthGrid()
{
	std::vector<double> az;
	const int n = 24;
	for (int i = 1; i <= n; ++i)
		az.push_back(-kPi + 2.0 * kPi * i / n);
	return az;
}

/// Elevations de [-pi/2, pi/2], bornes comprises, plus le voisinage des poles.
std::vector<double> ElevationGrid()
{
	std::vector<double> el;
	const int n = 18;
	for (int i = 0; i <= n; ++i)
		el.push_back(-kPi / 2 + kPi * i / n);
	for (double deg : { -89.999, -89.9, 89.9, 89.999 })
		el.push_back(deg * kDeg);
	return el;
}

} // namespace

// 1. Valeurs de la vue par defaut, (-30 deg, 20 deg) Z-up, figees comme oracle
// (relevees par camera reset + camera show, section 7.1 du plan), en
// colonne-majeur.
TEST(TEST_cgmath_turntable, default_view_is_turntable_minus30_20)
{
	const float table[16] = {
		 0.5f,          -0.2961981f,     0.8137977f,     0.f,
		 0.8660254f,     0.1710101f,    -0.4698463f,     0.f,
		 0.f,            0.9396926f,     0.3420201f,     0.f,
		 0.f,            0.f,            0.f,            1.f
	};
	const TMatrix4<float> r = RotationFromTurntable(-30.0 * kDeg, 20.0 * kDeg);
	for (int col = 0; col < 4; ++col)
		for (int row = 0; row < 4; ++row)
			EXPECT_NEAR(r.at(row, col), table[4 * col + row], 1e-7)
				<< "ligne " << row << " colonne " << col;
}

// 2. Sans roulis, l'extraction rend (az, el) a 1e-6 rad pres, poles compris.
TEST(TEST_cgmath_turntable, extraction_inverts_construction)
{
	double worstAz = 0.0, worstEl = 0.0;
	for (double el : ElevationGrid())
		for (double az : AzimuthGrid())
		{
			const TurntableAngles a = TurntableFromRotation(RotationFromTurntable(az, el));
			const double dAz = std::fabs(Wrap(a.azimuth - az));
			const double dEl = std::fabs(a.elevation - el);
			EXPECT_LE(dAz, 1e-6) << "az " << az << " el " << el;
			EXPECT_LE(dEl, 1e-6) << "az " << az << " el " << el;
			worstAz = std::max(worstAz, dAz);
			worstEl = std::max(worstEl, dEl);
		}
	std::cout << "[ TURNTABLE ] extraction : max |daz| = " << worstAz
	          << " rad, max |del| = " << worstEl << " rad\n";
}

// 3. Horizon droit exact (ligne 0 sans composante z) et haut en haut (cos el
// >= 0), y compris aux bornes.
TEST(TEST_cgmath_turntable, rows_are_level_and_upright)
{
	for (double el : ElevationGrid())
		for (double az : AzimuthGrid())
		{
			const TMatrix4<float> r = RotationFromTurntable(az, el);
			EXPECT_EQ(r.at(0, 2), 0.f) << "az " << az << " el " << el;
			EXPECT_GE(r.at(1, 2), 0.f) << "az " << az << " el " << el;
		}
}

// 4. Au-dela de la borne, l'elevation vaut la borne.
TEST(TEST_cgmath_turntable, elevation_is_clamped)
{
	for (double az : AzimuthGrid())
	{
		const TMatrix4<float> above = RotationFromTurntable(az, 2.0);
		const TMatrix4<float> top   = RotationFromTurntable(az, kTurntableMaxElevation);
		const TMatrix4<float> below = RotationFromTurntable(az, -2.0);
		const TMatrix4<float> down  = RotationFromTurntable(az, -kTurntableMaxElevation);
		for (int row = 0; row < 4; ++row)
			for (int col = 0; col < 4; ++col)
			{
				EXPECT_EQ(above.at(row, col), top.at(row, col)) << "az " << az;
				EXPECT_EQ(below.at(row, col), down.at(row, col)) << "az " << az;
			}
	}
}

// 5. Redressement R -> R_tt(extraction(R)) sur la grille de D1 : horizon droit,
// direction de vue conservee a 1e-6, image tournee de -roulis a 1e-6 rad hors du
// pole exact, et immobile au pole exact.
TEST(TEST_cgmath_turntable, straightening_keeps_view_and_turns_image_by_roll)
{
	const double elevations[] = { -90, -89.999, -89.9, -60, -30, 0, 30, 60, 89.9, 89.999, 90 };
	const double rolls[]      = { 0, 25, -25, 90, -90, 179, 180 };
	const double azimuths[]   = { -3.0, -1.2, 0.0, 0.7, 2.0, kPi };

	double worstR3 = 0.0, worstRot = 0.0, worstPole = 0.0;
	int cases = 0;
	for (double elDeg : elevations)
		for (double rhoDeg : rolls)
			for (double az : azimuths)
			{
				const TMatrix4<float> r = RolledTurntable(az, elDeg * kDeg, rhoDeg * kDeg);
				const TurntableAngles a = TurntableFromRotation(r);
				const TMatrix4<float> s = RotationFromTurntable(a.azimuth, a.elevation);
				++cases;

				EXPECT_EQ(s.at(0, 2), 0.f);
				EXPECT_GE(s.at(1, 2), 0.f);

				double dR3 = 0.0;
				for (int i = 0; i < 3; ++i)
					dR3 = std::max(dR3, std::fabs((double)s.at(2, i) - (double)r.at(2, i)));
				EXPECT_LE(dR3, 1e-6) << "el " << elDeg << " roulis " << rhoDeg << " az " << az;
				worstR3 = std::max(worstR3, dR3);

				const double rotation = ImageRotation(r, s);
				if (std::fabs(elDeg) == 90.0)
				{
					EXPECT_LE(std::fabs(rotation), 1e-6) << "pole, roulis " << rhoDeg << " az " << az;
					worstPole = std::max(worstPole, std::fabs(rotation));
				}
				else
				{
					const double roll = std::atan2((double)r.at(0, 2), (double)r.at(1, 2));
					const double err = std::fabs(Wrap(rotation + roll));
					EXPECT_LE(err, 1e-6) << "el " << elDeg << " roulis " << rhoDeg << " az " << az;
					worstRot = std::max(worstRot, err);
				}
			}
	EXPECT_EQ(cases, 462);
	std::cout << "[ TURNTABLE ] redressement, " << cases << " cas : max |r3' - r3| = " << worstR3
	          << ", max |rotation + roulis| = " << worstRot << " rad, pole : " << worstPole << " rad\n";
}

// 5 bis. Roulis : 0 pour une vue de dessus exacte, zeros signes compris
// (l'identite de `camera azel 0 0` ; atan2(+0, -0) vaudrait pi) ; la valeur
// posee hors du pole ; pi pour une vue retournee.
TEST(TEST_cgmath_turntable, roll_is_zero_at_exact_top_view)
{
	for (float r02 : { 0.f, -0.f })
		for (float r12 : { 0.f, -0.f })
		{
			TMatrix4<float> r;   // identite : vue de dessus
			r.at(0, 2) = r02;
			r.at(1, 2) = r12;
			EXPECT_EQ(RollOfRotation(r), 0.0)
				<< "R[0][2] = " << (std::signbit(r02) ? "-0" : "+0")
				<< ", R[1][2] = " << (std::signbit(r12) ? "-0" : "+0");
		}

	EXPECT_NEAR(RollOfRotation(RolledTurntable(0.7, 30.0 * kDeg, 25.0 * kDeg)), 25.0 * kDeg, 1e-6);
	EXPECT_NEAR(std::fabs(RollOfRotation(RolledTurntable(0.7, 30.0 * kDeg, kPi))), kPi, 1e-6);
}
