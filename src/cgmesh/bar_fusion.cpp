#include "bar_fusion.h"
#include "bar_fusion_parallel.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include "clipper2/clipper.h"
#include "mesh.h"
#include "polygon2.h"

namespace
{
	using Clipper2Lib::ClipType;
	using Clipper2Lib::Path64;
	using Clipper2Lib::Paths64;
	using Clipper2Lib::Point64;

	// Tolerance d'arc des decalages, en pas : celle des bandes et de
	// l'ouverture, et celle, plus fine, de l'erosion d'imbrication et du dilate
	// de controle (0,1 pas sur des marges de quelques pas).
	constexpr double kArcToleranceSteps        = 0.64;
	constexpr double kNestingArcToleranceSteps = 0.1;
	constexpr double kMiterLimit               = 2.0;

	// Tolerance de cote sameHeight = kSameHeight fois la hauteur de la plus
	// grande barre, la meme pour toutes les barres : deux cotes plus proches
	// sont une seule hauteur (la plus basse du groupe est gardee) ; une barre
	// est a sa crete quand sa crete est a moins de sameHeight du niveau, et un
	// sommet de son profil appartient a la crete quand sa cote mise a l'echelle
	// est a moins de sameHeight de la crete. La coupe de l'empreinte est prise a
	// kFootSection fois la hauteur de la plus grande barre au-dessus du pied.
	constexpr double kSameHeight    = 1e-6;
	constexpr double kFootSection   = 1e-7;

	struct ProfileExtent
	{
		double uMin = 0.0;
		double uMax = 0.0;
	};

	ProfileExtent checkedProfile (const Profile2D &profile)
	{
		if (profile.points.size() < 3)
			throw std::invalid_argument("bar fusion: le profil doit compter au moins 3 points");
		ProfileExtent e { profile.points[0].x, profile.points[0].x };
		for (const Vector2d &p : profile.points)
		{
			if (!std::isfinite(p.x) || !std::isfinite(p.y))
				throw std::invalid_argument("bar fusion: coordonnee de profil non finie");
			e.uMin = std::min(e.uMin, p.x);
			e.uMax = std::max(e.uMax, p.x);
		}
		if (!(e.uMax > e.uMin))
			throw std::invalid_argument("bar fusion: profil de hauteur nulle");
		return e;
	}

	void checkScale (double f)
	{
		if (!std::isfinite(f) || !(f > 0.0))
			throw std::invalid_argument("bar fusion: echelle de barre " + std::to_string(f)
			                            + " non finie ou non positive");
	}

	std::vector<double> levelHeights (const Profile2D &profile, const ProfileExtent &e,
	                                  const std::vector<double> &scales)
	{
		if (scales.empty())
			return {};
		const double fMax = *std::max_element(scales.begin(), scales.end());
		const double range = (e.uMax - e.uMin) * fMax;
		std::vector<double> candidates = { e.uMin };
		for (const double f : scales)
			for (const Vector2d &p : profile.points)
				if (p.x > e.uMin)
					candidates.push_back(e.uMin + (p.x - e.uMin) * f);
		std::sort(candidates.begin(), candidates.end());
		std::vector<double> distinct;
		for (const double u : candidates)
			if (distinct.empty() || u - distinct.back() > kSameHeight * range)
				distinct.push_back(u);

		const double step = range / kBarFusionMinLevelSteps;
		std::vector<double> heights;
		for (std::size_t i = 0; i < distinct.size(); ++i)
		{
			heights.push_back(distinct[i]);
			if (i + 1 == distinct.size())
				break;
			const double gap = distinct[i + 1] - distinct[i];
			const int n = static_cast<int>(std::ceil(gap / step - 1e-9));
			for (int j = 1; j < n; ++j)
				heights.push_back(distinct[i] + gap * j / n);
		}
		return heights;
	}

	// Coupe du profil (ferme) a la cote u : intervalles de v, croissants.
	std::vector<std::pair<double, double>> profileSection (const Profile2D &profile, double u)
	{
		std::vector<double> vs;
		const std::size_t n = profile.points.size();
		for (std::size_t i = 0; i < n; ++i)
		{
			const Vector2d &a = profile.points[i];
			const Vector2d &b = profile.points[(i + 1) % n];
			if ((a.x <= u && b.x > u) || (b.x <= u && a.x > u))
				vs.push_back(a.y + (u - a.x) / (b.x - a.x) * (b.y - a.y));
		}
		std::sort(vs.begin(), vs.end());
		std::vector<std::pair<double, double>> out;
		for (std::size_t i = 0; i + 1 < vs.size(); i += 2)
			if (vs[i + 1] > vs[i])
				out.emplace_back(vs[i], vs[i + 1]);
		return out;
	}

	// Ruban de crete d'une barre d'echelle f : les v des sommets dont la cote,
	// mise a l'echelle, est a moins de `sameHeight` de la crete, elargis de
	// kBarFusionApexHalfWidthSteps pas apres mise a l'echelle.
	std::pair<double, double> apexRibbon (const Profile2D &profile, const ProfileExtent &e, double f,
	                                      double sameHeight)
	{
		double lo = 0.0, hi = 0.0;
		bool first = true;
		for (const Vector2d &p : profile.points)
			if ((e.uMax - p.x) * f <= sameHeight)
			{
				lo = first ? p.y : std::min(lo, p.y);
				hi = first ? p.y : std::max(hi, p.y);
				first = false;
			}
		const double d = kBarFusionApexHalfWidthSteps / kBarFusionGridScale / f;
		return { lo - d, hi + d };
	}

