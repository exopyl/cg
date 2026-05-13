#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <array>
#include <functional>

#include "mesh.h"
#include "../cgmath/cgmath.h"
#include "../cgimg/cgimg.h"
#include "octree.h"

extern "C" {
#include "../../extern/glutess/glutess.h"
}

//
// Face
//
void Face::Init (void)
{
	m_nVertices = 0;
	m_pVertices = nullptr;
	m_bUseTextureCoordinates = false;
	m_pTextureCoordinatesIndices = nullptr;
	m_pTextureCoordinates = nullptr;
	m_iMaterialId = MATERIAL_NONE;
};

Face::Face ()
{
	Init ();
	SetNVertices (3);
}

Face::Face (const Face &f)
{
	Init ();
	SetNVertices(f.m_nVertices);

	m_nVertices = f.m_nVertices;
	memcpy (m_pVertices, f.m_pVertices, f.m_nVertices*sizeof(unsigned int));

	m_bUseTextureCoordinates = f.m_bUseTextureCoordinates;
	if (m_bUseTextureCoordinates)
	{
		m_pTextureCoordinatesIndices = new unsigned int[2*f.m_nVertices];
		memcpy (m_pTextureCoordinatesIndices, f.m_pTextureCoordinatesIndices, 2*m_nVertices*sizeof(float));

		m_pTextureCoordinates = new float[2*m_nVertices];
		memcpy (m_pTextureCoordinates, f.m_pTextureCoordinates, 2*m_nVertices*sizeof(float));
	}
	else
		m_pTextureCoordinates = nullptr;

	m_iMaterialId = f.m_iMaterialId;
}

Face::~Face ()
{
	if (m_pVertices)
		delete[] m_pVertices;
	if (m_pTextureCoordinatesIndices)
		delete[] m_pTextureCoordinatesIndices;
	if (m_pTextureCoordinates)
		delete[] m_pTextureCoordinates;
};

int Face::SetNVertices (unsigned int n)
{
	if (m_nVertices == n)
		return 0;

	m_nVertices = n;
	if (m_pVertices)
		delete[] m_pVertices;
	m_pVertices = new unsigned int[m_nVertices];

	if (m_pTextureCoordinatesIndices)
		delete[] m_pTextureCoordinatesIndices;
	m_pTextureCoordinatesIndices = new unsigned int[2*m_nVertices];

	if (m_pTextureCoordinates)
		delete[] m_pTextureCoordinates;
	m_pTextureCoordinates = new float[2*m_nVertices];

	return 0;
}

void Face::SetTriangle (unsigned int a, unsigned int b, unsigned int c)
{
	SetNVertices (3);
	m_pVertices[0] = a;
	m_pVertices[1] = b;
	m_pVertices[2] = c;
}

void Face::SetQuad (unsigned int a, unsigned int b, unsigned int c, unsigned int d)
{
	SetNVertices (4);
	m_pVertices[0] = a;
	m_pVertices[1] = b;
	m_pVertices[2] = c;
	m_pVertices[3] = d;
}

void Face::dump (void)
{
	printf ("%d vertices stored in %p : ", m_nVertices, (void*)m_pVertices);
	for (unsigned int i=0; i<m_nVertices; i++)
		printf ("%d ", m_pVertices[i]);
	printf ("\n");
}

//
// Mesh
//
void Mesh::Init ()
{
	m_name = std::string("#NoName#");
	m_nVertices = 0;
	m_nFaces = 0;
	m_pVertices.clear();
	m_pVertexNormals.clear();
	m_pVertexColors.clear();
	m_pFaces = nullptr;
	m_pFaceNormals.clear();
	m_pMaterials.clear();
	m_nTextureCoordinates = 0;
	m_pTextureCoordinates.clear();
	m_pTensors.clear();
	m_pOctree = nullptr;
	m_revision = 0;
}

void Mesh::InitVertexColors (float r, float g, float b)
{
	m_pVertexColors.clear();

	if (m_nVertices > 0)
	{
		m_pVertexColors.resize(3*m_nVertices);
		for (int i=0; i<m_nVertices; i++)
		{
			m_pVertexColors[3*i]   = r;
			m_pVertexColors[3*i+1] = g;
			m_pVertexColors[3*i+2] = b;
		}
	}
}

void Mesh::InitVertexColorsFromCurvatures (Tensor::eCurvature curvature)
{
	if (m_pTensors.empty())
		return;

	if (m_nVertices > 0)
	{
		float *array = (float*)malloc(m_nVertices*sizeof(float));
		char *defined = (char*)malloc(m_nVertices*sizeof(char));
		for (int i=0; i<m_nVertices; i++)
		{
			if (m_pTensors[i])
			{
				defined[i] = 1;
				float kappa1 = m_pTensors[i]->GetKappaMax ();
				float kappa2 = m_pTensors[i]->GetKappaMin ();
				switch (curvature)
				{
				case Tensor::CURVATURE_MAX:
					array[i] = kappa1;
					break;
				case Tensor::CURVATURE_MIN:
					array[i] = kappa2;
					break;
				case Tensor::CURVATURE_GAUSSIAN:
					array[i] = kappa1*kappa2;
					break;
				case Tensor::CURVATURE_MEAN:
					array[i] = (kappa1+kappa2)/2.0;
					break;
				default:
					printf ("type unknown\n");
					break;
				}
			}
			else
				defined[i] = 0;
		}

		InitVertexColorsFromArray (array, defined);

		free (defined);
		free (array);
	}
}

void Mesh::InitVertexColorsFromArray (float *array, char *defined)
{
	if (!array)
		return;

	m_pVertexColors.clear();

	if (m_nVertices > 0)
	{
		m_pVertexColors.resize(3*m_nVertices);

		float min = array[0];
		float max = array[0];
		for (int i=1; i<m_nVertices; i++)
		{
			if (defined && !defined[i])
				continue;

			if (min > array[i]) min = array[i];
			if (max < array[i]) max = array[i];
		}
		/*if (fabs(min) > fabs(max))
		{
			min = -fabs(min);
			max =  fabs(min);
		}
		else
		{
			min =  fabs(max);
			max = -fabs(max);
		}*/

		float r, g, b;
		if (min == max)
		{
			//color_jet(0.5, &r, &g, &b);
			InitVertexColors (0., 0., 0.);
		}
		else
		{
			m_nTextureCoordinates = m_nVertices;
			m_pTextureCoordinates.assign(2 * m_nTextureCoordinates, 0.0f);

			float invdenom = 1./(max-min);
			for (int i=0; i<m_nVertices; i++)
			{
				m_pTextureCoordinates[2 * i] = (array[i] - min)*invdenom;
				m_pTextureCoordinates[2 * i + 1] = .5f;

				if (defined && !defined[i])
				{
					m_pVertexColors[3*i]   = 0.;
					m_pVertexColors[3*i+1] = 0.;
					m_pVertexColors[3*i+2] = 0.;
				}
				color_jet((array[i]-min)*invdenom, &m_pVertexColors[3*i], &m_pVertexColors[3*i+1], &m_pVertexColors[3*i+2]);

			}

			
			for (int i = 0; i < m_nFaces; i++)
			{
				Face* pFace = m_pFaces[i];
				pFace->ActivateTextureCoordinatesIndices();
				pFace->InitTexCoord();
			}
		}
	}	
}

void Mesh::InitVertices (unsigned int nVertices)
{
	m_pVertices.clear();
	m_pVertexColors.clear();
	m_pVertexNormals.clear();

	m_nVertices = nVertices;
	if (m_nVertices)
	{
		m_pVertices.assign(3*nVertices, 0.0f);
		m_pVertexNormals.assign(3*nVertices, 0.0f);
		m_pVertexColors.assign(3*nVertices, 0.5f);
	}
}

