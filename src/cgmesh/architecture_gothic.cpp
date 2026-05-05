#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "architecture_gothic.h"

namespace
{
	// Tessellate the visible silhouette of an offset arch into a CCW polyline.
	// arcLeft and arcRight are sub-arcs of the offset arch, parameterized so
	// that arcLeft.pointAt(0) is the right foot and arcLeft.pointAt(1) = apex
	// = arcRight.pointAt(0), and arcRight.pointAt(1) is the left foot.
	std::vector<Vector2d> archOutlineCcw (const ArchGeom &arch, double maxAngleRad)
	{
		auto leftPts  = arch.arcLeft.tessellateAdaptive(maxAngleRad);    // foot-right -> apex
		auto rightPts = arch.arcRight.tessellateAdaptive(maxAngleRad);   // apex -> foot-left

		std::vector<Vector2d> outline;
		outline.reserve(leftPts.size() + rightPts.size());
		for (const auto &p : leftPts) outline.push_back(p);
		// Skip the first point of rightPts (= apex, duplicates last of leftPts).
		for (size_t i = 1; i < rightPts.size(); ++i)
			outline.push_back(rightPts[i]);
		return outline;
	}

	// Pack a contour (vector of Vector2d) into a flat float array (x0 y0 x1 y1 ...)
	// suitable for Polygon2::add_contour.
	std::vector<float> packContour (const std::vector<Vector2d> &pts)
	{
		std::vector<float> out;
		out.reserve(pts.size() * 2);
		for (const auto &p : pts)
		{
			out.push_back(static_cast<float>(p.x));
			out.push_back(static_cast<float>(p.y));
		}
		return out;
	}

	// Sample a circle into a CCW polyline. Step controls the angular resolution.
	std::vector<Vector2d> circleOutlineCcw (const Circle &c, double maxAngleRad)
	{
		const double TWO_PI = 2.0 * 3.14159265358979323846;
		int n = static_cast<int>(std::ceil(TWO_PI / maxAngleRad));
		if (n < 8) n = 8;
		std::vector<Vector2d> pts;
		pts.reserve(n);
		for (int i = 0; i < n; ++i)
		{
			double theta = TWO_PI * i / n;
			pts.push_back(c.pointAt(theta));
		}
		return pts;
	}

	double signedAreaShoelace (const std::vector<Vector2d> &polygon)
	{
		double sum = 0.0;
		size_t n = polygon.size();
		for (size_t i = 0; i < n; ++i)
		{
			size_t j = (i + 1) % n;
			sum += polygon[i].x * polygon[j].y - polygon[j].x * polygon[i].y;
		}
		return 0.5 * sum;
	}

	// Append a contour as a CW hole. If the input is CCW, reverse it first.
	// If already CW, push as is.
	void pushAsHoleAnyOrientation (std::vector<std::vector<Vector2d>> &holes,
	                                std::vector<Vector2d> contour)
	{
		if (signedAreaShoelace(contour) > 0.0)
			std::reverse(contour.begin(), contour.end());   // CCW -> CW
		holes.push_back(std::move(contour));
	}

	// Append a contour as a CW hole (input is CCW, will be reversed).
	void pushAsHole (std::vector<std::vector<Vector2d>> &holes,
	                  std::vector<Vector2d> ccwOutline)
	{
		std::reverse(ccwOutline.begin(), ccwOutline.end());
		holes.push_back(std::move(ccwOutline));
	}

	// Build the closed boundary of a pointed foil. The boundary is :
	//   arcLeft  : baseLeft -> apex
	//   arcRight : apex -> baseRight  (skip first to avoid duplicating apex)
	//   (implicit closure from baseRight back to baseLeft : chord)
	//
	// We deliberately use a CHORD (straight line) rather than the rim arc on
	// the foil ring's outer circle. Reasons :
	//   1. The rim arc would coincide with edges of the rosette / lancet head
	//      circle if both are added as holes, which causes GLU's tessellator
	//      to assert (coincident edges). Chord closure avoids this.
	//   2. The chord-closed shape is the geometrically meaningful "foil leaf",
	//      excluding the small sliver between the foil and the rosette rim.
	std::vector<Vector2d> pointedFoilContour (const PointedFoil &pf,
	                                            double maxAngleRad)
	{
		std::vector<Vector2d> contour;
		auto leftPts  = pf.arcLeft.tessellateAdaptive(maxAngleRad);
		auto rightPts = pf.arcRight.tessellateAdaptive(maxAngleRad);

		for (const auto &p : leftPts) contour.push_back(p);
		for (size_t i = 1; i < rightPts.size(); ++i) contour.push_back(rightPts[i]);

		return contour;
	}
}

