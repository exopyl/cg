#include "contour_validation.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "polygon2.h"

namespace
{
	struct Pt
	{
		double x = 0.0;
		double y = 0.0;
	};

	using Contours = std::vector<std::vector<Pt>>;

	constexpr unsigned    kNone   = std::numeric_limits<unsigned>::max ();
	constexpr std::size_t kNoEdge = std::numeric_limits<std::size_t>::max ();

	double orient (const Pt &a, const Pt &b, const Pt &c)
	{
		return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
	}

	double dist2 (const Pt &a, const Pt &b)
	{
		const double dx = b.x - a.x;
		const double dy = b.y - a.y;
		return dx * dx + dy * dy;
	}

	double pointSegmentDist2 (const Pt &p, const Pt &a, const Pt &b)
	{
		const double ux = b.x - a.x;
		const double uy = b.y - a.y;
		const double len2 = ux * ux + uy * uy;
		if (len2 <= 0.0)
			return dist2 (p, a);
		const double t = ((p.x - a.x) * ux + (p.y - a.y) * uy) / len2;
		if (t <= 0.0)
			return dist2 (p, a);
		if (t >= 1.0)
			return dist2 (p, b);
		const Pt q {a.x + t * ux, a.y + t * uy};
		return dist2 (p, q);
	}

	bool strictlyOpposite (double u, double v) { return (u > 0.0 && v < 0.0) || (u < 0.0 && v > 0.0); }

	// Distance entre deux segments : nulle s'ils se croisent proprement, sinon
	// le minimum des quatre distances sommet-segment (contact, chevauchement
	// colineaire et proximite y sont tous pris).
	double segmentDist2 (const Pt &a0, const Pt &a1, const Pt &b0, const Pt &b1)
	{
		if (strictlyOpposite (orient (b0, b1, a0), orient (b0, b1, a1))
		 && strictlyOpposite (orient (a0, a1, b0), orient (a0, a1, b1)))
			return 0.0;
		return std::min (std::min (pointSegmentDist2 (a0, b0, b1), pointSegmentDist2 (a1, b0, b1)),
		                 std::min (pointSegmentDist2 (b0, a0, a1), pointSegmentDist2 (b1, a0, a1)));
	}

	struct Edge
	{
		unsigned contour = 0;
		unsigned index = 0;      // arete index : point index -> point (index + 1) mod n
		Pt       a, b;
		unsigned clusterA = 0;   // grappes d'extremite (voir SelfIntersection, en-tete)
		unsigned clusterB = 0;
		bool     isShort = false;
	};

	void addDefect (ContourValidationReport &report, const ContourValidationOptions &options,
	                ContourDefectRecord record)
	{
		std::size_t &count = report.counts[static_cast<std::size_t> (record.kind)];
		if (count < options.maxRecordsPerKind)
			report.records.push_back (std::move (record));
		++count;
	}

	// Indice de cellule ou de bande, borne a [0, kMaxIndex]. Les indices
	// bornes regroupent des points eloignes : cela ne coute que du temps.
	constexpr std::uint64_t kMaxIndex = 0x7FFFFFFFull;

	std::uint64_t gridIndex (double v)
	{
		if (!(v > 0.0))
			return 0;
		if (v >= static_cast<double> (kMaxIndex))
			return kMaxIndex;
		return static_cast<std::uint64_t> (v);
	}

	double lowerQuartile (std::vector<double> v)
	{
		if (v.empty ())
			return 0.0;
		const auto q = v.begin () + static_cast<std::ptrdiff_t> (v.size () / 4);
		std::nth_element (v.begin (), q, v.end ());
		return *q;
	}

	// Pas d'un index creux : au moins le premier quartile des longueurs (une
	// petite arete occupe peu de cellules ; la mediane tomberait sur le mode
	// long d'une distribution bimodale -- barres longues et etroites -- et
	// mettrait tout dans une cellule), la tolerance, et total / (16n) (le nombre
	// d'inscriptions reste proportionnel a n quelle que soit la repartition des
	// longueurs). Aucune borne ne depend de l'emprise : des aretes concentrees
	// dans un coin d'une grande emprise ne partagent pas leurs cellules.
	double gridStep (double quartileLength, double totalLength, std::size_t n, double tolerance)
	{
		const double step = std::max ({quartileLength, tolerance,
		                               totalLength / (16.0 * static_cast<double> (std::max<std::size_t> (n, 1)))});
		return std::isfinite (step) ? step : std::numeric_limits<double>::max ();
	}