void Mesh::InitFaces (unsigned int nFaces)
{
	m_pFaces = nullptr;
	m_pFaceNormals.clear();

	m_nFaces = nFaces;
	if (m_nFaces)
	{
		m_pFaces = new Face*[m_nFaces];
		for (int  i=0; i<m_nFaces; i++)
			m_pFaces[i] = new Face ();
		m_pFaceNormals.assign(3*m_nFaces, 0.0f);
	}
}

void Mesh::InitTensors (void)
{
	m_pTensors.clear();
	if (m_nVertices)
		m_pTensors.resize(m_nVertices); // unique_ptr default-constructs to nullptr
}

Tensor* Mesh::GetTensor (unsigned int index)
{
	if (index >= m_pTensors.size()) return nullptr;
	return m_pTensors[index].get();
}

void Mesh::Init (unsigned int nVertices, unsigned int nFaces)
{
	Init ();

	InitVertices (nVertices);
	InitFaces (nFaces);

	m_pMaterials.clear();

	IncrementRevision();
}

Mesh::Mesh () : Geometry()
{
	Init ();
}

Mesh::Mesh (unsigned int nVertices, unsigned int nFaces)
{
	Init (nVertices, nFaces);
}

Mesh::Mesh (Mesh &m)
{
}

void Mesh::DeleteFaces (void)
{
	if (m_pFaces)
	{
		for (unsigned int i=0; i<m_nFaces; i++)
		{
			Face *pFace = m_pFaces[i];
			delete pFace;
		}
		delete[] m_pFaces;
	}
	m_pFaces = nullptr;
	m_nFaces = 0;
}

Mesh::~Mesh ()
{
	// vector + unique_ptr members destruct themselves
	DeleteFaces ();
	if (m_pOctree) delete m_pOctree;
}

void Mesh::Dump ()
{
	printf ("nVertices : %d\n", m_nVertices);
	printf("pVertices : %p\n", (void*)m_pVertices.data());
	printf ("pVertexNormals : %p\n", (void*)m_pVertexNormals.data());
	printf("pVertexColors : %p\n", (void*)m_pVertexColors.data());
	printf ("nFace : %d\n", m_nFaces);
	printf ("pFaces : %p\n", (void*)m_pFaces);
	printf ("pTextureCoordinates : %p\n", (void*)m_pTextureCoordinates.data());
	printf ("nMaterials : %zu\n", m_pMaterials.size());
	for (const auto& mat : m_pMaterials)
		if (mat) mat->Dump();
}

uint64_t Mesh::GetRevision() const
{
	return m_revision;
}

void Mesh::IncrementRevision()
{ 
	m_revision++;
}

unsigned int* Mesh::GetTriangles (void)
{
	unsigned int *pFaces = (unsigned int*)malloc(3*m_nFaces*sizeof(unsigned int));
	for (unsigned int i=0; i<m_nFaces; i++)
	{
		pFaces[3*i]   = m_pFaces[i]->GetVertex(0);
		pFaces[3*i+1] = m_pFaces[i]->GetVertex(1);
		pFaces[3*i+2] = m_pFaces[i]->GetVertex(2);
	}
	return pFaces;
}

// ----- Polygon triangulation ------------------------------------------------
//
// All three Mesh APIs that produce triangles (BuildTriangulation,
// BuildPolygonRenderData, Triangulate) funnel through forEachFaceTriangle()
// below. The helper yields LOCAL-TO-FACE indices (0..N-1) via the caller's
// emit callback; each caller maps those local indices to whatever it needs
// (global vertex indices, expansion slots, fresh Face*).
//
// Convex faces (including all triangles) fan-triangulate from vertex 0;
// concave faces go through extern/glutess. Self-intersecting polygons that
// would require a new combine vertex are flagged and the affected sub-
// triangles are silently dropped (no UINT32_MAX poison reaches the IBO).
//
namespace {

constexpr unsigned int kInvalidLocalIdx = ~0u;

struct TessCtx
{
    // The emit callback is type-erased through std::function so this struct
    // can serve every triangulation entry point without templates inside C
    // callbacks.
    std::function<void(unsigned int, unsigned int, unsigned int)>* emit = nullptr;

    bool         combineHit  = false;        // set if a combine vertex was requested
    unsigned int triLocal[3] = { 0, 0, 0 };  // batched local indices
    int          triCount    = 0;