Polygon2 buildBayStonePolygon (const WindowGeometry &geom, const GothicMeshParams &params)
{
	// Outer contour : main inner offset, CCW.
	std::vector<Vector2d> outer = archOutlineCcw(geom.mainOffset.inner, params.maxAngleRad);

	std::vector<std::vector<Vector2d>> holes;

	// Phase 1 : each lancet's inner offset is a hole (= the lancet glass void).
	for (const auto &l : geom.subwindows.lancets)
		pushAsHole(holes, archOutlineCcw(l.offset.inner, params.maxAngleRad));

	// Phase 2 : rosette + foils as additional holes for richer tracery detail.
	//
	// Note : because GLU's NONZERO winding rule does not let us nest "void
	// inside void", placing foil holes inside an already-void area (e.g. lancet
	// inner) creates stone "islands" at the foil positions. This is acceptable
	// for visualization — it shows the foil layout — but it does not exactly
	// mirror the architectural meaning.

	if (geom.hasRosette)
		pushAsHole(holes, circleOutlineCcw(geom.rosette.circle, params.maxAngleRad));

	auto pushFoils = [&](const FoilRing &ring) {
		for (const auto &foil : ring.foils)
		{
			if (std::holds_alternative<RoundFoil>(foil))
			{
				const RoundFoil &rf = std::get<RoundFoil>(foil);
				pushAsHole(holes, circleOutlineCcw(rf.circle, params.maxAngleRad));
			}
			else
			{
				const PointedFoil &pf = std::get<PointedFoil>(foil);
				std::vector<Vector2d> contour = pointedFoilContour(pf, params.maxAngleRad);
				// The natural traversal order may be CCW or CW depending on geometry ;
				// push with auto-orientation correction.
				pushAsHoleAnyOrientation(holes, std::move(contour));
			}
		}
	};

	if (geom.hasRosetteFoils)
		pushFoils(geom.rosetteFoils);

	if (geom.hasSubwindowFoils)
		for (const FoilRing &ring : geom.subwindowFoils)
			pushFoils(ring);

	// Pack contours into Polygon2.
	Polygon2 poly;
	poly.alloc_contours(1 + static_cast<int>(holes.size()));

	std::vector<float> outerPacked = packContour(outer);
	poly.add_contour(0, static_cast<unsigned int>(outer.size()), outerPacked.data());

	for (size_t i = 0; i < holes.size(); ++i)
	{
		std::vector<float> holePacked = packContour(holes[i]);
		poly.add_contour(static_cast<unsigned int>(i + 1),
		                 static_cast<unsigned int>(holes[i].size()),
		                 holePacked.data());
	}

	return poly;
}

void tessellateToMesh (Polygon2 &polygon, Mesh &out, double z)
{
	float        *pV = nullptr;
	unsigned int  nV = 0;
	unsigned int *pF = nullptr;
	unsigned int  nF = 0;

	polygon.tesselate(&pV, &nV, &pF, &nF);

	if (nV == 0 || nF == 0)
	{
		if (pV) free(pV);
		if (pF) free(pF);
		throw std::runtime_error("tessellateToMesh: tessellation produced no output");
	}

	// Polygon2 packs vertices as (x, y, 0). Override z if non-zero.
	if (z != 0.0)
	{
		const float zf = static_cast<float>(z);
		for (unsigned int i = 0; i < nV; ++i)
			pV[3*i + 2] = zf;
	}

	out.SetVertices(nV, pV);
	out.SetFaces(nF, 3, pF);

	free(pV);
	free(pF);
}

std::vector<Vector2d> rectangularProfile ()
{
	// CCW from path-tangent perspective (= when looking along +T, with B = "up",
	// N = "right") :
	//   (0, 0) bottom-left  -> (1, 0) top-left  -> (1, 1) top-right  -> (0, 1) bottom-right
	return { Vector2d(0.0, 0.0), Vector2d(1.0, 0.0),
	         Vector2d(1.0, 1.0), Vector2d(0.0, 1.0) };
}

