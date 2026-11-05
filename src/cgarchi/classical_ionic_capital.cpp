#include <cgarchi/classical_ionic_capital.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

#include "mesh.h"
#include "polygon2.h"

namespace cgarchi
{
namespace classical
{

namespace
{

using Edge = std::pair<unsigned, unsigned>;

Edge Undirected (unsigned a, unsigned b)
{
	return a < b ? Edge {a, b} : Edge {b, a};
}

std::set<Edge> Edges (const std::vector<unsigned> &polygon)
{
	std::set<Edge> edges;
	for (std::size_t i = 0; i < polygon.size (); ++i)
		edges.insert (Undirected (polygon[i], polygon[(i + 1) % polygon.size ()]));
	return edges;
}

class MeshBuilder
{
public:
	explicit MeshBuilder (const std::vector<VolutePoint> &points, std::array<double, 4> levels)
		: m_points (points), m_levels (levels), m_index (points.size (), {-1, -1, -1, -1})
	{
	}

	unsigned Vertex (unsigned point, int level)
	{
		int &index = m_index[point][level];
		if (index < 0)
		{
			index = static_cast<int> (m_positions.size ());
			m_positions.push_back ({m_points[point].u, m_points[point].v, m_levels[level]});
		}
		return static_cast<unsigned> (index);
	}

	void Triangle (unsigned a, unsigned b, unsigned c)
	{
		m_triangles.push_back ({a, b, c});
	}

	// Paroi de l'arete a -> b (sens trigonometrique de sa region), normale
	// sortante, entre deux niveaux.
	void Wall (unsigned a, unsigned b, int low, int high)
	{
		const unsigned a0 = Vertex (a, low), b0 = Vertex (b, low);
		const unsigned a1 = Vertex (a, high), b1 = Vertex (b, high);
		Triangle (a0, b0, b1);
		Triangle (a0, b1, a1);
	}

	void Face (const std::vector<std::array<unsigned, 3>> &triangles, int level, bool up)
	{
		for (const std::array<unsigned, 3> &t : triangles)
		{
			const unsigned a = Vertex (t[0], level), b = Vertex (t[1], level), c = Vertex (t[2], level);
			if (up)
				Triangle (a, b, c);
			else
				Triangle (a, c, b);
		}
	}

