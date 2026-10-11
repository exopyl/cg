#include "mesh_validation.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "mesh.h"

namespace
{
	using Vec3 = std::array<double, 3>;

	Vec3 sub (const Vec3 &a, const Vec3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
	Vec3 add (const Vec3 &a, const Vec3 &b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
	Vec3 scale (const Vec3 &a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
	Vec3 cross (const Vec3 &a, const Vec3 &b)
	{
		return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
	}
	double dot (const Vec3 &a, const Vec3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
	double norm (const Vec3 &a) { return std::sqrt (dot (a, a)); }

	constexpr unsigned kNone = std::numeric_limits<unsigned>::max ();

	unsigned findRoot (std::vector<unsigned> &parent, unsigned x)
	{
		while (parent[x] != x)
		{
			parent[x] = parent[parent[x]];
			x = parent[x];
		}
		return x;
	}

	void unite (std::vector<unsigned> &parent, unsigned a, unsigned b)
	{
		const unsigned ra = findRoot (parent, a);
		const unsigned rb = findRoot (parent, b);
		if (ra != rb)
			parent[std::max (ra, rb)] = std::min (ra, rb);
	}

	using Cell = std::array<long long, 3>;

	struct CellHash
	{
		std::size_t operator() (const Cell &c) const noexcept
		{
			std::uint64_t h = 1469598103934665603ull;
			for (long long v : c)
			{
				h ^= static_cast<std::uint64_t> (v);
				h *= 1099511628211ull;
			}
			return static_cast<std::size_t> (h);
		}
	};

	// Indice de cellule borne a +-kCellLimit. Une cellule qui touche la borne
	// regroupe des points arbitrairement eloignes : ses membres ne sont soudes
	// que sur mesure de distance.
	constexpr long long kCellLimit = 4000000000000000000LL;

	long long cellIndex (double x, double tol)
	{
		constexpr double limit = static_cast<double> (kCellLimit);
		const double c = std::floor (x / tol);
		return static_cast<long long> (std::max (-limit, std::min (limit, c)));
	}

	bool isBounded (const Cell &c)
	{
		for (long long v : c)
			if (v <= -kCellLimit || v >= kCellLimit)
				return false;
		return true;
	}

	class Recorder
	{
	public:
		Recorder (MeshValidationReport &report, std::size_t cap) : m_report (report), m_cap (cap) {}

		void add (MeshDefect kind, std::vector<unsigned> faces = {}, std::vector<unsigned> vertices = {})
		{
			const std::size_t k = static_cast<std::size_t> (kind);
			if (m_report.counts[k]++ < m_cap)
				m_report.records.push_back ({kind, std::move (faces), std::move (vertices)});
		}

	private:
		MeshValidationReport &m_report;
		std::size_t           m_cap;
	};

	// -----------------------------------------------------------------------
	//  Auto-intersection
	// -----------------------------------------------------------------------
	//
	// Regle, avec t = weldTolerance et d(P, X) la distance signee de P au plan
	// de X :
	//
	//  - Paire coplanaire : |d| <= t pour chaque sommet de l'un par rapport au
	//    plan de l'autre, dans les deux sens. Les deux triangles sont exprimes
	//    dans une base orthonormee du plan de A, A est decoupe par B
	//    (Sutherland-Hodgman) et la paire est signalee si l'aire du recouvrement
	//    depasse t fois son perimetre -- un recouvrement plus epais qu'environ
	//    2t. L'aire est sommee relativement au premier sommet du polygone.
	//    Deux triangles adjacents d'une meme face plane, ou qui ne se touchent
	//    qu'en un sommet, ont un recouvrement d'aire nulle.
	//
	//  - Paire non coplanaire, signalee si l'un des deux tests reussit :
	//      a) une arete de l'un a ses extremites de part et d'autre du plan de
	//         l'autre, chacune a plus de t, et le point de traversee est a
	//         l'interieur de l'autre triangle a plus de t de ses cotes ;
	//      b) chaque triangle a des sommets a plus de t des deux cotes du plan de
	//         l'autre, et les segments d'intersection exacts de chacun avec le
	//         plan de l'autre (extremites aux changements de signe de d, sans
	//         tolerance), projetes sur la droite D = nA x nB, se recouvrent sur
	//         plus de t. Ce test couvre la traversee franche dont les deux
	//         extremites tombent sur des cotes des triangles (croix, lames a
	//         sommet commun), que a) manque. La tolerance n'entre que dans les
	//         seuils, jamais dans la position des extremites.
	//    Un sommet partage est dans le plan de l'autre triangle. Une paire a
	//    arete commune n'est donc jamais signalee ; une paire a sommet commun ne
	//    l'est que si elle se traverse au-dela de ce sommet. Un simple contact
	//    n'est pas signale.
	//
	//  - Zone grise : une paire presque coplanaire dont un sommet est entre t et
	//    environ 2t du plan de l'autre n'est ni coplanaire ni traversante au sens
	//    ci-dessus, et n'est pas signalee.
	//
	// Exclus : faces de taille != 3, faces degenerees, faces a sommet non fini,
	// et paires de meme ensemble de sommets (deja DuplicateFace).
	struct Tri
	{
		std::array<unsigned, 3> w;      // sommets soudes
		unsigned                face;   // face originale
		Vec3                    n;      // normale unitaire
		Vec3                    lo, hi; // boite elargie de t
	};

	bool insideByMoreThan (const Vec3 &x, const std::array<Vec3, 3> &p, const Vec3 &n, double tol)
	{
		for (int k = 0; k < 3; ++k)
		{
			const Vec3 &a = p[k];
			const Vec3 &b = p[(k + 1) % 3];
			const Vec3  e = sub (b, a);
			const double len = norm (e);
			if (len <= 0.0)
				return false;
			if (dot (cross (e, sub (x, a)), n) / len <= tol)
				return false;
		}
		return true;
	}

	bool edgeCrossesTriangle (const Vec3 &p, const Vec3 &q, const std::array<Vec3, 3> &t, const Vec3 &n, double tol)
	{
		const double dp = dot (sub (p, t[0]), n);
		const double dq = dot (sub (q, t[0]), n);
		if (!((dp > tol && dq < -tol) || (dp < -tol && dq > tol)))
			return false;
		const Vec3 x = add (p, scale (sub (q, p), dp / (dp - dq)));
		return insideByMoreThan (x, t, n, tol);
	}

	using Vec2    = std::array<double, 2>;
	using Polygon = std::vector<Vec2>;

	double signedArea (const Polygon &p)
	{
		// Relatif au premier sommet : le bruit d'arrondi suit la taille du
		// polygone, et non sa distance a l'origine de la base.
		double a = 0.0;
		for (std::size_t i = 0; i < p.size (); ++i)
		{
			const Vec2 &q = p[(i + 1) % p.size ()];
			const Vec2  u {p[i][0] - p[0][0], p[i][1] - p[0][1]};
			const Vec2  w {q[0] - p[0][0], q[1] - p[0][1]};
			a += u[0] * w[1] - u[1] * w[0];
		}
		return 0.5 * a;
	}

	double perimeter (const Polygon &p)
	{
		double s = 0.0;
		for (std::size_t i = 0; i < p.size (); ++i)
		{
			const Vec2 &u = p[i];
			const Vec2 &w = p[(i + 1) % p.size ()];
			s += std::hypot (w[0] - u[0], w[1] - u[1]);
		}
		return s;
	}

	// Sutherland-Hodgman : `subject` decoupe par le triangle convexe `clip`,
	// tous deux dans le sens trigonometrique.
	Polygon clipByTriangle (Polygon subject, const Polygon &clip)
	{
		for (std::size_t i = 0; i < clip.size () && !subject.empty (); ++i)
		{
			const Vec2 &a = clip[i];
			const Vec2 &b = clip[(i + 1) % clip.size ()];
			auto side = [&a, &b] (const Vec2 &p) {
				return (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0]);
			};
			Polygon out;
			for (std::size_t k = 0; k < subject.size (); ++k)
			{
				const Vec2 &p = subject[k];
				const Vec2 &r = subject[(k + 1) % subject.size ()];
				const double sp = side (p), sr = side (r);
				if (sp >= 0.0)
					out.push_back (p);
				if ((sp > 0.0 && sr < 0.0) || (sp < 0.0 && sr > 0.0))
				{
					const double s = sp / (sp - sr);
					out.push_back ({p[0] + s * (r[0] - p[0]), p[1] + s * (r[1] - p[1])});
				}
			}
			subject = std::move (out);
		}
		return subject;
	}

	bool coplanarOverlap (const std::array<Vec3, 3> &A, const std::array<Vec3, 3> &B, const Vec3 &nA, double tol)
	{
		const Vec3 e = sub (A[1], A[0]);
		const double len = norm (e);
		if (len <= 0.0)
			return false;
		const Vec3 u = scale (e, 1.0 / len);
		const Vec3 v = cross (nA, u);
		auto toPlane = [&] (const std::array<Vec3, 3> &T) {
			Polygon p;
			for (const Vec3 &x : T)
				p.push_back ({dot (sub (x, A[0]), u), dot (sub (x, A[0]), v)});
			if (signedArea (p) < 0.0)
				std::swap (p[1], p[2]);
			return p;
		};
		const Polygon overlap = clipByTriangle (toPlane (A), toPlane (B));
		if (overlap.size () < 3)
			return false;
		const double area = std::fabs (signedArea (overlap));
		return area > tol * perimeter (overlap);
	}

	bool straddles (const std::array<double, 3> &d, double tol)
	{
		return std::max ({d[0], d[1], d[2]}) > tol && std::min ({d[0], d[1], d[2]}) < -tol;
	}

	// Intervalle, sur la droite unitaire D, de l'intersection exacte du triangle T
	// avec le plan de l'autre triangle ; d = distances signees des sommets de T a
	// ce plan. Extremites : les points d'arete ou d change de signe, et les
	// sommets de distance nulle. Un sommet a moins de t du plan n'en est pas une :
	// il est a d / sin(diedre) de la droite commune aux deux plans ; le prendre
	// pour extremite deplace celle-ci d'autant, sans borne quand les plans sont
	// presque paralleles.
	std::pair<double, double> intervalOnLine (const std::array<Vec3, 3> &T, const std::array<double, 3> &d,
	                                          const Vec3 &D)
	{
		double lo = std::numeric_limits<double>::max ();
		double hi = -lo;
		auto take = [&] (const Vec3 &x) {
			const double s = dot (x, D);
			lo = std::min (lo, s);
			hi = std::max (hi, s);
		};
		for (int i = 0; i < 3; ++i)
		{
			const int j = (i + 1) % 3;
			if (d[i] == 0.0)
				take (T[i]);
			if ((d[i] > 0.0 && d[j] < 0.0) || (d[i] < 0.0 && d[j] > 0.0))
				take (add (T[i], scale (sub (T[j], T[i]), d[i] / (d[i] - d[j]))));
		}
		return {lo, hi};
	}

	bool trianglesIntersect (const std::array<Vec3, 3> &A, const Vec3 &nA,
	                         const std::array<Vec3, 3> &B, const Vec3 &nB, double tol)
	{
		std::array<double, 3> dA {}, dB {};   // dA : sommets de A au plan de B
		for (int k = 0; k < 3; ++k)
		{
			dA[k] = dot (sub (A[k], B[0]), nB);
			dB[k] = dot (sub (B[k], A[0]), nA);
		}
		auto maxAbs = [] (const std::array<double, 3> &d) {
			return std::max ({std::fabs (d[0]), std::fabs (d[1]), std::fabs (d[2])});
		};
		if (maxAbs (dA) <= tol && maxAbs (dB) <= tol)
			return coplanarOverlap (A, B, nA, tol);

		for (int k = 0; k < 3; ++k)
		{
			if (edgeCrossesTriangle (A[k], A[(k + 1) % 3], B, nB, tol))
				return true;
			if (edgeCrossesTriangle (B[k], B[(k + 1) % 3], A, nA, tol))
				return true;
		}

		if (!straddles (dA, tol) || !straddles (dB, tol))
			return false;
		Vec3 D = cross (nA, nB);
		const double dn = norm (D);
		if (dn <= 0.0)
			return false;
		D = scale (D, 1.0 / dn);
		const auto [la, ha] = intervalOnLine (A, dA, D);
		const auto [lb, hb] = intervalOnLine (B, dB, D);
		return std::min (ha, hb) - std::max (la, lb) > tol;
	}

	// Arbre de boites a decoupe mediane, parcouru contre lui-meme. Choisi plutot
	// qu'une grille uniforme parce que les tailles de triangles d'un maillage de
	// CAO varient de plusieurs ordres de grandeur (longs triangles de capot
	// tessellee contre petites facettes d'arc) : une grille dimensionnee sur les
	// petits referencerait chaque grand triangle dans une multitude de cellules.
	// La classe BVH du module ne convient pas : ses noeuds sont prives, elle
	// travaille en float et exige un Mesh non const.
	class SelfBvh
	{
	public:
		explicit SelfBvh (const std::vector<Tri> &tris) : m_tris (tris)
		{
			m_order.resize (tris.size ());
			std::iota (m_order.begin (), m_order.end (), 0u);
			if (!tris.empty ())
				build (0, static_cast<unsigned> (tris.size ()));
		}

		template <class Visit>
		bool forEachOverlappingPair (Visit &&visit) const
		{
			if (m_nodes.empty ())
				return true;
			std::vector<std::pair<unsigned, unsigned>> stack {{0u, 0u}};
			while (!stack.empty ())
			{
				const auto [ia, ib] = stack.back ();
				stack.pop_back ();
				const Node &a = m_nodes[ia];
				const Node &b = m_nodes[ib];
				if (ia == ib)
				{
					if (a.leaf ())
					{
						for (unsigned i = a.start; i < a.start + a.count; ++i)
							for (unsigned j = i + 1; j < a.start + a.count; ++j)
								if (!testPair (m_order[i], m_order[j], visit))
									return false;
					}
					else
					{
						stack.push_back ({a.left, a.left});
						stack.push_back ({a.right, a.right});
						stack.push_back ({a.left, a.right});
					}
					continue;
				}
				if (!overlap (a.lo, a.hi, b.lo, b.hi))
					continue;
				if (a.leaf () && b.leaf ())
				{
					for (unsigned i = a.start; i < a.start + a.count; ++i)
						for (unsigned j = b.start; j < b.start + b.count; ++j)
							if (!testPair (m_order[i], m_order[j], visit))
								return false;
				}
				else if (b.leaf () || (!a.leaf () && a.count >= b.count))
				{
					stack.push_back ({a.left, ib});
					stack.push_back ({a.right, ib});
				}
				else
				{
					stack.push_back ({ia, b.left});
					stack.push_back ({ia, b.right});
				}
			}
			return true;
		}

	private:
		struct Node
		{
			Vec3     lo, hi;
			unsigned left = kNone, right = kNone;
			unsigned start = 0, count = 0;
			bool leaf () const { return left == kNone; }
		};

		static constexpr unsigned kLeafSize = 4;

		static bool overlap (const Vec3 &alo, const Vec3 &ahi, const Vec3 &blo, const Vec3 &bhi)
		{
			for (int k = 0; k < 3; ++k)
				if (ahi[k] < blo[k] || bhi[k] < alo[k])
					return false;
			return true;
		}

		template <class Visit>
		bool testPair (unsigned i, unsigned j, Visit &visit) const
		{
			if (overlap (m_tris[i].lo, m_tris[i].hi, m_tris[j].lo, m_tris[j].hi))
				return visit (i, j);
			return true;
		}

		unsigned build (unsigned begin, unsigned end)
		{
			const unsigned id = static_cast<unsigned> (m_nodes.size ());
			m_nodes.push_back ({});
			Vec3 lo = m_tris[m_order[begin]].lo, hi = m_tris[m_order[begin]].hi;
			Vec3 clo {std::numeric_limits<double>::max (), std::numeric_limits<double>::max (), std::numeric_limits<double>::max ()};
			Vec3 chi = scale (clo, -1.0);
			for (unsigned i = begin; i < end; ++i)
			{
				const Tri &t = m_tris[m_order[i]];
				for (int k = 0; k < 3; ++k)
				{
					lo[k] = std::min (lo[k], t.lo[k]);
					hi[k] = std::max (hi[k], t.hi[k]);
					const double c = 0.5 * (t.lo[k] + t.hi[k]);
					clo[k] = std::min (clo[k], c);
					chi[k] = std::max (chi[k], c);
				}
			}
			m_nodes[id].lo = lo;
			m_nodes[id].hi = hi;
			m_nodes[id].start = begin;
			m_nodes[id].count = end - begin;
			if (end - begin <= kLeafSize)
				return id;

			int axis = 0;
			for (int k = 1; k < 3; ++k)
				if (chi[k] - clo[k] > chi[axis] - clo[axis])
					axis = k;
			const unsigned mid = begin + (end - begin) / 2;
			std::nth_element (m_order.begin () + begin, m_order.begin () + mid, m_order.begin () + end,
				[this, axis] (unsigned x, unsigned y) {
					const double cx = m_tris[x].lo[axis] + m_tris[x].hi[axis];
					const double cy = m_tris[y].lo[axis] + m_tris[y].hi[axis];
					return cx != cy ? cx < cy : x < y;
				});
			const unsigned left  = build (begin, mid);
			const unsigned right = build (mid, end);
			m_nodes[id].left  = left;
			m_nodes[id].right = right;
			return id;
		}

		const std::vector<Tri> &m_tris;
		std::vector<unsigned>   m_order;
		std::vector<Node>       m_nodes;
	};
}

const char *MeshDefectName (MeshDefect kind)
{
	switch (kind)
	{
	case MeshDefect::InvalidIndex:             return "invalid_index";
	case MeshDefect::NonTriangleFace:          return "non_triangle_face";
	case MeshDefect::NonFiniteVertex:          return "non_finite_vertex";
	case MeshDefect::UnreferencedVertex:       return "unreferenced_vertex";
	case MeshDefect::DegenerateFace:           return "degenerate_face";
	case MeshDefect::DuplicateFace:            return "duplicate_face";
	case MeshDefect::BoundaryEdge:             return "boundary_edge";
	case MeshDefect::NonManifoldEdge:          return "non_manifold_edge";
	case MeshDefect::NonManifoldVertex:        return "non_manifold_vertex";
	case MeshDefect::InconsistentOrientation:  return "inconsistent_orientation";
	case MeshDefect::InvertedShell:            return "inverted_shell";
	case MeshDefect::UnexpectedComponentCount: return "unexpected_component_count";
	case MeshDefect::SelfIntersection:         return "self_intersection";
	}
	return "unknown";
}

bool MeshValidationReport::IsValid () const
{
	return !selfIntersectionTruncated
	    && std::all_of (counts.begin (), counts.end (), [] (std::size_t n) { return n == 0; });
}

namespace
{
	struct ReferencedVertices
	{
		std::vector<char> used, finite;
	};

	bool isFiniteVertex (const std::vector<float> &V, unsigned i)
	{
		return std::isfinite (V[3 * i]) && std::isfinite (V[3 * i + 1]) && std::isfinite (V[3 * i + 2]);
	}

	// Faces de 3 sommets ou plus, indices dans les bornes ; marque les sommets cites.
	std::vector<std::pair<unsigned, std::vector<unsigned>>> collectFaces (const Mesh &mesh, std::vector<char> &used,
	                                                                     Recorder *recorder)
	{
		const unsigned nv = mesh.GetNVertices ();
		std::vector<std::pair<unsigned, std::vector<unsigned>>> faces;
		faces.reserve (mesh.GetNFaces ());
		used.assign (nv, 0);
		for (unsigned f = 0; f < mesh.GetNFaces (); ++f)
		{
			const auto face = mesh.FaceAt (f);
			if (!face)
				continue;
			const int n = face->GetNVertices ();
			std::vector<unsigned> idx (static_cast<std::size_t> (std::max (n, 0)));
			bool inRange = true;
			for (int k = 0; k < n; ++k)
			{
				const int v = face->GetVertex (static_cast<unsigned> (k));
				if (v < 0 || static_cast<unsigned> (v) >= nv)
					inRange = false;
				else
					idx[k] = static_cast<unsigned> (v);
			}
			if (!inRange)
			{
				if (recorder)
					recorder->add (MeshDefect::InvalidIndex, {f});
				continue;
			}
			if (n != 3 && recorder)
				recorder->add (MeshDefect::NonTriangleFace, {f});
			if (n < 3)
				continue;
			for (unsigned v : idx)
				used[v] = 1;
			faces.emplace_back (f, std::move (idx));
		}
		return faces;
	}
}

double DefaultWeldTolerance (const Mesh &mesh)
{
	const std::vector<float> &V = mesh.GetVertices ();
	std::vector<char> used;
	collectFaces (mesh, used, nullptr);
	bool any = false;
	Vec3 lo {}, hi {};
	double maxAbs = 0.0;
	for (unsigned i = 0; i < used.size (); ++i)
	{
		if (!used[i] || !isFiniteVertex (V, i))
			continue;
		for (int k = 0; k < 3; ++k)
		{
			const double x = V[3 * i + k];
			lo[k] = any ? std::min (lo[k], x) : x;
			hi[k] = any ? std::max (hi[k], x) : x;
			maxAbs = std::max (maxAbs, std::fabs (x));
		}
		any = true;
	}
	const double tol = std::max (1e-6 * norm (sub (hi, lo)), 4.0 * FLT_EPSILON * maxAbs);
	return tol > 0.0 ? tol : 1.0;
}

MeshValidationReport ValidateMesh (const Mesh &mesh, const MeshValidationOptions &options)
{
	MeshValidationReport report;
	Recorder rec (report, options.maxRecordsPerKind);

	if (options.weldTolerance && !(std::isfinite (*options.weldTolerance) && *options.weldTolerance > 0.0))
		throw std::invalid_argument ("ValidateMesh: weldTolerance must be finite and > 0");
	if (options.degenerateArea && !(std::isfinite (*options.degenerateArea) && *options.degenerateArea >= 0.0))
		throw std::invalid_argument ("ValidateMesh: degenerateArea must be finite and >= 0");

	const double tol = options.weldTolerance ? *options.weldTolerance : DefaultWeldTolerance (mesh);
	const double degenerateArea = options.degenerateArea ? *options.degenerateArea : tol * tol;
	report.weldTolerance  = tol;
	report.degenerateArea = degenerateArea;

	const unsigned nv = mesh.GetNVertices ();
	const std::vector<float> &V = mesh.GetVertices ();
	auto position = [&V] (unsigned i) { return Vec3 {V[3 * i], V[3 * i + 1], V[3 * i + 2]}; };

	std::vector<char> finite (nv, 1);
	for (unsigned i = 0; i < nv; ++i)
		if (!isFiniteVertex (V, i))
		{
			finite[i] = 0;
			rec.add (MeshDefect::NonFiniteVertex, {}, {i});
		}

	std::vector<char> used;
	const auto rawFaces = collectFaces (mesh, used, &rec);
	for (unsigned i = 0; i < nv; ++i)
		if (!used[i])
			rec.add (MeshDefect::UnreferencedVertex, {}, {i});

	bool firstBox = true;
	for (unsigned i = 0; i < nv; ++i)
	{
		if (!used[i] || !finite[i])
			continue;
		const Vec3 p = position (i);
		for (int k = 0; k < 3; ++k)
		{
			report.bbMin[k] = firstBox ? p[k] : std::min (report.bbMin[k], p[k]);
			report.bbMax[k] = firstBox ? p[k] : std::max (report.bbMax[k], p[k]);
		}
		firstBox = false;
	}

	// --- Soudure ---
	auto cellOf = [tol] (const Vec3 &p) {
		return Cell {cellIndex (p[0], tol), cellIndex (p[1], tol), cellIndex (p[2], tol)};
	};
	// Deux points d'une meme cellule (non bornee) sont a moins de t sur chaque
	// axe : ils sont unis directement au premier membre, en O(n). Les distances
	// ne se mesurent qu'entre cellules voisines, et seulement si leurs boites
	// sont a moins de t et leurs grappes encore distinctes.
	struct CellData
	{
		std::vector<unsigned> members;
		Vec3                  lo, hi;
	};
	std::unordered_map<Cell, CellData, CellHash> cells;
	for (unsigned i = 0; i < nv; ++i)
	{
		if (!used[i] || !finite[i])
			continue;
		const Vec3 p = position (i);
		CellData &cd = cells[cellOf (p)];
		if (cd.members.empty ())
			cd.lo = cd.hi = p;
		for (int k = 0; k < 3; ++k)
		{
			cd.lo[k] = std::min (cd.lo[k], p[k]);
			cd.hi[k] = std::max (cd.hi[k], p[k]);
		}
		cd.members.push_back (i);
	}

	std::vector<unsigned> cluster (nv);
	std::iota (cluster.begin (), cluster.end (), 0u);
	auto chebyshev = [&position] (unsigned i, unsigned j) {
		const Vec3 p = position (i), q = position (j);
		return std::max ({std::fabs (p[0] - q[0]), std::fabs (p[1] - q[1]), std::fabs (p[2] - q[2])});
	};
	// stopAtFirst : valable quand chaque cellule est deja une seule grappe.
	auto uniteClosePairs = [&] (const std::vector<unsigned> &a, const std::vector<unsigned> &b, bool sameCell,
	                            bool stopAtFirst) {
		for (std::size_t x = 0; x < a.size (); ++x)
			for (std::size_t y = sameCell ? x + 1 : 0; y < b.size (); ++y)
				if (chebyshev (a[x], b[y]) <= tol)
				{
					unite (cluster, a[x], b[y]);
					if (stopAtFirst)
						return;
				}
	};
	for (const auto &[cell, cd] : cells)
	{
		if (isBounded (cell))
			for (unsigned j : cd.members)
				unite (cluster, cd.members.front (), j);
		else
			uniteClosePairs (cd.members, cd.members, true, false);
	}
	for (const auto &[cell, cd] : cells)
		for (int dx = -1; dx <= 1; ++dx)
			for (int dy = -1; dy <= 1; ++dy)
				for (int dz = -1; dz <= 1; ++dz)
				{
					const Cell other {cell[0] + dx, cell[1] + dy, cell[2] + dz};
					if (!(cell < other))
						continue;
					const auto it = cells.find (other);
					if (it == cells.end ())
						continue;
					const CellData &od = it->second;
					double gap = 0.0;
					for (int k = 0; k < 3; ++k)
						gap = std::max ({gap, od.lo[k] - cd.hi[k], cd.lo[k] - od.hi[k]});
					if (gap > tol)
						continue;
					const bool bounded = isBounded (cell) && isBounded (other);
					if (bounded && findRoot (cluster, cd.members.front ()) == findRoot (cluster, od.members.front ()))
						continue;
					uniteClosePairs (cd.members, od.members, false, bounded);
				}

	// Une grappe par racine ; la racine est le plus petit indice original.
	std::vector<unsigned> rootToWelded (nv, kNone);
	std::vector<unsigned> weld (nv, kNone);
	std::vector<Vec3>     W;
	std::vector<unsigned> weldedOrigin;   // plus petit indice original de chaque grappe
	std::vector<char>     weldedFinite;
	for (unsigned i = 0; i < nv; ++i)
	{
		if (!used[i])
			continue;
		const unsigned root = findRoot (cluster, i);
		unsigned &w = rootToWelded[root];
		if (w == kNone)
		{
			w = static_cast<unsigned> (W.size ());
			W.push_back (position (i));
			weldedOrigin.push_back (i);
			weldedFinite.push_back (finite[i]);
		}
		else
			W[w] = std::min (W[w], position (i));
		weld[i] = w;
	}
	report.weldedVertices = static_cast<unsigned> (W.size ());

	const std::size_t nf = rawFaces.size ();
	std::vector<std::vector<unsigned>> faces (nf);
	for (std::size_t fi = 0; fi < nf; ++fi)
	{
		const auto &raw = rawFaces[fi].second;
		faces[fi].resize (raw.size ());
		for (std::size_t k = 0; k < raw.size (); ++k)
			faces[fi][k] = weld[raw[k]];
	}
	auto originalFace = [&rawFaces] (std::size_t fi) { return rawFaces[fi].first; };

	// --- Geometrie par face ---
	std::vector<char>   geometric (nf, 0);   // tous sommets finis
	std::vector<char>   degenerate (nf, 0);
	std::vector<double> faceVolume (nf, 0.0);
	for (std::size_t fi = 0; fi < nf; ++fi)
	{
		const auto &f = faces[fi];
		std::vector<unsigned> sorted = f;
		std::sort (sorted.begin (), sorted.end ());
		const bool repeated = std::adjacent_find (sorted.begin (), sorted.end ()) != sorted.end ();
		geometric[fi] = std::all_of (f.begin (), f.end (), [&weldedFinite] (unsigned w) { return weldedFinite[w] != 0; });
		double area = 0.0;
		if (geometric[fi])
		{
			Vec3 normal {0.0, 0.0, 0.0};
			for (std::size_t k = 1; k + 1 < f.size (); ++k)
			{
				const Vec3 &a = W[f[0]];
				const Vec3 &b = W[f[k]];
				const Vec3 &d = W[f[k + 1]];
				normal = add (normal, cross (sub (b, a), sub (d, a)));
				faceVolume[fi] += dot (a, cross (b, d)) / 6.0;
			}
			area = 0.5 * norm (normal);
			report.area += area;
			report.signedVolume += faceVolume[fi];
		}
		if (repeated || (geometric[fi] && area < degenerateArea))
		{
			degenerate[fi] = 1;
			rec.add (MeshDefect::DegenerateFace, {originalFace (fi)});
		}
	}

	// --- Faces dupliquees ---
	{
		std::vector<std::pair<std::vector<unsigned>, unsigned>> keyed;
		keyed.reserve (nf);
		for (std::size_t fi = 0; fi < nf; ++fi)
		{
			std::vector<unsigned> key = faces[fi];
			std::sort (key.begin (), key.end ());
			keyed.emplace_back (std::move (key), static_cast<unsigned> (fi));
		}
		std::sort (keyed.begin (), keyed.end ());
		std::size_t groupStart = 0;
		for (std::size_t i = 1; i < keyed.size (); ++i)
		{
			if (keyed[i].first != keyed[groupStart].first)
			{
				groupStart = i;
				continue;
			}
			rec.add (MeshDefect::DuplicateFace,
			         {originalFace (keyed[groupStart].second), originalFace (keyed[i].second)});
		}
	}

	// --- Aretes ---
	struct EdgeUse
	{
		std::uint64_t key;
		unsigned      face;
		int           dir;
	};
	std::vector<EdgeUse> edges;
	for (std::size_t fi = 0; fi < nf; ++fi)
	{
		const auto &f = faces[fi];
		for (std::size_t k = 0; k < f.size (); ++k)
		{
			const unsigned a = f[k];
			const unsigned b = f[(k + 1) % f.size ()];
			if (a == b)
				continue;
			const std::uint64_t lo = std::min (a, b);
			const std::uint64_t hi = std::max (a, b);
			edges.push_back ({(lo << 32) | hi, static_cast<unsigned> (fi), a < b ? +1 : -1});
		}
	}
	std::sort (edges.begin (), edges.end (), [] (const EdgeUse &x, const EdgeUse &y) {
		return x.key != y.key ? x.key < y.key : x.face < y.face;
	});

	std::vector<unsigned> component (nf);
	std::iota (component.begin (), component.end (), 0u);
	std::vector<char> openEdgeFace (nf, 0);
	for (std::size_t i = 0; i < edges.size ();)
	{
		std::size_t j = i;
		while (j < edges.size () && edges[j].key == edges[i].key)
			++j;
		const std::size_t n = j - i;
		auto edgeRecord = [&] (MeshDefect kind) {
			std::vector<unsigned> fs;
			for (std::size_t k = i; k < j; ++k)
				fs.push_back (originalFace (edges[k].face));
			const unsigned a = static_cast<unsigned> (edges[i].key >> 32);
			const unsigned b = static_cast<unsigned> (edges[i].key & 0xFFFFFFFFu);
			rec.add (kind, std::move (fs), {weldedOrigin[a], weldedOrigin[b]});
		};
		if (n == 1)
			edgeRecord (MeshDefect::BoundaryEdge);
		else if (n > 2)
			edgeRecord (MeshDefect::NonManifoldEdge);
		else if (edges[i].dir == edges[i + 1].dir)
			edgeRecord (MeshDefect::InconsistentOrientation);
		if (n != 2 || edges[i].dir == edges[i + 1].dir)
			for (std::size_t k = i; k < j; ++k)
				openEdgeFace[edges[k].face] = 1;
		for (std::size_t k = i + 1; k < j; ++k)
			unite (component, edges[i].face, edges[k].face);
		i = j;
	}

	// --- Composantes et coques inversees ---
	{
		std::vector<double> volume (nf, 0.0);
		std::vector<char>   open (nf, 0);
		for (std::size_t fi = 0; fi < nf; ++fi)
		{
			const unsigned r = findRoot (component, static_cast<unsigned> (fi));
			volume[r] += faceVolume[fi];
			if (openEdgeFace[fi] || !geometric[fi])
				open[r] = 1;
		}
		for (std::size_t fi = 0; fi < nf; ++fi)
		{
			if (findRoot (component, static_cast<unsigned> (fi)) != fi)
				continue;
			++report.components;
			if (!open[fi] && volume[fi] < 0.0)
				rec.add (MeshDefect::InvertedShell, {originalFace (fi)}, {weldedOrigin[faces[fi].front ()]});
		}
		if (options.expectedComponents && *options.expectedComponents != report.components)
			rec.add (MeshDefect::UnexpectedComponentCount);
	}

	// --- Sommets non manifold : plus d'un eventail ---
	{
		std::vector<std::vector<unsigned>> vertexFaces (W.size ());
		for (std::size_t fi = 0; fi < nf; ++fi)
		{
			std::vector<unsigned> distinct = faces[fi];
			std::sort (distinct.begin (), distinct.end ());
			distinct.erase (std::unique (distinct.begin (), distinct.end ()), distinct.end ());
			for (unsigned v : distinct)
				vertexFaces[v].push_back (static_cast<unsigned> (fi));
		}
		std::vector<std::pair<unsigned, unsigned>> spokes;
		std::vector<unsigned> fan;
		for (unsigned v = 0; v < W.size (); ++v)
		{
			const auto &fs = vertexFaces[v];
			if (fs.size () < 2)
				continue;
			spokes.clear ();
			for (unsigned i = 0; i < fs.size (); ++i)
			{
				const auto &f = faces[fs[i]];
				const std::size_t n = f.size ();
				for (std::size_t k = 0; k < n; ++k)
				{
					if (f[k] != v)
						continue;
					for (unsigned o : {f[(k + n - 1) % n], f[(k + 1) % n]})
						if (o != v)
							spokes.emplace_back (o, i);
				}
			}
			std::sort (spokes.begin (), spokes.end ());
			fan.resize (fs.size ());
			std::iota (fan.begin (), fan.end (), 0u);
			for (std::size_t k = 1; k < spokes.size (); ++k)
				if (spokes[k].first == spokes[k - 1].first)
					unite (fan, spokes[k].second, spokes[k - 1].second);
			unsigned fans = 0;
			for (unsigned i = 0; i < fs.size (); ++i)
				if (findRoot (fan, i) == i)
					++fans;
			if (fans > 1)
			{
				std::vector<unsigned> incident;
				for (unsigned fi : fs)
					incident.push_back (originalFace (fi));
				rec.add (MeshDefect::NonManifoldVertex, std::move (incident), {weldedOrigin[v]});
			}
		}
	}

	// --- Auto-intersections ---
	if (options.checkSelfIntersection)
	{
		report.selfIntersectionChecked = true;
		std::vector<Tri> tris;
		for (std::size_t fi = 0; fi < nf; ++fi)
		{
			const auto &f = faces[fi];
			if (f.size () != 3 || degenerate[fi] || !geometric[fi])
				continue;
			Tri t;
			t.w = {f[0], f[1], f[2]};
			t.face = originalFace (fi);
			const Vec3 n = cross (sub (W[f[1]], W[f[0]]), sub (W[f[2]], W[f[0]]));
			const double len = norm (n);
			if (len <= 0.0)
				continue;
			t.n = scale (n, 1.0 / len);
			for (int k = 0; k < 3; ++k)
			{
				t.lo[k] = std::min ({W[f[0]][k], W[f[1]][k], W[f[2]][k]}) - tol;
				t.hi[k] = std::max ({W[f[0]][k], W[f[1]][k], W[f[2]][k]}) + tol;
			}
			tris.push_back (t);
		}
		const SelfBvh bvh (tris);
		const std::size_t budget = options.maxSelfIntersectionPairs
			? *options.maxSelfIntersectionPairs
			: std::max (kMinSelfIntersectionPairs, kSelfIntersectionPairsPerTriangle * tris.size ());
		report.selfIntersectionPairBudget = budget;
		const bool complete = bvh.forEachOverlappingPair ([&] (unsigned i, unsigned j) {
			if (report.selfIntersectionPairsTested >= budget)
				return false;
			++report.selfIntersectionPairsTested;
			const Tri &a = tris[i];
			const Tri &b = tris[j];
			std::array<unsigned, 3> sa = a.w, sb = b.w;
			std::sort (sa.begin (), sa.end ());
			std::sort (sb.begin (), sb.end ());
			if (sa == sb)
				return true;
			const std::array<Vec3, 3> A {W[a.w[0]], W[a.w[1]], W[a.w[2]]};
			const std::array<Vec3, 3> B {W[b.w[0]], W[b.w[1]], W[b.w[2]]};
			if (trianglesIntersect (A, a.n, B, b.n, tol))
				rec.add (MeshDefect::SelfIntersection, {std::min (a.face, b.face), std::max (a.face, b.face)});
			return true;
		});
		report.selfIntersectionTruncated = !complete;
	}

	return report;
}