	// Contour sur la grille, en sens trigonometrique ; `side` vaut -1 si le
	// contour a ete retourne (la droite du parcours d'origine est alors a gauche).
	Path64 gridRing (const std::vector<Vector2d> &outline, double &side)
	{
		double twiceArea = 0.0;
		for (std::size_t i = 0; i < outline.size(); ++i)
		{
			const Vector2d &a = outline[i], &b = outline[(i + 1) % outline.size()];
			twiceArea += a.x * b.y - b.x * a.y;
		}
		side = (twiceArea < 0.0) ? -1.0 : 1.0;
		Path64 p;
		p.reserve(outline.size());
		auto push = [&p](const Vector2d &v) {
			const Point64 q(static_cast<int64_t>(std::llround(v.x * kBarFusionGridScale)),
			                static_cast<int64_t>(std::llround(v.y * kBarFusionGridScale)));
			if (p.empty() || !(p.back() == q))
				p.push_back(q);
		};
		if (side > 0.0)
			for (const Vector2d &v : outline) push(v);
		else
			for (auto it = outline.rbegin(); it != outline.rend(); ++it) push(*it);
		while (p.size() > 1 && p.front() == p.back())
			p.pop_back();
		return p;
	}

	Paths64 offsetBy (const Paths64 &paths, double steps, double arcToleranceSteps)
	{
		if (steps == 0.0)
			return paths;
		Clipper2Lib::ClipperOffset offset(kMiterLimit, arcToleranceSteps);
		offset.AddPaths(paths, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);
		Paths64 out;
		offset.Execute(steps, out);
		return out;
	}

	Paths64 booleanOp (ClipType op, const Paths64 &subject, const Paths64 &clip)
	{
		Clipper2Lib::Clipper64 clipper;
		clipper.PreserveCollinear(false);
		clipper.AddSubject(subject);
		if (!clip.empty())
			clipper.AddClip(clip);
		Paths64 out;
		if (!clipper.Execute(op, Clipper2Lib::FillRule::NonZero, out))
			throw std::runtime_error("bar fusion: echec du booleen Clipper2");
		return out;
	}

	struct GridBar
	{
		Path64 ring;
		double side  = 1.0;
		double scale = 1.0;
	};

	// Section de toutes les barres a la cote h (repere du profil). `sameHeight`
	// est la tolerance de fusion des hauteurs, en cote : une barre dont la crete
	// est a moins de cette distance de h y donne son ruban de crete.
	Paths64 levelSection (const std::vector<GridBar> &bars, const Profile2D &profile,
	                      const ProfileExtent &e, double h, double sameHeight)
	{
		Paths64 bands;
		for (const GridBar &bar : bars)
		{
			const double crest = e.uMin + (e.uMax - e.uMin) * bar.scale;
			std::vector<std::pair<double, double>> intervals;
			if (h > crest + sameHeight)
				continue;
			if (h >= crest - sameHeight)
				intervals.push_back(apexRibbon(profile, e, bar.scale, sameHeight));
			else
				intervals = profileSection(profile, e.uMin + (h - e.uMin) / bar.scale);
			for (const auto &iv : intervals)
			{
				double a = iv.first * bar.scale * bar.side * kBarFusionGridScale;
				double c = iv.second * bar.scale * bar.side * kBarFusionGridScale;
				if (a > c) std::swap(a, c);
				const Paths64 outer = offsetBy({ bar.ring }, c, kArcToleranceSteps);
				const Paths64 inner = offsetBy({ bar.ring }, a, kArcToleranceSteps);
				const Paths64 band = inner.empty() ? outer : booleanOp(ClipType::Difference, outer, inner);
				bands.insert(bands.end(), band.begin(), band.end());
			}
		}
		return booleanOp(ClipType::Union, bands, {});
	}

	// `level`, dilate de kBarFusionNestedMarginSteps, reste dans `below` : la
	// difference est vide sur la grille.
	int nodePass (Paths64 &A, Paths64 &B);
	int nodeSelf (Paths64 &level);
	int nodeCount (const Paths64 &A, const Paths64 &B);

	// Le niveau se croise ou se touche : une arete en coupe une autre, ou un
	// sommet est pose sur une autre arete (d'un meme contour ou de deux).
	bool crossesItself (const Paths64 &level)
	{
		return nodeCount(level, level) > 0;
	}

	// Noeud du niveau contre lui-meme, au plus kBarFusionNodePasses passes :
	// chaque croisement (arrondi a la grille) et chaque sommet pose sur une
	// autre arete devient un sommet commun des aretes concernees. *converged
	// dit si la derniere passe n'a plus rien trouve : le niveau rendu ne se
	// croise alors pas.
	Paths64 untangled (Paths64 level, bool *converged)
	{
		*converged = false;
		for (int pass = 0; pass < kBarFusionNodePasses; ++pass)
			if (nodeSelf(level) == 0)
			{
				*converged = true;
				break;
			}
		return level;
	}

	// `level`, simple, dilate de kBarFusionNestedMarginSteps, reste dans `below`.
	// `untangledLevel` : le noeud de `level` a converge, sa simplicite est acquise.
	bool keepsNestedMargin (const Paths64 &level, const Paths64 &below, bool untangledLevel)
	{
		if (level.empty())
			return true;
		if (!untangledLevel && crossesItself(level))
			return false;
		const Paths64 outside = booleanOp(ClipType::Difference,
		                                  offsetBy(level, kBarFusionNestedMarginSteps, kNestingArcToleranceSteps),
		                                  below);
		for (const Path64 &p : outside)
			if (Clipper2Lib::Area(p) != 0.0)
				return false;
		return true;
	}

	std::vector<BarGridRing> toRings (const Paths64 &paths)
	{
		std::vector<BarGridRing> rings;
		rings.reserve(paths.size());
		for (const Path64 &p : paths)
		{
			BarGridRing r;
			r.reserve(p.size());
			for (const Point64 &q : p)
				r.push_back({ q.x, q.y });
			rings.push_back(std::move(r));
		}
		return rings;
	}
}

