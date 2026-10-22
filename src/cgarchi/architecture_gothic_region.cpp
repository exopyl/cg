#include <cgarchi/architecture_gothic_region.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

#include "clipper2/clipper.h"
#include "contour_validation.h"

namespace cgarchi
{

namespace
{
	using Clipper2Lib::FillRule;
	using Clipper2Lib::PathD;
	using Clipper2Lib::PathsD;

	// Bornes de ClipperD (CheckPrecisionRange) : hors de cette plage, Clipper2
	// rend un resultat vide sans erreur.
	constexpr int kClipperMinPrecision = -8;
	constexpr int kClipperMaxPrecision = 8;

	double signedArea (const GothicRing &ring)
	{
		double sum = 0.0;
		const std::size_t n = ring.size();
		for (std::size_t i = 0; i < n; ++i)
		{
			const Vector2d &a = ring[i];
			const Vector2d &b = ring[(i + 1) % n];
			sum += a.x * b.y - b.x * a.y;
		}
		return 0.5 * sum;
	}

	// Anneau lu comme plein : sens trigonometrique, pour que la regle NonZero le
	// remplisse quel que soit le sens dans lequel il a ete trace, et que deux
	// anneaux qui se recouvrent s'additionnent au lieu de s'annuler.
	PathD filledPath (const GothicRing &ring)
	{
		PathD p;
		p.reserve(ring.size());
		if (signedArea(ring) >= 0.0)
			for (const Vector2d &v : ring) p.emplace_back(v.x, v.y);
		else
			for (auto it = ring.rbegin(); it != ring.rend(); ++it) p.emplace_back(it->x, it->y);
		return p;
	}

	// Booleen Clipper2 sans sommets colineaires : un sommet pose au milieu d'une
	// arete droite (pied d'arc accroche a la grille sur la verticale d'un
	// jambage) donne a la tessellation un triangle d'aire nulle.
	PathsD booleanOp (Clipper2Lib::ClipType op, const PathsD &subject, const PathsD &clip,
	                  int precision)
	{
		Clipper2Lib::ClipperD clipper(precision);
		clipper.PreserveCollinear(false);
		clipper.AddSubject(subject);
		if (!clip.empty())
			clipper.AddClip(clip);
		PathsD out;
		if (!clipper.Execute(op, FillRule::NonZero, out))
			throw std::runtime_error("buildGothicStoneRegion: echec du booleen Clipper2");
		return out;
	}

	// Echelle de la grille de ClipperD a cette precision : 2^(ilogb(10^precision) + 1)
	// points par unite.
	double gridScale (int precision)
	{
		return std::pow(static_cast<double>(std::numeric_limits<double>::radix),
		                std::ilogb(std::pow(10.0, precision)) + 1);
	}

	// Decalage de delta (unites des anneaux), jonctions rondes, calcule sur la
	// grille entiere du booleen : les anneaux sortis d'un booleen y sont deja,
	// la conversion est exacte.
	PathsD offsetOnGrid (const PathsD &paths, double delta, int precision)
	{
		const double scale = gridScale(precision);
		Clipper2Lib::Paths64 in;
		in.reserve(paths.size());
		for (const PathD &p : paths)
		{
			Clipper2Lib::Path64 q;
			q.reserve(p.size());
			for (const auto &pt : p)
				q.emplace_back(static_cast<int64_t>(std::llround(pt.x * scale)),
				               static_cast<int64_t>(std::llround(pt.y * scale)));
			in.push_back(std::move(q));
		}
		Clipper2Lib::ClipperOffset offset;
		offset.AddPaths(in, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);
		Clipper2Lib::Paths64 solution;
		offset.Execute(delta * scale, solution);
		PathsD out;
		out.reserve(solution.size());
		for (const Clipper2Lib::Path64 &p : solution)
		{
			PathD q;
			q.reserve(p.size());
			for (const auto &pt : p) q.emplace_back(pt.x / scale, pt.y / scale);
			out.push_back(std::move(q));
		}
		return out;
	}

	// Fermeture (dilatation puis erosion) de la reunion des vides, de rayon r.
	PathsD closeVoids (const PathsD &voids, double r, int precision)
	{
		auto unite = [precision](const PathsD &p) {
			return booleanOp(Clipper2Lib::ClipType::Union, p, {}, precision);
		};
		return unite(offsetOnGrid(unite(offsetOnGrid(voids, r, precision)), -r, precision));
	}

	// Ouverture puis fermeture de la reunion des vides, de rayon r.
	PathsD openThenClose (const PathsD &voids, double r, int precision)
	{
		auto unite = [precision](const PathsD &p) {
			return booleanOp(Clipper2Lib::ClipType::Union, p, {}, precision);
		};
		const PathsD opened = unite(offsetOnGrid(unite(offsetOnGrid(voids, -r, precision)), r, precision));
		return closeVoids(opened, r, precision);
	}