	std::unique_ptr<Mesh> Build () const
	{
		auto mesh = std::make_unique<Mesh> (static_cast<unsigned> (m_positions.size ()),
		                                    static_cast<unsigned> (m_triangles.size ()));
		for (std::size_t v = 0; v < m_positions.size (); ++v)
			mesh->SetVertex (static_cast<unsigned> (v), static_cast<float> (m_positions[v][0]),
			                 static_cast<float> (m_positions[v][1]), static_cast<float> (m_positions[v][2]));
		for (std::size_t f = 0; f < m_triangles.size (); ++f)
			mesh->FaceAt (static_cast<unsigned> (f))->SetTriangle (m_triangles[f][0], m_triangles[f][1], m_triangles[f][2]);
		return mesh;
	}

private:
	const std::vector<VolutePoint> &m_points;
	std::array<double, 4> m_levels;
	std::vector<std::array<int, 4>> m_index;
	std::vector<std::array<double, 3>> m_positions;
	std::vector<std::array<unsigned, 3>> m_triangles;
};

} // namespace

std::vector<std::array<unsigned, 3>> TessellatePolygon (const std::vector<VolutePoint> &points,
                                                        const std::vector<unsigned> &polygon)
{
	std::vector<float> x, y;
	x.reserve (polygon.size ());
	y.reserve (polygon.size ());
	for (unsigned index : polygon)
	{
		x.push_back (static_cast<float> (points.at (index).u));
		y.push_back (static_cast<float> (points.at (index).v));
	}
	Polygon2 contour;
	contour.input (x.data (), y.data (), static_cast<int> (polygon.size ()));

	float *rawVertices = nullptr;
	unsigned *rawFaces = nullptr;
	unsigned nVertices = 0, nFaces = 0;
	contour.tesselate (&rawVertices, &nVertices, &rawFaces, &nFaces);
	const std::unique_ptr<float, decltype (&std::free)> vertices (rawVertices, &std::free);
	const std::unique_ptr<unsigned, decltype (&std::free)> faces (rawFaces, &std::free);
	if (nVertices != polygon.size ())
		throw std::invalid_argument ("TessellatePolygon : le tessellateur a cree des sommets");
	if (nFaces == 0 || !faces)
		throw std::invalid_argument ("TessellatePolygon : aucun triangle");

	// Le tessellateur signale ses erreurs sans les rendre : une triangulation
	// partielle ou retournee se lit sur les aires.
	std::vector<std::array<unsigned, 3>> out;
	out.reserve (nFaces);
	double covered = 0.0;
	for (unsigned f = 0; f < nFaces; ++f)
	{
		const std::array<unsigned, 3> t {polygon[faces.get ()[3 * f]], polygon[faces.get ()[3 * f + 1]],
		                                 polygon[faces.get ()[3 * f + 2]]};
		const double area = PolygonArea (points, {t[0], t[1], t[2]});
		if (!(area > 0.0))
			throw std::invalid_argument ("TessellatePolygon : triangle d'aire non positive");
		covered += area;
		out.push_back (t);
	}
	const double expected = PolygonArea (points, polygon);
	if (std::fabs (covered - expected) > 1e-9 * std::fabs (expected))
		throw std::invalid_argument ("TessellatePolygon : les triangles ne couvrent pas le polygone");
	return out;
}

std::unique_ptr<Mesh> BuildBolster (const VoluteSection &section, double halfDepth, double relief,
                                    bool splitWallsAtFaces)
{
	if (!(halfDepth > 0.0) || !(relief > 0.0))
		throw std::invalid_argument ("BuildBolster : profondeur ou relief non positif");
	const double s = PolygonArea (section.points, section.body);
	if (!(s > 0.0) || std::fabs (PolygonArea (section.points, section.canal)
	                             + PolygonArea (section.points, section.raised) - s) > 1e-9 * s)
		throw std::invalid_argument ("BuildBolster : C et R ne partitionnent pas S");

	const std::set<Edge> body = Edges (section.body);
	const std::set<Edge> raised = Edges (section.raised);
	std::set<Edge> expected;
	for (const Edge &edge : body)
		if (!raised.count (edge))
			expected.insert (edge);
	for (const Edge &edge : raised)
		if (!body.count (edge))
			expected.insert (edge);
	if (Edges (section.canal) != expected)
		throw std::invalid_argument ("BuildBolster : le bord du canal n'est pas la difference des bords");

	// Niveaux : -(F + d), -F, F, F + d.
	MeshBuilder mesh (section.points, {-halfDepth - relief, -halfDepth, halfDepth, halfDepth + relief});

	const auto canal = TessellatePolygon (section.points, section.canal);
	const auto top = TessellatePolygon (section.points, section.raised);
	mesh.Face (canal, 2, true);
	mesh.Face (canal, 1, false);
	mesh.Face (top, 3, true);
	mesh.Face (top, 0, false);

	const std::size_t ns = section.body.size ();
	for (std::size_t i = 0; i < ns; ++i)
	{
		const unsigned a = section.body[i], b = section.body[(i + 1) % ns];
		if (!raised.count (Undirected (a, b)))
			mesh.Wall (a, b, 1, 2);
		else if (splitWallsAtFaces)
		{
			mesh.Wall (a, b, 0, 1);
			mesh.Wall (a, b, 1, 2);
			mesh.Wall (a, b, 2, 3);
		}
		else
			mesh.Wall (a, b, 0, 3);
	}
	const std::size_t nr = section.raised.size ();
	for (std::size_t i = 0; i < nr; ++i)
	{
		const unsigned a = section.raised[i], b = section.raised[(i + 1) % nr];
		if (body.count (Undirected (a, b)))
			continue;
		mesh.Wall (a, b, 0, 1);
		mesh.Wall (a, b, 2, 3);
	}
	return mesh.Build ();
}

std::vector<std::unique_ptr<Mesh>> BuildIonicVolutes (const OrderData &order, const ColumnProfile &profile,
                                                      const VoluteOptions &options)
{
	if (!order.volute)
		throw std::invalid_argument ("BuildIonicVolutes : pas de volute ionique pour " + order.key);
	const IonicVoluteData &v = *order.volute;

	bool found = false;
	double capitalTop = 0.0;
	for (const ProfileMember &m : profile.members)
		if (m.block == "chapiteau")
		{
			capitalTop = found ? std::max (capitalTop, m.y1) : m.y1;
			found = true;
		}
	if (!found)
		throw std::invalid_argument ("BuildIonicVolutes : profil sans chapiteau");
	const double zEye = capitalTop - v.topBelowCapital - v.centreFromTop;

	const VoluteSpiral spiral = BuildTwelveCentreVolute (v);
	const double closureU = -v.eyeDiameter / 4.0;   // cathete du premier centre, -a1
	const VoluteSection section = BuildVoluteSection (spiral, closureU, options.segmentsPerQuarter);
	const std::unique_ptr<Mesh> local = BuildBolster (section, v.faceSpacing / 2.0, v.listelFace);

	std::vector<std::unique_ptr<Mesh>> out;
	for (double side : { 1.0, -1.0 })
	{
		const unsigned nv = local->GetNVertices (), nf = local->GetNFaces ();
		auto mesh = std::make_unique<Mesh> (nv, nf);
		for (unsigned i = 0; i < nv; ++i)
		{
			float p[3];
			local->GetVertex (i, p);
			mesh->SetVertex (i, static_cast<float> (side * (v.eyeFromAxis + p[0]) * options.scale),
			                 static_cast<float> (p[2] * options.scale),
			                 static_cast<float> ((zEye + p[1]) * options.scale));
		}
		// (u, v, w) -> (side u, w, v) : determinant -side. Un determinant
		// negatif retourne les faces.
		const bool flip = side > 0.0;
		for (unsigned f = 0; f < nf; ++f)
		{
			const unsigned a = static_cast<unsigned> (local->GetFaceVertex (f, 0));
			const unsigned b = static_cast<unsigned> (local->GetFaceVertex (f, 1));
			const unsigned c = static_cast<unsigned> (local->GetFaceVertex (f, 2));
			if (flip)
				mesh->FaceAt (f)->SetTriangle (a, c, b);
			else
				mesh->FaceAt (f)->SetTriangle (a, b, c);
		}
		out.push_back (std::move (mesh));
	}
	return out;
}

} // namespace classical
} // namespace cgarchi
