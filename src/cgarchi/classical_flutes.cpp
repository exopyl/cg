#include <cgarchi/classical_flutes.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace cgarchi
{
namespace classical
{

namespace
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kHalfSqrt3 = 0.86602540378443865;

} // namespace

double FlutedRadius (double R, int count, double halfAngle, FluteSection section, double theta)
{
	const double pitch = 2.0 * kPi / count;
	const double offset = theta - std::round (theta / pitch) * pitch;
	// Arete : l'angle de grille d'une arete, recalcule modulo le pas, peut
	// tomber d'un ulp a l'interieur.
	if (std::fabs (offset) >= halfAngle - kFluteAngleTolerance)
		return R;
	const double s = std::sin (offset), c = std::cos (offset);
	const double chord = 2.0 * R * std::sin (halfAngle);
	double centre = 0.0, radius = 0.0;
	if (section == FluteSection::Semicircle)
	{
		centre = R * std::cos (halfAngle);
		radius = 0.5 * chord;
	}
	else
	{
		centre = R * std::cos (halfAngle) + chord * kHalfSqrt3;
		radius = chord;
	}
	// Racine proche de l'axe du rayon issu de l'axe avec le cercle de coupe.
	const double disc = radius * radius - centre * centre * s * s;
	return centre * c - std::sqrt (std::max (0.0, disc));
}

double FluteHalfAngle (const OrderData &order)
{
	if (!order.flutes)
		throw std::invalid_argument ("FluteHalfAngle : ordre sans cannelures : " + order.key);
	const FluteData &f = *order.flutes;
	double lambda = 0.0;
	if (f.listels)
		lambda = f.footListel / (kPi * order.general.lowerDiameter_p / f.count);
	return kPi / f.count * (1.0 - lambda);
}

double Segment60TowardAxisRadius (double R, double halfAngle, double offset)
{
	const double chord = 2.0 * R * std::sin (halfAngle);
	const double centre = R * std::cos (halfAngle) - chord * kHalfSqrt3;
	const double s = std::sin (offset), c = std::cos (offset);
	return centre * c + std::sqrt (std::max (0.0, chord * chord - centre * centre * s * s));
}

std::vector<double> FluteSampleOffsets (double halfAngle, FluteSection section, int samples)
{
	const int m = std::max (1, samples);
	// Geometrie a R = 1 : les decalages ne dependent que de beta.
	const double chord = 2.0 * std::sin (halfAngle);
	std::vector<double> out {0.0};
	if (section == FluteSection::Semicircle)
	{
		const double centre = std::cos (halfAngle), radius = 0.5 * chord;
		// Pied de la paroi : racine proche a l'angle beta, rayon cos 2 beta.
		const double foot = std::cos (2.0 * halfAngle);
		const double phiFoot = std::atan2 (foot * std::sin (halfAngle), centre - foot * std::cos (halfAngle));
		for (int k = 1; k < m; ++k)
		{
			const double phi = phiFoot * k / m;
			out.push_back (std::atan2 (radius * std::sin (phi), centre - radius * std::cos (phi)));
		}
		out.push_back (halfAngle * (1.0 - kFluteWallGap));
		out.push_back (halfAngle);
	}
	else
	{
		const double centre = std::cos (halfAngle) + chord * kHalfSqrt3, radius = chord;
		const double psiFoot = kPi / 6.0;   // demi-arc de 60 degres
		for (int k = 1; k < m; ++k)
		{
			const double psi = psiFoot * k / m;
			out.push_back (std::atan2 (radius * std::sin (psi), centre - radius * std::cos (psi)));
		}
		out.push_back (halfAngle);
	}
	return out;
}

std::vector<double> HeadHalfAngles (double halfAngle, FluteSection section, int samples, int rings)
{
	const std::vector<double> offsets = FluteSampleOffsets (halfAngle, section, samples);
	// Sommets de l'arc seuls (ni fond ni paroi), indices 1 .. m-1.
	const int m = std::max (1, samples);
	std::vector<double> out;
	const int n = std::max (0, std::min (rings, m - 1));
	for (int i = 1; i <= n; ++i)
		out.push_back (offsets[static_cast<std::size_t> (m - (i * m) / (n + 1))]);
	std::sort (out.begin (), out.end (), std::greater<double> ());
	out.erase (std::unique (out.begin (), out.end ()), out.end ());
	return out;
}

std::vector<double> FluteGridOffsets (double halfAngle, FluteSection section, int samples, int headRings)
{
	std::vector<double> out = FluteSampleOffsets (halfAngle, section, samples);
	if (section == FluteSection::Semicircle)
		for (double head : HeadHalfAngles (halfAngle, section, samples, headRings))
			out.push_back (head * (1.0 - kFluteWallGap));
	std::sort (out.begin (), out.end ());
	out.erase (std::unique (out.begin (), out.end (), [] (double x, double y) { return y - x < kFluteAngleTolerance; }),
	           out.end ());
	return out;
}

} // namespace classical
} // namespace cgarchi