	// Tri stable par cle, radix LSD octet par octet ; un octet constant sur
	// toutes les cles ne coute pas de passe.
	void radixSortByKey (std::vector<std::pair<std::uint64_t, unsigned>> &v)
	{
		std::uint64_t all = 0;
		std::uint64_t any = 0;
		for (const auto &e : v)
		{
			all |= e.first;
			any |= ~e.first;
		}
		const std::uint64_t varying = all & any;
		std::vector<std::pair<std::uint64_t, unsigned>> tmp (v.size ());
		for (unsigned shift = 0; shift < 64; shift += 8)
		{
			if (((varying >> shift) & 0xFFu) == 0)
				continue;
			std::array<std::size_t, 257> count {};
			for (const auto &e : v)
				++count[((e.first >> shift) & 0xFFu) + 1];
			for (int b = 0; b < 256; ++b)
				count[b + 1] += count[b];
			for (const auto &e : v)
				tmp[count[(e.first >> shift) & 0xFFu]++] = e;
			v.swap (tmp);
		}
	}

	// Seaux cle -> aretes, construits par tri : deterministes, sans table de
	// hachage, recherche par dichotomie.
	class SparseBuckets
	{
	public:
		static constexpr std::size_t kNone = std::numeric_limits<std::size_t>::max ();

		// Les entrees arrivent par arete croissante ; le tri par cle est stable,
		// donc chaque seau liste ses aretes par indice croissant.
		void Build (std::vector<std::pair<std::uint64_t, unsigned>> entries)
		{
			radixSortByKey (entries);
			m_keys.clear ();
			m_start.clear ();
			m_items.clear ();
			m_items.reserve (entries.size ());
			for (std::size_t i = 0; i < entries.size (); ++i)
			{
				if (i == 0 || entries[i].first != entries[i - 1].first)
				{
					m_keys.push_back (entries[i].first);
					m_start.push_back (m_items.size ());
				}
				m_items.push_back (entries[i].second);
			}
			m_start.push_back (m_items.size ());
		}

		std::size_t Find (std::uint64_t key) const
		{
			const auto it = std::lower_bound (m_keys.begin (), m_keys.end (), key);
			return (it != m_keys.end () && *it == key) ? static_cast<std::size_t> (it - m_keys.begin ()) : kNone;
		}

		const unsigned *Begin (std::size_t b) const { return m_items.data () + m_start[b]; }
		const unsigned *End (std::size_t b) const { return m_items.data () + m_start[b + 1]; }
		std::size_t Size (std::size_t b) const { return m_start[b + 1] - m_start[b]; }

	private:
		std::vector<std::uint64_t> m_keys;
		std::vector<std::size_t>   m_start {0};
		std::vector<unsigned>      m_items;
	};