    // Stable storage for the coordinates handed to gluTessVertex: glutess
    // keeps the pointers alive across the polygon, so the doubles must
    // outlive gluTessEndPolygon.
    std::vector<std::array<GLdouble, 3>> coords;
};

void GLAPIENTRY tessBeginCB(GLenum /*type*/, void* userData)
{
    static_cast<TessCtx*>(userData)->triCount = 0;
}

void GLAPIENTRY tessVertexCB(void* vertexData, void* userData)
{
    auto* ctx = static_cast<TessCtx*>(userData);
    ctx->triLocal[ctx->triCount++] = (unsigned int)(uintptr_t)vertexData;
    if (ctx->triCount == 3)
    {
        const bool poisoned = (ctx->triLocal[0] == kInvalidLocalIdx
                            || ctx->triLocal[1] == kInvalidLocalIdx
                            || ctx->triLocal[2] == kInvalidLocalIdx);
        if (!poisoned && ctx->emit)
            (*ctx->emit)(ctx->triLocal[0], ctx->triLocal[1], ctx->triLocal[2]);
        ctx->triCount = 0;
    }
}

void GLAPIENTRY tessEndCB(void* /*userData*/) {}

void GLAPIENTRY tessEdgeFlagCB(GLboolean /*flag*/, void* /*userData*/)
{
    // Forces glutess to emit GL_TRIANGLES (rather than fans/strips).
}

void GLAPIENTRY tessCombineCB(GLdouble /*coords*/[3], void* /*data*/[4],
                              GLfloat /*weight*/[4], void** outData,
                              void* userData)
{
    // Self-intersection: glutess wants a brand-new vertex we cannot create
    // (we don't extend the mesh on the fly). Mark the context and emit a
    // sentinel local index — the vertex callback above drops any triangle
    // that contains it.
    static_cast<TessCtx*>(userData)->combineHit = true;
    *outData = (void*)(uintptr_t)kInvalidLocalIdx;
}

void GLAPIENTRY tessErrorCB(GLenum errnum, void* /*userData*/)
{
    std::fprintf(stderr, "glutess error during triangulation: 0x%x\n", (unsigned)errnum);
}

GLUtesselator* makeTess()
{
    GLUtesselator* tess = gluNewTess();
    if (!tess) return nullptr;
    gluTessCallback(tess, GLU_TESS_BEGIN_DATA,     (_GLUfuncptr)tessBeginCB);
    gluTessCallback(tess, GLU_TESS_VERTEX_DATA,    (_GLUfuncptr)tessVertexCB);
    gluTessCallback(tess, GLU_TESS_END_DATA,       (_GLUfuncptr)tessEndCB);
    gluTessCallback(tess, GLU_TESS_EDGE_FLAG_DATA, (_GLUfuncptr)tessEdgeFlagCB);
    gluTessCallback(tess, GLU_TESS_COMBINE_DATA,   (_GLUfuncptr)tessCombineCB);
    gluTessCallback(tess, GLU_TESS_ERROR_DATA,     (_GLUfuncptr)tessErrorCB);
    return tess;
}

// True iff the polygon is convex when projected onto its Newell-method
// normal. N<4 is trivially convex. Newell handles non-planar faces robustly.
bool faceIsConvex(Face* face, Mesh& mesh)
{
    const unsigned int n = face->GetNVertices();
    if (n < 4) return true;

    auto vert = [&](unsigned int k) {
        const unsigned int idx = face->GetVertex(k);
        return std::array<double, 3>{
            mesh.m_pVertices[3*idx + 0],
            mesh.m_pVertices[3*idx + 1],
            mesh.m_pVertices[3*idx + 2]
        };
    };

    double nx = 0, ny = 0, nz = 0;
    for (unsigned int i = 0; i < n; ++i)
    {
        auto a = vert(i);
        auto b = vert((i + 1) % n);
        nx += (a[1] - b[1]) * (a[2] + b[2]);
        ny += (a[2] - b[2]) * (a[0] + b[0]);
        nz += (a[0] - b[0]) * (a[1] + b[1]);
    }

    double prevSign = 0;
    for (unsigned int i = 0; i < n; ++i)
    {
        auto a = vert((i + n - 1) % n);
        auto b = vert(i);
        auto c = vert((i + 1) % n);
        const double e1x = b[0] - a[0], e1y = b[1] - a[1], e1z = b[2] - a[2];
        const double e2x = c[0] - b[0], e2y = c[1] - b[1], e2z = c[2] - b[2];
        const double cx = e1y*e2z - e1z*e2y;
        const double cy = e1z*e2x - e1x*e2z;
        const double cz = e1x*e2y - e1y*e2x;
        const double dot = cx*nx + cy*ny + cz*nz;
        if (std::fabs(dot) < 1e-12) continue;
        const double sign = dot > 0 ? 1.0 : -1.0;
        if (prevSign == 0) prevSign = sign;
        else if (sign != prevSign) return false;
    }
    return true;
}

// Iterate the triangles of face m_pFaces[fi], invoking emit(localA, localB,
// localC) for each. Local indices are 0..N-1, addressing positions within
// the face's own vertex list. `tess` is a lazily-allocated, reusable
// tessellator handle (pass &nullptr on first call; caller cleans up).
template <class Emit>
void forEachFaceTriangle(Mesh& mesh, unsigned int fi, GLUtesselator*& tess, Emit&& emit)
{
    Face* face = mesh.m_pFaces[fi];
    if (!face) return;
    const unsigned int n = face->GetNVertices();
    if (n < 3) return;

    if (n == 3)
    {
        emit(0u, 1u, 2u);
        return;
    }

    if (faceIsConvex(face, mesh))
    {
        for (unsigned int k = 1; k + 1 < n; ++k)
            emit(0u, k, k + 1);
        return;
    }

    // Concave: glutess.
    TessCtx ctx;
    std::function<void(unsigned int, unsigned int, unsigned int)> emitFn =
        [&](unsigned int a, unsigned int b, unsigned int c) { emit(a, b, c); };
    ctx.emit = &emitFn;
    ctx.coords.reserve(n);
    for (unsigned int i = 0; i < n; ++i)
    {
        const unsigned int vi = face->GetVertex(i);
        ctx.coords.push_back({
            (GLdouble)mesh.m_pVertices[3*vi + 0],
            (GLdouble)mesh.m_pVertices[3*vi + 1],
            (GLdouble)mesh.m_pVertices[3*vi + 2]
        });
    }

    if (!tess) tess = makeTess();
    if (!tess) return; // gluNewTess failed (allocation)

    gluTessBeginPolygon(tess, &ctx);
    gluTessBeginContour(tess);
    for (unsigned int i = 0; i < n; ++i)
        gluTessVertex(tess, ctx.coords[i].data(), (void*)(uintptr_t)i);
    gluTessEndContour(tess);
    gluTessEndPolygon(tess);

    if (ctx.combineHit)
    {
        std::fprintf(stderr,
            "Triangulation: face %u self-intersects; affected sub-triangles dropped\n",
            fi);
    }
}

} // namespace

std::vector<unsigned int> Mesh::BuildTriangulation()
{
    std::vector<unsigned int> out;
    out.reserve(3 * m_nFaces);

    GLUtesselator* tess = nullptr;
    for (unsigned int fi = 0; fi < m_nFaces; ++fi)
    {
        Face* face = m_pFaces[fi];
        if (!face) continue;
        forEachFaceTriangle(*this, fi, tess,
            [&](unsigned int a, unsigned int b, unsigned int c) {
                out.push_back((unsigned int)face->GetVertex(a));
                out.push_back((unsigned int)face->GetVertex(b));
                out.push_back((unsigned int)face->GetVertex(c));
            });
    }
    if (tess) gluDeleteTess(tess);
    return out;
}

// ----- Polygon render data --------------------------------------------------
//
// BuildPolygonRenderData() expands the mesh into a per-polygon vertex layout:
// each face contributes its own N render-vertices, all carrying the face's
// Newell normal. Adjacent faces no longer share corner vertices, so the
// shading within each polygon is strictly uniform — eliminating the
// triangulation-diagonal kinks that smooth shading over a shared topology
// produces on non-planar n-gons.
//
namespace {

void computeNewellNormal(Face* face, Mesh& mesh, float outN[3])
{
    const unsigned int n = face->GetNVertices();
    double nx = 0, ny = 0, nz = 0;
    for (unsigned int i = 0; i < n; ++i)
    {
        const unsigned int viA = face->GetVertex(i);
        const unsigned int viB = face->GetVertex((i + 1) % n);
        const double ax = mesh.m_pVertices[3*viA + 0];
        const double ay = mesh.m_pVertices[3*viA + 1];
        const double az = mesh.m_pVertices[3*viA + 2];
        const double bx = mesh.m_pVertices[3*viB + 0];
        const double by = mesh.m_pVertices[3*viB + 1];
        const double bz = mesh.m_pVertices[3*viB + 2];
        nx += (ay - by) * (az + bz);
        ny += (az - bz) * (ax + bx);
        nz += (ax - bx) * (ay + by);
    }
    const double len = std::sqrt(nx*nx + ny*ny + nz*nz);
    if (len > 1e-12)
    {
        outN[0] = (float)(nx / len);
        outN[1] = (float)(ny / len);
        outN[2] = (float)(nz / len);
    }
    else
    {
        outN[0] = 0.0f; outN[1] = 0.0f; outN[2] = 1.0f;
    }
}

} // namespace

