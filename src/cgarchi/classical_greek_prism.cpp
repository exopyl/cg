#include <cgarchi/classical_greek_prism_detail.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <vector>

#include <cgarchi/classical_greek_section_detail.h>
#include "mesh.h"

namespace cgarchi
{
namespace classical
{
namespace detail
{

namespace
{

std::array<double, 3> Point (Extrusion axis, const std::array<double, 2> &uv, double w)
{
	switch (axis)
	{
	case Extrusion::AlongX: return {w, uv[0], uv[1]};
	case Extrusion::AlongY: return {uv[0], -w, uv[1]};
	case Extrusion::AlongZ: return {uv[0], uv[1], w};
	}
	return {};
}

std::vector<std::array<double, 2>> Cleaned (const std::vector<std::array<double, 2>> &in)
{
	std::vector<std::array<double, 2>> out;
	out.reserve (in.size ());
	for (const std::array<double, 2> &q : in)
		if (out.empty () || q != out.back ())
			out.push_back (q);
	while (out.size () > 1 && out.front () == out.back ())
		out.pop_back ();
	bool removed = true;
	while (removed && out.size () > 3)
	{
		removed = false;
		for (std::size_t i = 0; i < out.size (); ++i)
		{
			const std::size_t n = out.size ();
			if (Cross (out[(i + n - 1) % n], out[i], out[(i + 1) % n]) == 0.0)
			{
				out.erase (out.begin () + static_cast<std::ptrdiff_t> (i));
				removed = true;
				break;
			}
		}
	}
	return out;
}

// Polygone simple (IsSimple : aretes non voisines disjointes, sans repli), d'aire
// non nulle : la decoupe d'oreilles rend n - 2 triangles meme pour un polygone
// croise, elle ne suffit pas a le refuser.
bool SimplePolygon (const std::vector<std::array<double, 2>> &polygon)
{
	std::vector<RingSectionPoint> points;
	points.reserve (polygon.size ());
	for (const std::array<double, 2> &q : polygon)
		points.push_back ({q[0], q[1]});
	return polygon.size () >= 3 && IsSimple (points) && TwiceSignedArea (points) != 0.0;
}

} // namespace

std::unique_ptr<Mesh> PrismMesh (const std::vector<std::array<double, 2>> &outline, Extrusion axis, double w0, double w1)
{
	std::vector<std::array<double, 2>> polygon = Cleaned (outline);
	double area = 0.0;
	for (std::size_t i = 0; i < polygon.size (); ++i)
	{
		const std::array<double, 2> &a = polygon[i], &b = polygon[(i + 1) % polygon.size ()];
		area += StrictCrossDifference (a[0], b[1], b[0], a[1]);
	}
	if (area < 0.0)
		std::reverse (polygon.begin (), polygon.end ());
	const std::size_t n = polygon.size ();
	const std::vector<std::array<unsigned, 3>> caps = EarClip (polygon);
	if (n < 3 || !SimplePolygon (polygon) || caps.size () != n - 2)
		throw std::logic_error ("Prisme : profil non simple");

	std::vector<float> vertices;
	vertices.reserve (6 * n);
	for (const double w : {w0, w1})
		for (const std::array<double, 2> &uv : polygon)
			for (const double c : Point (axis, uv, w))
				vertices.push_back (static_cast<float> (c));
	const unsigned top = static_cast<unsigned> (n);
	std::vector<unsigned> faces;
	faces.reserve (3 * (2 * caps.size () + 2 * n));
	for (const std::array<unsigned, 3> &t : caps)
		faces.insert (faces.end (), {t[0], t[2], t[1]});
	for (const std::array<unsigned, 3> &t : caps)
		faces.insert (faces.end (), {top + t[0], top + t[1], top + t[2]});
	for (unsigned i = 0; i < top; ++i)
	{
		const unsigned j = (i + 1) % top;
		faces.insert (faces.end (), {i, j, top + j, i, top + j, top + i});
	}
	auto mesh = std::make_unique<Mesh> ();
	mesh->SetVertices (static_cast<unsigned> (vertices.size () / 3), vertices.data ());
	mesh->SetFaces (static_cast<unsigned> (faces.size () / 3), 3, faces.data ());
	return mesh;
}

std::unique_ptr<Mesh> BoxMesh (double x0, double x1, double y0, double y1, double z0, double z1)
{
	return PrismMesh ({{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}, Extrusion::AlongZ, z0, z1);
}

std::unique_ptr<Mesh> StripMesh (const std::vector<std::array<double, 2>> &profile, StripAxis axis, double face,
                                 StripOutward outward, double t0, bool mitre0, double t1, bool mitre1)
{
	for (const std::array<double, 2> &q : profile)
		if (!(std::isfinite (q[0]) && q[0] >= 0.0))
			throw std::logic_error ("Bande : saillie de profil negative ou non finie");
	std::vector<std::array<double, 2>> polygon = Cleaned (profile);
	double area = 0.0;
	for (std::size_t i = 0; i < polygon.size (); ++i)
	{
		const std::array<double, 2> &a = polygon[i], &b = polygon[(i + 1) % polygon.size ()];
		area += StrictCrossDifference (a[0], b[1], b[0], a[1]);
	}
	if (area < 0.0)
		std::reverse (polygon.begin (), polygon.end ());
	const std::size_t n = polygon.size ();
	const std::vector<std::array<unsigned, 3>> caps = EarClip (polygon);
	if (n < 3 || !SimplePolygon (polygon) || caps.size () != n - 2)
		throw std::logic_error ("Bande : profil non simple");

	// Repere (d, z, t) : direct si d x z = +t. Le long de X, d suit +-Y, d x z = +-X ;
	// le long de Y, d suit +-X, d x z = -+Y.
	const bool increasing = outward == StripOutward::Increasing;
	const bool direct = axis == StripAxis::AlongX ? increasing : !increasing;
	std::vector<float> vertices;
	vertices.reserve (6 * n);
	for (const bool end : {false, true})
		for (const std::array<double, 2> &q : polygon)
		{
			const double d = q[0];
			const double t = end ? (mitre1 ? t1 - d : t1) : (mitre0 ? t0 + d : t0);
			const double other = increasing ? face + d : face - d;
			const double x = axis == StripAxis::AlongX ? t : other, y = axis == StripAxis::AlongX ? other : t;
			vertices.insert (vertices.end (), {static_cast<float> (x), static_cast<float> (y), static_cast<float> (q[1])});
		}
	const unsigned top = static_cast<unsigned> (n);
	const std::size_t along = axis == StripAxis::AlongX ? 0 : 1;
	for (unsigned i = 0; i < top; ++i)
		if (!(vertices[3 * i + along] < vertices[3 * (top + i) + along]))
			throw std::logic_error ("Bande : un bout n'est pas strictement avant l'autre");
	std::vector<unsigned> faces;
	faces.reserve (3 * (2 * caps.size () + 2 * n));
	for (const std::array<unsigned, 3> &t : caps)
		faces.insert (faces.end (), direct ? std::initializer_list<unsigned> {t[0], t[2], t[1]}
		                                   : std::initializer_list<unsigned> {t[0], t[1], t[2]});
	for (const std::array<unsigned, 3> &t : caps)
		faces.insert (faces.end (), direct ? std::initializer_list<unsigned> {top + t[0], top + t[1], top + t[2]}
		                                   : std::initializer_list<unsigned> {top + t[0], top + t[2], top + t[1]});
	for (unsigned i = 0; i < top; ++i)
	{
		const unsigned j = (i + 1) % top;
		if (direct)
			faces.insert (faces.end (), {i, j, top + j, i, top + j, top + i});
		else
			faces.insert (faces.end (), {i, top + j, j, i, top + i, top + j});
	}
	auto mesh = std::make_unique<Mesh> ();
	mesh->SetVertices (static_cast<unsigned> (vertices.size () / 3), vertices.data ());
	mesh->SetFaces (static_cast<unsigned> (faces.size () / 3), 3, faces.data ());
	return mesh;
}

namespace
{

void RequireGridLines (const std::vector<double> &lines)
{
	if (lines.size () < 2)
		throw std::logic_error ("Grille : moins de deux lignes sur un axe");
	for (std::size_t k = 1; k < lines.size (); ++k)
		if (!(lines[k - 1] < lines[k]) || static_cast<float> (lines[k - 1]) == static_cast<float> (lines[k]))
			throw std::logic_error ("Grille : lignes non strictement croissantes, ou confondues en float");
}

} // namespace

std::unique_ptr<Mesh> GridSolidMesh (const std::vector<double> &xs, const std::vector<double> &ys, const std::vector<double> &zs,
                                     const std::function<bool (std::size_t, std::size_t, std::size_t)> &occupied)
{
	RequireGridLines (xs);
	RequireGridLines (ys);
	RequireGridLines (zs);
	const std::size_t nx = xs.size () - 1, ny = ys.size () - 1, nz = zs.size () - 1;
	std::vector<char> cells (nx * ny * nz);
	for (std::size_t k = 0; k < nz; ++k)
		for (std::size_t j = 0; j < ny; ++j)
			for (std::size_t i = 0; i < nx; ++i)
				cells[(k * ny + j) * nx + i] = occupied (i, j, k) ? 1 : 0;
	auto full = [&] (std::ptrdiff_t i, std::ptrdiff_t j, std::ptrdiff_t k) {
		return i >= 0 && j >= 0 && k >= 0 && static_cast<std::size_t> (i) < nx && static_cast<std::size_t> (j) < ny &&
		       static_cast<std::size_t> (k) < nz && cells[(static_cast<std::size_t> (k) * ny + static_cast<std::size_t> (j)) * nx +
		                                                  static_cast<std::size_t> (i)] != 0;
	};

	// Contacts par une arete seule (bloc 2 x 2 d'un plan d'axe) ou par un sommet seul (bloc 2 x 2 x 2).
	for (std::ptrdiff_t k = -1; k < static_cast<std::ptrdiff_t> (nz); ++k)
		for (std::ptrdiff_t j = -1; j < static_cast<std::ptrdiff_t> (ny); ++j)
			for (std::ptrdiff_t i = -1; i < static_cast<std::ptrdiff_t> (nx); ++i)
			{
				bool c[2][2][2];
				int count = 0;
				for (int a = 0; a < 2; ++a)
					for (int b = 0; b < 2; ++b)
						for (int d = 0; d < 2; ++d)
							count += (c[a][b][d] = full (i + a, j + b, k + d)) ? 1 : 0;
				for (int d = 0; d < 2; ++d)
				{
					const bool xy = (c[0][0][d] && c[1][1][d] && !c[1][0][d] && !c[0][1][d]) ||
					                (c[1][0][d] && c[0][1][d] && !c[0][0][d] && !c[1][1][d]);
					const bool xz = (c[0][d][0] && c[1][d][1] && !c[1][d][0] && !c[0][d][1]) ||
					                (c[1][d][0] && c[0][d][1] && !c[0][d][0] && !c[1][d][1]);
					const bool yz = (c[d][0][0] && c[d][1][1] && !c[d][1][0] && !c[d][0][1]) ||
					                (c[d][1][0] && c[d][0][1] && !c[d][0][0] && !c[d][1][1]);
					if (xy || xz || yz)
						throw std::logic_error ("Grille : deux cellules occupees ne se touchent que par une arete");
				}
				if (count == 2 && ((c[0][0][0] && c[1][1][1]) || (c[1][0][0] && c[0][1][1]) || (c[0][1][0] && c[1][0][1]) ||
				                   (c[0][0][1] && c[1][1][0])))
					throw std::logic_error ("Grille : deux cellules occupees ne se touchent que par un sommet");
				if (count == 6 && ((!c[0][0][0] && !c[1][1][1]) || (!c[1][0][0] && !c[0][1][1]) || (!c[0][1][0] && !c[1][0][1]) ||
				                   (!c[0][0][1] && !c[1][1][0])))
					throw std::logic_error ("Grille : deux cellules vides ne se touchent que par un sommet");
			}

	// Faces de bord, quadrangles de noeuds (i, j, k) dans le sens direct vu de l'exterieur de la cellule occupee.
	using Node = std::array<std::size_t, 3>;
	std::vector<std::array<Node, 4>> quads;
	for (std::size_t k = 0; k < nz; ++k)
		for (std::size_t j = 0; j < ny; ++j)
			for (std::size_t i = 0; i < nx; ++i)
			{
				const std::ptrdiff_t si = static_cast<std::ptrdiff_t> (i), sj = static_cast<std::ptrdiff_t> (j),
				                     sk = static_cast<std::ptrdiff_t> (k);
				if (!full (si, sj, sk))
					continue;
				if (!full (si + 1, sj, sk))
					quads.push_back ({Node {i + 1, j, k}, Node {i + 1, j + 1, k}, Node {i + 1, j + 1, k + 1}, Node {i + 1, j, k + 1}});
				if (!full (si - 1, sj, sk))
					quads.push_back ({Node {i, j, k}, Node {i, j, k + 1}, Node {i, j + 1, k + 1}, Node {i, j + 1, k}});
				if (!full (si, sj + 1, sk))
					quads.push_back ({Node {i, j + 1, k}, Node {i, j + 1, k + 1}, Node {i + 1, j + 1, k + 1}, Node {i + 1, j + 1, k}});
				if (!full (si, sj - 1, sk))
					quads.push_back ({Node {i, j, k}, Node {i + 1, j, k}, Node {i + 1, j, k + 1}, Node {i, j, k + 1}});
				if (!full (si, sj, sk + 1))
					quads.push_back ({Node {i, j, k + 1}, Node {i + 1, j, k + 1}, Node {i + 1, j + 1, k + 1}, Node {i, j + 1, k + 1}});
				if (!full (si, sj, sk - 1))
					quads.push_back ({Node {i, j, k}, Node {i, j + 1, k}, Node {i + 1, j + 1, k}, Node {i + 1, j, k}});
			}

	const std::size_t mx = nx + 1, my = ny + 1, mz = nz + 1;
	auto key = [&] (const Node &n) { return (n[2] * my + n[1]) * mx + n[0]; };
	constexpr unsigned kUnused = ~0u;
	std::vector<unsigned> index (mx * my * mz, kUnused);
	for (const std::array<Node, 4> &q : quads)
		for (const Node &n : q)
			index[key (n)] = 0;
	std::vector<float> vertices;
	unsigned next = 0;
	for (std::size_t k = 0; k < mz; ++k)
		for (std::size_t j = 0; j < my; ++j)
			for (std::size_t i = 0; i < mx; ++i)
			{
				unsigned &slot = index[(k * my + j) * mx + i];
				if (slot == kUnused)
					continue;
				slot = next++;
				vertices.insert (vertices.end (), {static_cast<float> (xs[i]), static_cast<float> (ys[j]), static_cast<float> (zs[k])});
			}
	std::vector<unsigned> faces;
	faces.reserve (6 * quads.size ());
	for (const std::array<Node, 4> &q : quads)
	{
		const unsigned a = index[key (q[0])], b = index[key (q[1])], c = index[key (q[2])], d = index[key (q[3])];
		faces.insert (faces.end (), {a, b, c, a, c, d});
	}
	auto mesh = std::make_unique<Mesh> ();
	mesh->SetVertices (static_cast<unsigned> (vertices.size () / 3), vertices.data ());
	mesh->SetFaces (static_cast<unsigned> (faces.size () / 3), 3, faces.data ());
	return mesh;
}

} // namespace detail
} // namespace classical
} // namespace cgarchi