	// Grille creuse sur les aretes. Chaque arete est inscrite dans toutes les
	// cellules qui rencontrent son voisinage de rayon `reach` (>= tolerance) :
	// deux aretes a distance < tolerance ont alors une cellule commune, celle
	// qui contient le point le plus proche de l'une. L'inscription suit le
	// segment rangee par rangee, pas sa boite : une longue arete oblique coute
	// O(longueur / pas) cellules, et non O((longueur / pas)^2).
	class EdgeGrid
	{
	public:
		EdgeGrid (const std::vector<Edge> &edges, double tolerance, double maxAbs)
			: m_reach (tolerance * (1.0 + 1e-6) + 4.0 * DBL_EPSILON * maxAbs)
		{
			double total = 0.0;
			std::vector<double> lengths;
			lengths.reserve (edges.size ());
			m_x0 = std::numeric_limits<double>::infinity ();
			m_y0 = m_x0;
			for (const Edge &e : edges)
			{
				m_x0 = std::min ({m_x0, e.a.x, e.b.x});
				m_y0 = std::min ({m_y0, e.a.y, e.b.y});
				lengths.push_back (std::sqrt (dist2 (e.a, e.b)));
				total += lengths.back ();
			}
			m_step = gridStep (lowerQuartile (std::move (lengths)), total, edges.size (), tolerance);

			std::vector<std::pair<std::uint64_t, unsigned>> entries;
			m_edgeStart.reserve (edges.size () + 1);
			for (unsigned i = 0; i < edges.size (); ++i)
			{
				m_edgeStart.push_back (entries.size ());
				forEachCell (edges[i], [&] (std::uint64_t key) { entries.emplace_back (key, i); });
			}
			m_edgeStart.push_back (entries.size ());
			std::vector<std::uint64_t> keys (entries.size ());
			for (std::size_t k = 0; k < entries.size (); ++k)
				keys[k] = entries[k].first;
			m_buckets.Build (std::move (entries));
			m_edgeBuckets.resize (keys.size ());
			for (std::size_t k = 0; k < keys.size (); ++k)
				m_edgeBuckets[k] = m_buckets.Find (keys[k]);
		}

		const SparseBuckets &Buckets () const { return m_buckets; }
		const std::size_t *CellsBegin (unsigned edge) const { return m_edgeBuckets.data () + m_edgeStart[edge]; }
		const std::size_t *CellsEnd (unsigned edge) const { return m_edgeBuckets.data () + m_edgeStart[edge + 1]; }

	private:
		std::uint64_t col (double x) const { return gridIndex ((x - m_x0) / m_step); }
		std::uint64_t row (double y) const { return gridIndex ((y - m_y0) / m_step); }

		template <class F> void forEachCell (const Edge &e, F &&f) const
		{
			const double r = m_reach;
			const std::uint64_t rowLo = row (std::min (e.a.y, e.b.y) - r);
			const std::uint64_t rowHi = row (std::max (e.a.y, e.b.y) + r);
			const double dx = e.b.x - e.a.x;
			const double dy = e.b.y - e.a.y;
			constexpr double inf = std::numeric_limits<double>::infinity ();
			for (std::uint64_t rw = rowLo; rw <= rowHi; ++rw)
			{
				// Rangees bornees ouvertes : row() y ramene tout point exterieur.
				const double bandLo = (rw == 0) ? -inf : m_y0 + static_cast<double> (rw) * m_step - r;
				const double bandHi = (rw == kMaxIndex) ? inf : m_y0 + static_cast<double> (rw + 1) * m_step + r;
				double tLo = 0.0;
				double tHi = 1.0;
				if (dy != 0.0)
				{
					const double t1 = (bandLo - e.a.y) / dy;
					const double t2 = (bandHi - e.a.y) / dy;
					tLo = std::max (0.0, std::min (t1, t2));
					tHi = std::min (1.0, std::max (t1, t2));
					if (tLo > tHi)
						continue;
				}
				const double xa = e.a.x + tLo * dx;
				const double xb = e.a.x + tHi * dx;
				const std::uint64_t colHi = col (std::max (xa, xb) + r);
				for (std::uint64_t cl = col (std::min (xa, xb) - r); cl <= colHi; ++cl)
					f ((rw << 32) | cl);
			}
		}

		double m_reach = 0.0;
		double m_x0 = 0.0;
		double m_y0 = 0.0;
		double m_step = 1.0;
		SparseBuckets            m_buckets;
		std::vector<std::size_t> m_edgeStart;     // cellules de l'arete i : [m_edgeStart[i], m_edgeStart[i + 1])
		std::vector<std::size_t> m_edgeBuckets;
	};