Mesh::PolygonRenderData Mesh::BuildPolygonRenderData()
{
    PolygonRenderData out;

    const bool hasNormals = !m_pVertexNormals.empty();
    const bool hasUV      = !m_pTextureCoordinates.empty();
    const bool hasColors  = !m_pVertexColors.empty();

    // Seed the output with the shared topology layout. Triangle faces will
    // reference these slots directly (smooth shading preserved). Only
    // N>=4 polygons append fresh slots below.
    out.positions.assign(m_pVertices.begin(),           m_pVertices.end());
    if (hasNormals) out.normals.assign  (m_pVertexNormals.begin(),       m_pVertexNormals.end());
    if (hasUV)      out.texCoords.assign(m_pTextureCoordinates.begin(),  m_pTextureCoordinates.end());
    if (hasColors)  out.colors.assign   (m_pVertexColors.begin(),        m_pVertexColors.end());
    out.indices.reserve(3u * m_nFaces);

    GLUtesselator* tess = nullptr;

    for (unsigned int fi = 0; fi < m_nFaces; ++fi)
    {
        Face* face = m_pFaces[fi];
        if (!face) continue;
        const unsigned int n = face->GetNVertices();
        if (n < 3) continue;

        if (n == 3)
        {
            // Triangle: index directly into the shared topology slots.
            out.indices.push_back((unsigned int)face->GetVertex(0));
            out.indices.push_back((unsigned int)face->GetVertex(1));
            out.indices.push_back((unsigned int)face->GetVertex(2));
            continue;
        }

        // N>=4: append N fresh render-vertices carrying the polygon's
        // Newell normal uniformly.
        float fn[3];
        computeNewellNormal(face, *this, fn);

        const unsigned int base = (unsigned int)(out.positions.size() / 3);

        for (unsigned int i = 0; i < n; ++i)
        {
            const unsigned int vi = face->GetVertex(i);

            out.positions.push_back(m_pVertices[3*vi + 0]);
            out.positions.push_back(m_pVertices[3*vi + 1]);
            out.positions.push_back(m_pVertices[3*vi + 2]);

            if (hasNormals)
            {
                out.normals.push_back(fn[0]);
                out.normals.push_back(fn[1]);
                out.normals.push_back(fn[2]);
            }

            if (hasUV)
            {
                if (2*vi + 1 < m_pTextureCoordinates.size())
                {
                    out.texCoords.push_back(m_pTextureCoordinates[2*vi + 0]);
                    out.texCoords.push_back(m_pTextureCoordinates[2*vi + 1]);
                }
                else
                {
                    out.texCoords.push_back(0.0f);
                    out.texCoords.push_back(0.0f);
                }
            }

            if (hasColors)
            {
                if (3*vi + 2 < m_pVertexColors.size())
                {
                    out.colors.push_back(m_pVertexColors[3*vi + 0]);
                    out.colors.push_back(m_pVertexColors[3*vi + 1]);
                    out.colors.push_back(m_pVertexColors[3*vi + 2]);
                }
                else
                {
                    out.colors.push_back(1.0f);
                    out.colors.push_back(1.0f);
                    out.colors.push_back(1.0f);
                }
            }
        }

        forEachFaceTriangle(*this, fi, tess,
            [&](unsigned int a, unsigned int b, unsigned int c) {
                out.indices.push_back(base + a);
                out.indices.push_back(base + b);
                out.indices.push_back(base + c);
            });
    }

    if (tess) gluDeleteTess(tess);
    return out;
}

// ----- In-place triangulation ----------------------------------------------
//
// Triangulate() walks faces and replaces every N>=4 polygon with (N-2)
// triangle Face objects (fan for convex, glutess for concave). Triangle
// faces are kept as-is. Material ids, face-relative texture-coordinate
// indices (m_pTextureCoordinatesIndices) and inline per-face UV coordinates
// (m_pTextureCoordinates) all propagate to each sub-triangle.
//
void Mesh::Triangulate()
{
    if (m_nFaces == 0) return;

    std::vector<Face*> newFaces;
    newFaces.reserve(m_nFaces);

    // Emit one triangle Face* from local-to-source-face indices a/b/c.
    auto emitTriangle = [&](Face* src, unsigned int a, unsigned int b, unsigned int c) {
        Face* f = new Face();
        f->SetNVertices(3);
        f->SetVertex(0, src->GetVertex(a));
        f->SetVertex(1, src->GetVertex(b));
        f->SetVertex(2, src->GetVertex(c));
        f->SetMaterialId((unsigned int)src->GetMaterialId());

        // Propagate the "this face uses texture coords" flag — the OBJ
        // exporter (mesh_io.cpp) consults it to decide whether to emit
        // 'vt' indices, so the data alone isn't enough.
        f->m_bUseTextureCoordinates = src->m_bUseTextureCoordinates;

        // Propagate face-relative texture-coordinate indices if the source
        // face owns them.
        if (src->m_pTextureCoordinatesIndices)
        {
            f->ActivateTextureCoordinatesIndices();
            f->m_pTextureCoordinatesIndices[0] = src->m_pTextureCoordinatesIndices[a];
            f->m_pTextureCoordinatesIndices[1] = src->m_pTextureCoordinatesIndices[b];
            f->m_pTextureCoordinatesIndices[2] = src->m_pTextureCoordinatesIndices[c];
        }
        // Propagate inline per-face UV coords if the source face owns them.
        if (src->m_pTextureCoordinates)
        {
            f->ActivateTextureCoordinates();
            f->m_pTextureCoordinates[0] = src->m_pTextureCoordinates[2*a + 0];
            f->m_pTextureCoordinates[1] = src->m_pTextureCoordinates[2*a + 1];
            f->m_pTextureCoordinates[2] = src->m_pTextureCoordinates[2*b + 0];
            f->m_pTextureCoordinates[3] = src->m_pTextureCoordinates[2*b + 1];
            f->m_pTextureCoordinates[4] = src->m_pTextureCoordinates[2*c + 0];
            f->m_pTextureCoordinates[5] = src->m_pTextureCoordinates[2*c + 1];
        }

        newFaces.push_back(f);
    };

    GLUtesselator* tess = nullptr;

    for (unsigned int fi = 0; fi < m_nFaces; ++fi)
    {
        Face* face = m_pFaces[fi];
        if (!face) continue;

        const unsigned int n = face->GetNVertices();
        if (n < 3)
        {
            delete face;
            m_pFaces[fi] = nullptr; // avoid dangling pointer in the source array
            continue;
        }

        if (n == 3)
        {
            // Reuse the existing Face — no churn.
            newFaces.push_back(face);
            m_pFaces[fi] = nullptr;
            continue;
        }

        forEachFaceTriangle(*this, fi, tess,
            [&](unsigned int a, unsigned int b, unsigned int c) {
                emitTriangle(face, a, b, c);
            });

        delete face;
        m_pFaces[fi] = nullptr;
    }

    if (tess) gluDeleteTess(tess);

    // Swap the new face array in.
    delete[] m_pFaces;
    m_pFaces = new Face*[newFaces.size()];
    for (size_t i = 0; i < newFaces.size(); ++i)
        m_pFaces[i] = newFaces[i];
    m_nFaces = (unsigned int)newFaces.size();

    ComputeNormals();      // also resizes m_pFaceNormals to 3*m_nFaces
    IncrementRevision();
}

int Mesh::SetVertices (unsigned int nVertices, float *pVertices)
{
	m_nVertices = nVertices;
	m_pVertices.assign(pVertices, pVertices + 3*nVertices);
	return 0;
}

int Mesh::SetVertexNormals(unsigned int nVertexNormals, float* pVertexNormals)
{
	if (nVertexNormals != m_nVertices)
	{
		return -1;
	}

	m_pVertexNormals.assign(pVertexNormals, pVertexNormals + 3*nVertexNormals);

	return 0;

}

int Mesh::SetFaces (unsigned int nFaces, unsigned int nVerticesPerFace, unsigned int *pFaces, unsigned int *pTextureCoordinates)
{
	if (m_pFaces) delete m_pFaces;
	m_pFaces = new Face*[nFaces];
	m_nFaces = nFaces;
	for (unsigned int i=0; i<nFaces; i++)
	{
		Face *pFace = new Face ();
		pFace->SetNVertices (nVerticesPerFace);
		for (unsigned int j=0; j<nVerticesPerFace; j++)
			pFace->m_pVertices[j] = pFaces[nVerticesPerFace*i+j];

		m_pFaces[i] = pFace;
	}
	return 1;
}

int Mesh::SetVertex (unsigned int i, float x, float y, float z)
{
	if (m_pVertices.empty() || i>=m_nVertices)
		return -1;

	m_pVertices[3*i+0] = x;
	m_pVertices[3*i+1] = y;
	m_pVertices[3*i+2] = z;

	return 0;
}