std::vector<double> barFusionLevelHeights (const Profile2D &profile, const std::vector<double> &scales)
{
	const ProfileExtent e = checkedProfile(profile);
	for (const double f : scales)
		checkScale(f);
	return levelHeights(profile, e, scales);
}

namespace
{
	struct GridLevel
	{
		double  height = 0.0;
		Paths64 region;
	};

	std::vector<GridLevel> gridLevels (const std::vector<FusedBar> &bars, const Profile2D &profile,
	                                   BarFusionReport *report);
}

std::vector<BarFusionLevel> buildBarFusionLevels (const std::vector<FusedBar> &bars,
                                                  const Profile2D &profile,
                                                  BarFusionReport *report)
{
	const std::vector<GridLevel> grid = gridLevels(bars, profile, report);
	std::vector<BarFusionLevel> levels(grid.size());
	for (std::size_t k = 0; k < grid.size(); ++k)
	{
		levels[k].height = grid[k].height;
		levels[k].region = toRings(grid[k].region);
	}
	return levels;
}

namespace
{
// Niveaux imbriques a partir des regions nettoyees R (une par hauteur,
// R[0] au pied), derniers niveaux vides retires.
std::vector<GridLevel> nestedLevels (const std::vector<Paths64> &R, const std::vector<double> &heights,
                                     BarFusionReport *report)
{
	const std::size_t count = R.size();
	// Imbrication : chaque niveau est intersecte avec l'erosion du dernier
	// niveau garde, puis controle ; un echec est repare par une erosion plus
	// forte, ou le niveau est retire. Les controles sont independants une
	// fois les niveaux connus : une chaine d'au plus barFusionThreadLimit ()
	// niveaux est construite en supposant chaque controle reussi, ses
	// controles tournent en parallele et sont lus dans l'ordre. Au premier
	// echec, ce niveau est repare ou retire et la chaine repart du niveau
	// suivant ; la borne limite le travail perdu apres un echec. Sur un seul
	// fil, la chaine n'a qu'un maillon : rien n'est calcule d'avance.
	bool converged = false;
	std::vector<Paths64> kept = { untangled(R[0], &converged) };
	std::vector<double> keptHeights = { heights[0] };
	const std::size_t window = barFusionThreadLimit();
	struct Link
	{
		Paths64 level;
		bool    converged = false;
	};
	for (std::size_t k = 1; k < count; )
	{
		const std::size_t end = std::min(count, k + window);
		std::vector<BarFusionDeferred<Link>> chain;
		chain.reserve(end - k);
		for (std::size_t j = k; j < end; ++j)
		{
			const Paths64 &below = chain.empty() ? kept.back() : chain.back().value.level;
			BarFusionDeferred<Link> link;
			link.run([&] {
				Link l;
				l.level = untangled(booleanOp(ClipType::Intersection, R[j],
				                              offsetBy(below, -kBarFusionNestingSteps, kNestingArcToleranceSteps)),
				                    &l.converged);
				return l;
			});
			chain.push_back(std::move(link));
			if (chain.back().error)
				break;
		}
		std::vector<BarFusionDeferred<bool>> checks(chain.size());
		barFusionParallelFor(chain.size(), [&](std::size_t i) {
			if (chain[i].error)
				return;
			const Paths64 &below = (i == 0) ? kept.back() : chain[i - 1].value.level;
			checks[i].run([&] { return keepsNestedMargin(chain[i].value.level, below, chain[i].value.converged); });
		});
		std::size_t passed = 0;
		for (; passed < chain.size(); ++passed)
		{
			Link &link = chain[passed].get();
			if (!checks[passed].get())
				break;
			kept.push_back(std::move(link.level));
			keptHeights.push_back(heights[k + passed]);
		}
		k += passed;
		if (passed < chain.size())
		{
			const Paths64 &below = kept.back();
			Paths64 level = untangled(booleanOp(ClipType::Intersection, R[k],
			                                    offsetBy(below, -kBarFusionRepairNestingSteps, kNestingArcToleranceSteps)),
			                          &converged);
			if (keepsNestedMargin(level, below, converged))
			{
				if (report) ++report->repairedLevels;
				kept.push_back(std::move(level));
				keptHeights.push_back(heights[k]);
			}
			else if (report)
				++report->droppedLevels;
			++k;
		}
	}

	std::size_t used = kept.size();
	while (used > 0 && kept[used - 1].empty())
		--used;
	std::vector<GridLevel> levels(used);
	for (std::size_t k = 0; k < used; ++k)
	{
		levels[k].height = keptHeights[k];
		levels[k].region = std::move(kept[k]);
	}
	return levels;
}

std::vector<GridLevel> gridLevels (const std::vector<FusedBar> &bars, const Profile2D &profile,
                                   BarFusionReport *report)
{
	if (report)
		*report = BarFusionReport();
	const ProfileExtent e = checkedProfile(profile);
	std::vector<GridBar> gridBars;
	std::vector<double> scales;
	for (const FusedBar &b : bars)
	{
		checkScale(b.scale);
		GridBar g;
		g.ring = gridRing(b.outline, g.side);
		g.scale = b.scale;
		if (g.ring.size() < 3)
			continue;
		scales.push_back(b.scale);
		gridBars.push_back(std::move(g));
	}
	const std::vector<double> heights = levelHeights(profile, e, scales);
	const std::size_t count = heights.size();
	if (count == 0)
		return {};

	const double range = heights.back() - heights.front();
	const double sameHeight = kSameHeight * (e.uMax - e.uMin) * *std::max_element(scales.begin(), scales.end());
	// Sections, puis nettoyage : chaque niveau ne lit que des donnees
	// partagees en lecture et n'ecrit que son propre R[k].
	std::vector<Paths64> R(count);
	barFusionParallelFor(count, [&](std::size_t k) {
		R[k] = levelSection(gridBars, profile, e, (k == 0) ? e.uMin + kFootSection * range : heights[k], sameHeight);
	});

	for (std::size_t k = count - 1; k-- > 0; )
		if (!R[k + 1].empty())
			R[k] = booleanOp(ClipType::Union, R[k], R[k + 1]);

	barFusionParallelFor(count, [&](std::size_t k) {
		Paths64 &level = R[k];
		level = booleanOp(ClipType::Union, Clipper2Lib::RamerDouglasPeucker(level, kBarFusionSimplifySteps), {});
		level = booleanOp(ClipType::Union,
		                  offsetBy(offsetBy(level, -kBarFusionOpeningSteps, kArcToleranceSteps),
		                           kBarFusionOpeningSteps, kArcToleranceSteps), {});
	});

	return nestedLevels(R, heights, report);
}
}