	// Bandes horizontales pour le rayon d'imbrication. Une arete non horizontale
	// est inscrite dans toutes les bandes que couvre son intervalle en y ; une
	// arete que traverse la droite y = py (min <= py < max) est donc dans la
	// bande de py, par monotonie de l'indice. Les aretes horizontales ne sont
	// jamais traversees et ne sont pas inscrites.
	class StripIndex
	{
	public:
		StripIndex (const std::vector<Edge> &edges, double tolerance)
		{
			double total = 0.0;
			std::vector<double> heights;
			m_y0 = std::numeric_limits<double>::infinity ();
			for (const Edge &e : edges)
			{
				m_y0 = std::min ({m_y0, e.a.y, e.b.y});
				if (e.a.y != e.b.y)
				{
					heights.push_back (std::fabs (e.b.y - e.a.y));
					total += heights.back ();
				}
			}
			const std::size_t count = heights.size ();
			m_step = gridStep (lowerQuartile (std::move (heights)), total, count, tolerance);

			std::vector<std::pair<std::uint64_t, unsigned>> entries;
			for (unsigned i = 0; i < edges.size (); ++i)
			{
				const Edge &e = edges[i];
				if (e.a.y == e.b.y)
					continue;
				const std::uint64_t hi = Strip (std::max (e.a.y, e.b.y));
				for (std::uint64_t s = Strip (std::min (e.a.y, e.b.y)); s <= hi; ++s)
					entries.emplace_back (s, i);
			}
			m_buckets.Build (std::move (entries));
		}

		std::uint64_t Strip (double y) const { return gridIndex ((y - m_y0) / m_step); }
		const SparseBuckets &Buckets () const { return m_buckets; }

	private:
		double m_y0 = 0.0;
		double m_step = 1.0;
		SparseBuckets m_buckets;
	};

	bool isFinite (const Pt &p) { return std::isfinite (p.x) && std::isfinite (p.y); }