int Mesh::SetFace (unsigned int i,
		   unsigned int a, unsigned int b, unsigned int c)
{
	if (m_pFaces[i])
		delete m_pFaces[i];
	m_pFaces[i] = new Face ();
	m_pFaces[i]->SetNVertices (3);
	m_pFaces[i]->m_pVertices[0] = a;
	m_pFaces[i]->m_pVertices[1] = b;
	m_pFaces[i]->m_pVertices[2] = c;

	return 0;
}

int Mesh::SetFace (unsigned int i,
		   unsigned int a, unsigned int b, unsigned int c, unsigned int d)
{
	if (m_pFaces[i])
		delete m_pFaces[i];
	m_pFaces[i] = new Face ();
	m_pFaces[i]->SetQuad (a, b, c, d);
	return 0;
}


int Mesh::computebbox (void)
{
	/*
	for (int i = 0; i < m_nFaces; i++)
	{
		auto pFace = m_pFaces[i];
		for (int j = 0; j < pFace->GetNVertices(); j++)
		{
			int vindex = pFace->GetVertex(j);
			m_bbox.AddPoint(m_pVertices[3 * vindex], m_pVertices[3 * vindex + 1], m_pVertices[3 * vindex + 2]);
		}
	}
	*/

	for (int i = 0; i < m_nVertices; i++)
		m_bbox.AddPoint(m_pVertices[3 * i], m_pVertices[3 * i + 1], m_pVertices[3 * i + 2]);
	
	return 0;
}

const BoundingBox& Mesh::bbox() const
{
	return m_bbox;
}

float Mesh::bbox_diagonal_length(void) const
{
	return m_bbox.GetDiagonalLength();
}

float Mesh::GetLargestLength(void) const
{
	return m_bbox.GetLargestLength();
}

//
// area
//
float Mesh::GetFaceArea (unsigned int fi)
{
	unsigned int vi1 = 3*m_pFaces[fi]->m_pVertices[0];
	unsigned int vi2 = 3*m_pFaces[fi]->m_pVertices[1];
	unsigned int vi3 = 3*m_pFaces[fi]->m_pVertices[2];
	vec3 v1, v2, v3;

	vec3_init (v1, m_pVertices[vi1], m_pVertices[vi1+1], m_pVertices[vi1+2]);
	vec3_init (v2, m_pVertices[vi2], m_pVertices[vi2+1], m_pVertices[vi2+2]);
	vec3_init (v3, m_pVertices[vi3], m_pVertices[vi3+1], m_pVertices[vi3+2]);
	
	return vec3_triangle_area (v1, v2, v3);
}

float Mesh::GetArea (void)
{
	float area = 0.;
	for (unsigned int i=0; i<m_nFaces; i++)
		area += GetFaceArea(i);
	return area;
}

float* Mesh::GetAreas (void)
{
	float *areas = (float*)malloc(m_nFaces*sizeof(float));
	for (unsigned int i=0; i<m_nFaces; i++)
		areas[i] = GetFaceArea(i);
	return areas;
}

float* Mesh::GetCumulativeAreas (void)
{
	float *areas = GetAreas ();
	for (unsigned int i=1; i<m_nFaces; i++)
		areas[i] += areas[i-1];
	return areas;
}

int Mesh::stats_vertices_in_faces (int *verticesinfaces, int n)
{	
	memset (verticesinfaces, 0, n*sizeof(int));
	for (int i=0; i<m_nFaces; i++)
	{
		Face *f = m_pFaces[i];
		if (f == nullptr)
			continue;
		if (f->GetNVertices() >= n)
			(verticesinfaces[n-1])++;
		else
			(verticesinfaces[f->GetNVertices()])++;
	}

	return 0;
}

unsigned int Mesh::GetNVertices() const
{
	return m_nVertices;
}

unsigned int Mesh::GetNFaces() const
{
	return m_nFaces;
}

bool Mesh::IsTriangleMesh() const
{
	for (int i = 0; i < m_nFaces; i++)
	{
		Face* pFace = m_pFaces[i];
		if (pFace->GetNVertices() != 3)
			return false;
	}
	return true;
}

//
//
//
void Mesh::ComputeNormals (void)
{
	m_pVertexNormals.assign(3*m_nVertices, 0.0f);
	m_pFaceNormals.assign(3*m_nFaces, 0.0f);
	

	//
	int *nfaces = new int[m_nVertices];
	memset (nfaces, 0, m_nVertices*sizeof(int));
	
	for (int i=0; i<m_nFaces; i++)
	{
		Face *pFace = m_pFaces[i];
		int k1 = pFace->m_pVertices[0];
		int k2 = pFace->m_pVertices[1];
		int k3 = pFace->m_pVertices[2];
		vec3 p1p2, p1p3, n;
		vec3_init (p1p2,
			   m_pVertices[3*k2]   - m_pVertices[3*k1],
			   m_pVertices[3*k2+1] - m_pVertices[3*k1+1],
			   m_pVertices[3*k2+2] - m_pVertices[3*k1+2]);
		vec3_init (p1p3,
			   m_pVertices[3*k3]   - m_pVertices[3*k1],
			   m_pVertices[3*k3+1] - m_pVertices[3*k1+1],
			   m_pVertices[3*k3+2] - m_pVertices[3*k1+2]);
		vec3_cross_product (n, p1p2, p1p3);
		vec3_normalize (n);
		m_pFaceNormals[3*i]   = n[0];
		m_pFaceNormals[3*i+1] = n[1];
		m_pFaceNormals[3*i+2] = n[2];
		
		for (int j=0; j<pFace->m_nVertices; j++)
		{
			int k = pFace->m_pVertices[j];
			
			m_pVertexNormals[3*k]   += m_pFaceNormals[3*i];
			m_pVertexNormals[3*k+1] += m_pFaceNormals[3*i+1];
			m_pVertexNormals[3*k+2] += m_pFaceNormals[3*i+2];
			
			nfaces[k]++;
		}
	}

	for (int i=0; i<m_nVertices; i++)
	{
		m_pVertexNormals[3*i]   /= nfaces[i];
		m_pVertexNormals[3*i+1] /= nfaces[i];
		m_pVertexNormals[3*i+2] /= nfaces[i];
	}

	// cleaning
	delete[] nfaces;
}

unsigned int Mesh::CountEdges (void)
{
	std::set<std::pair<unsigned int, unsigned int>> edges;
	for (unsigned int f = 0; f < m_nFaces; f++)
	{
		unsigned int nv = m_pFaces[f]->GetNVertices();
		for (unsigned int e = 0; e < nv; e++)
		{
			unsigned int a = m_pFaces[f]->GetVertex(e);
			unsigned int b = m_pFaces[f]->GetVertex((e + 1) % nv);
			if (a > b) std::swap(a, b);
			edges.insert({a, b});
		}
	}
	return (unsigned int)edges.size();
}

int Mesh::Append (Mesh *m)
{
	if (!m)
		return 0;

	// vertices
	unsigned int res_nv = m_nVertices + m->m_nVertices;
	m_pVertices.insert(m_pVertices.end(),
	                   m->m_pVertices.begin(),
	                   m->m_pVertices.begin() + 3 * m->m_nVertices);

	// faces
	unsigned int res_nf = m_nFaces + m->m_nFaces;
	Face **res_f = new Face*[res_nf];

	for (unsigned int i=0; i<m_nFaces; i++)
		res_f[i] = m_pFaces[i];

	const unsigned int matOffset = (unsigned int)m_pMaterials.size();

	for (unsigned int i=0; i<m->m_nFaces; i++)
	{
		res_f[m_nFaces+i] = new Face (*m->m_pFaces[i]);
		for (unsigned int j=0; j<res_f[m_nFaces+i]->m_nVertices; j++)
			res_f[m_nFaces+i]->m_pVertices[j] += m_nVertices;
		res_f[m_nFaces+i]->SetMaterialId(matOffset + m->m_pFaces[i]->GetMaterialId());
	}

	// Materials: transfer ownership from `m` to `this`. After Append, `m`'s
	// material array is emptied (it no longer owns them). This is a
	// behavior change from the previous raw-pointer code which shared
	// pointers between the two meshes — a guaranteed double-free at
	// destruction. Append currently has no callers so the swap is safe.
	for (auto& mat : m->m_pMaterials)
		m_pMaterials.push_back(std::move(mat));
	m->m_pMaterials.clear();

	m_nVertices = res_nv;

	m_nFaces = res_nf;
	m_pFaces = res_f;

	return 1;
}