namespace
{
	inline std::int64_t orient (const Point64 &a, const Point64 &b, const Point64 &c)
	{
		return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
	}

	inline int signOf (std::int64_t v) { return (v > 0) - (v < 0); }

	// p, colineaire a a-b, est strictement entre a et b.
	inline bool strictlyInside (const Point64 &a, const Point64 &b, const Point64 &p)
	{
		const std::int64_t d = (p.x - a.x) * (b.x - a.x) + (p.y - a.y) * (b.y - a.y);
		const std::int64_t l = (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y);
		return d > 0 && d < l;
	}

	struct Insertion
	{
		std::size_t path = 0, edge = 0;
		double      t = 0.0;
		Point64     p;
	};

	// Parametre de q le long de a-b.
	double along (const Point64 &a, const Point64 &b, const Point64 &q)
	{
		const double dx = static_cast<double>(b.x - a.x), dy = static_cast<double>(b.y - a.y);
		return (static_cast<double>(q.x - a.x) * dx + static_cast<double>(q.y - a.y) * dy) / (dx * dx + dy * dy);
	}

	void insertPoints (Paths64 &paths, std::vector<Insertion> &ins)
	{
		std::sort(ins.begin(), ins.end(), [](const Insertion &x, const Insertion &y) {
			return x.path != y.path ? x.path < y.path : x.edge != y.edge ? x.edge < y.edge : x.t < y.t;
		});
		std::size_t k = 0;
		for (std::size_t p = 0; p < paths.size() && k < ins.size(); ++p)
		{
			if (ins[k].path != p)
				continue;
			Path64 noded;
			noded.reserve(paths[p].size() + 8);
			for (std::size_t e = 0; e < paths[p].size(); ++e)
			{
				noded.push_back(paths[p][e]);
				for (; k < ins.size() && ins[k].path == p && ins[k].edge == e; ++k)
					if (!(noded.back() == ins[k].p) && !(paths[p][(e + 1) % paths[p].size()] == ins[k].p))
						noded.push_back(ins[k].p);
			}
			paths[p] = std::move(noded);
		}
	}

