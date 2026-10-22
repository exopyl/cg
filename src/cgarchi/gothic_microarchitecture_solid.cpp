#include <cgarchi/gothic_microarchitecture_solid.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <utility>

namespace cgarchi::microarch
{

namespace
{
	// Ecretage du champ, en pas de grille.
	constexpr double kClip = 2.0;
	// Ecart maximal d'un pas de polyligne a la courbe, en fraction de r.
	constexpr double kChordTolerance = 0.01;
	// Cote, en cellules, de la maille de la grille des segments.
	constexpr int kBrick = 32;

	std::string number (double v)
	{
		std::ostringstream os;
		os.precision(10);
		os << v;
		return os.str();
	}


	// Coordonnees de grille au plus : sous 2^24, un float represente tout entier.
	constexpr double kMaxGridCoordinate = 8388608.0;   // 2^23

	bool isNode (float v) { return v == std::floor(v); }

	struct Segment
	{
		double ax, ay, az, bx, by, bz;
	};

	double distanceToSegment (const Segment &s, double x, double y, double z)
	{
		const double ux = s.bx - s.ax, uy = s.by - s.ay, uz = s.bz - s.az;
		const double wx = x - s.ax, wy = y - s.ay, wz = z - s.az;
		const double uu = ux * ux + uy * uy + uz * uz;
		double t = uu > 0.0 ? (wx * ux + wy * uy + wz * uz) / uu : 0.0;
		t = std::clamp(t, 0.0, 1.0);
		const double dx = wx - t * ux, dy = wy - t * uy, dz = wz - t * uz;
		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

	// Champ de la reunion des tubes, repere de la grille (h = 1).
	struct TubeField
	{
		std::vector<Segment>                   segments;
		double                                 radius = 0.0;
		std::optional<double>                  cut;          // plan z = cut ; dessous : exterieur
		int                                    lo[3] = {0, 0, 0};
		int                                    dims[3] = {0, 0, 0};   // mailles par axe
		std::vector<std::vector<unsigned int>> cells;        // segments candidats par maille

		const std::vector<unsigned int> &candidates (double x, double y, double z) const
		{
			const double p[3] = {x, y, z};
			int c[3];
			for (int k = 0; k < 3; ++k)
				c[k] = std::clamp(static_cast<int>(std::floor((p[k] - lo[k]) / kBrick)), 0, dims[k] - 1);
			return cells[(static_cast<std::size_t>(c[2]) * dims[1] + c[1]) * dims[0] + c[0]];
		}

		double value (double x, double y, double z) const
		{
			double f = kClip;
			for (unsigned int i : candidates(x, y, z))
				f = std::min(f, distanceToSegment(segments[i], x, y, z) - radius);
			if (cut)
				f = std::max(f, *cut - z);
			return f;
		}

		static float eval (float x, float y, float z, void *self)
		{
			if (!isNode(x) || !isNode(y) || !isNode(z))
				return static_cast<float>(kClip);
			return static_cast<float>(static_cast<const TubeField *>(self)->value(x, y, z));
		}
	};

	// Liste de chaque maille : les segments dont la boite, dilatee de r + kClip, touche
	// la maille. Sur-ensemble des segments a moins de r + kClip d'un noeud de la maille :
	// deux mailles donnent la meme valeur a un noeud commun.
	void bucket (TubeField &field)
	{
		const std::size_t n = static_cast<std::size_t>(field.dims[0]) * field.dims[1] * field.dims[2];
		field.cells.assign(n, {});
		const double reach = field.radius + kClip;
		for (unsigned int i = 0; i < field.segments.size(); ++i)
		{
			const Segment &s = field.segments[i];
			const double a[3] = {s.ax, s.ay, s.az}, b[3] = {s.bx, s.by, s.bz};
			int c0[3], c1[3];
			for (int k = 0; k < 3; ++k)
			{
				const double lo = std::min(a[k], b[k]) - reach - field.lo[k];
				const double hi = std::max(a[k], b[k]) + reach - field.lo[k];
				c0[k] = std::clamp(static_cast<int>(std::floor(lo / kBrick)), 0, field.dims[k] - 1);
				c1[k] = std::clamp(static_cast<int>(std::floor(hi / kBrick)), 0, field.dims[k] - 1);
			}
			for (int z = c0[2]; z <= c1[2]; ++z)
				for (int y = c0[1]; y <= c1[1]; ++y)
					for (int x = c0[0]; x <= c1[0]; ++x)
						field.cells[(static_cast<std::size_t>(z) * field.dims[1] + y) * field.dims[0] + x].push_back(i);
		}
	}

	// Caracteristique d'Euler de chaque composante (union-find sur les triangles).
	std::vector<long> eulerByComponent (std::size_t nVertices, const std::vector<unsigned int> &faces)
	{
		std::vector<std::size_t> parent(nVertices);
		for (std::size_t v = 0; v < nVertices; ++v)
			parent[v] = v;
		auto root = [&](std::size_t v) {
			while (parent[v] != v)
				v = parent[v] = parent[parent[v]];
			return v;
		};
		std::vector<bool> used(nVertices, false);
		std::vector<std::pair<unsigned int, unsigned int>> edges;
		edges.reserve(faces.size());
		for (std::size_t f = 0; f + 2 < faces.size(); f += 3)
			for (int k = 0; k < 3; ++k)
			{
				const unsigned int a = faces[f + k], b = faces[f + (k + 1) % 3];
				used[a] = true;
				parent[root(a)] = root(b);
				edges.emplace_back(std::min(a, b), std::max(a, b));
			}
		std::sort(edges.begin(), edges.end());
		edges.erase(std::unique(edges.begin(), edges.end()), edges.end());

		std::vector<std::size_t> id(nVertices, nVertices);
		std::vector<long> chi;
		auto component = [&](std::size_t v) {
			const std::size_t r = root(v);
			if (id[r] == nVertices)
			{
				id[r] = chi.size();
				chi.push_back(0);
			}
			return id[r];
		};
		for (std::size_t v = 0; v < nVertices; ++v)
			if (used[v])
				++chi[component(v)];
		for (const auto &e : edges)
			--chi[component(e.first)];
		for (std::size_t f = 0; f + 2 < faces.size(); f += 3)
			++chi[component(faces[f])];
		return chi;
	}

	// Polyligne d'une courbe : echantillons de SampleParameters, puis subdivision tant
	// que le milieu d'un pas s'ecarte de plus de `tolerance` du milieu de la corde.
	void refine (const SpaceCurve &c, double s0, const Vector3d &p0, double s1, const Vector3d &p1, double tolerance,
	             int depth, std::vector<Vector3d> &out, std::vector<double> &abscissa)
	{
		const double sm = 0.5 * (s0 + s1);
		const Vector3d pm = PointAt(c, sm);
		const double dx = pm.x - 0.5 * (p0.x + p1.x), dy = pm.y - 0.5 * (p0.y + p1.y), dz = pm.z - 0.5 * (p0.z + p1.z);
		if (depth < 40 && dx * dx + dy * dy + dz * dz > tolerance * tolerance)
		{
			refine(c, s0, p0, sm, pm, tolerance, depth + 1, out, abscissa);
			refine(c, sm, pm, s1, p1, tolerance, depth + 1, out, abscissa);
			return;
		}
		out.push_back(p1);
		abscissa.push_back(s1);
	}

	// Polyligne du champ d'une courbe : points et abscisses s du plan.
	struct FieldPolyline
	{
		int                   curve = -1;
		std::vector<Vector3d> points;
		std::vector<double>   s;
	};

	// Polylignes du champ : echantillons de SampleParameters, puis subdivision tant que
	// le milieu d'un pas s'ecarte de plus de kChordTolerance * r de la corde. Vide et
	// erreur au-dela de SamplingLimits::max_network_points points.
	std::vector<FieldPolyline> fieldPolylines (const Network &network, double radius, MicroarchReport &report)
	{
		std::vector<FieldPolyline> out;
		std::size_t points = 0;
		for (std::size_t i = 0; i < network.curves.size(); ++i)
		{
			const SpaceCurve &c = network.curves[i];
			const std::vector<double> s = SampleParameters(c, SamplingParams{});
			if (s.empty())
				continue;
			FieldPolyline line;
			line.curve = static_cast<int>(i);
			Vector3d prev = PointAt(c, s[0]);
			line.points.push_back(prev);
			line.s.push_back(s[0]);
			for (std::size_t k = 1; k < s.size(); ++k)
			{
				const Vector3d next = PointAt(c, s[k]);
				refine(c, s[k - 1], prev, s[k], next, kChordTolerance * radius, 0, line.points, line.s);
				prev = next;
			}
			points += line.points.size();
			if (points > SamplingLimits::max_network_points)
			{
				report.Record(MicroarchError::SolidLimitExceeded, "solid.polylines",
				              "the field polylines would have more than " + std::to_string(SamplingLimits::max_network_points)
				              + " points ; increase radius_ratio");
				return {};
			}
			out.push_back(std::move(line));
		}
		return out;
	}

	bool validSolidParams (const SolidParams &params, MicroarchReport &report)
	{
		const std::size_t before = report.ErrorCount();
		if (!(std::isfinite(params.radius_ratio) && params.radius_ratio > 0.0 && params.radius_ratio <= SolidLimits::max_radius_ratio))
			report.Record(MicroarchError::SolidInvalidParameter, "solid.radius_ratio",
			              number(params.radius_ratio) + " outside (0, " + number(SolidLimits::max_radius_ratio) + "]");
		if (params.cells_per_radius < SolidLimits::min_cells_per_radius || params.cells_per_radius > SolidLimits::max_cells_per_radius)
			report.Record(MicroarchError::SolidInvalidParameter, "solid.cells_per_radius",
			              std::to_string(params.cells_per_radius) + " outside [" + std::to_string(SolidLimits::min_cells_per_radius)
			              + ", " + std::to_string(SolidLimits::max_cells_per_radius) + "]");
		return report.ErrorCount() == before;
	}

	// F ~ 3 A / h^2 (constante mesuree, plan d'execution §14.2) avec A = 2 pi r L, L la
	// longueur 3D des polylignes : majorant de l'aire de la reunion (recouvrements comptes).
	double estimateTriangles (const std::vector<std::vector<Vector3d>> &polylines, double radius, double cell)
	{
		double length = 0.0;
		for (const auto &line : polylines)
			for (std::size_t i = 0; i + 1 < line.size(); ++i)
			{
				const double dx = line[i + 1].x - line[i].x, dy = line[i + 1].y - line[i].y, dz = line[i + 1].z - line[i].z;
				length += std::sqrt(dx * dx + dy * dy + dz * dz);
			}
		const double kPi = 3.14159265358979323846;
		return 3.0 * 2.0 * kPi * radius * length / (cell * cell);
	}
}

const char *ExtractStatusName (GridIsoExtractor::Status status)
{
	switch (status)
	{
	case GridIsoExtractor::Status::Ok:              return "ok";
	case GridIsoExtractor::Status::AlreadyUsed:     return "already_used";
	case GridIsoExtractor::Status::NodesPerAxis:    return "nodes_per_axis";
	case GridIsoExtractor::Status::Nodes:           return "nodes";
	case GridIsoExtractor::Status::CacheAllocation: return "cache_allocation";
	case GridIsoExtractor::Status::Triangles:       return "triangles";
	case GridIsoExtractor::Status::BadIndex:        return "bad_index";
	}
	return "unknown";
}

GridIsoExtractor::GridIsoExtractor (Field field, void *context, std::size_t maxTriangles, std::size_t maxNodes)
	: m_field(field), m_context(context), m_maxTriangles(maxTriangles), m_maxNodes(maxNodes)
{
	set_orientation(0);
	set_value(0.f);
	set_eval_func(&GridIsoExtractor::evalNode);
	set_eval_data(this);
	set_vertex_func(&GridIsoExtractor::addCorner);
	data = this;
}

float GridIsoExtractor::evalNode (float x, float y, float z, void *self)
{
	auto *e = static_cast<GridIsoExtractor *>(self);
	// Plafond atteint : tout est exterieur, l'extraction finit sans triangle de plus.
	if (e->m_overflow || e->m_badIndex)
		return std::numeric_limits<float>::max();
	++e->m_evaluations;
	const float f = e->m_field(x, y, z, e->m_context);
	if (std::fabs(f) < GridIsoExtractor::kMargin)
		return f <= 0.f ? -GridIsoExtractor::kMargin : GridIsoExtractor::kMargin;
	return f;
}

std::uint64_t GridIsoExtractor::EdgeKey (const int node[3], int axis, const int keyOrigin[3])
{
	std::uint64_t key = static_cast<std::uint64_t>(axis);
	for (int k = 0; k < 3; ++k)
		key = (key << 20) | (static_cast<std::uint64_t>(node[k] - keyOrigin[k]) & 0xFFFFF);
	return key;
}

bool GridIsoExtractor::addCorner (float x, float y, float z, int index, void *self)
{
	auto *e = static_cast<GridIsoExtractor *>(self);
	// Plafond atteint : les cubes restants lisent encore la couche en cache contre des noeuds
	// ecretes a FLT_MAX ; leurs sommets tombent sur un noeud et ne sont pas des defauts de grille.
	if (e->m_overflow)
		return false;
	if (index < 0)
	{
		e->m_badIndex = true;
		return false;
	}
	const std::size_t i = static_cast<std::size_t>(index);
	if (i >= e->m_placed.size())
	{
		e->m_vertices.resize(3 * (i + 1), 0.f);
		e->m_keys.resize(i + 1, 0);
		e->m_placed.resize(i + 1, false);
	}
	if (!e->m_placed[i])
	{
		// Arete du sommet : la seule coordonnee non entiere (la marge exclut les noeuds).
		const float p[3] = {x, y, z};
		int axis = -1, node[3];
		for (int k = 0; k < 3; ++k)
		{
			node[k] = static_cast<int>(std::floor(p[k]));
			if (p[k] != static_cast<float>(node[k]))
				axis = axis < 0 ? k : 3;
		}
		if (axis < 0 || axis > 2)
		{
			e->m_badIndex = true;
			return false;
		}
		int next[3] = {node[0], node[1], node[2]};
		++next[axis];
		const double f0 = evalNode(static_cast<float>(node[0]), static_cast<float>(node[1]), static_cast<float>(node[2]), self);
		const double f1 = evalNode(static_cast<float>(next[0]), static_cast<float>(next[1]), static_cast<float>(next[2]), self);
		const double t = f0 / (f0 - f1);
		for (int k = 0; k < 3; ++k)
			e->m_vertices[3 * i + k] = static_cast<float>(k == axis ? node[k] + t : node[k]);
		e->m_keys[i] = EdgeKey(node, axis, e->m_keyOrigin);
		e->m_placed[i] = true;
	}
	e->m_corner[e->m_cornerCount++] = static_cast<unsigned int>(i);
	if (e->m_cornerCount < 3)
		return false;
	e->m_cornerCount = 0;
	if (e->m_faces.size() / 3 >= e->m_maxTriangles)
	{
		e->m_overflow = true;
		return true;
	}
	e->m_faces.insert(e->m_faces.end(), e->m_corner, e->m_corner + 3);
	return true;
}

GridIsoExtractor::Status GridIsoExtractor::Run (const int lo[3], const int hi[3])
{
	if (m_used)
		return Status::AlreadyUsed;
	m_used = true;
	std::size_t nodes = 1;
	for (int k = 0; k < 3; ++k)
	{
		const long n = static_cast<long>(hi[k]) - lo[k] + 1;
		if (n < 2 || n > SolidLimits::max_nodes_per_axis)
			return Status::NodesPerAxis;
		nodes *= static_cast<std::size_t>(n);
	}
	if (nodes > m_maxNodes)
		return Status::Nodes;
	set_bbox(static_cast<float>(lo[0]), static_cast<float>(lo[1]), static_cast<float>(lo[2]),
	         static_cast<float>(hi[0]), static_cast<float>(hi[1]), static_cast<float>(hi[2]));
	set_resolution_per_unit(1);
	// Resolution bornee ci-dessus (au plus 2 048 noeuds par axe) : compute n'echoue que sur
	// l'allocation de ses caches.
	if (!compute(1))
		return Status::CacheAllocation;
	if (m_badIndex)
		return Status::BadIndex;
	if (m_overflow)
		return Status::Triangles;
	return Status::Ok;
}

SolidMesh MeshTubeUnion (const std::vector<std::vector<Vector3d>> &polylines, double radius, double cell,
                         std::optional<double> base, MicroarchReport &report, const SolidLimits &limits)
{
	SolidMesh out;
	const double inf = std::numeric_limits<double>::infinity();
	double lo[3] = {inf, inf, inf}, hi[3] = {-inf, -inf, -inf};
	for (const auto &line : polylines)
		for (const Vector3d &p : line)
		{
			const double q[3] = {p.x, p.y, p.z};
			for (int k = 0; k < 3; ++k)
			{
				lo[k] = std::min(lo[k], q[k]);
				hi[k] = std::max(hi[k], q[k]);
			}
		}
	const std::size_t before = report.ErrorCount();
	if (!(std::isfinite(radius) && radius > 0.0))
		report.Record(MicroarchError::SolidInvalidParameter, "solid.radius", number(radius) + " is not a finite length > 0");
	if (!(std::isfinite(cell) && cell > 0.0))
		report.Record(MicroarchError::SolidInvalidParameter, "solid.cell", number(cell) + " is not a finite length > 0");
	std::size_t segments = 0;
	bool finite = true;
	for (const auto &line : polylines)
	{
		segments += line.size() > 1 ? line.size() - 1 : 0;
		for (const Vector3d &p : line)
			finite = finite && std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
	}
	if (segments == 0 || !finite)
		report.Record(MicroarchError::SolidInvalidParameter, "solid.polylines",
		              segments == 0 ? "no segment (empty polylines or single points)" : "non-finite coordinate");
	if (report.ErrorCount() != before)
		return out;

	TubeField field;
	field.radius = radius / cell;
	// Grille : boite des polylignes dilatee de r + 2h, plus une cellule.
	const double margin = field.radius + kClip + 1.0;

	// Origine : centre de la boite en x et y ; en z, base - h/2 (plan de coupe au milieu
	// d'une couche de cellules), releve d'un nombre entier de pas jusque sous le bas du
	// reseau quand celui-ci est loin au-dessus de la base ; centre sans coupe.
	double oz = 0.5 * (lo[2] + hi[2]);
	if (base)
	{
		const double lift = std::floor((lo[2] - *base) / cell - margin);
		oz = *base - 0.5 * cell + std::max(0.0, lift) * cell;
	}
	const Vector3d origin(0.5 * (lo[0] + hi[0]), 0.5 * (lo[1] + hi[1]), oz);
	const double o[3] = {origin.x, origin.y, origin.z};
	if (base)
		field.cut = (*base - origin.z) / cell;

	int gl[3], gh[3];
	for (int k = 0; k < 3; ++k)
	{
		const double a = (lo[k] - o[k]) / cell - margin, b = (hi[k] - o[k]) / cell + margin;
		if (!(std::fabs(a) < kMaxGridCoordinate && std::fabs(b) < kMaxGridCoordinate))
		{
			report.Record(MicroarchError::SolidLimitExceeded, "solid.grid.coordinates",
			              "the network extends " + number(std::max(std::fabs(a), std::fabs(b))) + " cells from the grid origin on axis "
			              + std::to_string(k) + " ; grid coordinates are limited to 2^23 cells (exact float integers)");
			return out;
		}
		gl[k] = static_cast<int>(std::floor(a));
		gh[k] = static_cast<int>(std::ceil(b));
	}
	if (base)
		gl[2] = std::max(gl[2], 0);   // couche z = 0 : sous le plan de coupe, exterieure

	std::size_t nodes = 1;
	for (int k = 0; k < 3; ++k)
	{
		const long n = static_cast<long>(gh[k]) - gl[k] + 1;
		if (n > SolidLimits::max_nodes_per_axis)
		{
			report.Record(MicroarchError::SolidLimitExceeded, "solid.grid.nodes_per_axis",
			              std::to_string(n) + " nodes on axis " + std::to_string(k) + " > "
			              + std::to_string(SolidLimits::max_nodes_per_axis)
			              + " ; increase radius_ratio or decrease cells_per_radius");
			return out;
		}
		nodes *= static_cast<std::size_t>(n);
	}
	// Estimation prealable : information seulement. Elle majore F sans borne connue (les
	// recouvrements de tubes sont comptes) ; seul le plafond exact, pendant l'extraction,
	// refuse.
	const double estimate = estimateTriangles(polylines, radius, cell);
	if (estimate > static_cast<double>(limits.max_triangles))
		report.Record(MicroarchWarning::SolidTriangleEstimateHigh, "solid.triangles",
		              "about " + number(estimate) + " triangles estimated (3 A / h^2, overlaps counted) > "
		              + std::to_string(limits.max_triangles) + " ; the extraction may be refused at the cap");
	auto refuseNodes = [&](std::size_t n, const char *what) {
		report.Record(MicroarchError::SolidLimitExceeded, "solid.grid.nodes",
		              std::to_string(n) + " " + what + " > " + std::to_string(limits.max_grid_nodes)
		              + " ; increase radius_ratio or decrease cells_per_radius");
		return SolidMesh{};
	};
	if (limits.full_grid && nodes > limits.max_grid_nodes)
		return refuseNodes(nodes, "grid nodes");

	for (const auto &line : polylines)
		for (std::size_t i = 0; i + 1 < line.size(); ++i)
			field.segments.push_back(Segment{(line[i].x - o[0]) / cell, (line[i].y - o[1]) / cell, (line[i].z - o[2]) / cell,
			                                 (line[i + 1].x - o[0]) / cell, (line[i + 1].y - o[1]) / cell,
			                                 (line[i + 1].z - o[2]) / cell});
	for (int k = 0; k < 3; ++k)
	{
		field.lo[k] = gl[k];
		field.dims[k] = (gh[k] - gl[k]) / kBrick + 1;
	}
	bucket(field);

	// Briques : mailles de la grille des segments ; actives si leur liste est non vide.
	struct Brick { int lo[3], hi[3]; };
	std::vector<Brick> bricks;
	std::size_t brickCount = 1;
	if (limits.full_grid)
		bricks.push_back(Brick{{gl[0], gl[1], gl[2]}, {gh[0], gh[1], gh[2]}});
	else
	{
		int nb[3];
		for (int k = 0; k < 3; ++k)
			nb[k] = (gh[k] - gl[k] + kBrick - 1) / kBrick;
		brickCount = static_cast<std::size_t>(nb[0]) * nb[1] * nb[2];
		std::size_t activeNodes = 0;
		for (int z = 0; z < nb[2]; ++z)
			for (int y = 0; y < nb[1]; ++y)
				for (int x = 0; x < nb[0]; ++x)
				{
					if (field.cells[(static_cast<std::size_t>(z) * field.dims[1] + y) * field.dims[0] + x].empty())
						continue;
					Brick b;
					const int c[3] = {x, y, z};
					std::size_t n = 1;
					for (int k = 0; k < 3; ++k)
					{
						b.lo[k] = gl[k] + kBrick * c[k];
						b.hi[k] = std::min(b.lo[k] + kBrick, gh[k]);
						n *= static_cast<std::size_t>(b.hi[k] - b.lo[k] + 1);
					}
					activeNodes += n;
					bricks.push_back(b);
				}
		if (activeNodes > limits.max_grid_nodes)
			return refuseNodes(activeNodes, "nodes in active bricks");
	}

	std::unordered_map<std::uint64_t, unsigned int> index;
	std::vector<float> vertices;
	std::vector<unsigned int> faces;
	std::size_t evaluated = 0;
	for (const Brick &b : bricks)
	{
		GridIsoExtractor extractor(&TubeField::eval, &field, limits.max_triangles - faces.size() / 3, limits.max_grid_nodes);
		extractor.SetKeyOrigin(gl);
		const GridIsoExtractor::Status status = extractor.Run(b.lo, b.hi);
		evaluated += extractor.Evaluations();
		if (status == GridIsoExtractor::Status::Triangles)
		{
			report.Record(MicroarchError::SolidLimitExceeded, "solid.triangles",
			              "more than " + std::to_string(limits.max_triangles)
			              + " triangles ; increase radius_ratio or decrease cells_per_radius");
			return SolidMesh{};
		}
		if (status != GridIsoExtractor::Status::Ok)
		{
			report.Record(MicroarchError::SolidLimitExceeded,
			              status == GridIsoExtractor::Status::CacheAllocation ? "solid.grid.cache" : "solid.grid",
			              std::string("extraction refused : ") + ExtractStatusName(status));
			return SolidMesh{};
		}
		const std::vector<float> &gv = extractor.Vertices();
		const std::vector<std::uint64_t> &keys = extractor.Keys();
		std::vector<unsigned int> local(keys.size());
		for (std::size_t i = 0; i < keys.size(); ++i)
		{
			const auto found = index.emplace(keys[i], static_cast<unsigned int>(vertices.size() / 3));
			if (found.second)
				for (int k = 0; k < 3; ++k)
					vertices.push_back(static_cast<float>(static_cast<double>(gv[3 * i + k]) * cell));
			local[i] = found.first->second;
		}
		for (unsigned int v : extractor.Faces())
			faces.push_back(local[v]);
	}
	out.bricks = brickCount;
	out.active_bricks = bricks.size();
	out.evaluated_nodes = evaluated;
	out.euler = eulerByComponent(vertices.size() / 3, faces);
	out.mesh = std::make_unique<Mesh>();
	out.mesh->SetVertices(static_cast<unsigned int>(vertices.size() / 3), vertices.data());
	out.mesh->SetFaces(static_cast<unsigned int>(faces.size() / 3), 3, faces.data());
	out.mesh->ComputeNormals();
	out.origin = origin;
	out.radius = radius;
	out.cell = cell;
	return out;
}

SolidMesh BuildSolidMesh (const Network &network, const SolidParams &params, MicroarchReport &report,
                          const SolidLimits &limits)
{
	if (!validSolidParams(params, report) || network.curves.empty())
		return SolidMesh{};

	const double radius = params.radius_ratio * network.tol.scale;
	const double cell = radius / params.cells_per_radius;
	const std::size_t before = report.ErrorCount();
	std::vector<FieldPolyline> lines = fieldPolylines(network, radius, report);
	if (report.ErrorCount() != before)
		return SolidMesh{};
	std::vector<std::vector<Vector3d>> polylines;
	polylines.reserve(lines.size());
	for (FieldPolyline &l : lines)
		polylines.push_back(std::move(l.points));

	std::optional<double> base;
	if (params.cut_at_base)
		base = NetworkBase(network);
	return MeshTubeUnion(polylines, radius, cell, base, report, limits);
}

std::size_t EstimateSolidTriangles (const Network &network, const SolidParams &params)
{
	MicroarchReport ignored;
	if (!validSolidParams(params, ignored) || network.curves.empty())
		return 0;
	const double radius = params.radius_ratio * network.tol.scale;
	const double cell = radius / params.cells_per_radius;
	std::vector<FieldPolyline> lines = fieldPolylines(network, radius, ignored);
	std::vector<std::vector<Vector3d>> polylines;
	for (FieldPolyline &l : lines)
		polylines.push_back(std::move(l.points));
	return static_cast<std::size_t>(estimateTriangles(polylines, radius, cell));
}

namespace
{
	constexpr double kPi = 3.14159265358979323846;
	// c_max = kWallDiscretization / q^2 : tolerance de la paroi hors des fourches, en fraction de
	// 2r. Mesure (plan d'execution, §14.13) : deficit 0,078 a q = 2, 0,037 a q = 3, 0,0047 a
	// q = 8 sur D4 segment et le moulinet, soit ~0,31 / q^2 ; le double.
	constexpr double kWallDiscretization = 0.6;
	// Auto-approche d'un meme morceau : un point a moins de r + 2h du sommet, mais a plus de
	// 1,25 (r + 2h) le long de la courbe. Sur une droite, tout point a moins de r + 2h est a
	// moins de r + 2h le long d'elle : seule une courbure (pli, epingle) depasse ce seuil.
	constexpr double kSelfApproach = 1.25;

