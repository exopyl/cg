#include "mesh_half_edge.h"
#include "bvh.h"

#include <vector>
#include <array>
#include <queue>
#include <cmath>
#include <unordered_map>
#include <utility>

//
// Mesh decimation by Quadric Error Metrics (QEM), method 1 of the
// "Methodes retenues" of simplification.md (Garland & Heckbert, SIGGRAPH 1997).
//
// MVP scope (Etape 1):
//   - geometric decimation only, no appearance attributes (Etape 3),
//   - manifold preserved (link condition + border kept frozen),
//   - optimal collapse position from quadric_minimize2 (cgmath/quadric.cpp),
//   - lazy-invalidation priority queue of candidate edges,
//   - anti-flip guard (reject a collapse that inverts an incident face normal),
//   - final compaction: a fresh, hole-free Mesh is rebuilt from the surviving
//     half-edge topology, so the renderer never sees invalid faces.
//
// The collapse mechanics reuse Che_mesh::edge_contract2 (link condition + map
// maintenance). The geometric Mesh_half_edge::edge_contract path (midpoint +
// m_pFaces=nullptr holes) is deliberately NOT used here: we work on the
// topology + a per-vertex quadric/position array and rebuild at the end.
//

namespace
{

// One quadric per vertex; layout matches quadric_t (double[10]), so a
// std::array's .data() can be passed straight to the quadric_* helpers.
typedef std::array<double, 10> Quadric;

// Safety cap on any one-ring walk: a structurally inconsistent ring (a risk
// flagged as a hypothesis in the feasibility study) must never spin forever.
const int RING_WALK_MAX = 100000;

inline void get_pos(const Mesh *m, int vi, Vector3f &p)
{
	p[0] = m->m_pVertices[3 * vi];
	p[1] = m->m_pVertices[3 * vi + 1];
	p[2] = m->m_pVertices[3 * vi + 2];
}

// Collect the one-ring vertex neighbours of vi (the m_v_end of each outgoing
// half-edge). Uses the map-based get_edge_from_vertex as the walk seed, since
// edge_contract2 maintains map_edges_vertex (not the m_edges_vertex vector).
// Returns true only if vi's one-ring is a clean CLOSED fan (every step lands on
// a valid edge and the walk returns to its start). A walk that hits an invalid
// edge, a -1 pair, or doesn't close signals an inconsistent / non-manifold
// neighbourhood — callers treat that as "do not collapse".
bool ring_neighbours(Che_mesh *che, int vi, std::vector<int> &out)
{
	out.clear();
	int start = che->get_edge_from_vertex(vi);
	if (start < 0)
		return false;

	int w = start;
	int guard = 0;
	do
	{
		const Che_edge &e = che->edge(w);
		if (!e.m_valid)
			return false;
		out.push_back(e.m_v_end);

		int n1 = che->edge(w).m_he_next;
		int n2 = che->edge(n1).m_he_next;
		w = che->edge(n2).m_pair;
		if (w < 0)
			return false; // open / inconsistent ring
		if (++guard >= RING_WALK_MAX)
			return false;
	} while (w != start);
	return true;
}

// Flag vertices that are non-manifold (bowtie) in the INPUT mesh: their
// incident triangles do not form a single fan/cycle. Computed once from the
// face list (independent of the half-edge), via the link of each vertex: each
// incident triangle (v,a,b) contributes the link edge a-b; v is manifold iff
// those link edges form a single path (boundary) or cycle (interior) — i.e.
// every link node has degree <= 2, there are 0 or 2 endpoints, and the link is
// connected. Such vertices are frozen so a collapse cannot weld two sheets into
// a non-manifold edge (the per-edge link condition cannot see this).
void mark_nonmanifold_vertices(Mesh *mesh, unsigned int nv, std::vector<char> &nm)
{
	nm.assign(nv, 0);

	std::vector<std::vector<std::pair<int, int>>> link(nv);
	for (unsigned int f = 0; f < mesh->m_nFaces; f++)
	{
		Face *face = mesh->m_pFaces[f];
		if (!face || face->m_nVertices < 3)
			continue;
		int a = face->m_pVertices[0], b = face->m_pVertices[1], c = face->m_pVertices[2];
		if (a < 0 || b < 0 || c < 0) continue;
		if ((unsigned)a < nv) link[a].push_back(std::make_pair(b, c));
		if ((unsigned)b < nv) link[b].push_back(std::make_pair(a, c));
		if ((unsigned)c < nv) link[c].push_back(std::make_pair(a, b));
	}

	for (unsigned int v = 0; v < nv; v++)
	{
		const std::vector<std::pair<int, int>> &edges = link[v];
		if (edges.empty())
			continue;

		std::unordered_map<int, std::vector<int>> adj;
		for (size_t i = 0; i < edges.size(); i++)
		{
			adj[edges[i].first].push_back(edges[i].second);
			adj[edges[i].second].push_back(edges[i].first);
		}

		int endpoints = 0;
		bool bad = false;
		for (auto &kv : adj)
		{
			int d = (int)kv.second.size();
			if (d > 2) { bad = true; break; }   // edge (v,kv) shared by >2 faces
			if (d == 1) endpoints++;
		}
		if (bad || (endpoints != 0 && endpoints != 2))
		{
			nm[v] = 1;
			continue;
		}

		// Connectivity: the link must be a single component.
		std::unordered_map<int, char> seen;
		std::vector<int> stack;
		int start = adj.begin()->first;
		stack.push_back(start);
		seen[start] = 1;
		int reached = 0;
		while (!stack.empty())
		{
			int x = stack.back();
			stack.pop_back();
			reached++;
			for (size_t k = 0; k < adj[x].size(); k++)
			{
				int y = adj[x][k];
				if (!seen[y]) { seen[y] = 1; stack.push_back(y); }
			}
		}
		if (reached != (int)adj.size())
			nm[v] = 1;
	}
}

// Correct link condition for a manifold edge collapse: the one-ring vertex
// neighbourhoods of the two endpoints must share exactly the two triangle
// apices opposite the edge. Any other shared neighbour would create a
// non-manifold edge or a topological fold. This replaces the in-tree
// Che_mesh::is_edge_contract2_valid, whose 1-ring walk only inspects a single
// neighbour (flagged [HYPOTHESE] in simplification.md).
bool link_condition_ok(Che_mesh *che, int ei)
{
	int u = che->edge(ei).m_v_begin;
	int v = che->edge(ei).m_v_end;

	// Apices of the two triangles sharing the edge.
	int iv3 = che->edge(che->edge(ei).m_he_next).m_v_end;
	int pair = che->edge(ei).m_pair;
	if (pair < 0)
		return false; // boundary edge (not collapsed here)
	int iv4 = che->edge(che->edge(pair).m_he_next).m_v_end;
	if (iv3 == iv4)
		return false; // degenerate / non-manifold around the edge

	// Full link condition: the link of the edge is the two apex VERTICES, and
	// must NOT contain the edge (iv3, iv4). If iv3 and iv4 are already directly
	// connected, collapsing welds a tetrahedral neighbourhood into a
	// non-manifold edge.
	if (che->get_edge(iv3, iv4) >= 0 || che->get_edge(iv4, iv3) >= 0)
		return false;

	// Both endpoints must have a clean, closed one-ring (interior, manifold);
	// otherwise the local connectivity is already inconsistent -> don't collapse.
	std::vector<int> nu, nv;
	if (!ring_neighbours(che, u, nu) || !ring_neighbours(che, v, nv))
		return false;

	// The collapse merges v's incident edges onto u. It is manifold-safe only if
	// u and v share NO neighbour other than the two apices iv3, iv4 — otherwise
	// an edge (u, w) would become incident to >2 faces (non-manifold). Test this
	// directly against the maintained edge map (get_edge), not by intersecting
	// two ring walks: this catches the case regardless of ring-walk order and
	// is robust to half-edge state the in-tree edge_contract2 may leave behind.
	for (size_t i = 0; i < nv.size(); i++)
	{
		int w = nv[i];
		if (w == u || w == iv3 || w == iv4)
			continue;
		if (che->get_edge(u, w) >= 0 || che->get_edge(w, u) >= 0)
			return false; // u already connected to w -> collapse doubles edge (u,w)
	}

	return true;
}

// Triangle of the face carrying half-edge e, in winding order.
inline void edge_triangle(Che_mesh *che, int e, int &a, int &b, int &c)
{
	a = che->edge(e).m_v_begin;
	b = che->edge(e).m_v_end;
	c = che->edge(che->edge(e).m_he_next).m_v_end;
}

// Would collapsing (u -> v) onto position `target` flip any surviving incident
// face? We compare each incident triangle's normal before and after moving u
// and v to `target`; faces that contain BOTH endpoints degenerate and are
// skipped (they are the (<=2) faces removed by the collapse).
bool normal_would_flip(Mesh *mesh, Che_mesh *che, int u, int v, const float target[3])
{
	int endpoints[2] = {u, v};
	std::vector<int> seen_faces;

	for (int s = 0; s < 2; s++)
	{
		int start = che->get_edge_from_vertex(endpoints[s]);
		if (start < 0)
			continue;

		int w = start;
		int guard = 0;
		do
		{
			const Che_edge &e = che->edge(w);
			if (!e.m_valid)
				break;

			int f = e.m_face;
			bool already = false;
			for (size_t k = 0; k < seen_faces.size(); k++)
				if (seen_faces[k] == f)
				{
					already = true;
					break;
				}
			if (!already)
			{
				seen_faces.push_back(f);

				int a, b, c;
				edge_triangle(che, w, a, b, c);

				bool hasU = (a == u || b == u || c == u);
				bool hasV = (a == v || b == v || c == v);
				if (!(hasU && hasV))
				{
					Vector3f a0, b0, c0, a1, b1, c1, n0, n1;
					get_pos(mesh, a, a0);
					get_pos(mesh, b, b0);
					get_pos(mesh, c, c0);

					a1 = a0;
					b1 = b0;
					c1 = c0;
					if (a == u || a == v) a1.Set (target[0], target[1], target[2]);
					if (b == u || b == v) b1.Set (target[0], target[1], target[2]);
					if (c == u || c == v) c1.Set (target[0], target[1], target[2]);

					n0 = Vector3f::evaluate_triangle_normal (a0, b0, c0);
					n1 = Vector3f::evaluate_triangle_normal (a1, b1, c1);
					if ((n0).DotProduct (n1) < 0.f)
						return true;
				}
			}

			int n1e = che->edge(w).m_he_next;
			int n2e = che->edge(n1e).m_he_next;
			w = che->edge(n2e).m_pair;
		} while (w >= 0 && w != start && ++guard < RING_WALK_MAX);
	}

	return false;
}

// Weight applied to feature/boundary constraint quadrics. Large relative to
// the unit-normal face quadrics so the optimal collapse position is pulled
// onto the feature line and collapsing across a feature is heavily penalised
// (Garland & Heckbert 1997, boundary preservation).
const float FEATURE_WEIGHT = 1000.0f;

// The constraint plane that contains the edge (pa -> pb) and is perpendicular
// to a face of normal `face_n`. Penalising motion off this plane keeps the
// feature line in place.
void feature_plane(const Vector3f &pa, const Vector3f &pb, const float *face_n, plane_t out)
{
	Vector3f edir = pb - pa;
	Vector3f fn (face_n[0], face_n[1], face_n[2]);
	Vector3f cn = edir.CrossProduct(fn); // perpendicular to the face, along the edge
	cn.Normalize();
	out[0] = cn[0];
	out[1] = cn[1];
	out[2] = cn[2];
	out[3] = -(cn[0] * pa[0] + cn[1] * pa[1] + cn[2] * pa[2]);
}

// Add that constraint plane (weighted) to a vertex's 3D quadric.
void add_feature_constraint(Quadric &q, const Vector3f &pa, const Vector3f &pb, const float *face_n)
{
	plane_t pl;
	feature_plane(pa, pb, face_n, pl);
	quadric_t qc;
	plane_quadric(pl, qc);
	quadric_scale(qc, qc, FEATURE_WEIGHT);
	quadric_add(q.data(), q.data(), qc);
}

// ---------------------------------------------------------------------------
// Voie B: generalized quadric over R^D, D = position(3) [+ colour(3)] [+ UV(2)]
// (Garland & Heckbert 1998, "Simplifying Surfaces with Color and Texture using
// Quadric Error Metrics"). Self-contained; does not touch cgmath's 3D quadric.
// Dimension is a runtime value (<= QN_MAX) shared by all vertices of a run; the
// matrix lives in a fixed buffer to avoid per-vertex heap allocation.
// Layout of the augmented vector: [x y z] [r g b] [u v], attribute blocks
// present only when included in the metric (see nd_color / nd_uv in simplify).
// ---------------------------------------------------------------------------
const int QN_MAX = 8; // 3 position + 3 colour + 2 UV

struct QN
{
	int dim;
	double A[QN_MAX * QN_MAX]; // dim x dim symmetric, row-major
	double b[QN_MAX];
	double c;
};

inline void qn_zero(QN &q, int dim)
{
	q.dim = dim;
	for (int i = 0; i < dim * dim; i++) q.A[i] = 0.0;
	for (int i = 0; i < dim; i++) q.b[i] = 0.0;
	q.c = 0.0;
}

inline void qn_add(QN &dst, const QN &s)
{
	const int d = dst.dim;
	for (int i = 0; i < d * d; i++) dst.A[i] += s.A[i];
	for (int i = 0; i < d; i++) dst.b[i] += s.b[i];
	dst.c += s.c;
}

inline double dotN(const double a[], const double b[], int d)
{
	double s = 0.0;
	for (int i = 0; i < d; i++) s += a[i] * b[i];
	return s;
}

// Accumulate the quadric of a triangle (augmented points P1,P2,P3) into q,
// weighted by `area`. Error of v = squared distance to the affine plane the
// triangle spans in R^dim: A = area*(I - e1 e1^T - e2 e2^T), etc.
void qn_add_face(QN &q, const double P1[], const double P2[], const double P3[], double area)
{
	const int d = q.dim;
	double E1[QN_MAX], E2[QN_MAX];
	for (int i = 0; i < d; i++) { E1[i] = P2[i] - P1[i]; E2[i] = P3[i] - P1[i]; }

	double n1 = sqrt(dotN(E1, E1, d));
	if (n1 < 1e-12) return;
	double e1[QN_MAX];
	for (int i = 0; i < d; i++) e1[i] = E1[i] / n1;

	double proj = dotN(E2, e1, d);
	double e2[QN_MAX];
	for (int i = 0; i < d; i++) e2[i] = E2[i] - proj * e1[i];
	double n2 = sqrt(dotN(e2, e2, d));
	if (n2 < 1e-12) return;
	for (int i = 0; i < d; i++) e2[i] /= n2;

	double p1e1 = dotN(P1, e1, d), p1e2 = dotN(P1, e2, d);
	for (int i = 0; i < d; i++)
		for (int j = 0; j < d; j++)
		{
			double a = ((i == j) ? 1.0 : 0.0) - e1[i] * e1[j] - e2[i] * e2[j];
			q.A[d * i + j] += area * a;
		}
	for (int i = 0; i < d; i++)
		q.b[i] += area * (p1e1 * e1[i] + p1e2 * e2[i] - P1[i]);
	q.c += area * (dotN(P1, P1, d) - p1e1 * p1e1 - p1e2 * p1e2);
}

// Add a purely geometric plane constraint (feature/boundary) in the position
// block: error += weight * (n . pos + d)^2. Only touches the first 3 dims.
void qn_add_plane_pos(QN &q, const plane_t pl, float weight)
{
	const int d = q.dim;
	double n[3] = {pl[0], pl[1], pl[2]};
	double dd = pl[3];
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			q.A[d * i + j] += weight * n[i] * n[j];
	for (int i = 0; i < 3; i++)
		q.b[i] += weight * dd * n[i];
	q.c += weight * dd * dd;
}

double qn_eval(const QN &q, const double v[])
{
	const int d = q.dim;
	double vAv = 0.0, bv = 0.0;
	for (int i = 0; i < d; i++)
	{
		double s = 0.0;
		for (int j = 0; j < d; j++) s += q.A[d * i + j] * v[j];
		vAv += v[i] * s;
		bv += q.b[i] * v[i];
	}
	return vAv + 2.0 * bv + q.c;
}

// Solve A x = -b (dim x dim) by Gaussian elimination with partial pivoting.
// Returns false if (near-)singular.
bool qn_minimize(const QN &q, double x[])
{
	const int d = q.dim;
	double M[QN_MAX][QN_MAX + 1];
	for (int i = 0; i < d; i++)
	{
		for (int j = 0; j < d; j++) M[i][j] = q.A[d * i + j];
		M[i][d] = -q.b[i];
	}
	for (int col = 0; col < d; col++)
	{
		int piv = col;
		double best = fabs(M[col][col]);
		for (int r = col + 1; r < d; r++)
		{
			double a = fabs(M[r][col]);
			if (a > best) { best = a; piv = r; }
		}
		if (best < 1e-10) return false;
		if (piv != col)
			for (int j = 0; j <= d; j++) { double t = M[col][j]; M[col][j] = M[piv][j]; M[piv][j] = t; }
		double dpiv = M[col][col];
		for (int r = 0; r < d; r++)
		{
			if (r == col) continue;
			double f = M[r][col] / dpiv;
			for (int j = col; j <= d; j++) M[r][j] -= f * M[col][j];
		}
	}
	for (int i = 0; i < d; i++) x[i] = M[i][d] / M[i][i];
	return true;
}

inline double tri_area(const double P1[], const double P2[], const double P3[])
{
	double u[3] = {P2[0] - P1[0], P2[1] - P1[1], P2[2] - P1[2]};
	double v[3] = {P3[0] - P1[0], P3[1] - P1[1], P3[2] - P1[2]};
	double cx = u[1] * v[2] - u[2] * v[1];
	double cy = u[2] * v[0] - u[0] * v[2];
	double cz = u[0] * v[1] - u[1] * v[0];
	return 0.5 * sqrt(cx * cx + cy * cy + cz * cz);
}

// A candidate collapse "remove v into u" with the optimal target position and
// the quadric cost. Endpoint version stamps support lazy invalidation: when an
// endpoint changes, its version is bumped and stale queue entries are dropped.
// target_attr carries the optimal attributes from the nD metric (Voie B):
// [0..2] = colour, [3..4] = UV (only the blocks present in the metric are set).
struct Candidate
{
	double cost;
	int u, v;
	int ver_u, ver_v;
	float target[3];
	float target_attr[5];
};

// std::priority_queue is a max-heap; invert so the smallest cost pops first.
struct CandidateGreater
{
	bool operator()(const Candidate &a, const Candidate &b) const { return a.cost > b.cost; }
};

} // namespace