	// Points d'intersection de A et B : croisements (arrondis a la grille) et
	// sommets de l'un poses sur une arete de l'autre, a inserer dans A (insA)
	// et dans B (insB). Les aretes de B sont rangees dans une grille de cases
	// de 4 unites, chaque case listant ses aretes par indice croissant. Rend le
	// nombre de points.
	int nodeCollect (const Paths64 &A, const Paths64 &B, std::vector<Insertion> &insA, std::vector<Insertion> &insB)
	{
		struct Segment { std::size_t path, edge; Point64 a, b; };
		std::vector<Segment> segs;
		std::size_t total = 0;
		for (const Path64 &p : B) total += p.size();
		segs.reserve(total);
		std::int64_t xmin = INT64_MAX, ymin = INT64_MAX, xmax = INT64_MIN, ymax = INT64_MIN;
		for (std::size_t p = 0; p < B.size(); ++p)
			for (std::size_t e = 0; e < B[p].size(); ++e)
			{
				const Point64 &a = B[p][e], &b = B[p][(e + 1) % B[p].size()];
				segs.push_back({ p, e, a, b });
				xmin = std::min({ xmin, a.x, b.x }); xmax = std::max({ xmax, a.x, b.x });
				ymin = std::min({ ymin, a.y, b.y }); ymax = std::max({ ymax, a.y, b.y });
			}
		if (segs.empty() || A.empty())
			return 0;
		const std::int64_t cell = 512;
		const std::int64_t nx = (xmax - xmin) / cell + 1;
		const std::int64_t ny = (ymax - ymin) / cell + 1;
		const std::size_t cells = static_cast<std::size_t>(nx * ny);
		std::vector<std::uint32_t> start(cells + 1, 0);
		for (const Segment &s : segs)
			for (std::int64_t cx = (std::min(s.a.x, s.b.x) - xmin) / cell; cx <= (std::max(s.a.x, s.b.x) - xmin) / cell; ++cx)
				for (std::int64_t cy = (std::min(s.a.y, s.b.y) - ymin) / cell; cy <= (std::max(s.a.y, s.b.y) - ymin) / cell; ++cy)
					++start[static_cast<std::size_t>(cx * ny + cy) + 1];
		for (std::size_t c = 0; c < cells; ++c)
			start[c + 1] += start[c];
		std::vector<std::uint32_t> items(start[cells]);
		{
			std::vector<std::uint32_t> fill(start.begin(), start.end() - 1);
			for (std::size_t i = 0; i < segs.size(); ++i)
			{
				const Segment &s = segs[i];
				for (std::int64_t cx = (std::min(s.a.x, s.b.x) - xmin) / cell; cx <= (std::max(s.a.x, s.b.x) - xmin) / cell; ++cx)
					for (std::int64_t cy = (std::min(s.a.y, s.b.y) - ymin) / cell; cy <= (std::max(s.a.y, s.b.y) - ymin) / cell; ++cy)
						items[fill[static_cast<std::size_t>(cx * ny + cy)]++] = static_cast<std::uint32_t>(i);
			}
		}
		std::vector<std::size_t> stamp(segs.size(), SIZE_MAX);
		std::size_t visit = 0;
		for (std::size_t p = 0; p < A.size(); ++p)
			for (std::size_t e = 0; e < A[p].size(); ++e, ++visit)
			{
				const Point64 a0 = A[p][e], a1 = A[p][(e + 1) % A[p].size()];
				if (std::max(a0.x, a1.x) < xmin || std::min(a0.x, a1.x) > xmax
				    || std::max(a0.y, a1.y) < ymin || std::min(a0.y, a1.y) > ymax)
					continue;
				const std::int64_t cx0 = std::max<std::int64_t>((std::min(a0.x, a1.x) - xmin) / cell, 0);
				const std::int64_t cx1 = std::min<std::int64_t>((std::max(a0.x, a1.x) - xmin) / cell, nx - 1);
				const std::int64_t cy0 = std::max<std::int64_t>((std::min(a0.y, a1.y) - ymin) / cell, 0);
				const std::int64_t cy1 = std::min<std::int64_t>((std::max(a0.y, a1.y) - ymin) / cell, ny - 1);
				for (std::int64_t cx = cx0; cx <= cx1; ++cx)
					for (std::int64_t cy = cy0; cy <= cy1; ++cy)
					{
						const std::size_t key = static_cast<std::size_t>(cx * ny + cy);
						for (std::uint32_t item = start[key]; item < start[key + 1]; ++item)
						{
							const std::size_t si = items[item];
							if (stamp[si] == visit)
								continue;
							stamp[si] = visit;
							const Segment &s = segs[si];
							const Point64 b0 = s.a, b1 = s.b;
							if (std::max(a0.x, a1.x) < std::min(b0.x, b1.x) || std::max(b0.x, b1.x) < std::min(a0.x, a1.x)
							    || std::max(a0.y, a1.y) < std::min(b0.y, b1.y) || std::max(b0.y, b1.y) < std::min(a0.y, a1.y))
								continue;
							const int o1 = signOf(orient(a0, a1, b0)), o2 = signOf(orient(a0, a1, b1));
							const int o3 = signOf(orient(b0, b1, a0)), o4 = signOf(orient(b0, b1, a1));
							if (o1 * o2 < 0 && o3 * o4 < 0)
							{
								const double d3 = static_cast<double>(orient(b0, b1, a0));
								const double d4 = static_cast<double>(orient(b0, b1, a1));
								const double t = d3 / (d3 - d4);
								const Point64 x(static_cast<int64_t>(std::llround(a0.x + t * (a1.x - a0.x))),
								                static_cast<int64_t>(std::llround(a0.y + t * (a1.y - a0.y))));
								if (!(x == a0) && !(x == a1)) insA.push_back({ p, e, along(a0, a1, x), x });
								if (!(x == b0) && !(x == b1)) insB.push_back({ s.path, s.edge, along(b0, b1, x), x });
								continue;
							}
							if (o1 == 0 && strictlyInside(a0, a1, b0)) insA.push_back({ p, e, along(a0, a1, b0), b0 });
							if (o2 == 0 && strictlyInside(a0, a1, b1)) insA.push_back({ p, e, along(a0, a1, b1), b1 });
							if (o3 == 0 && strictlyInside(b0, b1, a0)) insB.push_back({ s.path, s.edge, along(b0, b1, a0), a0 });
							if (o4 == 0 && strictlyInside(b0, b1, a1)) insB.push_back({ s.path, s.edge, along(b0, b1, a1), a1 });
						}
					}
			}
		return static_cast<int>(insA.size() + insB.size());
	}

	// Une passe du noeud : les points de nodeCollect inseres dans A et dans B.
	int nodePass (Paths64 &A, Paths64 &B)
	{
		std::vector<Insertion> insA, insB;
		const int inserted = nodeCollect(A, B, insA, insB);
		insertPoints(A, insA);
		insertPoints(B, insB);
		return inserted;
	}

	// Une passe du noeud d'un niveau contre lui-meme, inseree dans le niveau.
	int nodeSelf (Paths64 &level)
	{
		std::vector<Insertion> insA, insB;
		const int inserted = nodeCollect(level, level, insA, insB);
		insertPoints(level, insA);
		return inserted;
	}

	// Nombre de points d'une passe du noeud de A et B, sans insertion.
	int nodeCount (const Paths64 &A, const Paths64 &B)
	{
		std::vector<Insertion> insA, insB;
		return nodeCollect(A, B, insA, insB);
	}

	struct VertexKey
	{
		std::int64_t x, y;
		int          level;
		bool operator== (const VertexKey &o) const { return x == o.x && y == o.y && level == o.level; }
	};

	struct VertexKeyHash
	{
		std::size_t operator() (const VertexKey &k) const
		{
			return static_cast<std::size_t>((static_cast<std::uint64_t>(k.x) * 0x9E3779B97F4A7C15ull)
			                                ^ (static_cast<std::uint64_t>(k.y) * 0xC2B2AE3D27D4EB4Full)
			                                ^ (static_cast<std::uint64_t>(k.level) * 0x165667B19E3779F9ull));
		}
	};