void sweepProfileAlongArc (const Arc &arc,
                            const std::vector<Vector2d> &profile,
                            double scale_u,
                            double scale_v,
                            Mesh &out,
                            double maxAngleRad)
{
	if (profile.size() < 3)
		throw std::invalid_argument("sweepProfileAlongArc: profile must have at least 3 points");
	if (arc.length() <= 0.0)
		throw std::invalid_argument("sweepProfileAlongArc: arc has non-positive length");

	// Sample the arc into path positions.
	std::vector<Vector2d> pathPos = arc.tessellateAdaptive(maxAngleRad);
	const int N = static_cast<int>(pathPos.size());
	if (N < 2)
		throw std::invalid_argument("sweepProfileAlongArc: arc tessellation produced fewer than 2 points");

	const int M = static_cast<int>(profile.size());

	// Vertex layout : index = i * M + j, with i = path sample, j = profile point.
	std::vector<float> verts(static_cast<size_t>(3) * N * M);
	for (int i = 0; i < N; ++i)
	{
		const Vector2d &P = pathPos[i];
		// Tangent at parameter t = i / (N-1).
		const double t = (N == 1) ? 0.0 : static_cast<double>(i) / (N - 1);
		Vector2d T = arc.tangentAt(t);
		// In-plane "outward" normal : right of CCW walk = (T.y, -T.x).
		const double Nx = T.y;
		const double Ny = -T.x;

		for (int j = 0; j < M; ++j)
		{
			const double u = profile[j].x;
			const double v = profile[j].y;
			const int idx = i * M + j;
			verts[3*idx + 0] = static_cast<float>(P.x + v * scale_v * Nx);
			verts[3*idx + 1] = static_cast<float>(P.y + v * scale_v * Ny);
			verts[3*idx + 2] = static_cast<float>(u * scale_u);
		}
	}

	// Faces : (N - 1) path strips, each producing M quads (= 2 triangles each)
	// since the profile is closed.
	std::vector<unsigned int> faces;
	faces.reserve(static_cast<size_t>(3) * 2 * (N - 1) * M);
	for (int i = 0; i < N - 1; ++i)
	{
		for (int j = 0; j < M; ++j)
		{
			const int j1 = (j + 1) % M;
			const unsigned int v00 = i       * M + j;
			const unsigned int v10 = (i + 1) * M + j;
			const unsigned int v11 = (i + 1) * M + j1;
			const unsigned int v01 = i       * M + j1;

			// CCW from outside the tube (computed with profile in CCW order).
			faces.push_back(v00);
			faces.push_back(v10);
			faces.push_back(v11);

			faces.push_back(v00);
			faces.push_back(v11);
			faces.push_back(v01);
		}
	}

	out.SetVertices(static_cast<unsigned int>(N) * M, verts.data());
	out.SetFaces(static_cast<unsigned int>(faces.size() / 3), 3, faces.data());
}

void extrudeToMesh (Polygon2 &polygon, Mesh &out, double zBottom, double zTop)
{
	float        *pV = nullptr;
	unsigned int  nV = 0;
	unsigned int *pF = nullptr;
	unsigned int  nF = 0;

	polygon.tesselate(&pV, &nV, &pF, &nF);

	if (nV == 0 || nF == 0)
	{
		if (pV) free(pV);
		if (pF) free(pF);
		throw std::runtime_error("extrudeToMesh: tessellation produced no output");
	}

	// Vertex layout of the extruded mesh :
	//   [0,   nV)     : top vertices    (z = zTop)
	//   [nV,  2*nV)   : bottom vertices (z = zBottom)   — mirror of the top
	const unsigned int N = nV;
	const float zT = static_cast<float>(zTop);
	const float zB = static_cast<float>(zBottom);

	std::vector<float> verts(2 * N * 3);
	for (unsigned int i = 0; i < N; ++i)
	{
		verts[3*i + 0]         = pV[3*i + 0];
		verts[3*i + 1]         = pV[3*i + 1];
		verts[3*i + 2]         = zT;
		verts[3*(i + N) + 0]   = pV[3*i + 0];
		verts[3*(i + N) + 1]   = pV[3*i + 1];
		verts[3*(i + N) + 2]   = zB;
	}

	std::vector<unsigned int> faces;
	// Pre-allocate : top cap (nF tris) + bottom cap (nF tris) + side walls
	// (2 tris per polygon edge, one edge per contour vertex, plus a few extras).
	faces.reserve(3 * (2 * nF + 4 * 200));   // upper bound for typical configs

	// Top cap : keep original CCW triangles (normals +z).
	for (unsigned int i = 0; i < nF; ++i)
	{
		faces.push_back(pF[3*i + 0]);
		faces.push_back(pF[3*i + 1]);
		faces.push_back(pF[3*i + 2]);
	}

	// Bottom cap : same triangles, indices shifted by N, winding REVERSED so
	// that the normals point -z (outward, downward).
	for (unsigned int i = 0; i < nF; ++i)
	{
		faces.push_back(N + pF[3*i + 0]);
		faces.push_back(N + pF[3*i + 2]);
		faces.push_back(N + pF[3*i + 1]);
	}

	// Side walls : for each contour edge (P_i -> P_j), emit two triangles whose
	// outward normal points away from the stone region.
	//
	// Convention : in a CCW outer contour, walking P_i -> P_j has the polygon
	// stone on the LEFT, the outside on the RIGHT. In a CW hole, walking
	// P_i -> P_j has the polygon stone on the LEFT, the hole on the RIGHT.
	// In both cases, "right of walk" = outward-of-stone, so the SAME triangle
	// ordering produces correctly-oriented walls :
	//   tri 1 = (P_i_top, P_i_bot, P_j_bot)
	//   tri 2 = (P_i_top, P_j_bot, P_j_top)
	unsigned int contourStart = 0;
	for (int ci = 0; ci < polygon.get_n_contours(); ++ci)
	{
		const unsigned int nPts = polygon.get_n_points(ci);
		for (unsigned int i = 0; i < nPts; ++i)
		{
			const unsigned int j        = (i + 1) % nPts;
			const unsigned int p_i_top  = contourStart + i;
			const unsigned int p_j_top  = contourStart + j;
			const unsigned int p_i_bot  = N + p_i_top;
			const unsigned int p_j_bot  = N + p_j_top;

			faces.push_back(p_i_top);
			faces.push_back(p_i_bot);
			faces.push_back(p_j_bot);

			faces.push_back(p_i_top);
			faces.push_back(p_j_bot);
			faces.push_back(p_j_top);
		}
		contourStart += nPts;
	}

	out.SetVertices(2 * N, verts.data());
	out.SetFaces(static_cast<unsigned int>(faces.size() / 3), 3, faces.data());

	free(pV);
	free(pF);
}