	double defaultTolerance (const Contours &contours)
	{
		double x0 = std::numeric_limits<double>::infinity ();
		double y0 = x0;
		double x1 = -x0;
		double y1 = -x0;
		double maxAbs = 0.0;
		bool any = false;
		for (const auto &c : contours)
			for (const Pt &p : c)
			{
				if (!isFinite (p))
					continue;
				any = true;
				x0 = std::min (x0, p.x);
				y0 = std::min (y0, p.y);
				x1 = std::max (x1, p.x);
				y1 = std::max (y1, p.y);
				maxAbs = std::max ({maxAbs, std::fabs (p.x), std::fabs (p.y)});
			}
		if (!any)
			return 1.0;
		const double diag = std::sqrt ((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
		const double tol = std::max (1e-6 * diag, 4.0 * static_cast<double> (FLT_EPSILON) * maxAbs);
		return tol > 0.0 ? tol : 1.0;
	}

	// Grappes de points relies par des aretes courtes, numerotees le long du
	// contour. Toutes les aretes courtes : une seule grappe.
	std::vector<unsigned> clusters (const std::vector<bool> &shortEdge)
	{
		const std::size_t n = shortEdge.size ();
		std::vector<unsigned> cluster (n, 0);
		std::size_t start = n;
		for (std::size_t i = 0; i < n; ++i)
			if (!shortEdge[(i + n - 1) % n])
			{
				start = i;
				break;
			}
		if (start == n)
			return cluster;
		unsigned id = 0;
		cluster[start] = 0;
		for (std::size_t k = 1; k < n; ++k)
		{
			const std::size_t i = (start + k) % n;
			const std::size_t prev = (i + n - 1) % n;
			cluster[i] = shortEdge[prev] ? cluster[prev] : ++id;
		}
		return cluster;
	}

	bool sharesCluster (unsigned c, const Edge &e) { return c == e.clusterA || c == e.clusterB; }

	// Paire d'aretes d'un meme contour, aucune courte : voir SelfIntersection.
	bool selfPairDefect (const Edge &e, const Edge &f, double tol2)
	{
		const bool neighbours = sharesCluster (e.clusterA, f) || sharesCluster (e.clusterB, f);
		if (!neighbours)
			return segmentDist2 (e.a, e.b, f.a, f.b) < tol2;
		if (!sharesCluster (e.clusterA, f) && pointSegmentDist2 (e.a, f.a, f.b) < tol2) return true;
		if (!sharesCluster (e.clusterB, f) && pointSegmentDist2 (e.b, f.a, f.b) < tol2) return true;
		if (!sharesCluster (f.clusterA, e) && pointSegmentDist2 (f.a, e.a, e.b) < tol2) return true;
		if (!sharesCluster (f.clusterB, e) && pointSegmentDist2 (f.b, e.a, e.b) < tol2) return true;
		return false;
	}

	ContourValidationReport validate (const Contours &contours, const std::vector<bool> *holeFlags,
	                                  const ContourValidationOptions &options)
	{
		if (options.tolerance && !(std::isfinite (*options.tolerance) && *options.tolerance > 0.0))
			throw std::invalid_argument ("ValidateContours: tolerance must be finite and > 0");

		ContourValidationReport report;
		const double tol = options.tolerance ? *options.tolerance : defaultTolerance (contours);
		const double tol2 = tol * tol;
		report.tolerance = tol;
		report.contourCount = contours.size ();
		report.nestingLevel.assign (contours.size (), -1);
		report.signedArea.assign (contours.size (), 0.0);

		// --- par contour : finitude, taille, aretes courtes, aire ---------------
		std::vector<bool> included (contours.size (), false);
		std::vector<bool> negligible (contours.size (), false);
		std::vector<Edge> edges;
		std::vector<std::size_t> edgeBegin (contours.size (), 0);   // aretes du contour c : [edgeBegin[c], + taille)
		double maxAbs = 0.0;
		for (unsigned c = 0; c < contours.size (); ++c)
		{
			const auto &pts = contours[c];
			report.pointCount += pts.size ();
			bool finite = true;
			for (unsigned i = 0; i < pts.size (); ++i)
				if (!isFinite (pts[i]))
				{
					finite = false;
					addDefect (report, options, {ContourDefect::NonFinitePoint, {c}, {i}, {}});
				}
			if (pts.size () < 3)
				addDefect (report, options, {ContourDefect::TooFewPoints, {c}, {}, {}});
			if (!finite || pts.size () < 3)
				continue;
			included[c] = true;

			const std::size_t n = pts.size ();
			std::vector<bool> shortEdge (n, false);
			double area2 = 0.0;
			double perimeter = 0.0;
			for (unsigned i = 0; i < n; ++i)
			{
				const Pt &a = pts[i];
				const Pt &b = pts[(i + 1) % n];
				maxAbs = std::max ({maxAbs, std::fabs (a.x), std::fabs (a.y)});
				const double len2 = dist2 (a, b);
				perimeter += std::sqrt (len2);
				if (len2 < tol2)
				{
					shortEdge[i] = true;
					addDefect (report, options, {ContourDefect::ShortEdge, {c}, {}, {i}});
				}
				// Aire relative au premier point : l'arrondi suit alors la taille
				// du contour, pas sa distance a l'origine.
				area2 += orient (pts[0], a, b);
			}
			const double area = 0.5 * area2;
			report.signedArea[c] = area;
			if (std::fabs (area) <= tol * perimeter)
			{
				negligible[c] = true;
				addDefect (report, options, {ContourDefect::NegligibleArea, {c}, {}, {}});
			}

			const std::vector<unsigned> cluster = clusters (shortEdge);
			edgeBegin[c] = edges.size ();
			for (unsigned i = 0; i < n; ++i)
			{
				Edge e;
				e.contour  = c;
				e.index    = i;
				e.a        = pts[i];
				e.b        = pts[(i + 1) % n];
				e.clusterA = cluster[i];
				e.clusterB = cluster[(i + 1) % n];
				e.isShort  = shortEdge[i];
				edges.push_back (e);
			}
		}

		if (edges.empty ())
			return report;

		std::vector<bool> inContact (edges.size (), false);

		const std::size_t budget = options.maxCandidates
			? *options.maxCandidates
			: std::max (kMinContourCandidates, kContourCandidatesPerEdge * edges.size ());
		report.candidateBudget = budget;

		// --- paires d'aretes : auto-intersection et contact ---------------------
		// Les aretes sont rangees par contour puis par indice : a < b donne
		// contour(a) <= contour(b), et a l'interieur d'un contour index(a) < index(b).
		{
			const EdgeGrid grid (edges, tol, maxAbs);
			const SparseBuckets &cells = grid.Buckets ();
			std::unordered_set<std::uint64_t> contactPairs;
			std::vector<unsigned> stamp (edges.size (), kNone);
			for (unsigned ia = 0; ia < edges.size () && !report.truncated; ++ia)
			{
				const Edge &ea = edges[ia];
				for (const std::size_t *cell = grid.CellsBegin (ia); cell != grid.CellsEnd (ia); ++cell)
				{
					if (report.candidatesVisited + cells.Size (*cell) > budget)
					{
						report.truncated = true;
						break;
					}
					report.candidatesVisited += cells.Size (*cell);
					for (const unsigned *it = cells.Begin (*cell); it != cells.End (*cell); ++it)
					{
						const unsigned ib = *it;
						if (ib <= ia || stamp[ib] == ia)
							continue;
						stamp[ib] = ia;
						const Edge &eb = edges[ib];
						if (ea.contour == eb.contour)
						{
							if (ea.isShort || eb.isShort)
								continue;
							++report.edgePairsTested;
							if (selfPairDefect (ea, eb, tol2))
								addDefect (report, options, {ContourDefect::SelfIntersection,
								                             {ea.contour, eb.contour}, {}, {ea.index, eb.index}});
						}
						else
						{
							++report.edgePairsTested;
							if (segmentDist2 (ea.a, ea.b, eb.a, eb.b) < tol2)
							{
								addDefect (report, options, {ContourDefect::ContourContact,
								                             {ea.contour, eb.contour}, {}, {ea.index, eb.index}});
								contactPairs.insert ((static_cast<std::uint64_t> (ea.contour) << 32) | eb.contour);
								inContact[ia] = true;
								inContact[ib] = true;
							}
						}
					}
				}
			}
			report.contactContourPairs = contactPairs.size ();
		}

		// --- imbrication : rayon horizontal vers +x depuis le milieu de la plus
		// longue arete SANS contact du contour (a defaut, de la plus longue).
		// Phase sautee si les paires ont ete tronquees : les contacts, donc le
		// choix du point, sont incomplets ; et quand l'appelant ne la demande pas.
		if (!report.truncated && options.checkNesting)
		{
			const StripIndex strips (edges, tol);
			const SparseBuckets &bands = strips.Buckets ();
			std::vector<unsigned char> parity (contours.size (), 0);
			std::vector<unsigned> touched;
			for (unsigned c = 0; c < contours.size () && !report.truncated; ++c)
			{
				if (!included[c])
					continue;
				std::size_t pick = kNoEdge;
				std::size_t longest = kNoEdge;
				double pick2 = -1.0;
				double longest2 = -1.0;
				for (std::size_t i = edgeBegin[c]; i < edgeBegin[c] + contours[c].size (); ++i)
				{
					const double l2 = dist2 (edges[i].a, edges[i].b);
					if (l2 > longest2)
					{
						longest2 = l2;
						longest = i;
					}
					if (!inContact[i] && l2 > pick2)
					{
						pick2 = l2;
						pick = i;
					}
				}
				const Edge &probe = edges[pick != kNoEdge ? pick : longest];
				const Pt p {0.5 * (probe.a.x + probe.b.x), 0.5 * (probe.a.y + probe.b.y)};

				const std::size_t band = bands.Find (strips.Strip (p.y));
				if (band != SparseBuckets::kNone)
				{
					if (report.candidatesVisited + bands.Size (band) > budget)
					{
						report.truncated = true;
						break;
					}
					report.candidatesVisited += bands.Size (band);
					for (const unsigned *it = bands.Begin (band); it != bands.End (band); ++it)
					{
						const Edge &e = edges[*it];
						if (e.contour == c || (e.a.y > p.y) == (e.b.y > p.y))
							continue;
						const double x = e.a.x + (p.y - e.a.y) * (e.b.x - e.a.x) / (e.b.y - e.a.y);
						if (x > p.x)
						{
							if (parity[e.contour] == 0)
								touched.push_back (e.contour);
							parity[e.contour] ^= 1;
						}
					}
				}
				int level = 0;
				for (unsigned t : touched)
				{
					level += parity[t];
					parity[t] = 0;
				}
				touched.clear ();
				report.nestingLevel[c] = level;
			}
		}

		// --- orientation et imbrication -----------------------------------------
		for (unsigned c = 0; c < contours.size (); ++c)
		{
			const int level = report.nestingLevel[c];
			if (level < 0)
				continue;
			const bool odd = (level % 2) == 1;
			const bool levelTooDeep = options.maxNestingLevel && static_cast<unsigned> (level) > *options.maxNestingLevel;
			const bool roleMismatch = holeFlags && (*holeFlags)[c] != odd;
			if (levelTooDeep || roleMismatch)
				addDefect (report, options, {ContourDefect::BadNesting, {c}, {}, {}});

			if (options.orientation == ContourOrientation::Unchecked || negligible[c])
				continue;
			const bool ccw = report.signedArea[c] > 0.0;
			const bool expectCcw = (options.orientation == ContourOrientation::OuterCcwHoleCw) ? !odd : odd;
			if (ccw != expectCcw)
				addDefect (report, options, {ContourDefect::WrongOrientation, {c}, {}, {}});
		}
		return report;
	}

	Contours fromExtrude (const std::vector<ExtrudeContour> &in, std::vector<bool> *holeFlags)
	{
		Contours out (in.size ());
		if (holeFlags)
			holeFlags->resize (in.size ());
		for (std::size_t c = 0; c < in.size (); ++c)
		{
			out[c].reserve (in[c].pts.size ());
			for (const Vector2f &p : in[c].pts)
				out[c].push_back ({static_cast<double> (p.x), static_cast<double> (p.y)});
			if (holeFlags)
				(*holeFlags)[c] = in[c].isHole;
		}
		return out;
	}

	Contours fromPolygon (const Polygon2 &polygon)
	{
		const int nc = polygon.get_n_contours ();
		Contours out (static_cast<std::size_t> (std::max (nc, 0)));
		for (int c = 0; c < nc; ++c)
		{
			const int np = polygon.get_n_points (static_cast<unsigned> (c));
			const float *xy = polygon.get_points (static_cast<unsigned> (c));
			out[c].reserve (static_cast<std::size_t> (np));
			for (int i = 0; i < np; ++i)
				out[c].push_back ({static_cast<double> (xy[2 * i]), static_cast<double> (xy[2 * i + 1])});
		}
		return out;
	}
}

const char *ContourDefectName (ContourDefect kind)
{
	switch (kind)
	{
	case ContourDefect::NonFinitePoint:   return "non_finite_point";
	case ContourDefect::TooFewPoints:     return "too_few_points";
	case ContourDefect::ShortEdge:        return "short_edge";
	case ContourDefect::SelfIntersection: return "self_intersection";
	case ContourDefect::ContourContact:   return "contour_contact";
	case ContourDefect::WrongOrientation: return "wrong_orientation";
	case ContourDefect::BadNesting:       return "bad_nesting";
	case ContourDefect::NegligibleArea:   return "negligible_area";
	}
	return "unknown";
}

bool ContourValidationReport::IsValid () const
{
	return !truncated
	    && std::all_of (counts.begin (), counts.end (), [] (std::size_t n) { return n == 0; });
}

double DefaultContourTolerance (const std::vector<ExtrudeContour> &contours)
{
	return defaultTolerance (fromExtrude (contours, nullptr));
}

double DefaultContourTolerance (const Polygon2 &polygon)
{
	return defaultTolerance (fromPolygon (polygon));
}

ContourValidationReport ValidateContours (const std::vector<ExtrudeContour> &contours,
                                          const ContourValidationOptions &options)
{
	std::vector<bool> holeFlags;
	const Contours in = fromExtrude (contours, &holeFlags);
	return validate (in, &holeFlags, options);
}

ContourValidationReport ValidateContours (const Polygon2 &polygon, const ContourValidationOptions &options)
{
	return validate (fromPolygon (polygon), nullptr, options);
}