//
// edit
//
int Mesh::DeleteVertices (funcptr_v func)
{
	unsigned int nVertices = 0;
	for (unsigned int i=0; i<m_nVertices; i++)
	{
		float x = m_pVertices[3*i];
		float y = m_pVertices[3*i+1];
		float z = m_pVertices[3*i+2];
		if (!func (x, y, z))
		{
			m_pVertices[3*nVertices]   = m_pVertices[3*i];
			m_pVertices[3*nVertices+1] = m_pVertices[3*i+1];
			m_pVertices[3*nVertices+2] = m_pVertices[3*i+2];

			m_pVertexNormals[3*nVertices]   = m_pVertexNormals[3*i];
			m_pVertexNormals[3*nVertices+1] = m_pVertexNormals[3*i+1];
			m_pVertexNormals[3*nVertices+2] = m_pVertexNormals[3*i+2];
			if (!m_pVertexColors.empty())
			{
				m_pVertexColors[3*nVertices]   = m_pVertexColors[3*i];
				m_pVertexColors[3*nVertices+1] = m_pVertexColors[3*i+1];
				m_pVertexColors[3*nVertices+2] = m_pVertexColors[3*i+2];
			}

			nVertices++;
		}
	}
	m_nVertices = nVertices;
	return 0;
}

/*
bool Mesh::ColorizeVerticesDensity_Traverse (Octree &o, void *_data)
{
	if (o.m_nIndices)
	{
		float *data = (float*)_data;
		vec3 v0, v;
		vec3_init (v0, data[0], data[1], data[2]);
		float k = data[3];
		for (int i=0; i<o.m_nIndices; i++)
		{
			GetVertex (i, v);
			if (vec3_distance (v0, v) < k)
				data[4+i]++;
		}
		
	}
	return true;
}
*/
// Treatments on vertices
int Mesh::ColorizeVerticesDensity (float k)
{
	if (m_pVertexColors.empty())
		InitVertexColors (m_nVertices);

	unsigned int *neighbours = (unsigned int*)malloc(m_nVertices*sizeof(unsigned int));
	//unsigned int neighbours[m_nVertices];
	memset (neighbours, 0, m_nVertices*sizeof(unsigned int));
/*
	vec3 v, vtmp;
	for (int i=0; i<m_nVertices; i++)
	{
		printf ("%d / %d\n", i, m_nVertices);
		GetVertex (i, v);
		for (int j=0; j<m_nVertices; j++)
		{
			GetVertex (j, vtmp);
			if (vec3_distance (v, vtmp) < k)
				neighbours[i]++;
		}
	}
*/

	Octree *pOctree = new Octree ();
	pOctree->Build (m_pVertices.data(), m_nVertices, 200, 20);
	vec3 pt;
	for (int i=0; i<m_nVertices; i++)
	{
		//printf ("%d %d\n", i, m_nVertices);
		GetVertex (i, pt);
		neighbours[i] = pOctree->GetKNeighbours (pt, k);
	}

	unsigned int max = 0;
	for (int i=0; i<m_nVertices; i++)
		if (neighbours[i] > max)
			max = neighbours[i];

	for (int i=0; i<m_nVertices; i++)
	{

		color_jet ((float)neighbours[i]/max,
			   &m_pVertexColors[3*i], &m_pVertexColors[3*i+1], &m_pVertexColors[3*i+2]);
		
		//m_pVertexColors[3*i]   = (float)neighbours[i]/max;
		//m_pVertexColors[3*i+1] = (float)neighbours[i]/max;
		//m_pVertexColors[3*i+2] = (float)neighbours[i]/max;
	}

	// cleaning
	delete pOctree;
	free (neighbours);

	return 0;
}

// noise
void Mesh::add_gaussian_noise (float variance)
{
	for (int i=0; i<m_nVertices; i++)
	{
		static long idum = -247;
		vec3 disp;
		vec3_init (disp, gasdev(&idum), gasdev(&idum), gasdev(&idum));
		m_pVertices[3*i]   += variance*disp[0];
		m_pVertices[3*i+1] += variance*disp[1];
		m_pVertices[3*i+2] += variance*disp[2];
	}
	IncrementRevision();
}

// topology
void Mesh::GetTopologicIssues(vector<unsigned int>& nonManifoldEdges, vector<unsigned int>& borders) const
{
	nonManifoldEdges.clear();
	borders.clear();

	std::map<unsigned int, std::map<unsigned int, unsigned int>> occurences;
	for (int i = 0; i < m_nFaces; i++)
	{
		Face* pFace = m_pFaces[i];
		unsigned int nVertices = pFace->GetNVertices();
		for (int j = 0; j < nVertices; j++)
		{
			unsigned int v1 = pFace->GetVertex(j);
			unsigned int v2 = pFace->GetVertex((j + 1) % nVertices);
			if (v1 > v2) // v1 should be the smallest index
			{
				unsigned int tmp = v1;
				v1 = v2;
				v2 = tmp;
			}
			auto it = occurences.find(v1);
			if (it == occurences.end())
			{
				std::map<unsigned int, unsigned int> newOccurence;
				newOccurence.insert(std::pair<unsigned int, unsigned int>(v2, 1));
				occurences.insert(std::pair<unsigned int, std::map<unsigned int, unsigned int>>(v1, newOccurence));
			}
			else
			{
				auto it2 = it->second.find(v2);
				if (it2 != it->second.end())
				{
					//if (std::find(it->second.begin(), it->second.end(), v2) != it->second.end())
					it2->second++;
				}
				else
				{
					it->second.insert(std::pair<unsigned int, unsigned int>(v2, 1));
				}
			}
		}
	}

	for (auto occurence : occurences)
	{
		auto v1 = occurence.first;
		for (auto links : occurence.second)
		{
			auto v2 = links.first;
			auto n = links.second;
			if (n == 1)
			{
				borders.push_back(v1);
				borders.push_back(v2);
			}
			else if (n >= 3)
			{
				nonManifoldEdges.push_back(v1);
				nonManifoldEdges.push_back(v2);
			}
		}
	}

	occurences.clear();
}

//
// from class Geometry
//
bool Mesh::GetIntersectionBboxWithRay (vec3 o, vec3 d)
{
	float bbox_min[3];
	float bbox_max[3];
	m_bbox.GetMinMax(bbox_min, bbox_max);
	AABox *m_pAABox = new AABox (bbox_min[0], bbox_min[1], bbox_min[2]);
	if (m_pAABox)
	{
		m_pAABox->AddVertex (bbox_max[0], bbox_max[1], bbox_max[2]);
		Ray r (Vector3 (o[0], o[1], o[2]), Vector3 (d[0], d[1], d[2]));
		return m_pAABox->intersection (r, 0., 100.);
	}
	else
		return true;
}