	std::vector<GothicRing> toRings (const PathsD &paths)
	{
		std::vector<GothicRing> region;
		region.reserve(paths.size());
		for (const PathD &p : paths)
		{
			if (p.size() < 3)
				continue;
			GothicRing ring;
			ring.reserve(p.size());
			for (const auto &pt : p) ring.emplace_back(pt.x, pt.y);
			region.push_back(std::move(ring));
		}
		return region;
	}

	PathsD filledPaths (const std::vector<GothicRing> &rings)
	{
		PathsD out;
		out.reserve(rings.size());
		for (const GothicRing &r : rings)
			if (r.size() >= 3) out.push_back(filledPath(r));
		return out;
	}
}

std::vector<GothicRing> buildGothicStoneRegion (const GothicStoneInput &input, int precision,
                                                int guardGridSteps, bool *guardApplied,
                                                GothicGuardOutcome *outcome)
{
	if (precision < kClipperMinPrecision || precision > kClipperMaxPrecision)
		throw std::invalid_argument("buildGothicStoneRegion: precision " + std::to_string(precision)
		                            + " hors de [-8, 8]");
	if (guardGridSteps < 0)
		throw std::invalid_argument("buildGothicStoneRegion: guardGridSteps "
		                            + std::to_string(guardGridSteps) + " negatif");
	if (guardApplied)
		*guardApplied = false;
	if (outcome)
		*outcome = GothicGuardOutcome::NotNeeded;
	if (input.frame.size() < 3)
		return {};

	// Chaque fenetre ne borne que SES sous-vides : intersection par fenetre, puis
	// une seule reunion de tous les vides. Les anneaux simples sont orientes en
	// sens positif et chaque intersection est une region Clipper2 (enroulement 0
	// ou 1 partout) : la somme des enroulements est non nulle exactement la ou
	// au moins un vide couvre, donc la regle NonZero sur l'ensemble donne leur
	// reunion, recouvrements et ilots compris.
	PathsD allVoids = filledPaths(input.voids);
	for (const GothicClippedVoids &group : input.clippedVoids)
	{
		if (group.window.size() < 3)
			continue;
		const PathsD sub = booleanOp(Clipper2Lib::ClipType::Union, filledPaths(group.voids), {}, precision);
		if (sub.empty())
			continue;
		const PathsD inside = booleanOp(Clipper2Lib::ClipType::Intersection, sub,
		                                PathsD{ filledPath(group.window) }, precision);
		allVoids.insert(allVoids.end(), inside.begin(), inside.end());
	}
	const PathsD voids = booleanOp(Clipper2Lib::ClipType::Union, allVoids, {}, precision);
	const PathsD frame { filledPath(input.frame) };

	std::vector<GothicRing> region = toRings(booleanOp(Clipper2Lib::ClipType::Difference, frame,
	                                                   voids, precision));
	if (guardGridSteps == 0 || region.empty()
	    || ValidateContours(gothicRegionToPolygon2(region)).IsValid())
		return region;

	if (guardApplied)
		*guardApplied = true;
	const double radius = guardGridSteps / gridScale(precision);
	auto stoneOf = [&](const PathsD &guardedVoids) {
		return toRings(booleanOp(Clipper2Lib::ClipType::Difference, frame, guardedVoids, precision));
	};
	auto isValid = [](const std::vector<GothicRing> &r) {
		return ValidateContours(gothicRegionToPolygon2(r)).IsValid();
	};

	std::vector<GothicRing> guarded = stoneOf(openThenClose(voids, radius, precision));
	if (isValid(guarded))
	{
		if (outcome)
			*outcome = GothicGuardOutcome::Applied;
		return guarded;
	}
	std::vector<GothicRing> closed = stoneOf(closeVoids(voids, radius, precision));
	if (isValid(closed))
	{
		if (outcome)
			*outcome = GothicGuardOutcome::ClosingFallback;
		return closed;
	}
	if (outcome)
		*outcome = GothicGuardOutcome::Insufficient;
	return guarded;
}

Polygon2 gothicRegionToPolygon2 (const std::vector<GothicRing> &region)
{
	Polygon2 poly;
	poly.alloc_contours(static_cast<int>(region.size()));
	std::vector<float> packed;
	for (std::size_t i = 0; i < region.size(); ++i)
	{
		packed.clear();
		packed.reserve(2 * region[i].size());
		for (const Vector2d &v : region[i])
		{
			packed.push_back(static_cast<float>(v.x));
			packed.push_back(static_cast<float>(v.y));
		}
		poly.add_contour(static_cast<unsigned int>(i), static_cast<unsigned int>(region[i].size()),
		                 packed.data());
	}
	return poly;
}

std::vector<ExtrudeContour> gothicRegionToContours (const std::vector<GothicRing> &region)
{
	std::vector<ExtrudeContour> out;
	out.reserve(region.size());
	for (const GothicRing &ring : region)
	{
		ExtrudeContour c;
		c.pts.reserve(ring.size());
		for (const Vector2d &v : ring)
			c.pts.emplace_back(static_cast<float>(v.x), static_cast<float>(v.y));
		c.isHole = signedArea(ring) < 0.0;
		out.push_back(std::move(c));
	}
	return out;
}

} // namespace cgarchi