	// Segment d'une polyligne du champ, avec son morceau du reseau et ses abscisses 3D.
	struct PieceSegment
	{
		Vector3d    a, b;
		std::size_t piece = 0;
		double      la = 0.0, lb = 0.0;
	};

	double closestOnSegment (const PieceSegment &s, const Vector3d &p, Vector3d &closest, double &l)
	{
		const double ux = s.b.x - s.a.x, uy = s.b.y - s.a.y, uz = s.b.z - s.a.z;
		const double uu = ux * ux + uy * uy + uz * uz;
		double t = uu > 0.0 ? ((p.x - s.a.x) * ux + (p.y - s.a.y) * uy + (p.z - s.a.z) * uz) / uu : 0.0;
		t = std::clamp(t, 0.0, 1.0);
		closest = Vector3d(s.a.x + t * ux, s.a.y + t * uy, s.a.z + t * uz);
		l = s.la + t * (s.lb - s.la);
		const double dx = p.x - closest.x, dy = p.y - closest.y, dz = p.z - closest.z;
		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

	std::uint64_t cellKey (long x, long y, long z)
	{
		const long o = 1L << 20;
		return (static_cast<std::uint64_t>(x + o) & 0x1FFFFF) << 42 | (static_cast<std::uint64_t>(y + o) & 0x1FFFFF) << 21
		       | (static_cast<std::uint64_t>(z + o) & 0x1FFFFF);
	}

	std::string where (const Vector3d &p)
	{
		return "(" + number(p.x) + ", " + number(p.y) + ", " + number(p.z) + ")";
	}
}

double NetworkBase (const Network &network)
{
	if (network.curves.empty())
		return 0.0;
	for (const LevelSpec &l : network.levels)
		if (l.id == network.curves.front().level)
			return l.base;
	return 0.0;
}

std::string SolidToStl (const SolidMesh &solid, double mmPerUnit, double base, double *extentMm)
{
	if (!solid.mesh || !(std::isfinite(mmPerUnit) && mmPerUnit > 0.0))
		return std::string();
	const Mesh &m = *solid.mesh;
	const std::vector<float> &fv = m.GetVertices();
	const std::size_t nv = fv.size() / 3;
	double lo[3] = {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(),
	                std::numeric_limits<double>::infinity()};
	double hi[3] = {-lo[0], -lo[1], -lo[2]};
	const double o[3] = {solid.origin.x, solid.origin.y, solid.origin.z};
	for (std::size_t i = 0; i < nv; ++i)
		for (int k = 0; k < 3; ++k)
		{
			const double x = double(fv[3 * i + k]) + o[k];
			lo[k] = std::min(lo[k], x);
			hi[k] = std::max(hi[k], x);
		}
	if (nv == 0)
		return std::string();
	// Point du dessin -> mm : (p - shift) * mmPerUnit.
	const double shift[3] = {0.5 * (lo[0] + hi[0]), 0.5 * (lo[1] + hi[1]), base};
	if (extentMm)
		for (int k = 0; k < 3; ++k)
			extentMm[k] = (hi[k] - lo[k]) * mmPerUnit;
	const std::vector<unsigned int> tris = m.GetTriangles();
	const std::size_t nf = tris.size() / 3;
	if (nf > 0xFFFFFFFFu)
		return std::string();
	std::string out(84 + 50 * nf, '\0');
	const char header[] = "cgarchi gothic microarchitecture solid, binary STL, millimetres";
	std::memcpy(&out[0], header, sizeof header - 1);
	const std::uint32_t count = static_cast<std::uint32_t>(nf);
	std::memcpy(&out[80], &count, 4);
	char *w = &out[84];
	for (std::size_t f = 0; f < nf; ++f, w += 50)
	{
		double p[3][3];
		for (int c = 0; c < 3; ++c)
			for (int k = 0; k < 3; ++k)
				p[c][k] = (double(fv[3 * tris[3 * f + c] + k]) + o[k] - shift[k]) * mmPerUnit;
		const double e1[3] = {p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2]};
		const double e2[3] = {p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2]};
		double n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
		const double l = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
		float record[12];
		for (int k = 0; k < 3; ++k)
			record[k] = l > 0.0 ? static_cast<float>(n[k] / l) : 0.0f;
		for (int c = 0; c < 3; ++c)
			for (int k = 0; k < 3; ++k)
				record[3 + 3 * c + k] = static_cast<float>(p[c][k]);
		std::memcpy(w, record, sizeof record);   // 48 octets, puis 2 octets d'attribut nuls
	}
	return out;
}

const std::vector<Vector3d> &WindingDirections ()
{
	static const std::vector<Vector3d> directions = [] {
		const double raw[][3] = {{0.9998, 0.0137, 0.0071},  {-0.2113, 0.9711, 0.1107}, {0.1291, -0.3047, 0.9436},
		                         {-0.7071, -0.5774, 0.4082}, {0.5547, 0.2673, -0.7878}, {-0.0823, -0.9449, -0.3170},
		                         {0.3339, -0.6658, -0.6672}, {-0.8763, 0.1931, -0.4412}};
		std::vector<Vector3d> d;
		for (const auto &r : raw)
		{
			const double l = std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
			d.emplace_back(r[0] / l, r[1] / l, r[2] / l);
		}
		return d;
	}();
	return directions;
}

std::optional<long> WindingNumber (const std::vector<float> &vertices, const std::vector<unsigned int> &triangles,
                                   const std::vector<bool> &counted, const Vector3d &p, const std::vector<Vector3d> &directions)
{
	const std::size_t nf = triangles.size() / 3;
	if (!counted.empty() && counted.size() != nf)
		return std::nullopt;
	// Un ecart barycentrique sous kEdge, ou un depart a moins de kEdge (relatif au triangle) de
	// son plan, rend la direction ambigue : arete, sommet ou rayon rasant.
	constexpr double kEdge = 1e-9;
	std::optional<long> first;
	for (const Vector3d &d : directions)
	{
		long winding = 0;
		bool ambiguous = false;
		for (std::size_t f = 0; f < nf && !ambiguous; ++f)
		{
			if (!counted.empty() && !counted[f])
				continue;
			const float *a = &vertices[3 * triangles[3 * f]], *b = &vertices[3 * triangles[3 * f + 1]],
			            *c = &vertices[3 * triangles[3 * f + 2]];
			const double e1[3] = {double(b[0]) - a[0], double(b[1]) - a[1], double(b[2]) - a[2]};
			const double e2[3] = {double(c[0]) - a[0], double(c[1]) - a[1], double(c[2]) - a[2]};
			const double tv[3] = {p.x - a[0], p.y - a[1], p.z - a[2]};
			const double n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
			const double nl = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
			if (!(nl > 0.0))
				continue;
			const double size = std::sqrt(e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2])
			                    + std::sqrt(e2[0] * e2[0] + e2[1] * e2[1] + e2[2] * e2[2]);
			const double height = (tv[0] * n[0] + tv[1] * n[1] + tv[2] * n[2]) / nl;   // p au-dessus du plan
			const double dn = (d.x * n[0] + d.y * n[1] + d.z * n[2]) / nl;            // cosinus rayon / normale
			if (std::fabs(dn) <= kEdge)
			{
				if (std::fabs(height) <= kEdge * size)
					ambiguous = true;   // rayon dans le plan du triangle
				continue;
			}
			const double t = -height / dn;   // distance le long du rayon
			if (t < -kEdge * size)
				continue;
			const double x[3] = {tv[0] + t * d.x, tv[1] + t * d.y, tv[2] + t * d.z};   // point d'impact - a
			// Coordonnees barycentriques (u sur e1, w sur e2) par les aires signees.
			const double cx[3] = {x[1] * e2[2] - x[2] * e2[1], x[2] * e2[0] - x[0] * e2[2], x[0] * e2[1] - x[1] * e2[0]};
			const double cy[3] = {e1[1] * x[2] - e1[2] * x[1], e1[2] * x[0] - e1[0] * x[2], e1[0] * x[1] - e1[1] * x[0]};
			const double u = (cx[0] * n[0] + cx[1] * n[1] + cx[2] * n[2]) / (nl * nl);
			const double w = (cy[0] * n[0] + cy[1] * n[1] + cy[2] * n[2]) / (nl * nl);
			const double lo = std::min({u, w, 1.0 - u - w});
			if (lo < -kEdge)
				continue;
			if (lo <= kEdge || t <= kEdge * size)
			{
				ambiguous = true;
				continue;
			}
			winding += dn > 0.0 ? 1 : -1;   // sortie dans le sens de la normale
		}
		if (ambiguous)
			continue;
		// Deux directions propres consecutives doivent concorder ; sinon la derniere sert de reference.
		if (first && *first == winding)
			return winding;
		first = winding;
	}
	return std::nullopt;
}