int Mesh::GetIntersectionWithRayInOctree (vec3 vOrig, vec3 vDirection, float *_t, vec3 vIntersection, vec3 vNormal, Octree *pOctree)
{
	if (pOctree->IsLeaf ())
	{
		unsigned int nTriangles = pOctree->GetNTriangles();
		unsigned int *pTriangles = pOctree->GetTriangles();
		float fTCurrent, fT = -1.;
		Triangle tri;
		for (unsigned int i=0; i<nTriangles; i++)
		{
			float vIntersectionCurrent[3], vNormalCurrent[3];

			unsigned int vi1, vi2, vi3;
			vi1 = 3*pTriangles[3*i];
			vi2 = 3*pTriangles[3*i+1];
			vi3 = 3*pTriangles[3*i+2];
			tri.Init (m_pVertices[vi1], m_pVertices[vi1+1], m_pVertices[vi1+2],
						m_pVertices[vi2], m_pVertices[vi2+1], m_pVertices[vi2+2],
						m_pVertices[vi3], m_pVertices[vi3+1], m_pVertices[vi3+2]);
			if (tri.GetIntersectionWithRay (vOrig, vDirection, &fTCurrent, vIntersectionCurrent, vNormalCurrent))
			{
				if (fT < 0. || fTCurrent < fT)
				{
					fT = fTCurrent;
					vec3_copy (vIntersection, vIntersectionCurrent);
					vec3_copy (vNormal, vNormalCurrent);
				}
			}
		}

		*_t = fT;
		return (fT < 0.)? 0 : 1;
	}
	else
	{
		Ray ray (vOrig[0], vOrig[1], vOrig[2], vDirection[0], vDirection[1], vDirection[2]);
		float fTCurrent, fT = -1.;
		for (int i=0; i<8; i++)
		{
			Octree *pChild = pOctree->GetChildren()[i];
			if (!pChild)
				continue;
			AABox AABox(pChild->m_vecMin[0], pChild->m_vecMin[1], pChild->m_vecMin[2]);
			AABox.AddVertex (pChild->m_vecMax[0], pChild->m_vecMax[1], pChild->m_vecMax[2]);
			if (AABox.intersection (ray, 0., 10000.))
			{
				vec3 vIntersectionCurrent, vNormalCurrent; 
				if (GetIntersectionWithRayInOctree (vOrig, vDirection, &fTCurrent, vIntersectionCurrent, vNormalCurrent, pChild))
				{
					if (fT < 0. || fTCurrent < fT)
					{
						fT = fTCurrent;
						vec3_copy (vIntersection, vIntersectionCurrent);
						vec3_copy (vNormal, vNormalCurrent);
					}
				}
			}
		}
		*_t = fT;
		return (fT < 0.)? 0 : 1;
	}
}

int Mesh::GetIntersectionWithRay (vec3 vOrig, vec3 vDirection, float *_t, vec3 vIntersection, vec3 vNormal)
{
	/*
	// test the bbox
	bool bGotBBox = GetIntersectionBboxWithRay (vOrig, vDirection);
	if (bGotBBox == false)
		return 0;
	*/

	if (1 && !m_pOctree)
	{
		m_pOctree = new Octree ();
		m_pOctree->BuildForTriangles (m_pVertices.data(), m_nVertices,
									  100, // maxTriangles
									  5,  // maxDepth
								      GetTriangles (), m_nFaces);
	}

	if (m_pOctree)
	{
		return GetIntersectionWithRayInOctree (vOrig, vDirection, _t, vIntersection, vNormal, m_pOctree);
	}
	else // test alls the triangles
	{
		float fTCurrent, fT = -1.;
		Triangle *pTri = new Triangle ();
		for (unsigned int i=0; i<m_nFaces; i++)
		{
			float vIntersectionCurrent[3], vNormalCurrent[3];

			unsigned int vi1, vi2, vi3;
			vi1 = 3*m_pFaces[i]->GetVertex (0);
			vi2 = 3*m_pFaces[i]->GetVertex (1);
			vi3 = 3*m_pFaces[i]->GetVertex (2);
			pTri->Init (m_pVertices[vi1], m_pVertices[vi1+1], m_pVertices[vi1+2],
						m_pVertices[vi2], m_pVertices[vi2+1], m_pVertices[vi2+2],
						m_pVertices[vi3], m_pVertices[vi3+1], m_pVertices[vi3+2]);
			if (pTri->GetIntersectionWithRay (vOrig, vDirection, &fTCurrent, vIntersectionCurrent, vNormalCurrent))
			{
				if (fT < 0. || fTCurrent < fT)
				{
					fT = fTCurrent;
					vec3_copy (vIntersection, vIntersectionCurrent);
					vec3_copy (vNormal, vNormalCurrent);
				}
			}
		}
		delete pTri;
		*_t = fT;
		return (fT < 0.)? 0 : 1;
	}
}

int Mesh::GetIntersectionWithSegment (vec3 vStart, vec3 vEnd, float *_t, vec3 i, vec3 n)
{
	return 0;
}

void* Mesh::GetMaterial (void)
{
	if (m_pMaterials.empty())
	{
		// add a default material
		MaterialColorExt *pMaterial;
		pMaterial = new MaterialColorExt();
		pMaterial->Init_From_Library (MaterialColorExt::EMERALD);
		unsigned int id = Material_Add (pMaterial);
		ApplyMaterial (id);
	}
	return GetMaterial (0);
}

