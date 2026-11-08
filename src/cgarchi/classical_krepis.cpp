#include <cgarchi/classical_krepis.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cgarchi/classical_mesh.h>
#include <cgarchi/classical_temple_refinement.h>
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

std::unique_ptr<Mesh> BuildKrepisMesh (const KrepisSettings &settings, const RefinementStations &stations,
                                       KrepisLayout *layout, std::vector<KrepisStepSize> *steps)
{
	KrepisLayout built = LayoutKrepis (settings);
	std::vector<KrepisStepSize> sizes;
	sizes.reserve (built.steps.size ());
	// Sans aucune station : la krepis d'origine, au bit pres.
	if (stations.x.empty () && stations.y.empty () && stations.xLower.empty () && stations.yLower.empty ())
	{
		std::unique_ptr<Mesh> mesh = BuildKrepisMesh (settings);
		sizes.assign (built.steps.size (), KrepisStepSize {16, 12});
		if (layout)
			*layout = std::move (built);
		if (steps)
			*steps = std::move (sizes);
		return mesh;
	}

	// Une reservation pour toute la krepis.
	auto inside = [] (const std::vector<double> &s, double half) {
		return static_cast<std::size_t> (std::count_if (s.begin (), s.end (), [half] (double v) { return -half < v && v < half; }));
	};
	std::size_t totalVertices = 0;
	for (const KrepisBox &box : built.steps)
		totalVertices += (inside (stations.x, box.halfWidth) + 2) * (inside (stations.y, box.halfDepth) + 2) +
		                 (inside (stations.xLower, box.halfWidth) + 2) * (inside (stations.yLower, box.halfDepth) + 2);
	std::vector<std::array<double, 3>> positions;
	std::vector<std::array<unsigned, 3>> triangles;
	positions.reserve (totalVertices);
	triangles.reserve (2 * totalVertices);
	std::vector<unsigned> low, high;   // aretes d'un cote, reutilisees
	for (const KrepisBox &box : built.steps)
	{
		const std::vector<double> xt = SpanWithStations (-box.halfWidth, box.halfWidth, stations.x);
		const std::vector<double> yt = SpanWithStations (-box.halfDepth, box.halfDepth, stations.y);
		const std::vector<double> xb = SpanWithStations (-box.halfWidth, box.halfWidth, stations.xLower);
		const std::vector<double> yb = SpanWithStations (-box.halfDepth, box.halfDepth, stations.yLower);
		const unsigned firstVertex = static_cast<unsigned> (positions.size ());
		const std::size_t firstFace = triangles.size ();
		const unsigned top = firstVertex, bottom = firstVertex + static_cast<unsigned> (xt.size () * yt.size ());
		low.reserve (std::max (xb.size (), yb.size ()));
		high.reserve (std::max (xt.size (), yt.size ()));
		auto at = [] (unsigned grid, const std::vector<double> &ys, std::size_t i, std::size_t j) {
			return grid + static_cast<unsigned> (i * ys.size () + j);
		};
		for (const double x : xt)
			for (const double y : yt)
				positions.push_back ({x, y, box.zTop});
		for (const double x : xb)
			for (const double y : yb)
				positions.push_back ({x, y, box.zBottom});
		// Dessus, normale +z ; dessous, normale -z.
		for (std::size_t i = 0; i + 1 < xt.size (); ++i)
			for (std::size_t j = 0; j + 1 < yt.size (); ++j)
			{
				const unsigned a = at (top, yt, i, j), b = at (top, yt, i + 1, j), c = at (top, yt, i + 1, j + 1),
				               d = at (top, yt, i, j + 1);
				triangles.push_back ({a, b, c});
				triangles.push_back ({a, c, d});
			}
		for (std::size_t i = 0; i + 1 < xb.size (); ++i)
			for (std::size_t j = 0; j + 1 < yb.size (); ++j)
			{
				const unsigned a = at (bottom, yb, i, j), b = at (bottom, yb, i, j + 1), c = at (bottom, yb, i + 1, j + 1),
				               d = at (bottom, yb, i + 1, j);
				triangles.push_back ({a, b, c});
				triangles.push_back ({a, c, d});
			}
		// Cotes : facade y = -demi-cote (normale -y) et dos (+y) le long de X ;
		// flancs x = +demi-cote (normale +x) et -demi-cote (-x) le long de Y.
		for (const bool back : {false, true})
		{
			low.clear ();
			high.clear ();
			for (std::size_t i = 0; i < xb.size (); ++i)
				low.push_back (at (bottom, yb, i, back ? yb.size () - 1 : 0));
			for (std::size_t i = 0; i < xt.size (); ++i)
				high.push_back (at (top, yt, i, back ? yt.size () - 1 : 0));
			AppendZipper (low, xb, high, xt, back, triangles);
		}
		for (const bool left : {false, true})
		{
			low.clear ();
			high.clear ();
			for (std::size_t j = 0; j < yb.size (); ++j)
				low.push_back (at (bottom, yb, left ? 0 : xb.size () - 1, j));
			for (std::size_t j = 0; j < yt.size (); ++j)
				high.push_back (at (top, yt, left ? 0 : xt.size () - 1, j));
			AppendZipper (low, yb, high, yt, left, triangles);
		}
		sizes.push_back ({static_cast<unsigned> (positions.size ()) - firstVertex,
		                  static_cast<unsigned> (triangles.size () - firstFace)});
	}
	std::unique_ptr<Mesh> mesh = MakeTriangleMesh (positions, triangles);
	if (layout)
		*layout = std::move (built);
	if (steps)
		*steps = std::move (sizes);
	return mesh;
}

} // namespace classical
} // namespace cgarchi
