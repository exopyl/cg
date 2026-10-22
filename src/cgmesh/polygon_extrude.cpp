#include "polygon_extrude.h"

#include <cstdlib>
#include <stdexcept>
#include <vector>

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

	// Vertex layout : caps and walls get DISTINCT vertices so per-vertex normals
	// stay correct at the hard front/back-to-wall edges. If caps and walls
	// shared vertices, normal averaging would tilt the flat faces near every
	// contour edge (visible shading gradient / facets). Layout :
	//   [0,    N)   : cap top    (z = zTop)    — used only by the top cap
	//   [N,   2N)   : cap bottom (z = zBottom) — used only by the bottom cap
	//   [2N,  3N)   : wall top   (z = zTop)    — used only by side walls
	//   [3N,  4N)   : wall bottom(z = zBottom) — used only by side walls
	// Wall vertices are shared BETWEEN adjacent wall quads of the same contour,
	// so the walls smooth-shade nicely around the curved holes and arcs.
	const unsigned int N = nV;
	const float zT = static_cast<float>(zTop);
	const float zB = static_cast<float>(zBottom);

	std::vector<float> verts(4 * N * 3);
	for (unsigned int i = 0; i < N; ++i)
	{
		const float x = pV[3*i + 0];
		const float y = pV[3*i + 1];
		verts[3*(i      ) + 0] = x; verts[3*(i      ) + 1] = y; verts[3*(i      ) + 2] = zT; // cap top
		verts[3*(i +   N) + 0] = x; verts[3*(i +   N) + 1] = y; verts[3*(i +   N) + 2] = zB; // cap bottom
		verts[3*(i + 2*N) + 0] = x; verts[3*(i + 2*N) + 1] = y; verts[3*(i + 2*N) + 2] = zT; // wall top
		verts[3*(i + 3*N) + 0] = x; verts[3*(i + 3*N) + 1] = y; verts[3*(i + 3*N) + 2] = zB; // wall bottom
	}

	std::vector<unsigned int> faces;
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
	// outward normal points away from the stone region. Uses the dedicated wall
	// vertices (regions 2 = top, 3 = bottom).
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
			const unsigned int p_i_top  = 2*N + contourStart + i;
			const unsigned int p_j_top  = 2*N + contourStart + j;
			const unsigned int p_i_bot  = N + p_i_top;   // region 3 = region 2 + N
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

	out.SetVertices(4 * N, verts.data());
	out.SetFaces(static_cast<unsigned int>(faces.size() / 3), 3, faces.data());

	free(pV);
	free(pF);
}

void appendMesh (Mesh &dest, const Mesh &src)
{
	// Read dest's current vertex/face count.
	Mesh &destNc = const_cast<Mesh &>(dest);  // GetNVertices / GetVertex are non-const
	Mesh &srcNc  = const_cast<Mesh &>(src);

	const unsigned int destNV = destNc.GetNVertices();
	const unsigned int destNF = destNc.GetNFaces();
	const unsigned int srcNV  = srcNc.GetNVertices();
	const unsigned int srcNF  = srcNc.GetNFaces();

	// Build a combined vertex array.
	std::vector<float> verts;
	verts.reserve(3 * (destNV + srcNV));
	for (unsigned int i = 0; i < destNV; ++i)
	{
		float v[3];
		destNc.GetVertex(i, v);
		verts.push_back(v[0]); verts.push_back(v[1]); verts.push_back(v[2]);
	}
	for (unsigned int i = 0; i < srcNV; ++i)
	{
		float v[3];
		srcNc.GetVertex(i, v);
		verts.push_back(v[0]); verts.push_back(v[1]); verts.push_back(v[2]);
	}

	// Build a combined face array, shifting src's indices by destNV.
	std::vector<unsigned int> faces;
	faces.reserve(3 * (destNF + srcNF));
	for (unsigned int i = 0; i < destNF; ++i)
		for (unsigned int j = 0; j < 3; ++j)
			faces.push_back(static_cast<unsigned int>(destNc.GetFaceVertex(i, j)));
	for (unsigned int i = 0; i < srcNF; ++i)
		for (unsigned int j = 0; j < 3; ++j)
			faces.push_back(static_cast<unsigned int>(srcNc.GetFaceVertex(i, j)) + destNV);

	dest.SetVertices(destNV + srcNV, verts.data());
	dest.SetFaces(destNF + srcNF, 3, faces.data());
}