//
// MergeVertices
// Merge vertices that are closer than the given tolerance.
// Face indices are remapped accordingly. Degenerate faces are removed.
//
// Implementation : spatial hash grid of cell size = tolerance. Each vertex is
// checked only against vertices in the 3x3x3 = 27 neighbouring cells, giving
// average-case O(N) time instead of the naive O(N^2).
//
// If tolerance <= 0, the function only merges exact duplicates (cell size set
// to a tiny epsilon so each unique coord triple goes in its own cell).
//
int Mesh::MergeVertices (float tolerance)
{
	if (m_nVertices == 0)
		return 0;

	// Per-vertex attribute arrays are merged in sync with positions IF they
	// are sized 2*nVertices (UV) or 3*nVertices (normals/colors) — i.e. they
	// are indexed by vertex index, parallel to m_pVertices. Otherwise they
	// belong to a face-indexed model (typical OBJ flow) and we leave them
	// untouched.
	const bool uvParallel    = !m_pTextureCoordinates.empty()
	                        && m_pTextureCoordinates.size() == 2u * m_nVertices;
	const bool normParallel  = !m_pVertexNormals.empty()
	                        && m_pVertexNormals.size()      == 3u * m_nVertices;
	const bool colorParallel = !m_pVertexColors.empty()
	                        && m_pVertexColors.size()       == 3u * m_nVertices;

	unsigned int *remap = new unsigned int[m_nVertices];
	float *newVertices = new float[3 * m_nVertices];
	// For each new slot, remember the original vertex index that filled it
	// (the "winner" of the merge group). Used to look up the winner's
	// attributes during seam-aware matching and during rebuild below.
	std::vector<unsigned int> newOrig;
	newOrig.reserve(m_nVertices);

	unsigned int nNewVertices = 0;
	const float tol  = std::fabs(tolerance);
	const float tol2 = tol * tol;
	const float uvTol2    = tol2;            // UVs share the position tolerance
	const float colorTol2 = tol2;            // ditto for colors
	const float normalCosMin = 0.9999f;      // ~0.81° angular tolerance

	// Cell size : tolerance (or a tiny epsilon when tolerance == 0 so we still
	// catch exact duplicates without exploding the grid).
	const float cell = (tol > 0.0f) ? tol : 1e-12f;
	const float invCell = 1.0f / cell;

	struct CellKey { int x, y, z; };
	struct CellKeyHash {
		size_t operator() (const CellKey &k) const noexcept {
			uint64_t h = (uint64_t)(uint32_t)k.x;
			h ^= ((uint64_t)(uint32_t)k.y + 0x9E3779B97F4A7C15ULL + (h<<6) + (h>>2));
			h ^= ((uint64_t)(uint32_t)k.z + 0x9E3779B97F4A7C15ULL + (h<<6) + (h>>2));
			return (size_t)h;
		}
	};
	struct CellKeyEq {
		bool operator() (const CellKey &a, const CellKey &b) const noexcept {
			return a.x == b.x && a.y == b.y && a.z == b.z;
		}
	};
	std::unordered_map<CellKey, std::vector<unsigned int>, CellKeyHash, CellKeyEq> grid;
	grid.reserve(m_nVertices);

	for (unsigned int i = 0; i < m_nVertices; i++)
	{
		const float xi = m_pVertices[3 * i];
		const float yi = m_pVertices[3 * i + 1];
		const float zi = m_pVertices[3 * i + 2];

		const int cx = (int)std::floor(xi * invCell);
		const int cy = (int)std::floor(yi * invCell);
		const int cz = (int)std::floor(zi * invCell);

		bool found = false;
		unsigned int hit = 0;
		for (int dz = -1; dz <= 1 && !found; ++dz)
		for (int dy = -1; dy <= 1 && !found; ++dy)
		for (int dx = -1; dx <= 1 && !found; ++dx)
		{
			CellKey k { cx + dx, cy + dy, cz + dz };
			auto it = grid.find(k);
			if (it == grid.end()) continue;
			for (unsigned int j : it->second)
			{
				// Position check (always)
				const float ex = newVertices[3 * j]     - xi;
				const float ey = newVertices[3 * j + 1] - yi;
				const float ez = newVertices[3 * j + 2] - zi;
				if (ex * ex + ey * ey + ez * ez > tol2)
					continue;

				// Attribute checks against the winner (newOrig[j]) — required
				// to preserve UV seams / normal creases / colour islands.
				const unsigned int jOrig = newOrig[j];

				if (uvParallel)
				{
					const float du = m_pTextureCoordinates[2 * jOrig    ] - m_pTextureCoordinates[2 * i    ];
					const float dv = m_pTextureCoordinates[2 * jOrig + 1] - m_pTextureCoordinates[2 * i + 1];
					if (du * du + dv * dv > uvTol2) continue;
				}
				if (normParallel)
				{
					const float dotN = m_pVertexNormals[3 * jOrig    ] * m_pVertexNormals[3 * i    ]
					                 + m_pVertexNormals[3 * jOrig + 1] * m_pVertexNormals[3 * i + 1]
					                 + m_pVertexNormals[3 * jOrig + 2] * m_pVertexNormals[3 * i + 2];
					if (dotN < normalCosMin) continue;
				}
				if (colorParallel)
				{
					const float dr = m_pVertexColors[3 * jOrig    ] - m_pVertexColors[3 * i    ];
					const float dg = m_pVertexColors[3 * jOrig + 1] - m_pVertexColors[3 * i + 1];
					const float db = m_pVertexColors[3 * jOrig + 2] - m_pVertexColors[3 * i + 2];
					if (dr * dr + dg * dg + db * db > colorTol2) continue;
				}

				hit = j;
				found = true;
				break;
			}
		}

		if (found)
		{
			remap[i] = hit;
		}
		else
		{
			newVertices[3 * nNewVertices]     = xi;
			newVertices[3 * nNewVertices + 1] = yi;
			newVertices[3 * nNewVertices + 2] = zi;
			remap[i] = nNewVertices;
			newOrig.push_back(i);
			grid[CellKey{cx, cy, cz}].push_back(nNewVertices);
			nNewVertices++;
		}
	}

	// Replace vertex array
	m_pVertices.assign(newVertices, newVertices + 3 * nNewVertices);
	delete[] newVertices;

	// Rebuild parallel attribute arrays using the merge winners. Each new
	// slot ni copies attributes from newOrig[ni] in the source mesh, which
	// is the vertex that first filled that slot during the matching pass.

	if (uvParallel)
	{
		std::vector<float> newUVs(2 * nNewVertices);
		for (unsigned int ni = 0; ni < nNewVertices; ++ni)
		{
			const unsigned int orig = newOrig[ni];
			newUVs[2 * ni    ] = m_pTextureCoordinates[2 * orig    ];
			newUVs[2 * ni + 1] = m_pTextureCoordinates[2 * orig + 1];
		}
		m_pTextureCoordinates = std::move(newUVs);
		m_nTextureCoordinates = nNewVertices;
	}

	if (normParallel)
	{
		std::vector<float> newNormals(3 * nNewVertices);
		for (unsigned int ni = 0; ni < nNewVertices; ++ni)
		{
			const unsigned int orig = newOrig[ni];
			newNormals[3 * ni    ] = m_pVertexNormals[3 * orig    ];
			newNormals[3 * ni + 1] = m_pVertexNormals[3 * orig + 1];
			newNormals[3 * ni + 2] = m_pVertexNormals[3 * orig + 2];
		}
		m_pVertexNormals = std::move(newNormals);
	}
	else if (!m_pVertexNormals.empty())
	{
		// Best-effort: shrink to new size. Caller typically follows with
		// ComputeNormals() which fully overwrites this.
		m_pVertexNormals.assign(3 * nNewVertices, 0.0f);
	}

	if (colorParallel)
	{
		std::vector<float> newColors(3 * nNewVertices);
		for (unsigned int ni = 0; ni < nNewVertices; ++ni)
		{
			const unsigned int orig = newOrig[ni];
			newColors[3 * ni    ] = m_pVertexColors[3 * orig    ];
			newColors[3 * ni + 1] = m_pVertexColors[3 * orig + 1];
			newColors[3 * ni + 2] = m_pVertexColors[3 * orig + 2];
		}
		m_pVertexColors = std::move(newColors);
	}
	else if (!m_pVertexColors.empty())
	{
		// Size mismatch case: try a best-effort remap and clamp.
		std::vector<float> newColors(3 * nNewVertices, 0.0f);
		const size_t srcSize = m_pVertexColors.size();
		for (unsigned int i = 0; i < m_nVertices; i++)
		{
			if (3u * i + 2 >= srcSize) continue;
			unsigned int ni = remap[i];
			newColors[3 * ni    ] = m_pVertexColors[3 * i    ];
			newColors[3 * ni + 1] = m_pVertexColors[3 * i + 1];
			newColors[3 * ni + 2] = m_pVertexColors[3 * i + 2];
		}
		m_pVertexColors = std::move(newColors);
	}

	unsigned int nOldVertices = m_nVertices;
	m_nVertices = nNewVertices;

	// Remap face indices and remove degenerate faces
	unsigned int nNewFaces = 0;
	for (unsigned int i = 0; i < m_nFaces; i++)
	{
		Face *f = m_pFaces[i];
		for (unsigned int v = 0; v < f->m_nVertices; v++)
			f->m_pVertices[v] = remap[f->m_pVertices[v]];

		// Check for degenerate face (duplicate vertex indices)
		bool degenerate = false;
		for (unsigned int a = 0; a < f->m_nVertices && !degenerate; a++)
			for (unsigned int b = a + 1; b < f->m_nVertices && !degenerate; b++)
				if (f->m_pVertices[a] == f->m_pVertices[b])
					degenerate = true;

		if (!degenerate)
		{
			m_pFaces[nNewFaces] = m_pFaces[i];
			nNewFaces++;
		}
		else
		{
			delete m_pFaces[i];
		}
	}
	m_nFaces = nNewFaces;

	delete[] remap;

	IncrementRevision();

	return (int)(nOldVertices - nNewVertices);
}