	struct PointHash
	{
		std::size_t operator() (const std::pair<std::int64_t, std::int64_t> &p) const
		{
			return static_cast<std::size_t>((static_cast<std::uint64_t>(p.first) * 0x9E3779B97F4A7C15ull)
			                                ^ (static_cast<std::uint64_t>(p.second) * 0xC2B2AE3D27D4EB4Full));
		}
	};

	struct Contour
	{
		const Path64 *path     = nullptr;
		bool          reversed = false;
		int           level    = 0;
	};

	struct FreeDeleter
	{
		void operator() (void *p) const { std::free(p); }
	};

	// Tessellation calculee hors du maillage (ShellBuilder::compute), versee
	// ensuite par ShellBuilder::commit.
	struct Tessellation
	{
		struct Placed { std::int64_t x, y; int level; };

		std::unique_ptr<unsigned int[], FreeDeleter> faces;
		unsigned int        faceCount = 0;
		std::vector<Placed> placed;            // point de la grille et niveau de chaque sommet
		bool                ok        = false; // aucun sommet cree hors des contours
		bool                complete  = true;  // passes de delaunayFlips completes
	};

	// Maillage soude : un sommet par (x, y, cote). `heights[level]` est la cote.
	class ShellBuilder
	{
	public:
		explicit ShellBuilder (std::vector<double> heights) : m_heights(std::move(heights)) {}

		bool delaunayComplete () const { return m_delaunayComplete; }

		// Capacite pour `vertices` sommets soudes (et environ deux faces par sommet).
		void reserve (std::size_t vertices)
		{
			m_ids.reserve(vertices);
			m_vertices.reserve(3 * vertices);
			m_faces.reserve(6 * vertices);
		}

		// Tessellation Positive des contours, sommets portes a la cote de leur
		// contour, calculee sans toucher au maillage. Elle est refusee (ok
		// faux) si un sommet est cree hors d'un point des contours (position
		// absente, ou hors de la grille), ou si deux contours de cotes
		// differentes partagent un point.
		static Tessellation compute (const std::vector<Contour> &contours)
		{
			Tessellation t;
			Polygon2 poly;
			poly.alloc_contours(static_cast<int>(contours.size()));
			std::vector<std::pair<Point64, int>> inputs;
			std::unordered_map<std::pair<std::int64_t, std::int64_t>, int, PointHash> levelAt;
			{
				std::size_t points = 0;
				for (const Contour &c : contours)
					points += c.path->size();
				inputs.reserve(points);
				levelAt.reserve(points);
			}
			std::vector<float> packed;
			for (std::size_t c = 0; c < contours.size(); ++c)
			{
				const Path64 &p = *contours[c].path;
				packed.clear();
				for (std::size_t i = 0; i < p.size(); ++i)
				{
					const Point64 &q = contours[c].reversed ? p[p.size() - 1 - i] : p[i];
					packed.push_back(static_cast<float>(q.x / kBarFusionGridScale));
					packed.push_back(static_cast<float>(q.y / kBarFusionGridScale));
					inputs.emplace_back(q, contours[c].level);
					const auto it = levelAt.emplace(std::make_pair(q.x, q.y), contours[c].level);
					if (!it.second && it.first->second != contours[c].level)
						return t;
				}
				poly.add_contour(static_cast<unsigned int>(c), static_cast<unsigned int>(p.size()), packed.data());
			}
			float *v = nullptr;
			unsigned int nv = 0, *f = nullptr, nf = 0;
			poly.tesselate(&v, &nv, &f, &nf, WindingRule::Positive);
			const std::unique_ptr<float[], FreeDeleter> vertices(v);
			t.faces.reset(f);
			t.faceCount = nf;
			delaunayFlips(v, f, nf, &t.complete);
			t.placed.resize(nv);
			bool ok = true;
			for (unsigned int i = 0; i < nv && ok; ++i)
			{
				if (i < inputs.size())
				{
					t.placed[i] = { inputs[i].first.x, inputs[i].first.y, inputs[i].second };
					continue;
				}
				const double gx = static_cast<double>(v[3 * i]) * kBarFusionGridScale;
				const double gy = static_cast<double>(v[3 * i + 1]) * kBarFusionGridScale;
				const std::int64_t x = std::llround(gx), y = std::llround(gy);
				const auto it = levelAt.find(std::make_pair(x, y));
				if (it == levelAt.end() || gx != static_cast<double>(x) || gy != static_cast<double>(y))
					ok = false;
				else
					t.placed[i] = { x, y, it->second };
			}
			t.ok = ok;
			return t;
		}

		// Verse une tessellation acceptee dans le maillage ; rien n'est emis et
		// false est rendu si elle a ete refusee.
		bool commit (const Tessellation &t, bool facingDown)
		{
			if (!t.ok)
				return false;
			m_delaunayComplete = m_delaunayComplete && t.complete;
			auto id = [&](unsigned int i) { return vertex(t.placed[i].x, t.placed[i].y, t.placed[i].level); };
			for (unsigned int k = 0; k < t.faceCount; ++k)
			{
				const unsigned int a = id(t.faces[3 * k]), b = id(t.faces[3 * k + 1]), c = id(t.faces[3 * k + 2]);
				if (facingDown) triangle(a, c, b); else triangle(a, b, c);
			}
			return true;
		}

		bool tessellate (const std::vector<Contour> &contours, bool facingDown)
		{
			return commit(compute(contours), facingDown);
		}

		// Parois verticales le long de boucles (region a gauche), du niveau
		// `bottom` au niveau `top`, tournees vers l'exterieur.
		void walls (const Paths64 &loops, int bottom, int top)
		{
			for (const Path64 &p : loops)
				for (std::size_t i = 0; i < p.size(); ++i)
				{
					const Point64 &a = p[i], &b = p[(i + 1) % p.size()];
					const unsigned int at = vertex(a.x, a.y, top), ab = vertex(a.x, a.y, bottom);
					const unsigned int bt = vertex(b.x, b.y, top), bb = vertex(b.x, b.y, bottom);
					triangle(at, ab, bb);
					triangle(at, bb, bt);
				}
		}