void Mesh_half_edge::simplify(float target_ratio, const SimplifyOptions &options)
{
	const bool preserve_features = options.preserve_features;
	const float feature_angle_deg = options.feature_angle_deg;
	const bool preserve_attributes = options.preserve_attributes;

	Mesh *mesh = m_pMesh;
	if (!mesh || mesh->m_nFaces == 0)
		return;

	if (target_ratio < 0.f) target_ratio = 0.f;
	if (target_ratio > 1.f) target_ratio = 1.f;

	// 1. Ensure a pure triangle mesh and a fresh half-edge built from it.
	if (!mesh->IsTriangleMesh())
		mesh->Triangulate();
	// Voie B (UV): explode UV seams into duplicated vertices so UVs become
	// vertex-parallel and seams become frozen topological borders.
	if (options.preserve_uv)
		mesh->SplitVerticesByUVSeams();
	create_half_edge();
	Che_mesh *che = GetCheMesh();

	unsigned int nv = mesh->m_nVertices;
	unsigned int nf0 = mesh->m_nFaces;
	unsigned int target_faces = (unsigned int)(nf0 * target_ratio);

	// Appearance attributes present and transportable as per-vertex data.
	bool have_colors = preserve_attributes && mesh->m_pVertexColors.size() == 3 * (size_t)nv;
	bool have_normals = preserve_attributes && mesh->m_pVertexNormals.size() == 3 * (size_t)nv;
	bool have_uv = options.preserve_uv && mesh->m_pTextureCoordinates.size() == 2 * (size_t)nv;

	// Voie B: generalized R^D quadric. attribute_metric folds the present
	// attributes (colour and/or UV) into the error metric; otherwise they are
	// merely transported by interpolation. Layout: [pos(3)][col(3)?][uv(2)?].
	bool nd_color = options.attribute_metric && have_colors;
	bool nd_uv    = options.attribute_metric && have_uv;
	bool use_nd   = nd_color || nd_uv;
	float aw = options.attribute_weight < 1e-6f ? 1e-6f : options.attribute_weight;
	float ndw = sqrtf(aw);
	if (use_nd) have_normals = false; // normals recomputed in Voie B
	int nd_dim    = 3 + (nd_color ? 3 : 0) + (nd_uv ? 2 : 0);
	int nd_uv_off = 3 + (nd_color ? 3 : 0); // UV block offset in the nD vector

	// Error-bounded stop. Two enforcements of max_error:
	//   - QEM proxy (cheap): cost ~ squared surface deviation, threshold (e*diag)^2.
	//   - exact: per-collapse distance of the new vertex to the ORIGINAL surface.
	mesh->computebbox();
	float diag = mesh->bbox_diagonal_length();
	bool exact = options.exact_error && options.max_error > 0.f;
	double err_thresh2 = (options.max_error > 0.f && !use_nd && !exact)
	                   ? (double)(options.max_error * diag) * (double)(options.max_error * diag)
	                   : -1.0;
	float exact_thresh = options.max_error * diag; // linear bound for the exact gate

	// Snapshot of the original (triangulated) geometry + BVH for the exact gate.
	// A copy is mandatory: simplify() mutates m_pVertices in place, and the BVH
	// references the vertex array it was built on.
	Mesh orig_snapshot;
	BVH orig_bvh;
	if (exact)
	{
		orig_snapshot.SetVertices(nv, mesh->m_pVertices.data());
		unsigned int *t = mesh->GetTriangles();
		orig_snapshot.SetFaces(nf0, 3, t);
		free(t);
		orig_bvh.build(orig_snapshot);
	}

	std::vector<float> col = have_colors ? mesh->m_pVertexColors : std::vector<float>();
	std::vector<float> nrm = have_normals ? mesh->m_pVertexNormals : std::vector<float>();
	std::vector<float> uv  = have_uv ? mesh->m_pTextureCoordinates : std::vector<float>();

	// Build the nD augmented point of vertex vi: [pos][col?][uv?], attributes
	// scaled by ndw. Returns the dimension.
	auto nd_point = [&](int vi, double P[QN_MAX])
	{
		P[0] = mesh->m_pVertices[3 * vi];
		P[1] = mesh->m_pVertices[3 * vi + 1];
		P[2] = mesh->m_pVertices[3 * vi + 2];
		if (nd_color)
		{
			P[3] = ndw * col[3 * vi];
			P[4] = ndw * col[3 * vi + 1];
			P[5] = ndw * col[3 * vi + 2];
		}
		if (nd_uv)
		{
			P[nd_uv_off]     = ndw * uv[2 * vi];
			P[nd_uv_off + 1] = ndw * uv[2 * vi + 1];
		}
	};

	// 2. Per-vertex quadrics = sum of the face quadrics of incident faces.
	//    Voie A/MVP: 3D quadrics (Q). Voie B: R^D quadrics (Qn).
	std::vector<Quadric> Q;
	std::vector<QN> Qn;

	if (use_nd)
	{
		Qn.resize(nv);
		for (unsigned int i = 0; i < nv; i++) qn_zero(Qn[i], nd_dim);

		for (unsigned int f = 0; f < nf0; f++)
		{
			Face *face = mesh->m_pFaces[f];
			if (!face || face->m_nVertices < 3)
				continue;
			int a = face->m_pVertices[0];
			int b = face->m_pVertices[1];
			int c = face->m_pVertices[2];

			double Pa[QN_MAX], Pb[QN_MAX], Pc[QN_MAX];
			nd_point(a, Pa);
			nd_point(b, Pb);
			nd_point(c, Pc);

			QN qf;
			qn_zero(qf, nd_dim);
			qn_add_face(qf, Pa, Pb, Pc, tri_area(Pa, Pb, Pc));
			qn_add(Qn[a], qf);
			qn_add(Qn[b], qf);
			qn_add(Qn[c], qf);
		}
	}
	else
	{
		Q.assign(nv, Quadric());
		for (unsigned int i = 0; i < nv; i++) Q[i].fill(0.0);

		for (unsigned int f = 0; f < nf0; f++)
		{
			Face *face = mesh->m_pFaces[f];
			if (!face || face->m_nVertices < 3)
				continue;

			int a = face->m_pVertices[0];
			int b = face->m_pVertices[1];
			int c = face->m_pVertices[2];
			Vector3f va, vb, vc;
			get_pos(mesh, a, va);
			get_pos(mesh, b, vb);
			get_pos(mesh, c, vc);

			plane_t pl;
			plane_init(pl, va, vb, vc);
			quadric_t qf;
			plane_quadric(pl, qf);

			quadric_add(Q[a].data(), Q[a].data(), qf);
			quadric_add(Q[b].data(), Q[b].data(), qf);
			quadric_add(Q[c].data(), Q[c].data(), qf);
		}
	}

	// 2bis. Feature preservation (Etape 2): add perpendicular constraint
	// quadrics on the endpoints of sharp interior edges (crease) and material
	// seams. Borders are already frozen below, so they need no constraint.
	if (preserve_features)
	{
		// Per-face unit normals, packed 3 floats per face (vec3 is a C array
		// type and cannot be a std::vector element directly).
		std::vector<float> fn(3 * nf0, 0.f);
		for (unsigned int f = 0; f < nf0; f++)
		{
			Face *face = mesh->m_pFaces[f];
			if (!face || face->m_nVertices < 3)
				continue;
			Vector3f va, vb, vc;
			get_pos(mesh, face->m_pVertices[0], va);
			get_pos(mesh, face->m_pVertices[1], vb);
			get_pos(mesh, face->m_pVertices[2], vc);
			Vector3f nf = Vector3f::evaluate_triangle_normal(va, vb, vc);
			nf.Normalize();
			fn[3 * f] = nf[0]; fn[3 * f + 1] = nf[1]; fn[3 * f + 2] = nf[2];
		}

		const double PI = 3.14159265358979323846;
		const float cos_thr = (float)cos((double)feature_angle_deg * PI / 180.0);

		for (int e = 0; e < che->m_ne; e++)
		{
			const Che_edge &ed = che->edge(e);
			if (!ed.m_valid || ed.m_pair < 0)
				continue;
			if (e > ed.m_pair) // canonical representative of the undirected edge
				continue;

			int f1 = ed.m_face;
			int f2 = che->edge(ed.m_pair).m_face;
			if (f1 < 0 || f2 < 0 || f1 >= (int)nf0 || f2 >= (int)nf0)
				continue;

			bool crease = Vector3f(fn[3*f1], fn[3*f1+1], fn[3*f1+2]).DotProduct(Vector3f(fn[3*f2], fn[3*f2+1], fn[3*f2+2])) < cos_thr;
			bool seam = (mesh->m_pFaces[f1]->m_iMaterialId != mesh->m_pFaces[f2]->m_iMaterialId);
			if (!crease && !seam)
				continue;

			int a = ed.m_v_begin;
			int b = ed.m_v_end;
			Vector3f pa, pb;
			get_pos(mesh, a, pa);
			get_pos(mesh, b, pb);

			// One constraint plane per adjacent face, on both endpoints.
			if (use_nd)
			{
				plane_t pl1, pl2;
				feature_plane(pa, pb, &fn[3 * f1], pl1);
				feature_plane(pa, pb, &fn[3 * f2], pl2);
				qn_add_plane_pos(Qn[a], pl1, FEATURE_WEIGHT);
				qn_add_plane_pos(Qn[b], pl1, FEATURE_WEIGHT);
				qn_add_plane_pos(Qn[a], pl2, FEATURE_WEIGHT);
				qn_add_plane_pos(Qn[b], pl2, FEATURE_WEIGHT);
			}
			else
			{
				add_feature_constraint(Q[a], pa, pb, &fn[3 * f1]);
				add_feature_constraint(Q[b], pa, pb, &fn[3 * f1]);
				add_feature_constraint(Q[a], pa, pb, &fn[3 * f2]);
				add_feature_constraint(Q[b], pa, pb, &fn[3 * f2]);
			}
		}
	}

	std::vector<char> alive(nv, 1);
	std::vector<int> ver(nv, 0);

	// Freeze vertices that are non-manifold in the input (bowties): collapses
	// incident to them would create non-manifold edges. Detected once from the
	// faces; clean manifold meshes have none (so they decimate fully).
	std::vector<char> nm_vertex;
	mark_nonmanifold_vertices(mesh, nv, nm_vertex);

	// Cost + optimal collapse of edge (u -> v): keep u, remove v.
	// out_target = optimal position; out_attr = optimal attributes from the nD
	// metric ([0..2] colour, [3..4] UV — only the present blocks are written).
	auto eval = [&](int u, int v, float out_target[3], float out_attr[5]) -> double
	{
		if (use_nd)
		{
			QN q = Qn[u];
			qn_add(q, Qn[v]);

			double Pu[QN_MAX], Pv[QN_MAX];
			nd_point(u, Pu);
			nd_point(v, Pv);

			double sol[QN_MAX];
			double err;

			bool ok = qn_minimize(q, sol);
			if (ok)
			{
				// Reject a wild optimum. A rank-deficient (near-flat) augmented
				// quadric has a whole subspace of min-error points, and
				// qn_minimize may return one far from the edge — which would move
				// the merged vertex arbitrarily. Accept the free optimum only if
				// its POSITION lies near the collapsed edge; otherwise fall back
				// to the best on-edge point (mirrors quadric_minimize2's cascade).
				double ex = Pv[0] - Pu[0], ey = Pv[1] - Pu[1], ez = Pv[2] - Pu[2];
				double len2 = ex * ex + ey * ey + ez * ez;
				double mx = 0.5 * (Pu[0] + Pv[0]), my = 0.5 * (Pu[1] + Pv[1]), mz = 0.5 * (Pu[2] + Pv[2]);
				double dx = sol[0] - mx, dy = sol[1] - my, dz = sol[2] - mz;
				double d2 = dx * dx + dy * dy + dz * dz;
				const double BOUND2 = 16.0; // accept within 4 x edge length of the midpoint
				if (d2 > BOUND2 * len2)
					ok = false;
			}

			if (ok)
			{
				err = qn_eval(q, sol);
			}
			else
			{
				// Best of the two augmented endpoints / midpoint (all on the edge).
				double Pm[QN_MAX];
				for (int i = 0; i < nd_dim; i++) Pm[i] = 0.5 * (Pu[i] + Pv[i]);
				double eu = qn_eval(q, Pu), ev = qn_eval(q, Pv), em = qn_eval(q, Pm);
				const double *best = Pu; double be = eu;
				if (ev < be) { best = Pv; be = ev; }
				if (em < be) { best = Pm; be = em; }
				for (int i = 0; i < nd_dim; i++) sol[i] = best[i];
				err = be;
			}
			out_target[0] = (float)sol[0];
			out_target[1] = (float)sol[1];
			out_target[2] = (float)sol[2];
			if (nd_color)
			{
				out_attr[0] = (float)(sol[3] / ndw);
				out_attr[1] = (float)(sol[4] / ndw);
				out_attr[2] = (float)(sol[5] / ndw);
			}
			if (nd_uv)
			{
				out_attr[3] = (float)(sol[nd_uv_off] / ndw);
				out_attr[4] = (float)(sol[nd_uv_off + 1] / ndw);
			}
			return (err < 0.0) ? 0.0 : err;
		}

		Quadric q;
		quadric_add(q.data(), Q[u].data(), Q[v].data());

		Vector3f vu, vv, vnew;
		get_pos(mesh, u, vu);
		get_pos(mesh, v, vv);

		float err = 0.f;
		if (quadric_minimize2(q.data(), vnew, &err, vu, vv) != 0)
		{
			vnew[0] = (vu[0] + vv[0]) * 0.5f;
			vnew[1] = (vu[1] + vv[1]) * 0.5f;
			vnew[2] = (vu[2] + vv[2]) * 0.5f;
			err = (float)quadric_eval(q.data(), vnew);
		}
		out_target[0] = vnew[0];
		out_target[1] = vnew[1];
		out_target[2] = vnew[2];
		return (err < 0.0) ? 0.0 : (double)err;
	};

	// 3. Seed the priority queue: one candidate per interior undirected edge.
	std::priority_queue<Candidate, std::vector<Candidate>, CandidateGreater> pq;
	for (int e = 0; e < che->m_ne; e++)
	{
		const Che_edge &ed = che->edge(e);
		if (!ed.m_valid || ed.m_pair < 0)
			continue;
		if (e > ed.m_pair) // canonical representative of the undirected edge
			continue;

		Candidate cand;
		cand.u = ed.m_v_begin;
		cand.v = ed.m_v_end;
		cand.cost = eval(cand.u, cand.v, cand.target, cand.target_attr);
		cand.ver_u = ver[cand.u];
		cand.ver_v = ver[cand.v];
		pq.push(cand);
	}

	// 4. Greedy collapse loop.
	unsigned int cur_faces = nf0;
	while (cur_faces > target_faces && !pq.empty())
	{
		Candidate c = pq.top();
		pq.pop();

		int u = c.u, v = c.v;
		if (!alive[u] || !alive[v])
			continue;
		if (ver[u] != c.ver_u || ver[v] != c.ver_v) // stale entry
			continue;

		// Error-bounded stop: the heap pops in increasing cost, so once the
		// cheapest live collapse exceeds the bound, none is admissible.
		if (err_thresh2 >= 0.0 && c.cost > err_thresh2)
			break;

		// Freeze borders (border-aware decimation is Etape 2) and input
		// non-manifold (bowtie) vertices (collapses there create non-manifold edges).
		if (che->is_border(u) || che->is_border(v))
			continue;
		if (nm_vertex[u] || nm_vertex[v])
			continue;

		int ei = che->get_edge(u, v);
		if (ei < 0)
			continue;
		const Che_edge &ed = che->edge(ei);
		if (!ed.m_valid || ed.m_pair < 0)
			continue;

		if (!link_condition_ok(che, ei))
			continue;
		if (normal_would_flip(mesh, che, u, v, c.target))
			continue;

		// Reliable error bound: reject this collapse if its new vertex would lie
		// farther than the bound from the ORIGINAL surface (true point-to-surface
		// distance). Per-collapse rejection (not a global break): surface distance
		// is not monotone in QEM-cost order.
		if (exact)
		{
			float d2 = orig_bvh.closest_distance2(Vector3f(c.target[0], c.target[1], c.target[2]));
			if (d2 >= 0.f && d2 > exact_thresh * exact_thresh)
				continue;
		}

		// Topological collapse (also re-checks its own validity + maintains maps).
		if (che->edge_contract2(ei) != 0)
			continue;

		// Edge parameter t of the target along [u, v] (clamped), used to
		// interpolate the attributes that are NOT folded into the nD metric.
		// Computed BEFORE moving u.
		bool needs_t = (have_colors && !nd_color) || have_normals || (have_uv && !nd_uv);
		float t = 0.f;
		if (needs_t)
		{
			Vector3f pu, pv;
			get_pos(mesh, u, pu);
			get_pos(mesh, v, pv);
			Vector3f e;
			e = pv - pu;
			float len2 = (e).DotProduct (e);
			if (len2 > 0.f)
			{
				Vector3f d;
				d[0] = c.target[0] - pu[0];
				d[1] = c.target[1] - pu[1];
				d[2] = c.target[2] - pu[2];
				t = (d).DotProduct (e) / len2;
				if (t < 0.f) t = 0.f;
				else if (t > 1.f) t = 1.f;
			}
		}

		// Colour: from the nD optimum if in the metric, else interpolated.
		if (nd_color)
			for (int k = 0; k < 3; k++) col[3 * u + k] = c.target_attr[k];
		else if (have_colors)
			for (int k = 0; k < 3; k++)
				col[3 * u + k] = (1.f - t) * col[3 * u + k] + t * col[3 * v + k];

		// Normals are only ever transported outside the nD metric (have_normals
		// is forced false when use_nd, where they are recomputed instead).
		if (have_normals)
		{
			Vector3f n;
			for (int k = 0; k < 3; k++)
				n[k] = (1.f - t) * nrm[3 * u + k] + t * nrm[3 * v + k];
			(n).Normalize ();
			nrm[3 * u] = n[0];
			nrm[3 * u + 1] = n[1];
			nrm[3 * u + 2] = n[2];
		}

		// UV: from the nD optimum if in the metric, else interpolated.
		if (nd_uv)
		{
			uv[2 * u] = c.target_attr[3];
			uv[2 * u + 1] = c.target_attr[4];
		}
		else if (have_uv)
			for (int k = 0; k < 2; k++)
				uv[2 * u + k] = (1.f - t) * uv[2 * u + k] + t * uv[2 * v + k];

		// Commit geometry only after the topology collapse succeeded.
		mesh->m_pVertices[3 * u] = c.target[0];
		mesh->m_pVertices[3 * u + 1] = c.target[1];
		mesh->m_pVertices[3 * u + 2] = c.target[2];
		if (use_nd)
			qn_add(Qn[u], Qn[v]);
		else
			quadric_add(Q[u].data(), Q[u].data(), Q[v].data());

		alive[v] = 0;
		ver[u]++;
		cur_faces -= 2; // an interior edge collapse removes exactly two faces

		// Re-evaluate the edges now incident to u and push fresh candidates.
		int start = che->get_edge_from_vertex(u);
		if (start >= 0)
		{
			int w = start;
			int guard = 0;
			do
			{
				const Che_edge &we = che->edge(w);
				if (!we.m_valid)
					break;

				if (we.m_v_begin == u && we.m_pair >= 0)
				{
					int n = we.m_v_end;
					if (alive[n])
					{
						Candidate nc;
						nc.u = u;
						nc.v = n;
						nc.cost = eval(u, n, nc.target, nc.target_attr);
						nc.ver_u = ver[u];
						nc.ver_v = ver[n];
						pq.push(nc);
					}
				}

				int n1 = che->edge(w).m_he_next;
				int n2 = che->edge(n1).m_he_next;
				w = che->edge(n2).m_pair;
			} while (w >= 0 && w != start && ++guard < RING_WALK_MAX);
		}
	}

	// 5. Compaction: rebuild a fresh, hole-free Mesh from surviving topology.
	std::vector<unsigned int> new_faces; // flat triplets, old vertex indices
	std::vector<char> face_seen(nf0, 0);
	std::vector<char> v_used(nv, 0);

	for (int e = 0; e < che->m_ne; e++)
	{
		const Che_edge &ed = che->edge(e);
		if (!ed.m_valid)
			continue;
		int f = ed.m_face;
		if (f < 0 || f >= (int)nf0 || face_seen[f])
			continue;
		face_seen[f] = 1;

		int a, b, cc;
		edge_triangle(che, e, a, b, cc);
		if (a == b || b == cc || a == cc)
			continue; // skip any degenerate triangle defensively

		new_faces.push_back((unsigned int)a);
		new_faces.push_back((unsigned int)b);
		new_faces.push_back((unsigned int)cc);
		v_used[a] = v_used[b] = v_used[cc] = 1;
	}

	std::vector<int> remap(nv, -1);
	std::vector<float> new_verts;
	std::vector<float> new_col, new_nrm, new_uv;
	new_verts.reserve(3 * nv);
	int new_nv = 0;
	for (unsigned int i = 0; i < nv; i++)
	{
		if (!v_used[i])
			continue;
		remap[i] = new_nv++;
		new_verts.push_back(mesh->m_pVertices[3 * i]);
		new_verts.push_back(mesh->m_pVertices[3 * i + 1]);
		new_verts.push_back(mesh->m_pVertices[3 * i + 2]);
		if (have_colors)
		{
			new_col.push_back(col[3 * i]);
			new_col.push_back(col[3 * i + 1]);
			new_col.push_back(col[3 * i + 2]);
		}
		if (have_normals)
		{
			new_nrm.push_back(nrm[3 * i]);
			new_nrm.push_back(nrm[3 * i + 1]);
			new_nrm.push_back(nrm[3 * i + 2]);
		}
		if (have_uv)
		{
			new_uv.push_back(uv[2 * i]);
			new_uv.push_back(uv[2 * i + 1]);
		}
	}
	for (size_t i = 0; i < new_faces.size(); i++)
		new_faces[i] = (unsigned int)remap[new_faces[i]];

	Mesh *pNew = new Mesh();
	pNew->SetVertices((unsigned int)new_nv, new_verts.data());
	pNew->SetFaces((unsigned int)(new_faces.size() / 3), 3, new_faces.data());
	pNew->m_name = mesh->m_name;
	// ComputeNormals populates face normals (and geometric vertex normals); the
	// preserved per-vertex attributes then override what should be kept.
	pNew->ComputeNormals();
	if (have_normals)
		pNew->m_pVertexNormals = new_nrm;
	if (have_colors)
		pNew->m_pVertexColors = new_col;
	if (have_uv)
	{
		pNew->m_pTextureCoordinates = new_uv;   // vertex-parallel (size 2*new_nv)
		pNew->m_nTextureCoordinates = (unsigned int)new_nv;
	}
	pNew->computebbox();

	delete m_pMesh;
	m_pMesh = pNew;
	create_half_edge();
}