namespace
{
	bool endsWithCaseInsensitive (const std::string &s, const std::string &suffix)
	{
		if (s.size() < suffix.size()) return false;
		for (size_t i = 0; i < suffix.size(); ++i)
		{
			char a = s[s.size() - suffix.size() + i];
			char b = suffix[i];
			if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
			if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
			if (a != b) return false;
		}
		return true;
	}

	// Minimal STL ASCII writer. cgmesh's Mesh::export_stl is currently a stub
	// (returns -1), so we provide our own implementation.
	void writeMeshAsStlAscii (Mesh &mesh, const std::string &filePath,
	                           const std::string &solidName)
	{
		std::ofstream f(filePath);
		if (!f.is_open())
			throw std::runtime_error("writeMeshAsStlAscii: cannot open " + filePath);

		f << "solid " << solidName << "\n";
		unsigned int nF = mesh.GetNFaces();
		for (unsigned int i = 0; i < nF; ++i)
		{
			int a = mesh.GetFaceVertex(i, 0);
			int b = mesh.GetFaceVertex(i, 1);
			int c = mesh.GetFaceVertex(i, 2);
			if (a < 0 || b < 0 || c < 0) continue;

			float va[3], vb[3], vc[3];
			if (mesh.GetVertex((unsigned int) a, va) != 0) continue;
			if (mesh.GetVertex((unsigned int) b, vb) != 0) continue;
			if (mesh.GetVertex((unsigned int) c, vc) != 0) continue;

			// Triangle normal = (b-a) x (c-a), normalized.
			float ux = vb[0]-va[0], uy = vb[1]-va[1], uz = vb[2]-va[2];
			float vx = vc[0]-va[0], vy = vc[1]-va[1], vz = vc[2]-va[2];
			float nx = uy*vz - uz*vy;
			float ny = uz*vx - ux*vz;
			float nz = ux*vy - uy*vx;
			float len = std::sqrt(nx*nx + ny*ny + nz*nz);
			if (len > 1e-12f) { nx /= len; ny /= len; nz /= len; }
			else { nx = 0.0f; ny = 0.0f; nz = 1.0f; }    // degenerate triangle, default normal

			f << "facet normal " << nx << " " << ny << " " << nz << "\n";
			f << "  outer loop\n";
			f << "    vertex " << va[0] << " " << va[1] << " " << va[2] << "\n";
			f << "    vertex " << vb[0] << " " << vb[1] << " " << vb[2] << "\n";
			f << "    vertex " << vc[0] << " " << vc[1] << " " << vc[2] << "\n";
			f << "  endloop\n";
			f << "endfacet\n";
		}
		f << "endsolid " << solidName << "\n";
	}
}

void writeBayMesh (const WindowGeometry &geom, const std::string &filePath,
                   const GothicMeshParams &params)
{
	Polygon2 poly = buildBayStonePolygon(geom, params);
	Mesh mesh;

	if (params.zHeight > 0.0)
		extrudeToMesh(poly, mesh, 0.0, params.zHeight);
	else
		tessellateToMesh(poly, mesh, 0.0);

	// .stl needs a custom writer (cgmesh's export_stl is a stub returning -1).
	if (endsWithCaseInsensitive(filePath, ".stl"))
	{
		writeMeshAsStlAscii(mesh, filePath, "gothic_window");
		return;
	}

	int rc = mesh.save(filePath.c_str());
	if (rc < 0)
		throw std::runtime_error("writeBayMesh: Mesh::save failed for " + filePath);
}