		void write (Mesh &out)
		{
			out.SetVertices(static_cast<unsigned int>(m_vertices.size() / 3), m_vertices.data());
			out.SetFaces(static_cast<unsigned int>(m_faces.size() / 3), 3, m_faces.data());
		}

	private:
		unsigned int vertex (std::int64_t x, std::int64_t y, int level)
		{
			const auto it = m_ids.emplace(VertexKey{ x, y, level }, static_cast<unsigned int>(m_vertices.size() / 3));
			if (it.second)
			{
				m_vertices.push_back(static_cast<float>(x / kBarFusionGridScale));
				m_vertices.push_back(static_cast<float>(y / kBarFusionGridScale));
				m_vertices.push_back(static_cast<float>(m_heights[level]));
			}
			return it.first->second;
		}

		void triangle (unsigned int a, unsigned int b, unsigned int c)
		{
			if (a == b || b == c || a == c)
				return;
			m_faces.insert(m_faces.end(), { a, b, c });
		}

		std::vector<double>                                      m_heights;
		std::unordered_map<VertexKey, unsigned int, VertexKeyHash> m_ids;
		std::vector<float>                                       m_vertices;
		std::vector<unsigned int>                                m_faces;
		bool                                                     m_delaunayComplete = true;
	};

	Paths64 plateOnGrid (const Polygon2 &plate)
	{
		Paths64 paths;
		for (int c = 0; c < plate.get_n_contours(); ++c)
		{
			const float *pts = plate.get_points(static_cast<unsigned int>(c));
			Path64 p;
			for (int i = 0; i < plate.get_n_points(static_cast<unsigned int>(c)); ++i)
			{
				const Point64 q(static_cast<int64_t>(std::llround(static_cast<double>(pts[2 * i]) * kBarFusionGridScale)),
				                static_cast<int64_t>(std::llround(static_cast<double>(pts[2 * i + 1]) * kBarFusionGridScale)));
				if (p.empty() || !(p.back() == q))
					p.push_back(q);
			}
			while (p.size() > 1 && p.front() == p.back())
				p.pop_back();
			if (p.size() >= 3)
				paths.push_back(std::move(p));
		}
		return booleanOp(ClipType::Union, paths, {});
	}

	// Boucles du bord de la region Positive des contours, sur la grille. Rend
	// false si glutess echoue, si un sommet du bord tombe hors de la grille, ou
	// si un point de la grille apparait deux fois dans les boucles (la region y
	// est pincee : les parois y donneraient une arete a quatre faces).
	bool boundaryLoops (const Paths64 &a, const Paths64 &b, Paths64 &out)
	{
		Polygon2 poly;
		poly.alloc_contours(static_cast<int>(a.size() + b.size()));
		std::vector<float> packed;
		unsigned int c = 0;
		for (const Paths64 *set : { &a, &b })
			for (const Path64 &p : *set)
			{
				packed.clear();
				for (const Point64 &q : p)
				{
					packed.push_back(static_cast<float>(q.x / kBarFusionGridScale));
					packed.push_back(static_cast<float>(q.y / kBarFusionGridScale));
				}
				poly.add_contour(c++, static_cast<unsigned int>(p.size()), packed.data());
			}
		Polygon2 loops;
		out.clear();
		if (poly.boundary(WindingRule::Positive, loops) < 0)
			return false;
		for (int l = 0; l < loops.get_n_contours(); ++l)
		{
			const float *pts = loops.get_points(static_cast<unsigned int>(l));
			Path64 p;
			for (int i = 0; i < loops.get_n_points(static_cast<unsigned int>(l)); ++i)
			{
				const double gx = static_cast<double>(pts[2 * i]) * kBarFusionGridScale;
				const double gy = static_cast<double>(pts[2 * i + 1]) * kBarFusionGridScale;
				const std::int64_t x = std::llround(gx), y = std::llround(gy);
				if (gx != static_cast<double>(x) || gy != static_cast<double>(y))
					return false;
				p.emplace_back(x, y);
			}
			out.push_back(std::move(p));
		}
		std::vector<std::pair<std::int64_t, std::int64_t>> points;
		for (const Path64 &p : out)
			for (const Point64 &q : p)
				points.emplace_back(q.x, q.y);
		std::sort(points.begin(), points.end());
		return std::adjacent_find(points.begin(), points.end()) == points.end();
	}
}

namespace
{
FusedBarsResult assemble (const Polygon2 &plate, std::vector<GridLevel> levels, double zTop, Mesh &out);
}

FusedBarsResult fuseBarsOnPlate (const Polygon2 &plate, const std::vector<FusedBar> &bars,
                                 const Profile2D &barProfile, double zTop, Mesh &out)
{
	out = Mesh();
	BarFusionReport nesting;
	std::vector<GridLevel> levels = gridLevels(bars, barProfile, &nesting);
	FusedBarsResult result = assemble(plate, std::move(levels), zTop, out);
	result.nesting = nesting;
	return result;
}

FusedBarsResult assembleFusedLevels (const Polygon2 &plate, const std::vector<BarFusionLevel> &levels,
                                     double zTop, Mesh &out)
{
	out = Mesh();
	std::vector<GridLevel> grid(levels.size());
	for (std::size_t k = 0; k < levels.size(); ++k)
	{
		grid[k].height = levels[k].height;
		for (const BarGridRing &r : levels[k].region)
		{
			Path64 p;
			for (const BarGridPoint &q : r)
				p.emplace_back(q.x, q.y);
			grid[k].region.push_back(std::move(p));
		}
	}
	return assemble(plate, std::move(grid), zTop, out);
}

