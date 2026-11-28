//
// Decalage de profondeur du remplissage (src/cgre/polygon_offset.h).
//
// Regle testee : le decalage applique a TOUTES les surfaces ne doit jamais
// inverser l'ordre de profondeur de deux faces non paralleles. Cas construit :
// un pan et une face cachee 2,3 mm dessous, inclinee de 13,75 degres (pente du
// pan du toit d'Hephaistos) autour d'un axe perpendiculaire au regard. C'est la
// configuration la plus defavorable au terme de pente, pas la geometrie des vues
// mesurees, qui regardent le long du faitage.
//
// AUCUN CONTEXTE OPENGL. Le decalage est calcule par la formule de la
// specification (OpenGL 4.6, 14.6.5) : o = m * factor + r * units, ou m est la
// pente de la profondeur fenetre de la face (approximation autorisee
// max(|dz/dx|, |dz/dy|)) et r la resolution d'un tampon de profondeur de 24 bits.
// Les profondeurs fenetre viennent d'une projection perspective (gluPerspective,
// 45 degres) evaluee aux centres de pixels.
//
// Ce que ce test GARANTIT : les constantes de cgre n'inversent pas l'ordre des
// deux faces selon la specification, de l'incidence de la vue A (24,5 degres)
// jusqu'a 1 degre. Il contraint en pratique le facteur ; la valeur des unites
// n'est contrainte que par son signe, le choix de 8 releve des mesures. Comme
// dz/dx est nul par construction, l'approximation max et la norme coincident.
// Ce qu'il ne garantit PAS : le comportement d'un pilote (ecart d'interpolation
// entre lignes et faces, MSAA) ni la visibilite du fil de fer, mesures par la
// reproduction hors ecran (debt_sinaia.md, section 8) ; que les trois sites
// d'appel lisent ces constantes, verifie par lecture.
//

#ifdef CG_HAS_CGRE

#include <gtest/gtest.h>

#include "../src/cgre/polygon_offset.h"

#include <algorithm>
#include <cmath>

namespace
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;

// Distance, plans de coupe et viewport de la vue A de sinaia (la geometrie des
// faces, elle, est construite : voir MakeRoofCase).
constexpr double kDistance = 14505.0;
constexpr double kNear = 38.9514;
constexpr double kFar = 38951.4;
constexpr double kViewportHeight = 598.0;
constexpr double kFovy = 45.0 * kDeg;

constexpr double kGapMm = 2.3;
constexpr double kPanSlope = 13.75 * kDeg;

// Plus petite difference de profondeur fenetre resoluble, tampon de 24 bits.
const double kDepthResolution = std::ldexp (1.0, -24);

struct Vec3 { double x, y, z; };

double Dot (const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }

struct Plane { Vec3 point; Vec3 normal; };

// Oeil a l'origine, regard vers -z. (px, py) en pixels depuis le centre.
double WindowDepth (const Plane& plane, double px, double py)
{
	const double s = std::tan (kFovy / 2.0) / (kViewportHeight / 2.0);
	const Vec3 dir { px * s, py * s, -1.0 };
	const double t = Dot (plane.normal, plane.point) / Dot (plane.normal, dir);
	const double zEye = -t;
	const double zNdc = (kFar + kNear) / (kFar - kNear) + 2.0 * kFar * kNear / ((kFar - kNear) * zEye);
	return 0.5 * zNdc + 0.5;
}

double MaxDepthSlope (const Plane& plane)
{
	const double dzdx = WindowDepth (plane, 0.5, 0.0) - WindowDepth (plane, -0.5, 0.0);
	const double dzdy = WindowDepth (plane, 0.0, 0.5) - WindowDepth (plane, 0.0, -0.5);
	return std::max (std::fabs (dzdx), std::fabs (dzdy));
}

double Offset (const Plane& plane, double factor, double units)
{
	return MaxDepthSlope (plane) * factor + kDepthResolution * units;
}

// Le pan, vu a `elevationDeg` au-dessus de son plan, et la face cachee
// horizontale passant kGapMm sous lui au point vise.
struct RoofCase
{
	Plane pan;
	Plane hidden;
};

RoofCase MakeRoofCase (double elevationDeg)
{
	const double e = elevationDeg * kDeg;
	const Vec3 panNormal { 0.0, std::cos (e), std::sin (e) };
	const Vec3 target { 0.0, 0.0, -kDistance };
	const Vec3 below { target.x - kGapMm * panNormal.x, target.y - kGapMm * panNormal.y, target.z - kGapMm * panNormal.z };
	const Vec3 hiddenNormal { 0.0, std::cos (e + kPanSlope), std::sin (e + kPanSlope) };
	return { { target, panNormal }, { below, hiddenNormal } };
}

// Vrai si le pan reste devant la face cachee sous le test GL_LESS, les deux
// remplissages portant le meme decalage, dans les deux ordres de dessin.
bool PanStaysInFront (const RoofCase& c, double factor, double units)
{
	const double zPan = WindowDepth (c.pan, 0.0, 0.0) + Offset (c.pan, factor, units);
	const double zHidden = WindowDepth (c.hidden, 0.0, 0.0) + Offset (c.hidden, factor, units);
	return zPan < zHidden;
}

constexpr double kElevationsDeg[] = { 24.5, 10.0, 5.0, 2.0, 1.0 };

} // namespace

TEST (CgreSurfacePolygonOffset, SlopeFactorIsZero)
{
	EXPECT_EQ (cgre::kSurfacePolygonOffsetFactor, 0.0f);
}

TEST (CgreSurfacePolygonOffset, UnitsArePositive)
{
	// Le terme constant est le seul qui garde les surimpressions au-dessus d'une
	// face vue de face.
	EXPECT_GT (cgre::kSurfacePolygonOffsetUnits, 0.0f);
}

// L'instrument doit savoir echouer : sans decalage le pan est devant, et un
// terme de pente de 1 le fait passer derriere a toutes les incidences testees.
TEST (CgreSurfacePolygonOffset, ModelDetectsPiercingWithSlopeTerm)
{
	for (double elevation : kElevationsDeg)
	{
		const RoofCase c = MakeRoofCase (elevation);
		EXPECT_TRUE (PanStaysInFront (c, 0.0, 0.0)) << "elevation " << elevation;
		EXPECT_FALSE (PanStaysInFront (c, 1.0, 1.0)) << "elevation " << elevation;
	}
}

TEST (CgreSurfacePolygonOffset, HiddenFaceStaysBehindRoofPan)
{
	for (double elevation : kElevationsDeg)
	{
		const RoofCase c = MakeRoofCase (elevation);
		EXPECT_TRUE (PanStaysInFront (c, cgre::kSurfacePolygonOffsetFactor, cgre::kSurfacePolygonOffsetUnits))
			<< "elevation " << elevation;
	}
}

#endif // CG_HAS_CGRE
