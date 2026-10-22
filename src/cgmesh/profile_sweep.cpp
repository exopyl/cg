#include "profile_sweep.h"

#include <cmath>
#include <stdexcept>
#include <vector>

std::vector<Vector2d> rectangularProfile ()
{
	// CCW from path-tangent perspective (= when looking along +T, with B = "up",
	// N = "right") :
	//   (0, 0) bottom-left  -> (1, 0) top-left  -> (1, 1) top-right  -> (0, 1) bottom-right
	return { Vector2d(0.0, 0.0), Vector2d(1.0, 0.0),
	         Vector2d(1.0, 1.0), Vector2d(0.0, 1.0) };
}

std::vector<Vector2d> chamferProfile (double depth)
{
	// 5-point profile : the outside-bottom corner of a unit square is replaced
	// by a 45-deg bevel of size `depth` (in normalized [0,1] coords).
	return {
		Vector2d(0.0,   depth),    // outside, just above chamfer
		Vector2d(depth, 0.0),      // bottom, at end of chamfer
		Vector2d(1.0,   0.0),      // inner-bottom corner
		Vector2d(1.0,   1.0),      // inner-top corner
		Vector2d(0.0,   1.0)       // outside-top corner
	};
}

std::vector<Vector2d> cavettoProfile (double depth, int nCurveSegments)
{
	// Concave quarter-circle replacing the outside-bottom corner. The curve is
	// centered at (depth, depth) with radius `depth`, sampled CCW from angle pi
	// (point (0, depth)) to angle 3*pi/2 (point (depth, 0)).
	if (nCurveSegments < 2) nCurveSegments = 2;
	std::vector<Vector2d> profile;
	profile.reserve(nCurveSegments + 4);
	const double PI = 3.14159265358979323846;
	for (int i = 0; i <= nCurveSegments; ++i)
	{
		double t = static_cast<double>(i) / nCurveSegments;
		double angle = PI + t * (PI / 2.0);
		double x = depth + depth * std::cos(angle);
		double y = depth + depth * std::sin(angle);
		profile.push_back(Vector2d(x, y));
	}
	profile.push_back(Vector2d(1.0, 0.0));
	profile.push_back(Vector2d(1.0, 1.0));
	profile.push_back(Vector2d(0.0, 1.0));
	return profile;
}

namespace
{
	// Build the (vertices, faces) buffers of one tube section for `arc`.
	// Vertex layout : index = i * M + j with i = path sample, j = profile point.
	void sweepArcBuffers (const Arc &arc,
	                       const std::vector<Vector2d> &profile,
	                       double scale_u,
	                       double scale_v,
	                       double maxAngleRad,
	                       std::vector<float> &verts,
	                       std::vector<unsigned int> &faces)
	{
		std::vector<Vector2d> pathPos = arc.tessellateAdaptive(maxAngleRad);
		const int N = static_cast<int>(pathPos.size());
		const int M = static_cast<int>(profile.size());

		verts.resize(static_cast<size_t>(3) * N * M);
		for (int i = 0; i < N; ++i)
		{
			const Vector2d &P = pathPos[i];
			const double t = (N == 1) ? 0.0 : static_cast<double>(i) / (N - 1);
			Vector2d T = arc.tangentAt(t);
			const double Nx =  T.y;
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

		faces.reserve(faces.size() + static_cast<size_t>(3) * 2 * (N - 1) * M);
		for (int i = 0; i < N - 1; ++i)
		{
			for (int j = 0; j < M; ++j)
			{
				const int j1 = (j + 1) % M;
				const unsigned int v00 = i       * M + j;
				const unsigned int v10 = (i + 1) * M + j;
				const unsigned int v11 = (i + 1) * M + j1;
				const unsigned int v01 = i       * M + j1;

				faces.push_back(v00);
				faces.push_back(v10);
				faces.push_back(v11);

				faces.push_back(v00);
				faces.push_back(v11);
				faces.push_back(v01);
			}
		}
	}
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

	std::vector<float> verts;
	std::vector<unsigned int> faces;
	sweepArcBuffers(arc, profile, scale_u, scale_v, maxAngleRad, verts, faces);

	if (verts.empty() || faces.empty())
		throw std::invalid_argument("sweepProfileAlongArc: arc tessellation produced empty mesh");

	out.SetVertices(static_cast<unsigned int>(verts.size() / 3), verts.data());
	out.SetFaces(static_cast<unsigned int>(faces.size() / 3), 3, faces.data());
}

void sweepProfileAlongArcs (const std::vector<Arc> &arcs,
                             const std::vector<Vector2d> &profile,
                             double scale_u,
                             double scale_v,
                             Mesh &out,
                             double maxAngleRad)
{
	if (arcs.empty())
		throw std::invalid_argument("sweepProfileAlongArcs: arcs must not be empty");
	if (profile.size() < 3)
		throw std::invalid_argument("sweepProfileAlongArcs: profile must have at least 3 points");

	std::vector<float> totalVerts;
	std::vector<unsigned int> totalFaces;

	for (const Arc &arc : arcs)
	{
		if (arc.length() <= 0.0)
			continue;     // skip degenerate arcs silently

		std::vector<float> sectionVerts;
		std::vector<unsigned int> sectionFaces;
		sweepArcBuffers(arc, profile, scale_u, scale_v, maxAngleRad,
		                sectionVerts, sectionFaces);

		const unsigned int offset = static_cast<unsigned int>(totalVerts.size() / 3);
		totalVerts.insert(totalVerts.end(), sectionVerts.begin(), sectionVerts.end());
		for (unsigned int idx : sectionFaces)
			totalFaces.push_back(idx + offset);
	}

	if (totalVerts.empty() || totalFaces.empty())
		throw std::invalid_argument("sweepProfileAlongArcs: all arcs were degenerate");

	out.SetVertices(static_cast<unsigned int>(totalVerts.size() / 3), totalVerts.data());
	out.SetFaces(static_cast<unsigned int>(totalFaces.size() / 3), 3, totalFaces.data());
}