namespace
{
FusedBarsResult assemble (const Polygon2 &plate, std::vector<GridLevel> levels, double zTop, Mesh &out)
{
	FusedBarsResult result;
	Paths64 S = plateOnGrid(plate);
	if (S.empty())
	{
		result.failed = true;
		result.failure = "stone plate is empty";
		return result;
	}
	Paths64 R0 = levels.empty() ? Paths64() : levels.front().region;

	// Cotes : 0 (fond), zTop (plan de la face, R_0), puis zTop + h_k.
	std::vector<double> heights = { 0.0, zTop };
	for (std::size_t k = 1; k < levels.size(); ++k)
		heights.push_back(zTop + levels[k].height);
	if (std::any_of(heights.begin() + 1, heights.end(), [](double z) { return !(std::isfinite(z) && z > 0.0); }))
	{
		result.failed = true;
		result.failure = "a level height is not finite or not above the plate bottom";
		return result;
	}
	auto levelIndex = [](std::size_t k) { return static_cast<int>(k == 0 ? 1 : 1 + k); };

	bool noded = false;
	for (int pass = 0; pass <= kBarFusionNodePasses; ++pass)
	{
		const int inserted = (pass < kBarFusionNodePasses) ? nodePass(S, R0) : -1;
		if (inserted == 0) { noded = true; break; }
		if (inserted > 0) result.nodeInsertions += inserted;
	}
	if (!noded)
	{
		result.failed = true;
		result.failure = "plate and bar footprint noding did not converge";
		return result;
	}
	if (!levels.empty())
		levels.front().region = R0;

	ShellBuilder shell(heights);
	{
		// Chaque point de P et de R_0 a deux cotes (fond et face), chaque point
		// d'un niveau au moins une.
		std::size_t points = 0;
		for (const Path64 &p : S)
			points += 2 * p.size();
		for (const GridLevel &l : levels)
			for (const Path64 &p : l.region)
				points += 2 * p.size();
		shell.reserve(points);
	}
	std::vector<Contour> base;
	for (const Path64 &p : S) base.push_back({ &p, false, 0 });
	for (const Path64 &p : R0) base.push_back({ &p, false, 0 });
	std::vector<Contour> face;
	for (const Path64 &p : S) face.push_back({ &p, false, 1 });
	for (const Path64 &p : R0) face.push_back({ &p, true, 1 });
	auto ringContours = [&](std::size_t k, std::size_t next) {
		std::vector<Contour> ring;
		for (const Path64 &p : levels[k].region) ring.push_back({ &p, false, levelIndex(k) });
		for (const Path64 &p : levels[next].region) ring.push_back({ &p, true, levelIndex(next) });
		return ring;
	};
	auto capContours = [&](std::size_t k) {
		std::vector<Contour> cap;
		for (const Path64 &p : levels[k].region) cap.push_back({ &p, false, levelIndex(k) });
		return cap;
	};

	// Etapes independantes calculees d'avance : fond, bord, plan zTop, capot
	// du dernier niveau et anneau (k - 1, k) de chaque niveau k >= 1. Elles
	// sont versees dans l'ordre de l'assemblage sequentiel ; un anneau ou un
	// capot qui suit un niveau saute est recalcule a la demande.
	const std::size_t count = levels.size();
	BarFusionDeferred<Tessellation> baseStep, faceStep, lastCapStep;
	BarFusionDeferred<bool> boundaryStep;
	Paths64 loops;
	std::vector<BarFusionDeferred<Tessellation>> ringSteps(count);
	barFusionParallelFor(3 + count, [&](std::size_t task) {
		if (task == 0)
			baseStep.run([&] { return ShellBuilder::compute(base); });
		else if (task == 1)
			boundaryStep.run([&] { return boundaryLoops(S, R0, loops); });
		else if (task == 2)
			faceStep.run([&] { return ShellBuilder::compute(face); });
		else if (task == 3)
			lastCapStep.run([&] { return ShellBuilder::compute(capContours(count - 1)); });
		else
		{
			const std::size_t next = task - 3;
			// Deux bords qui se touchent ou se croisent : anneau refuse.
			ringSteps[next].run([&] {
				return (nodeCount(levels[next - 1].region, levels[next].region) == 0)
				       ? ShellBuilder::compute(ringContours(next - 1, next)) : Tessellation();
			});
		}
	});

	if (!shell.commit(baseStep.get(), true) || !boundaryStep.get() || !shell.commit(faceStep.get(), false))
	{
		result.failed = true;
		result.failure = "plate base, outer boundary or face plane rejected (created vertex or pinched boundary)";
		return result;
	}
	shell.walls(loops, 0, 1);

	if (count > 0)
	{
		std::size_t k = 0;
		result.levels = 1;
		for (std::size_t next = 1; next < count; ++next)
		{
			const bool joined = (k + 1 == next)
			                    ? shell.commit(ringSteps[next].get(), false)
			                    : nodeCount(levels[k].region, levels[next].region) == 0
			                      && shell.tessellate(ringContours(k, next), false);
			if (joined)
			{
				k = next;
				++result.levels;
			}
			else
				++result.skippedLevels;
		}
		const bool capped = (k + 1 == count) ? shell.commit(lastCapStep.get(), false)
		                                     : shell.tessellate(capContours(k), false);
		if (!capped)
		{
			result.failed = true;
			result.failure = "top cap rejected (created vertex)";
			return result;
		}
	}

	result.delaunayComplete = shell.delaunayComplete();
	shell.write(out);
	return result;
}
}
