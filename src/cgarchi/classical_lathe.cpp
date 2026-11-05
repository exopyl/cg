#include <cgarchi/classical_lathe.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

#include <cgarchi/classical_flutes.h>
#include "mesh.h"

namespace cgarchi
{
namespace classical
{

namespace
{

constexpr double kPi = 3.14159265358979323846;

int RoundToEight (int n)
{
	return std::max (8, (n + 7) / 8 * 8);
}

bool SameY (double a, double b)
{
	return std::fabs (a - b) <= 1e-12 * std::max (1.0, std::max (std::fabs (a), std::fabs (b)));
}

} // namespace

std::vector<double> MergeAngles (std::vector<double> primary, const std::vector<double> &secondary)
{
	const double tol = kFluteAngleTolerance;
	auto normalise = [] (std::vector<double> &v) {
		for (double &a : v)
		{
			a = std::fmod (a, 2.0 * kPi);
			if (a < 0.0)
				a += 2.0 * kPi;
		}
		std::sort (v.begin (), v.end ());
		v.erase (std::unique (v.begin (), v.end (), [] (double a, double b) { return b - a < kFluteAngleTolerance; }),
		         v.end ());
		while (v.size () > 1 && v.back () > 2.0 * kPi - kFluteAngleTolerance)
			v.pop_back ();
	};
	normalise (primary);
	std::vector<double> extra = secondary;
	normalise (extra);
	auto near = [&] (double a) {
		const auto it = std::lower_bound (primary.begin (), primary.end (), a - tol);
		if (it != primary.end () && *it <= a + tol)
			return true;
		// Voisinage de 0 et 2 pi.
		return (!primary.empty () && (a < tol || a > 2.0 * kPi - tol) && primary.front () < tol);
	};
	std::vector<double> out = primary;
	for (double a : extra)
		if (!near (a))
			out.push_back (a);
	std::sort (out.begin (), out.end ());
	return out;
}

std::vector<double> LatheAngles (const ColumnProfile &profile, const LatheOptions &options)
{
	std::vector<double> angles;
	if (profile.flutes)
	{
		// Grille des cannelures : centres, sommets interieurs et aretes de
		// chaque cannelure (aretes des anneaux de tete comprises), plus les
		// angles k pi/4 des anneaux carres.
		const FluteGrid &g = *profile.flutes;
		for (int j = 0; j < g.count; ++j)
			for (double offset : g.offsets)
				for (double side : { -1.0, 1.0 })
				{
					double t = 2.0 * kPi * j / g.count + side * offset;
					if (t < 0.0)
						t += 2.0 * kPi;
					angles.push_back (t);
				}
		for (int k = 0; k < 8; ++k)
			angles.push_back (kPi * k / 4.0);
	}
	else
	{
		const int n = RoundToEight (options.radialSegments);
		angles.reserve (n);
		for (int i = 0; i < n; ++i)
			angles.push_back (2.0 * kPi * i / n);
	}

	std::vector<double> crossings;
	if (options.splitAtPlanCrossings)
	{
		for (std::size_t i = 1; i < profile.vertices.size (); ++i)
		{
			const ProfileVertex &lo = profile.vertices[i - 1];
			const ProfileVertex &hi = profile.vertices[i];
			if (!SameY (lo.y, hi.y))
				continue;
			// Une racine t de [0, pi/4] vaut pour les 8 images k pi/2 +- t.
			for (double t : RingCrossings (lo.ring, hi.ring))
				for (int k = 0; k < 4; ++k)
				{
					crossings.push_back (k * 0.5 * kPi + t);
					crossings.push_back (k * 0.5 * kPi + 0.5 * kPi - t);
				}
		}
	}
	return MergeAngles (std::move (angles), crossings);
}

std::unique_ptr<Mesh> LatheColumnProfile (const ColumnProfile &profile, const LatheOptions &options)
{
	const std::vector<ProfileVertex> &pv = profile.vertices;
	if (pv.size () < 2)
		throw std::invalid_argument ("LatheColumnProfile : profil de moins de deux sommets");

	const std::vector<double> angles = LatheAngles (profile, options);
	const std::size_t na = angles.size ();
	const double scale = options.scale;

	std::vector<std::array<double, 3>> positions;
	std::vector<std::array<unsigned, 3>> triangles;
	positions.reserve (pv.size () * na + 2);
	triangles.reserve (2 * pv.size () * na);

	std::vector<double> cosines (na), sines (na);
	for (std::size_t k = 0; k < na; ++k)
	{
		cosines[k] = std::cos (angles[k]);
		sines[k] = std::sin (angles[k]);
	}

	std::vector<unsigned> previous, current (na);
	std::vector<double> previousRadius, currentRadius (na);
	for (std::size_t i = 0; i < pv.size (); ++i)
	{
		const bool flat = i > 0 && SameY (pv[i - 1].y, pv[i].y);
		for (std::size_t k = 0; k < na; ++k)
		{
			const double r = pv[i].ring.RadiusAt (angles[k]);
			currentRadius[k] = r;
			if (flat && std::fabs (r - previousRadius[k]) <= 1e-9 * std::max (1.0, r))
			{
				current[k] = previous[k];
				continue;
			}
			current[k] = static_cast<unsigned> (positions.size ());
			positions.push_back ({r * cosines[k] * scale, r * sines[k] * scale, pv[i].y * scale});
		}

		if (i > 0)
		{
			for (std::size_t k = 0; k < na; ++k)
			{
				const std::size_t k1 = (k + 1) % na;
				const unsigned a = previous[k], b = previous[k1], c = current[k1], d = current[k];
				const bool ad = a == d, bc = b == c;
				if (ad && bc)
					continue;
				if (ad)
					triangles.push_back ({a, b, c});
				else if (bc)
					triangles.push_back ({a, b, d});
				else
				{
					triangles.push_back ({a, b, c});
					triangles.push_back ({a, c, d});
				}
			}
		}
		std::swap (previous, current);
		std::swap (previousRadius, currentRadius);
		current.resize (na);
		currentRadius.resize (na);
	}

	// Fermetures en eventail. Le premier anneau occupe les indices 0 .. na-1
	// (il ne partage aucun sommet) ; le dernier est `previous` apres la boucle.
	std::vector<unsigned> first (na);
	for (std::size_t k = 0; k < na; ++k)
		first[k] = static_cast<unsigned> (k);
	const std::vector<unsigned> &last = previous;

	const unsigned bottom = static_cast<unsigned> (positions.size ());
	positions.push_back ({0.0, 0.0, pv.front ().y * scale});
	const unsigned top = static_cast<unsigned> (positions.size ());
	positions.push_back ({0.0, 0.0, pv.back ().y * scale});
	for (std::size_t k = 0; k < na; ++k)
	{
		const std::size_t k1 = (k + 1) % na;
		triangles.push_back ({bottom, first[k1], first[k]});
		triangles.push_back ({top, last[k], last[k1]});
	}

	auto mesh = std::make_unique<Mesh> (static_cast<unsigned> (positions.size ()),
	                                    static_cast<unsigned> (triangles.size ()));
	for (std::size_t v = 0; v < positions.size (); ++v)
		mesh->SetVertex (static_cast<unsigned> (v), static_cast<float> (positions[v][0]),
		                 static_cast<float> (positions[v][1]), static_cast<float> (positions[v][2]));
	for (std::size_t f = 0; f < triangles.size (); ++f)
		mesh->FaceAt (static_cast<unsigned> (f))->SetTriangle (triangles[f][0], triangles[f][1], triangles[f][2]);
	return mesh;
}

} // namespace classical
} // namespace cgarchi