SolidPrintVerdict CheckSolidPrintable (const SolidMesh &solid, const Network &network, const PrintConstraints *constraints,
                                       MicroarchReport &report, const SolidPrintOptions &options)
{
	SolidPrintVerdict v;
	if (!solid.mesh || !(solid.radius > 0.0 && solid.cell > 0.0) || network.curves.empty())
		return v;
	const Mesh &m = *solid.mesh;
	const double r = solid.radius, h = solid.cell;
	if (options.max_verified_triangles && m.GetNFaces() > *options.max_verified_triangles)
	{
		v.verified = false;
		report.Record(MicroarchWarning::SolidUnverified, "solid.verdict",
		              std::to_string(m.GetNFaces()) + " triangles > " + std::to_string(*options.max_verified_triangles)
		              + " : the printability verdict is not computed ; export marked unverified");
		return v;
	}
	const double q = std::max(1.0, std::round(r / h));
	v.c_max = kWallDiscretization / (q * q);
	const double base = NetworkBase(network);
	const std::vector<float> &fv = m.GetVertices();
	const std::size_t nv = fv.size() / 3;
	auto point = [&](std::size_t i) {
		return Vector3d(fv[3 * i] + solid.origin.x, fv[3 * i + 1] + solid.origin.y, fv[3 * i + 2] + solid.origin.z);
	};
	// Normales de sommet, ponderees par l'aire des faces (unitaires, vers l'exterieur).
	std::vector<double> vn(3 * nv, 0.0);
	{
		const std::vector<unsigned int> t = m.GetTriangles();
		for (std::size_t f = 0; f + 2 < t.size(); f += 3)
		{
			const float *a = &fv[3 * t[f]], *b = &fv[3 * t[f + 1]], *c = &fv[3 * t[f + 2]];
			const double e1[3] = {double(b[0]) - a[0], double(b[1]) - a[1], double(b[2]) - a[2]};
			const double e2[3] = {double(c[0]) - a[0], double(c[1]) - a[1], double(c[2]) - a[2]};
			const double cr[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
			for (int k = 0; k < 3; ++k)
				for (int d = 0; d < 3; ++d)
					vn[3 * t[f + k] + d] += cr[d];
		}
	}
	auto normal = [&](std::size_t i) {
		const double x = vn[3 * i], y = vn[3 * i + 1], z = vn[3 * i + 2];
		const double l = std::sqrt(x * x + y * y + z * z);
		return l > 0.0 ? Vector3d(x / l, y / l, z / l) : Vector3d(0.0, 0.0, 0.0);
	};

	// --- Composantes du maillage, caracteristique d'Euler, volume signe (cavites).
	const std::vector<unsigned int> tris = m.GetTriangles();
	std::vector<std::size_t> parent(nv);
	for (std::size_t i = 0; i < nv; ++i)
		parent[i] = i;
	auto root = [&](std::size_t i) {
		while (parent[i] != i)
			i = parent[i] = parent[parent[i]];
		return i;
	};
	for (std::size_t f = 0; f + 2 < tris.size(); f += 3)
	{
		parent[root(tris[f])] = root(tris[f + 1]);
		parent[root(tris[f])] = root(tris[f + 2]);
	}
	std::size_t count = 0;
	std::vector<std::size_t> comp(nv, nv), id(nv, nv);
	std::vector<bool> used(nv, false);
	for (unsigned int t : tris)
		used[t] = true;
	for (std::size_t i = 0; i < nv; ++i)
		if (used[i])
		{
			const std::size_t rt = root(i);
			if (id[rt] == nv)
				id[rt] = count++;
			comp[i] = id[rt];
		}
	std::vector<long> chi(count, 0);
	std::vector<double> zmin(count, std::numeric_limits<double>::infinity());
	std::vector<Vector3d> lowest(count);
	for (std::size_t i = 0; i < nv; ++i)
		if (used[i])
		{
			++chi[comp[i]];
			const Vector3d p = point(i);
			if (p.z < zmin[comp[i]])
			{
				zmin[comp[i]] = p.z;
				lowest[comp[i]] = p;
			}
		}
	{
		std::vector<std::pair<unsigned int, unsigned int>> edges;
		edges.reserve(tris.size());
		for (std::size_t f = 0; f + 2 < tris.size(); f += 3)
			for (int k = 0; k < 3; ++k)
				edges.emplace_back(std::min(tris[f + k], tris[f + (k + 1) % 3]), std::max(tris[f + k], tris[f + (k + 1) % 3]));
		std::sort(edges.begin(), edges.end());
		edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
		for (const auto &e : edges)
			--chi[comp[e.first]];
		for (std::size_t f = 0; f + 2 < tris.size(); f += 3)
			++chi[comp[tris[f]]];
	}
	// Volume signe : une composante de volume negatif est une cavite scellee.
	std::vector<double> volume(count, 0.0);
	for (std::size_t f = 0; f + 2 < tris.size(); f += 3)
	{
		const float *a = &fv[3 * tris[f]], *b = &fv[3 * tris[f + 1]], *c = &fv[3 * tris[f + 2]];
		volume[comp[tris[f]]] += (double(a[0]) * (double(b[1]) * c[2] - double(b[2]) * c[1])
		                          - double(a[1]) * (double(b[0]) * c[2] - double(b[2]) * c[0])
		                          + double(a[2]) * (double(b[0]) * c[1] - double(b[1]) * c[0])) / 6.0;
	}
	// Cavite : volume negatif, et nombre d'enroulement 1 d'un de ses sommets par rapport a toutes
	// les autres composantes, chacune avec son orientation. Un ilot retourne dans une cavite a 0.
	std::vector<bool> cavity(count, false);
	std::vector<std::size_t> sample(count, nv);
	for (std::size_t i = 0; i < nv; ++i)
		if (used[i] && sample[comp[i]] == nv)
			sample[comp[i]] = i;
	for (std::size_t c = 0; c < count; ++c)
		if (volume[c] < 0.0 && sample[c] < nv)
		{
			std::vector<bool> others(tris.size() / 3);
			for (std::size_t f = 0; f < others.size(); ++f)
				others[f] = comp[tris[3 * f]] != c;
			const Vector3d o(fv[3 * sample[c]], fv[3 * sample[c] + 1], fv[3 * sample[c] + 2]);
			const std::optional<long> winding = WindingNumber(fv, tris, others, o);
			if (winding && *winding == 1)
			{
				cavity[c] = true;
				++v.cavities;
			}
		}
	v.components = count - v.cavities;

	// --- P1-P5 : validation, budget de paires en option (M32 a).
	MeshValidationOptions vo;
	vo.weldTolerance = 1e-3 * h;
	v.validation = ValidateMesh(m, vo);
	std::string defects;
	for (std::size_t k = 0; k < kMeshDefectCount; ++k)
	{
		// Une cavite scellee est une coquille orientee vers l'interieur : InvertedShell attendu.
		const std::size_t allowed = static_cast<MeshDefect>(k) == MeshDefect::InvertedShell ? v.cavities : 0;
		if (v.validation.counts[k] != allowed)
			defects += std::string(defects.empty() ? "" : ", ") + MeshDefectName(static_cast<MeshDefect>(k)) + " "
			           + std::to_string(v.validation.counts[k]);
	}
	bool sound = defects.empty();
	if (v.validation.selfIntersectionTruncated)
	{
		sound = false;
		defects += std::string(defects.empty() ? "" : ", ") + "self-intersection test truncated";
	}
	if (!sound)
		report.Record(MicroarchError::SolidGeneratorDefect, "solid.mesh", "the solid mesh is not sound : " + defects);

	// --- Segments du champ, par morceau du reseau (coupures aux incidences de jonction).
	MicroarchReport ignored;
	const std::vector<FieldPolyline> lines = fieldPolylines(network, r, ignored);
	std::vector<std::vector<double>> cuts(network.curves.size());
	for (const Junction &j : network.junctions)
		for (const Incidence &i : j.incidences)
			if (i.curve >= 0 && static_cast<std::size_t>(i.curve) < cuts.size())
				cuts[static_cast<std::size_t>(i.curve)].push_back(i.s);
	std::vector<std::size_t> pieceBase(network.curves.size() + 1, 0);
	for (std::size_t c = 0; c < cuts.size(); ++c)
	{
		std::sort(cuts[c].begin(), cuts[c].end());
		pieceBase[c + 1] = pieceBase[c] + cuts[c].size() + 1;
	}
	std::vector<PieceSegment> segments;
	for (const FieldPolyline &line : lines)
	{
		const std::size_t c = static_cast<std::size_t>(line.curve);
		double l = 0.0;
		for (std::size_t i = 0; i + 1 < line.points.size(); ++i)
		{
			PieceSegment s;
			s.a = line.points[i];
			s.b = line.points[i + 1];
			const double dx = s.b.x - s.a.x, dy = s.b.y - s.a.y, dz = s.b.z - s.a.z;
			s.la = l;
			s.lb = l += std::sqrt(dx * dx + dy * dy + dz * dz);
			const double mid = 0.5 * (line.s[i] + line.s[i + 1]);
			s.piece = pieceBase[c] + static_cast<std::size_t>(std::upper_bound(cuts[c].begin(), cuts[c].end(), mid) - cuts[c].begin());
			segments.push_back(s);
		}
	}
	const double reach = r + 2.0 * h;
	std::unordered_map<std::uint64_t, std::vector<unsigned int>> grid;
	for (unsigned int i = 0; i < segments.size(); ++i)
	{
		const PieceSegment &s = segments[i];
		const long x0 = static_cast<long>(std::floor((std::min(s.a.x, s.b.x) - reach) / reach));
		const long x1 = static_cast<long>(std::floor((std::max(s.a.x, s.b.x) + reach) / reach));
		const long y0 = static_cast<long>(std::floor((std::min(s.a.y, s.b.y) - reach) / reach));
		const long y1 = static_cast<long>(std::floor((std::max(s.a.y, s.b.y) + reach) / reach));
		const long z0 = static_cast<long>(std::floor((std::min(s.a.z, s.b.z) - reach) / reach));
		const long z1 = static_cast<long>(std::floor((std::max(s.a.z, s.b.z) + reach) / reach));
		for (long z = z0; z <= z1; ++z)
			for (long y = y0; y <= y1; ++y)
				for (long x = x0; x <= x1; ++x)
					grid[cellKey(x, y, z)].push_back(i);
	}

	// --- P7 : zone de fourche, tolerance de discretisation hors zone, col des fourches.
	for (std::size_t i = 0; i < nv; ++i)
	{
		const Vector3d p = point(i);
		if (p.z < base + h)
			continue;
		const auto cellIt = grid.find(cellKey(static_cast<long>(std::floor(p.x / reach)), static_cast<long>(std::floor(p.y / reach)),
		                                      static_cast<long>(std::floor(p.z / reach))));
		double d1 = std::numeric_limits<double>::infinity(), l1 = 0.0;
		std::size_t piece1 = 0;
		unsigned int k1 = 0;
		Vector3d c1;
		if (cellIt != grid.end())
			for (unsigned int k : cellIt->second)
			{
				Vector3d c;
				double l = 0.0;
				const double d = closestOnSegment(segments[k], p, c, l);
				if (d < d1)
				{
					d1 = d;
					l1 = l;
					piece1 = segments[k].piece;
					k1 = k;
					c1 = c;
				}
			}
		double d2 = std::numeric_limits<double>::infinity();
		Vector3d c2;
		if (cellIt != grid.end())
			for (unsigned int k : cellIt->second)
			{
				Vector3d c;
				double l = 0.0;
				const double d = closestOnSegment(segments[k], p, c, l);
				const bool other = segments[k].piece != piece1 || std::fabs(l - l1) > kSelfApproach * reach;
				if (other && d < d2)
				{
					d2 = d;
					c2 = c;
				}
			}
		if (d2 < reach)
		{
			++v.fork_vertices;
			const double ex = c1.x - c2.x, ey = c1.y - c2.y, ez = c1.z - c2.z;
			const double e2 = ex * ex + ey * ey + ez * ez;
			if (e2 < 4.0 * r * r)
			{
				const double neck = 2.0 * std::sqrt(r * r - e2 / 4.0);
				if (!v.fork_neck || neck < *v.fork_neck)
				{
					v.fork_neck = neck;
					v.fork_neck_at = p;
				}
			}
			continue;
		}
		++v.off_fork_vertices;
		double wall = 0.0;
		if (std::isfinite(d1))
		{
			// Rayon p - t n dans le cylindre de rayon r autour de la droite du segment k1.
			const PieceSegment &sg = segments[k1];
			double ux = sg.b.x - sg.a.x, uy = sg.b.y - sg.a.y, uz = sg.b.z - sg.a.z;
			const double ul = std::sqrt(ux * ux + uy * uy + uz * uz);
			if (ul > 0.0)
			{
				ux /= ul;
				uy /= ul;
				uz /= ul;
			}
			const Vector3d n = normal(i);
			const double wx = p.x - sg.a.x, wy = p.y - sg.a.y, wz = p.z - sg.a.z;
			const double wu = wx * ux + wy * uy + wz * uz, nu = -(n.x * ux + n.y * uy + n.z * uz);
			const double px = wx - wu * ux, py = wy - wu * uy, pz = wz - wu * uz;
			const double qx = -n.x - nu * ux, qy = -n.y - nu * uy, qz = -n.z - nu * uz;
			const double A = qx * qx + qy * qy + qz * qz, B = px * qx + py * qy + pz * qz;
			const double C = px * px + py * py + pz * pz - r * r;
			if (A < 1e-12)
				wall = std::numeric_limits<double>::infinity();
			else
			{
				const double disc = B * B - A * C;
				wall = disc < 0.0 ? 0.0 : (-B + std::sqrt(disc)) / A;
			}
		}
		v.off_fork_wall = v.off_fork_wall ? std::min(*v.off_fork_wall, wall / (2.0 * r)) : wall / (2.0 * r);
	}
	const bool discretized = !v.off_fork_wall || *v.off_fork_wall >= 1.0 - v.c_max;
	if (!v.off_fork_wall)
		report.Record(MicroarchWarning::SolidDiscretizationUnchecked, "solid.discretization",
		              "every vertex is in a fork zone : the discretization check has nothing to measure");
	else if (!discretized)
		report.Record(MicroarchError::SolidGeneratorDefect, "solid.discretization",
		              "the thinnest wall off the forks is " + number(*v.off_fork_wall) + " x 2r ; the generator guarantees at least "
		              + number(1.0 - v.c_max) + " x 2r");
	v.generator_ok = sound && discretized;
	if (v.fork_neck)
		report.Record(MicroarchWarning::SolidForkNeck, "solid.forks",
		              "narrowest fork neck " + number(*v.fork_neck) + " at " + where(v.fork_neck_at)
		              + " ; information, not part of the verdict");

	for (std::size_t c = 0; c < count; ++c)
		if (cavity[c])
			report.Record(MicroarchWarning::SolidSealedCavity, "solid.components",
			              "sealed cavity (a network cell closed on all sides), lowest point " + where(lowest[c])
			              + " : it traps uncured resin ; information, not part of the verdict");
		else if (zmin[c] > base + 1e-3 * h)
		{
			v.floating.push_back(c);
			report.Record(MicroarchWarning::SolidFloatingComponent, "solid.components",
			              "component " + std::to_string(c) + " does not touch the base ; lowest point " + where(lowest[c]));
		}

	// Graphe contracte a 2r ; chaque composante du graphe est rattachee a la composante du
	// solide du sommet le plus proche de l'un de ses sommets.
	const NetworkGraph graph = BuildNetworkGraph(network, 2.0 * r);
	v.expected_components = graph.components;
	std::unordered_map<std::uint64_t, std::vector<unsigned int>> vertexGrid;
	const double cellV = 2.0 * r;
	for (std::size_t i = 0; i < nv; ++i)
		if (used[i] && !cavity[comp[i]])
		{
			const Vector3d p = point(i);
			vertexGrid[cellKey(static_cast<long>(std::floor(p.x / cellV)), static_cast<long>(std::floor(p.y / cellV)),
			                   static_cast<long>(std::floor(p.z / cellV)))].push_back(static_cast<unsigned int>(i));
		}
	const std::size_t J = network.junctions.size();
	std::vector<std::vector<std::size_t>> graphOf(count);
	std::vector<bool> mapped(graph.components, false);
	for (std::size_t gv = 0; gv < graph.vertex_component.size(); ++gv)
	{
		const std::size_t g = graph.vertex_component[gv];
		if (mapped[g])
			continue;
		Vector3d p;
		if (gv < J)
			p = network.junctions[gv].position;
		else
		{
			const Incidence &e = network.free_ends[gv - J];
			p = PointAt(network.curves[static_cast<std::size_t>(e.curve)], e.s);
		}
		double best = std::numeric_limits<double>::infinity();
		std::size_t at = nv;
		const long cx = static_cast<long>(std::floor(p.x / cellV)), cy = static_cast<long>(std::floor(p.y / cellV)),
		           cz = static_cast<long>(std::floor(p.z / cellV));
		for (long z = cz - 1; z <= cz + 1; ++z)
			for (long y = cy - 1; y <= cy + 1; ++y)
				for (long x = cx - 1; x <= cx + 1; ++x)
				{
					const auto it = vertexGrid.find(cellKey(x, y, z));
					if (it == vertexGrid.end())
						continue;
					for (unsigned int k : it->second)
					{
						const Vector3d w = point(k);
						const double d = (w.x - p.x) * (w.x - p.x) + (w.y - p.y) * (w.y - p.y) + (w.z - p.z) * (w.z - p.z);
						if (d < best)
						{
							best = d;
							at = k;
						}
					}
				}
		if (at < nv)
		{
			mapped[g] = true;
			graphOf[comp[at]].push_back(g);
		}
	}
	bool fusion = v.components < v.expected_components;
	bool split = v.components > v.expected_components;
	for (std::size_t c = 0; c < count; ++c)
	{
		if (cavity[c])
			continue;
		if (graphOf[c].size() > 1)
			fusion = true;
		if (graphOf[c].empty())
			split = true;
		long expected = 0;
		for (std::size_t g : graphOf[c])
			expected += graph.component_euler[g];
		if (!graphOf[c].empty() && chi[c] != expected)
		{
			v.closed_cells.push_back(c);
			report.Record(MicroarchWarning::SolidClosedCell, "solid.components",
			              "component " + std::to_string(c) + " has Euler characteristic " + std::to_string(chi[c]) + ", the network "
			              + std::to_string(expected) + " : a network cell is closed by the solid (component near "
			              + where(lowest[c]) + ") ; information, not part of the verdict");
		}
	}
	if (fusion)
		report.Record(MicroarchWarning::SolidUnplannedFusion, "solid.components",
		              std::to_string(v.components) + " solid components for " + std::to_string(v.expected_components)
		              + " network components : the solid joins parts the network keeps apart");
	if (split)
		report.Record(MicroarchWarning::SolidUnplannedSplit, "solid.components",
		              std::to_string(v.components) + " solid components for " + std::to_string(v.expected_components)
		              + " network components : the solid breaks a part the network keeps whole");
	v.fusion = fusion;
	v.split = split;

	// --- Echelle, paroi (2r >= minWallMm, exact), enveloppe.
	v.scale_set = constraints && constraints->mmPerUnit && std::isfinite(*constraints->mmPerUnit) && *constraints->mmPerUnit > 0.0;
	bool wallOk = false, envelopeOk = false;
	if (v.scale_set)
	{
		v.wall_mm = 2.0 * r * *constraints->mmPerUnit;
		wallOk = v.wall_mm >= constraints->minWallMm * (1.0 - 1e-12);
		if (!wallOk)
			report.Record(MicroarchWarning::SolidWallTooThin, "solid.radius_ratio",
			              "tube diameter " + number(v.wall_mm) + " mm < minWallMm " + number(constraints->minWallMm)
			                  + " mm ; minimum scale " + number(constraints->minWallMm / (2.0 * r))
			                  + " mm per drawing unit (minWallMm / 2r), or a larger radius");
		const MeshValidationReport &vr = v.validation;
		PrintExtent extent;
		for (int k = 0; k < 3; ++k)
		{
			const double o = k == 0 ? solid.origin.x : k == 1 ? solid.origin.y : solid.origin.z;
			extent.lo[k] = vr.bbMin[k] + o;
			extent.hi[k] = vr.bbMax[k] + o;
		}
		v.envelope = CheckPrintEnvelope(extent, *constraints);
		envelopeOk = v.envelope.IsPrintable();
	}
	v.printable = v.generator_ok && !fusion && !split && v.floating.empty() && v.scale_set && wallOk && envelopeOk;
	return v;
}

} // namespace cgarchi::microarch
