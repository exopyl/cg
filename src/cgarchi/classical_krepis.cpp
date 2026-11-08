#include <cgarchi/classical_krepis.h>

#include <cfloat>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

#include "mesh.h"
#include "polygon2.h"
#include "polygon_extrude.h"

namespace cgarchi
{
namespace classical
{

namespace
{

[[noreturn]] void Fail (const std::string &field, const std::string &rule)
{
	throw std::invalid_argument ("Krepis : '" + field + "' " + rule);
}

std::string StepField (std::size_t k, const char *member)
{
	return "lowerSteps[" + std::to_string (k) + "]." + member;
}

// Une longueur est dans [FLT_MIN, kKrepisMaxLength] : le maillage est en float.
bool InLengthRange (double value)
{
	return value >= static_cast<double> (FLT_MIN) && value <= kKrepisMaxLength;
}

void RequireLength (const std::string &field, double value)
{
	if (!(std::isfinite (value) && value > 0.0))
		Fail (field, "doit etre fini et strictement positif");
	if (!InLengthRange (value))
		Fail (field, "hors de la plage des longueurs representables en float");
}

bool SameFloat (double a, double b)
{
	return static_cast<float> (a) == static_cast<float> (b);
}

// Ajoute `step` a `sum`, apres avoir verifie qu'il n'est pas absorbe par
// l'arrondi en float de la somme.
void Accumulate (double &sum, double step, const std::string &field, const char *rule)
{
	if (SameFloat (sum, sum + step))
		Fail (field, rule);
	sum += step;
}

} // namespace

void ValidateKrepisSettings (const KrepisSettings &s)
{
	RequireLength ("stylobateHalfWidth", s.stylobateHalfWidth);
	RequireLength ("stylobateHalfDepth", s.stylobateHalfDepth);
	RequireLength ("stylobateHeight", s.stylobateHeight);
	if (s.lowerSteps.size () > static_cast<std::size_t> (kKrepisMaxLowerSteps))
		Fail ("lowerSteps", "compte " + std::to_string (s.lowerSteps.size ()) + " degres, plus de " +
		                        std::to_string (kKrepisMaxLowerSteps));
	for (std::size_t k = 0; k < s.lowerSteps.size (); ++k)
	{
		RequireLength (StepField (k, "height"), s.lowerSteps[k].height);
		RequireLength (StepField (k, "outsetX"), s.lowerSteps[k].outsetX);
		RequireLength (StepField (k, "outsetY"), s.lowerSteps[k].outsetY);
	}

	// Memes sommes, dans le meme ordre, que LayoutKrepis.
	const char *const heightRule = "absorbe par l'arrondi en float a la cote du pied du degre";
	double z = 0.0;
	for (std::size_t k = s.lowerSteps.size (); k-- > 0;)
		Accumulate (z, s.lowerSteps[k].height, StepField (k, "height"), heightRule);
	Accumulate (z, s.stylobateHeight, "stylobateHeight", heightRule);

	const char *const outsetRule = "absorbe par l'arrondi en float au demi-cote du degre de dessus";
	double halfWidth = s.stylobateHalfWidth;
	double halfDepth = s.stylobateHalfDepth;
	for (std::size_t k = 0; k < s.lowerSteps.size (); ++k)
	{
		Accumulate (halfWidth, s.lowerSteps[k].outsetX, StepField (k, "outsetX"), outsetRule);
		Accumulate (halfDepth, s.lowerSteps[k].outsetY, StepField (k, "outsetY"), outsetRule);
	}
}

KrepisLayout LayoutKrepis (const KrepisSettings &s)
{
	ValidateKrepisSettings (s);

	const std::size_t n = s.lowerSteps.size ();
	KrepisLayout layout;
	layout.steps.resize (n + 1);

	layout.steps[0].halfWidth = s.stylobateHalfWidth;
	layout.steps[0].halfDepth = s.stylobateHalfDepth;
	for (std::size_t k = 1; k <= n; ++k)
	{
		layout.steps[k].halfWidth = layout.steps[k - 1].halfWidth + s.lowerSteps[k - 1].outsetX;
		layout.steps[k].halfDepth = layout.steps[k - 1].halfDepth + s.lowerSteps[k - 1].outsetY;
	}

	// Une seule variable cumulee : le pied d'un degre est le dessus du degre de
	// dessous, au bit pres.
	double z = 0.0;
	for (std::size_t k = n; k >= 1; --k)
	{
		layout.steps[k].zBottom = z;
		z += s.lowerSteps[k - 1].height;
		layout.steps[k].zTop = z;
	}
	layout.steps[0].zBottom = z;
	z += s.stylobateHeight;
	layout.steps[0].zTop = z;
	layout.stylobateTop = z;
	return layout;
}

std::unique_ptr<Mesh> BuildKrepisMesh (const KrepisSettings &settings, KrepisLayout *layout)
{
	KrepisLayout built = LayoutKrepis (settings);

	auto mesh = std::make_unique<Mesh> ();
	for (const KrepisBox &box : built.steps)
	{
		const float x = static_cast<float> (box.halfWidth);
		const float y = static_cast<float> (box.halfDepth);
		// Contour exterieur en sens trigonometrique (convention d'extrudeToMesh).
		float corners[8] = {-x, -y, x, -y, x, y, -x, y};
		Polygon2 rectangle;
		rectangle.add_contour (0, 4, corners);
		Mesh step;
		extrudeToMesh (rectangle, step, box.zBottom, box.zTop);
		appendMesh (*mesh, step);
	}
	if (layout)
		*layout = std::move (built);
	return mesh;
}

} // namespace classical
} // namespace cgarchi
