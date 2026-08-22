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
#include "mesh_raycast.h"

extern "C" {
#include "../../extern/glutess/glutess.h"
}

//
// Mesh
//
void Mesh::Init ()
{
	m_name = std::string("#NoName#");
	m_nVertices = 0;
	m_pVertices.clear();
	m_vertexNormals.clear();
	m_vertexColors.clear();
	DeleteFaces ();
	m_faceNormals.clear();
	m_materials.clear();
	m_nTexCoords = 0;
	m_texCoords.clear();
	m_lines.clear();
	m_points.clear();
	m_tensors.clear();
	m_revision = 0;
	m_tensorsRevision = (uint64_t)-1;
}

void Mesh::InitVertexColors (float r, float g, float b)
{
	m_vertexColors.clear();

	if (m_nVertices > 0)
	{
		m_vertexColors.resize(3*m_nVertices);
		for (int i=0; i<m_nVertices; i++)
		{
			m_vertexColors[3*i]   = r;
			m_vertexColors[3*i+1] = g;
			m_vertexColors[3*i+2] = b;
		}
	}
}

void Mesh::InitVertexColorsFromCurvatures (CurvatureType curvature)
{
	if (m_tensors.empty() || m_tensors.size() < m_nVertices)
		return;

	if (!AreTensorsValid())
		printf ("Mesh::InitVertexColorsFromCurvatures: warning, curvature tensors "
		        "are stale (geometry changed since they were computed)\n");

	if (m_nVertices > 0)
	{
		float *array = (float*)malloc(m_nVertices*sizeof(float));
		char *defined = (char*)malloc(m_nVertices*sizeof(char));
		for (int i=0; i<m_nVertices; i++)
		{
			if (m_tensors[i])
			{
				defined[i] = 1;
				array[i] = m_tensors[i]->GetCurvature (curvature);
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

	m_vertexColors.clear();

	if (m_nVertices > 0)
	{
		m_vertexColors.resize(3*m_nVertices);

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
			m_nTexCoords = m_nVertices;
			m_texCoords.assign(2 * m_nTexCoords, 0.0f);

			float invdenom = 1./(max-min);
			for (int i=0; i<m_nVertices; i++)
			{
				m_texCoords[2 * i] = (array[i] - min)*invdenom;
				m_texCoords[2 * i + 1] = .5f;

				if (defined && !defined[i])
				{
					m_vertexColors[3*i]   = 0.;
					m_vertexColors[3*i+1] = 0.;
					m_vertexColors[3*i+2] = 0.;
				}
				color_jet((array[i]-min)*invdenom, &m_vertexColors[3*i], &m_vertexColors[3*i+1], &m_vertexColors[3*i+2]);

			}

			
			for (int i = 0; i < GetNFaces (); i++)
			{
				auto pFace = FaceAt (i);
				pFace->ActivateTextureCoordinatesIndices();
				pFace->InitTexCoord();
			}
		}
	}	
}

void Mesh::InitVertices (unsigned int nVertices)
{
	m_pVertices.clear();
	m_vertexColors.clear();
	m_vertexNormals.clear();

	m_nVertices = nVertices;
	if (m_nVertices)
	{
		m_pVertices.assign(3*nVertices, 0.0f);
		m_vertexNormals.assign(3*nVertices, 0.0f);
		m_vertexColors.assign(3*nVertices, 0.5f);
	}

	IncrementRevision ();
}

void Mesh::InitFaces (unsigned int nFaces)
{
	// Arite UNIFORME a 3, sur laquelle les appelants s'appuient : l'importateur
	// OBJ n'appelle SetNVertices que `if (fvn != 3)`.
	//
	// Ni offsets ni UV : un maillage neuf n'en a pas.
	m_faceVertices.assign (3 * (size_t)nFaces, 0u);
	m_faceOffsets.clear ();
	m_faceCorners.clear ();
	m_faceArity = 3;
	m_faceMaterial.assign ((size_t)nFaces, (unsigned int)MATERIAL_NONE);
	m_faceRemoved.clear ();
	m_faceTexIndices.clear ();
	m_faceTexCoords.clear ();
	m_faceHasTex.clear ();

	m_faceNormals.clear();
	if (nFaces)
		m_faceNormals.assign(3*(size_t)nFaces, 0.0f);

	AssertFaceStorageFull ();
}

void Mesh::MaterialiseFaceOffsets (void)
{
	if (!m_faceOffsets.empty ())
		return;   // deja sous forme mixte
	const unsigned int nf = GetNFaces ();
	m_faceOffsets.resize (nf);
	m_faceCorners.assign (nf, m_faceArity);
	for (unsigned int i = 0; i < nf; i++)
		m_faceOffsets[i] = i * m_faceArity;
	m_faceArity = 0;
}

void Mesh::SetFaceArity (unsigned int fi, unsigned int n)
{
	if (!FaceExists (fi) || FaceArity (fi) == n)
		return;

	// Une seule face : l'arite reste uniforme, il suffit de redimensionner.
	if (GetNFaces () == 1 && m_faceOffsets.empty ())
	{
		m_faceVertices.assign (n, 0u);
		m_faceArity = n;
		if (!m_faceTexIndices.empty ()) m_faceTexIndices.assign (n, 0u);
		if (!m_faceTexCoords.empty ())  m_faceTexCoords.assign (2*(size_t)n, 0.0f);
		IncrementRevision ();
		AssertFaceStorage ();
		return;
	}

	MaterialiseFaceOffsets ();

	if (n <= m_faceCorners[fi])
	{
		// Reduction : on garde la tranche, on baisse le compte.
		m_faceCorners[fi] = n;
	}
	else
	{
		// Agrandissement : AJOUT d'une tranche en fin de pool plutot qu'un decalage
		// de tout ce qui suit. L'ancienne tranche devient orpheline, ce qui est le
		// prix du temps constant amorti (cf. mesh.h).
		const unsigned int oldBegin = m_faceOffsets[fi];
		const unsigned int oldN     = m_faceCorners[fi];
		const unsigned int newBegin = (unsigned int)m_faceVertices.size ();

		m_faceVertices.resize ((size_t)newBegin + n, 0u);
		for (unsigned int i = 0; i < oldN; i++)
			m_faceVertices[newBegin+i] = m_faceVertices[oldBegin+i];

		if (!m_faceTexIndices.empty ())
		{
			m_faceTexIndices.resize ((size_t)newBegin + n, 0u);
			for (unsigned int i = 0; i < oldN; i++)
				m_faceTexIndices[newBegin+i] = m_faceTexIndices[oldBegin+i];
		}
		if (!m_faceTexCoords.empty ())
		{
			m_faceTexCoords.resize (2*((size_t)newBegin + n), 0.0f);
			for (unsigned int i = 0; i < 2*oldN; i++)
				m_faceTexCoords[2*(size_t)newBegin+i] = m_faceTexCoords[2*(size_t)oldBegin+i];
		}

		m_faceOffsets[fi] = newBegin;
		m_faceCorners[fi] = n;
	}

	IncrementRevision ();
	AssertFaceStorage ();
}

void Mesh::EnsureTexIndices (void)
{
	if (m_faceTexIndices.empty ())
		m_faceTexIndices.assign (m_faceVertices.size (), 0u);
}

void Mesh::EnsureTexCoords (void)
{
	if (m_faceTexCoords.empty ())
		m_faceTexCoords.assign (2 * m_faceVertices.size (), 0.0f);
}

void Mesh::EnsureHasTexFlags (void)
{
	if (m_faceHasTex.empty ())
		m_faceHasTex.assign ((size_t)GetNFaces (), (uint8_t)0);
}

// Invariants de TAILLE, en temps constant.
//
// ⚠ Ne RIEN y mettre qui parcoure les faces : SetFaceArity l'appelle une fois par
// face, donc un parcours ici rendrait tout import quadratique. La version
// lineaire est AssertFaceStorageFull.
void Mesh::AssertFaceStorage (void) const
{
#ifdef _DEBUG
	const size_t nf = m_faceMaterial.size ();
	// Le descripteur d'arite doit correspondre au pool.
	if (m_faceOffsets.empty ())
	{
		assert (m_faceCorners.empty ());
		assert (m_faceVertices.size () == nf * (size_t)m_faceArity);
	}
	else
	{
		assert (m_faceArity == 0);
		assert (m_faceOffsets.size () == nf && m_faceCorners.size () == nf);
	}
	if (!m_faceTexIndices.empty ())
		assert (m_faceTexIndices.size () == m_faceVertices.size ());
	if (!m_faceTexCoords.empty ())
		assert (m_faceTexCoords.size () == 2 * m_faceVertices.size ());
	if (!m_faceHasTex.empty ())
		assert (m_faceHasTex.size () == nf);
	if (!m_faceRemoved.empty ())
		assert (m_faceRemoved.size () == nf);
#endif
}

// Verification COMPLETE : chaque tranche doit tenir dans le pool. Lineaire, donc
// reservee aux operations en masse -- jamais a une operation par face.
void Mesh::AssertFaceStorageFull (void) const
{
#ifdef _DEBUG
	AssertFaceStorage ();
	if (m_faceOffsets.empty ())
		return;
	for (size_t i = 0; i < m_faceOffsets.size (); i++)
		assert ((size_t)m_faceOffsets[i] + m_faceCorners[i] <= m_faceVertices.size ());
#endif
}

void Mesh::InitTensors (void)
{
	m_tensors.clear();
	if (m_nVertices)
		m_tensors.resize(m_nVertices); // optional se construit VIDE : un emplacement par sommet, tous vides
}

void Mesh::ClearTensors (void)
{
	m_tensors.clear();
	m_tensorsRevision = (uint64_t)-1;
}

Tensor* Mesh::GetTensor (unsigned int index)
{
	if (index >= m_tensors.size () || !m_tensors[index])
		return nullptr;
	return &*m_tensors[index];
}

const Tensor* Mesh::GetTensor (unsigned int index) const
{
	if (index >= m_tensors.size () || !m_tensors[index])
		return nullptr;
	return &*m_tensors[index];
}

void Mesh::SetTensor (unsigned int index, Tensor *t)
{
	// Un indice hors bornes libere quand meme : les emplacements sont paralleles
	// aux sommets, les agrandir romprait ce parallelisme.
	if (index < m_tensors.size ())
	{
		if (t)
			m_tensors[index] = *t;
		else
			m_tensors[index].reset ();
	}
	delete t;
}

void Mesh::SetNFaces (unsigned int nFaces)
{
	DeleteFaces ();
	InitFaces (nFaces);
	IncrementRevision ();
}

void Mesh::RemoveFace (unsigned int fi)
{
	if (!FaceExists (fi))
		return;
	// Le tableau des trous n'existe que s'il y a au moins un trou.
	if (m_faceRemoved.empty ())
		m_faceRemoved.assign ((size_t)GetNFaces (), (uint8_t)0);
	m_faceRemoved[fi] = 1;
	IncrementRevision ();
}

void Mesh::DeleteFaces (void)
{
	m_faceVertices.clear ();
	m_faceOffsets.clear ();
	m_faceCorners.clear ();
	m_faceArity = 0;
	m_faceMaterial.clear ();
	m_faceRemoved.clear ();
	m_faceTexIndices.clear ();
	m_faceTexCoords.clear ();
	m_faceHasTex.clear ();
}

void Mesh::Init (unsigned int nVertices, unsigned int nFaces)
{
	Init ();

	InitVertices (nVertices);
	InitFaces (nFaces);

	m_materials.clear();

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

Mesh::~Mesh ()
{
	// Tous les membres sont des types valeur : rien a liberer a la main.
}

void Mesh::Dump ()
{
	printf ("nVertices : %d\n", m_nVertices);
	printf("pVertices : %p\n", (void*)m_pVertices.data());
	printf ("pVertexNormals : %p\n", (void*)m_vertexNormals.data());
	printf("pVertexColors : %p\n", (void*)m_vertexColors.data());
	printf ("nFace : %u\n", GetNFaces ());
	printf ("faceVertices : %zu coins, arite %u, offsets %zu\n",
		m_faceVertices.size (), m_faceArity, m_faceOffsets.size ());
	printf ("pTextureCoordinates : %p\n", (void*)m_texCoords.data());
	printf ("nMaterials : %zu\n", m_materials.size());
	for (const auto& mat : m_materials)
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

void Mesh::MarkTensorsComputed()
{
	m_tensorsRevision = m_revision;
}

bool Mesh::AreTensorsValid() const
{
	return !m_tensors.empty() && m_tensorsRevision == m_revision;
}

void Mesh::AdoptTensorsFrom(const Mesh& src)
{
	// Vertex order/count must match: src is the half-edge copy of *this*.
	if (src.m_tensors.size() != (size_t)m_nVertices)
	{
		ClearTensors();
		return;
	}

	// L'affectation du vector EST la copie profonde : les emplacements vides
	// (sommet de bord / non manifold) le restent.
	m_tensors = src.m_tensors;

	MarkTensorsComputed();
}

std::vector<unsigned int> Mesh::GetTriangles (void)
{
	// Delegue a BuildTriangulation() : eventail pour les faces convexes et les
	// triangles, glutess pour les concaves, abandon propre des
	// auto-intersections. ⚠ N'emettre qu'un triangle par face (v0,v1,v2) laisse
	// des trous des le premier quad -- il en faut N-2.
	return BuildTriangulation();
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
bool faceIsConvex(Mesh::ConstFaceRef face, Mesh& mesh)
{
    const unsigned int n = face->GetNVertices();
    if (n < 4) return true;

    auto vert = [&](unsigned int k) {
        const unsigned int idx = face->GetVertex(k);
        return std::array<double, 3>{
            mesh.GetVertices ()[3*idx + 0],
            mesh.GetVertices ()[3*idx + 1],
            mesh.GetVertices ()[3*idx + 2]
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

// Iterate the triangles of face fi, invoking emit(localA, localB,
// localC) for each. Local indices are 0..N-1, addressing positions within
// the face's own vertex list. `tess` is a lazily-allocated, reusable
// tessellator handle (pass &nullptr on first call; caller cleans up).
template <class Emit>
void forEachFaceTriangle(Mesh& mesh, unsigned int fi, GLUtesselator*& tess, Emit&& emit)
{
    auto face = mesh.FaceAt (fi);
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
            (GLdouble)mesh.GetVertices ()[3*vi + 0],
            (GLdouble)mesh.GetVertices ()[3*vi + 1],
            (GLdouble)mesh.GetVertices ()[3*vi + 2]
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
    out.reserve(3 * GetNFaces ());

    GLUtesselator* tess = nullptr;
    for (unsigned int fi = 0; fi < GetNFaces (); ++fi)
    {
        auto face = FaceAt (fi);
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

void computeNewellNormal(Mesh::ConstFaceRef face, Mesh& mesh, float outN[3])
{
    const unsigned int n = face->GetNVertices();
    double nx = 0, ny = 0, nz = 0;
    for (unsigned int i = 0; i < n; ++i)
    {
        const unsigned int viA = face->GetVertex(i);
        const unsigned int viB = face->GetVertex((i + 1) % n);
        const double ax = mesh.GetVertices ()[3*viA + 0];
        const double ay = mesh.GetVertices ()[3*viA + 1];
        const double az = mesh.GetVertices ()[3*viA + 2];
        const double bx = mesh.GetVertices ()[3*viB + 0];
        const double by = mesh.GetVertices ()[3*viB + 1];
        const double bz = mesh.GetVertices ()[3*viB + 2];
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

Mesh::PolygonRenderData Mesh::BuildPolygonRenderData(bool flat)
{
    PolygonRenderData out;

    const bool hasNormals = !m_vertexNormals.empty();
    const bool hasUV      = !m_texCoords.empty();
    const bool hasColors  = !m_vertexColors.empty();

    // Per-corner UVs (OBJ f v/vt/vn): a shared vertex carries DIFFERENT UVs in
    // different faces, so the UV cannot be stored per vertex. Such a mesh keeps
    // face->HasTexCoordIndices () where at least one corner's UV index
    // differs from its vertex index. When present, every face must be expanded
    // into its own corners (no shared slot can hold two UVs) and the UV sourced
    // through those indices. Formats with genuine per-vertex UVs (3DS/3DM set
    // index == vertex index via InitTexCoord) do NOT trigger this and keep the
    // fast shared-slot path unchanged.
    bool perCornerUV = false;
    if (hasUV)
    {
        for (unsigned int fi = 0; fi < GetNFaces () && !perCornerUV; ++fi)
        {
            auto f = FaceAt (fi);
            if (!f || !f->UsesTextureCoordinates () || !f->HasTexCoordIndices ())
                continue;
            const unsigned int n = f->GetNVertices();
            for (unsigned int i = 0; i < n; ++i)
                if (f->GetTexCoordIndex (i) != (unsigned int)f->GetVertex(i))
                {
                    perCornerUV = true;
                    break;
                }
        }
    }

    // Smooth shading: seed the output with the shared topology layout; triangle
    // faces reference these slots directly (per-vertex normals preserved); only
    // N>=4 polygons append fresh slots below. Flat shading: seed nothing —
    // EVERY face (triangles included) is expanded into its own corners carrying
    // the face normal, so no slot is shared and each triangle is uniform.
    // Per-corner UVs force the same full expansion (seed nothing) so positions /
    // normals / texCoords / colors stay parallel as each face appends its corners.
    if (!flat && !perCornerUV)
    {
        out.positions.assign(m_pVertices.begin(),           m_pVertices.end());
        if (hasNormals) out.normals.assign  (m_vertexNormals.begin(),       m_vertexNormals.end());
        if (hasUV)      out.texCoords.assign(m_texCoords.begin(),  m_texCoords.end());
        if (hasColors)  out.colors.assign   (m_vertexColors.begin(),        m_vertexColors.end());
    }
    out.indices.reserve(3u * GetNFaces ());

    // Group triangle indices by material so the VBO path can draw one run per
    // material. std::map keeps a stable material order.
    std::map<unsigned int, std::vector<unsigned int>> buckets;

    GLUtesselator* tess = nullptr;

    for (unsigned int fi = 0; fi < GetNFaces (); ++fi)
    {
        auto face = FaceAt (fi);
        if (!face) continue;
        const unsigned int n = face->GetNVertices();
        if (n < 3) continue;

        const unsigned int matId = (unsigned int)face->GetMaterialId();
        std::vector<unsigned int>& bucket = buckets[matId];

        if (n == 3 && !flat && !perCornerUV)
        {
            // Smooth triangle: index directly into the shared topology slots.
            bucket.push_back((unsigned int)face->GetVertex(0));
            bucket.push_back((unsigned int)face->GetVertex(1));
            bucket.push_back((unsigned int)face->GetVertex(2));
            continue;
        }

        // Flat triangles AND all N>=4 polygons: append N fresh render-vertices
        // carrying the face's (Newell) normal uniformly — for a triangle this is
        // its own face normal, giving true flat shading.
        float fn[3];
        computeNewellNormal(face, *this, fn);

        // Normal choice for the appended corners: the face (Newell) normal for
        // flat shading and for N>=4 polygons (unchanged — avoids fan-diagonal
        // kinks on non-planar n-gons); the per-vertex normal for a triangle that
        // is only expanded because of per-corner UVs, so smooth shading survives.
        const bool useFaceNormal = flat || (n >= 4);

        const unsigned int base = (unsigned int)(out.positions.size() / 3);

        for (unsigned int i = 0; i < n; ++i)
        {
            const unsigned int vi = face->GetVertex(i);

            out.positions.push_back(m_pVertices[3*vi + 0]);
            out.positions.push_back(m_pVertices[3*vi + 1]);
            out.positions.push_back(m_pVertices[3*vi + 2]);

            if (hasNormals)
            {
                if (useFaceNormal || 3*vi + 2 >= m_vertexNormals.size())
                {
                    out.normals.push_back(fn[0]);
                    out.normals.push_back(fn[1]);
                    out.normals.push_back(fn[2]);
                }
                else
                {
                    out.normals.push_back(m_vertexNormals[3*vi + 0]);
                    out.normals.push_back(m_vertexNormals[3*vi + 1]);
                    out.normals.push_back(m_vertexNormals[3*vi + 2]);
                }
            }

            if (hasUV)
            {
                // Per-corner UV index when the face carries one (OBJ), else the
                // vertex index (per-vertex UVs: 3DS/3DM, scalar-field colouring).
                unsigned int uvIdx = vi;
                if (face->UsesTextureCoordinates () && face->HasTexCoordIndices ())
                    uvIdx = face->GetTexCoordIndex (i);

                if (2*uvIdx + 1 < m_texCoords.size())
                {
                    out.texCoords.push_back(m_texCoords[2*uvIdx + 0]);
                    out.texCoords.push_back(m_texCoords[2*uvIdx + 1]);
                }
                else
                {
                    out.texCoords.push_back(0.0f);
                    out.texCoords.push_back(0.0f);
                }
            }

            if (hasColors)
            {
                if (3*vi + 2 < m_vertexColors.size())
                {
                    out.colors.push_back(m_vertexColors[3*vi + 0]);
                    out.colors.push_back(m_vertexColors[3*vi + 1]);
                    out.colors.push_back(m_vertexColors[3*vi + 2]);
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
                bucket.push_back(base + a);
                bucket.push_back(base + b);
                bucket.push_back(base + c);
            });
    }

    if (tess) gluDeleteTess(tess);

    // Flatten the per-material buckets into one index array + range table.
    for (auto& kv : buckets)
    {
        MaterialRange r;
        r.materialId = kv.first;
        r.offset     = (unsigned int)out.indices.size();
        r.count      = (unsigned int)kv.second.size();
        out.indices.insert(out.indices.end(), kv.second.begin(), kv.second.end());
        out.materialRanges.push_back(r);
    }

    return out;
}

// ----- In-place triangulation ----------------------------------------------
//
// Triangulate() walks faces and replaces every N>=4 polygon with (N-2)
// triangle Face objects (fan for convex, glutess for concave). Triangle
// faces are kept as-is. Material ids, face-relative texture-coordinate
// indices (m_pTexCoordIndices) and inline per-face UV coordinates
// (m_texCoords) all propagate to each sub-triangle.
//
void Mesh::Triangulate()
{
    const unsigned int nf0 = GetNFaces ();
    if (nf0 == 0) return;

    // Construction dans des tableaux NEUFS, puis echange. Le resultat est d'arite
    // uniforme 3, donc sans tableau d'offsets. Les UV ne sont alloues que si la
    // source en portait.
    const bool hasTexIdx = !m_faceTexIndices.empty ();
    const bool hasTexUV  = !m_faceTexCoords.empty ();
    const bool hasFlags  = !m_faceHasTex.empty ();

    std::vector<unsigned int> outVerts;
    std::vector<unsigned int> outMaterial;
    std::vector<unsigned int> outTexIdx;
    std::vector<float>        outTexUV;
    std::vector<uint8_t>      outFlags;
    outVerts.reserve (3 * (size_t)nf0);
    outMaterial.reserve (nf0);

    // Emet un triangle depuis les indices LOCAUX a/b/c de la face source fi.
    auto emitTriangle = [&](unsigned int fi, unsigned int a, unsigned int b, unsigned int c) {
        auto src = FaceAt (fi);
        const unsigned int loc[3] = { a, b, c };

        for (int k = 0; k < 3; k++)
            outVerts.push_back ((unsigned int)src->GetVertex (loc[k]));

        outMaterial.push_back ((unsigned int)src->GetMaterialId ());

        // Le drapeau « cette face utilise des UV » doit voyager : l'exportateur OBJ
        // le consulte pour decider s'il emet des indices 'vt'.
        if (hasFlags)
            outFlags.push_back (src->UsesTextureCoordinates () ? 1 : 0);

        if (hasTexIdx)
            for (int k = 0; k < 3; k++)
                outTexIdx.push_back ((unsigned int)src->GetTexCoordIndex (loc[k]));

        if (hasTexUV)
            for (int k = 0; k < 3; k++)
            {
                float uv[2] = { 0.f, 0.f };
                src->GetTexCoord (loc[k], uv);
                outTexUV.push_back (uv[0]);
                outTexUV.push_back (uv[1]);
            }
    };

    GLUtesselator* tess = nullptr;

    for (unsigned int fi = 0; fi < nf0; ++fi)
    {
        auto face = FaceAt (fi);
        if (!face) continue;                       // trou

        const unsigned int n = (unsigned int)face->GetNVertices ();
        if (n < 3) continue;                       // face degeneree : abandonnee

        if (n == 3)
        {
            emitTriangle (fi, 0u, 1u, 2u);
            continue;
        }

        forEachFaceTriangle (*this, fi, tess,
            [&](unsigned int a, unsigned int b, unsigned int c) {
                emitTriangle (fi, a, b, c);
            });
    }

    if (tess) gluDeleteTess (tess);

    m_faceVertices.swap (outVerts);
    m_faceMaterial.swap (outMaterial);
    m_faceTexIndices.swap (outTexIdx);
    m_faceTexCoords.swap (outTexUV);
    m_faceHasTex.swap (outFlags);
    m_faceOffsets.clear ();
    m_faceCorners.clear ();
    m_faceArity = 3;
    // Les trous ont disparu avec la reconstruction.
    m_faceRemoved.clear ();

    AssertFaceStorageFull ();
    ComputeNormals();      // redimensionne aussi les normales par face
    IncrementRevision();
}

int Mesh::SetVertices (unsigned int nVertices, const float *pVertices)
{
	const bool countChanged = (nVertices != m_nVertices);

	m_nVertices = nVertices;
	m_pVertices.assign(pVertices, pVertices + 3*nVertices);

	// Les attributs par sommet sont indexes par sommet : un compte different les
	// rend structurellement invalides, et les laisser en place ferait lire hors
	// bornes. Un compte identique les laisse valides -- deplacer des sommets
	// n'invalide ni leur couleur ni leur normale.
	if (countChanged)
	{
		m_vertexColors.clear();
		m_vertexNormals.clear();
	}

	IncrementRevision ();
	return 0;
}

int Mesh::SetVertexNormals(unsigned int nVertexNormals, const float* pVertexNormals)
{
	if (nVertexNormals != m_nVertices)
	{
		return -1;
	}

	m_vertexNormals.assign(pVertexNormals, pVertexNormals + 3*nVertexNormals);

	return 0;

}

int Mesh::SetVertexNormals(std::vector<float> normals)
{
	// Vide = pas de normales, licite.
	if (!normals.empty() && normals.size() != 3*(size_t)m_nVertices)
		return -1;

	m_vertexNormals = std::move (normals);

	return 0;
}

int Mesh::SetFaces (unsigned int nFaces, unsigned int nVerticesPerFace, unsigned int *pFaces, unsigned int *pTextureCoordinates)
{
	// DeleteFaces + InitFaces : les faces en place sont liberees et m_faceNormals
	// redimensionne au nouveau compte. La revision est incrementee, les faces
	// etant de la geometrie.
	DeleteFaces ();
	InitFaces (nFaces);

	// Toutes les faces ont la MEME arite : la poser comme arite uniforme, et NON
	// appeler SetNVertices face par face -- cela materialiserait les offsets des la
	// premiere face non triangulaire.
	m_faceArity = nVerticesPerFace;
	m_faceOffsets.clear ();
	m_faceCorners.clear ();
	m_faceVertices.assign ((size_t)nFaces * nVerticesPerFace, 0u);
	for (size_t k = 0; k < m_faceVertices.size (); k++)
		m_faceVertices[k] = pFaces[k];

	IncrementRevision ();
	AssertFaceStorageFull ();
	return 1;
}

int Mesh::SetVertexComponent (unsigned int i, unsigned int dim, float value)
{
	if (dim > 2 || 3*i+dim >= m_pVertices.size())
		return -1;

	m_pVertices[3*i+dim] = value;

	IncrementRevision ();
	return 0;
}

int Mesh::SetVertex (unsigned int i, float x, float y, float z)
{
	if (m_pVertices.empty() || i>=m_nVertices)
		return -1;

	m_pVertices[3*i+0] = x;
	m_pVertices[3*i+1] = y;
	m_pVertices[3*i+2] = z;

	IncrementRevision ();
	return 0;
}

int Mesh::SetFace (unsigned int i,
		   unsigned int a, unsigned int b, unsigned int c)
{
	// Remplace la face : elle repart d'un materiau vide et sans UV, comme le
	// faisait `m_pFaces[i] = new Face ()`. Un emplacement troue redevient vivant.
	ResetFace (i);
	FaceAt (i)->SetTriangle (a, b, c);
	return 0;
}

int Mesh::SetFace (unsigned int i,
		   unsigned int a, unsigned int b, unsigned int c, unsigned int d)
{
	ResetFace (i);
	FaceAt (i)->SetQuad (a, b, c, d);
	return 0;
}


int Mesh::computebbox (void)
{
	m_bbox.Clear();
	for (int i = 0; i < m_nVertices; i++)
		m_bbox.AddPoint(m_pVertices[3 * i], m_pVertices[3 * i + 1], m_pVertices[3 * i + 2]);
	
	return 0;
}

int Mesh::SetVertexColor (unsigned int i, float r, float g, float b)
{
	if (3*i+2 >= m_vertexColors.size()) return -1;
	m_vertexColors[3*i]   = r;
	m_vertexColors[3*i+1] = g;
	m_vertexColors[3*i+2] = b;
	return 0;
}

int Mesh::SetVertexColorComponent (unsigned int i, unsigned int dim, float value)
{
	if (dim > 2 || 3*i+dim >= m_vertexColors.size()) return -1;
	m_vertexColors[3*i+dim] = value;
	return 0;
}

int Mesh::SetVertexColors (std::vector<float> colors)
{
	// Vide = pas de couleurs, licite.
	if (!colors.empty() && colors.size() != 3*(size_t)m_nVertices)
		return -1;

	m_vertexColors = std::move (colors);
	return 0;
}

int Mesh::SetTextureCoordinate (unsigned int i, float u, float v)
{
	if (2*i+1 >= m_texCoords.size()) return -1;
	m_texCoords[2*i]   = u;
	m_texCoords[2*i+1] = v;
	return 0;
}

int Mesh::SetTextureCoordinates (std::vector<float> uv, unsigned int nTexCoords)
{
	m_texCoords  = std::move (uv);
	m_nTexCoords = nTexCoords;
	IncrementRevision ();
	return 0;
}

void Mesh::InitVertexNormals (void)
{
	m_vertexNormals.assign (3*m_nVertices, 0.0f);
}

int Mesh::SetVertexNormal (unsigned int i, float x, float y, float z)
{
	if (3*i+2 >= m_vertexNormals.size()) return -1;
	m_vertexNormals[3*i]   = x;
	m_vertexNormals[3*i+1] = y;
	m_vertexNormals[3*i+2] = z;
	return 0;
}

int Mesh::SetVertexNormalComponent (unsigned int i, unsigned int dim, float value)
{
	if (dim > 2 || 3*i+dim >= m_vertexNormals.size()) return -1;
	m_vertexNormals[3*i+dim] = value;
	return 0;
}

void Mesh::AddPoint (unsigned int v)
{
	m_points.push_back (v);
	IncrementRevision ();
}

void Mesh::SetPoints (std::vector<unsigned int> points)
{
	m_points = std::move (points);
	IncrementRevision ();
}

void Mesh::AddLine (unsigned int a, unsigned int b)
{
	m_lines.push_back (a);
	m_lines.push_back (b);
	IncrementRevision ();
}

void Mesh::SetLines (std::vector<unsigned int> lines)
{
	m_lines = std::move (lines);
	IncrementRevision ();
}

int Mesh::SetFaceNormal (unsigned int fi, float x, float y, float z)
{
	if (3*fi+2 >= m_faceNormals.size()) return -1;
	m_faceNormals[3*fi]   = x;
	m_faceNormals[3*fi+1] = y;
	m_faceNormals[3*fi+2] = z;
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
	unsigned int vi1 = 3*FaceAt (fi)->GetVertex (0);
	unsigned int vi2 = 3*FaceAt (fi)->GetVertex (1);
	unsigned int vi3 = 3*FaceAt (fi)->GetVertex (2);
	Vector3f v1, v2, v3;

	v1.Set (m_pVertices[vi1], m_pVertices[vi1+1], m_pVertices[vi1+2]);
	v2.Set (m_pVertices[vi2], m_pVertices[vi2+1], m_pVertices[vi2+2]);
	v3.Set (m_pVertices[vi3], m_pVertices[vi3+1], m_pVertices[vi3+2]);
	
	return Vector3f::evaluate_triangle_area (v1, v2, v3);
}

float Mesh::GetArea (void)
{
	float area = 0.;
	for (unsigned int i=0; i<GetNFaces (); i++)
		area += GetFaceArea(i);
	return area;
}

float* Mesh::GetAreas (void)
{
	float *areas = (float*)malloc(GetNFaces ()*sizeof(float));
	for (unsigned int i=0; i<GetNFaces (); i++)
		areas[i] = GetFaceArea(i);
	return areas;
}

float* Mesh::GetCumulativeAreas (void)
{
	float *areas = GetAreas ();
	for (unsigned int i=1; i<GetNFaces (); i++)
		areas[i] += areas[i-1];
	return areas;
}

int Mesh::stats_vertices_in_faces (int *verticesinfaces, int n)
{	
	memset (verticesinfaces, 0, n*sizeof(int));
	for (int i=0; i<GetNFaces (); i++)
	{
		auto f = FaceAt (i);
		if (!f)
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
	// Derive du tableau des materiaux, seul tableau par face toujours present.
	// Aucun membre de comptage, donc aucune divergence possible.
	return (unsigned int)m_faceMaterial.size ();
}

// Remet la face fi a l'etat neuf : triangle, sans materiau, sans UV, et vivante.
void Mesh::ResetFace (unsigned int fi)
{
	if (fi >= m_faceMaterial.size ())
		return;
	if (!m_faceRemoved.empty ())
		m_faceRemoved[fi] = 0;
	m_faceMaterial[fi] = (unsigned int)MATERIAL_NONE;
	if (!m_faceHasTex.empty ())
		m_faceHasTex[fi] = 0;
	SetFaceArity (fi, 3);
}

bool Mesh::IsTriangleMesh() const
{
	// O(1) quand l'arite est uniforme. La forme mixte exige le parcours : le
	// descripteur dit « mixte », et non « pas tout triangles » -- une suite
	// d'editions peut avoir ramene chaque face a 3.
	if (m_faceCorners.empty ())
		return m_faceArity == 3;
	const unsigned int nf = GetNFaces ();
	for (unsigned int i = 0; i < nf; i++)
		if (FaceAt (i)->GetNVertices () != 3)
			return false;
	return true;
}

//
//
//
void Mesh::ComputeNormals (void)
{
	m_vertexNormals.assign(3*m_nVertices, 0.0f);
	m_faceNormals.assign(3*GetNFaces (), 0.0f);
	

	//
	int *nfaces = new int[m_nVertices];
	memset (nfaces, 0, m_nVertices*sizeof(int));
	
	for (int i=0; i<GetNFaces (); i++)
	{
		auto pFace = FaceAt (i);
		int k1 = pFace->GetVertex (0);
		int k2 = pFace->GetVertex (1);
		int k3 = pFace->GetVertex (2);
		Vector3f p1p2, p1p3, n;
		p1p2.Set (
			   m_pVertices[3*k2]   - m_pVertices[3*k1],
			   m_pVertices[3*k2+1] - m_pVertices[3*k1+1],
			   m_pVertices[3*k2+2] - m_pVertices[3*k1+2]);
		p1p3.Set (
			   m_pVertices[3*k3]   - m_pVertices[3*k1],
			   m_pVertices[3*k3+1] - m_pVertices[3*k1+1],
			   m_pVertices[3*k3+2] - m_pVertices[3*k1+2]);
		n = (p1p2).CrossProduct (p1p3);
		(n).Normalize ();
		m_faceNormals[3*i]   = n[0];
		m_faceNormals[3*i+1] = n[1];
		m_faceNormals[3*i+2] = n[2];
		
		for (int j=0; j<pFace->GetNVertices (); j++)
		{
			int k = pFace->GetVertex (j);
			
			m_vertexNormals[3*k]   += m_faceNormals[3*i];
			m_vertexNormals[3*k+1] += m_faceNormals[3*i+1];
			m_vertexNormals[3*k+2] += m_faceNormals[3*i+2];
			
			nfaces[k]++;
		}
	}

	// Normalise the per-vertex normal to UNIT length. Averaging incident unit
	// face normals yields a non-unit vector (e.g. ~0.58 at a convex corner,
	// ~1 on a flat region); leaving it non-unit makes lighting intensity vary
	// with the magnitude — visible as shading discontinuities between triangles
	// when GL_NORMALIZE is off. (Dividing by the face count is unnecessary once
	// we normalise — it only scales the vector.) Guard the zero vector
	// (isolated vertex, or opposite faces cancelling) to avoid NaNs.
	for (int i=0; i<m_nVertices; i++)
	{
		float *vn = &m_vertexNormals[3*i];
		float len = sqrtf(vn[0]*vn[0] + vn[1]*vn[1] + vn[2]*vn[2]);
		if (len > 1e-12f)
		{
			vn[0] /= len; vn[1] /= len; vn[2] /= len;
		}
	}

	// cleaning
	delete[] nfaces;
}

unsigned int Mesh::CountEdges (void)
{
	std::set<std::pair<unsigned int, unsigned int>> edges;
	for (unsigned int f = 0; f < GetNFaces (); f++)
	{
		unsigned int nv = FaceAt (f)->GetNVertices();
		for (unsigned int e = 0; e < nv; e++)
		{
			unsigned int a = FaceAt (f)->GetVertex(e);
			unsigned int b = FaceAt (f)->GetVertex((e + 1) % nv);
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

	const unsigned int shift = m_nVertices;
	const unsigned int matOffset = (unsigned int)m_materials.size();

	// vertices
	m_pVertices.insert(m_pVertices.end(),
	                   m->m_pVertices.begin(),
	                   m->m_pVertices.begin() + 3 * m->m_nVertices);

	// ---- faces : concatenation des deux pools ----
	const bool anyTexIdx  = !m_faceTexIndices.empty () || !m->m_faceTexIndices.empty ();
	const bool anyTexUV   = !m_faceTexCoords.empty ()  || !m->m_faceTexCoords.empty ();
	const bool anyFlags   = !m_faceHasTex.empty ()     || !m->m_faceHasTex.empty ();
	const bool anyRemoved = !m_faceRemoved.empty ()    || !m->m_faceRemoved.empty ();

	std::vector<unsigned int> outVerts, outOffsets, outCorners, outMaterial, outTexIdx;
	std::vector<float>        outTexUV;
	std::vector<uint8_t>      outFlags, outRemoved;

	// Recopie la TRANCHE BRUTE de la face fi : FaceBegin / FaceArity et non FaceAt,
	// pour transporter aussi les faces trouees.
	auto copyFace = [&](const Mesh &src, unsigned int fi,
			    unsigned int vertexShift, unsigned int materialShift)
	{
		const unsigned int n = src.FaceArity (fi);
		const unsigned int b = src.FaceBegin (fi);

		outOffsets.push_back ((unsigned int)outVerts.size ());
		outCorners.push_back (n);

		for (unsigned int k = 0; k < n; k++)
			outVerts.push_back (src.m_faceVertices[b+k] + vertexShift);

		// ⚠ MATERIAL_NONE reste MATERIAL_NONE : lui ajouter un decalage le fait
		// deborder vers un indice de materiau valide et arbitraire.
		const unsigned int mat = src.m_faceMaterial[fi];
		outMaterial.push_back (mat == (unsigned int)MATERIAL_NONE
				       ? mat : mat + materialShift);

		if (anyTexIdx)
		{
			const bool has = !src.m_faceTexIndices.empty ();
			for (unsigned int k = 0; k < n; k++)
				outTexIdx.push_back (has ? src.m_faceTexIndices[b+k] : 0u);
		}
		if (anyTexUV)
		{
			const bool has = !src.m_faceTexCoords.empty ();
			for (unsigned int k = 0; k < n; k++)
			{
				outTexUV.push_back (has ? src.m_faceTexCoords[2*((size_t)b+k)]   : 0.0f);
				outTexUV.push_back (has ? src.m_faceTexCoords[2*((size_t)b+k)+1] : 0.0f);
			}
		}
		if (anyFlags)
			outFlags.push_back (src.m_faceHasTex.empty () ? 0 : src.m_faceHasTex[fi]);
		if (anyRemoved)
			outRemoved.push_back (src.m_faceRemoved.empty () ? 0 : src.m_faceRemoved[fi]);
	};

	const unsigned int nf0 = GetNFaces ();
	const unsigned int nf1 = m->GetNFaces ();
	outVerts.reserve (m_faceVertices.size () + m->m_faceVertices.size ());
	outOffsets.reserve ((size_t)nf0 + nf1);
	outCorners.reserve ((size_t)nf0 + nf1);
	outMaterial.reserve ((size_t)nf0 + nf1);

	for (unsigned int i = 0; i < nf0; i++)
		copyFace (*this, i, 0u, 0u);
	for (unsigned int i = 0; i < nf1; i++)
		copyFace (*m, i, shift, matOffset);

	// Materials: transfer ownership from `m` to `this`. After Append, `m`'s
	// material array is emptied (it no longer owns them).
	for (auto& mat : m->m_materials)
		m_materials.push_back(std::move(mat));
	m->m_materials.clear();

	m_nVertices += m->m_nVertices;

	// Le compte de sommets change, donc les attributs par sommet ne couvrent plus
	// que le prefixe : on les invalide. Les concatener serait possible, mais il
	// faudrait inventer une valeur pour le cas ou un seul des deux maillages en
	// porte -- a faire le jour ou un appelant en a besoin.
	m_vertexColors.clear();
	m_vertexNormals.clear();

	m_faceVertices.swap (outVerts);
	m_faceMaterial.swap (outMaterial);
	m_faceTexIndices.swap (outTexIdx);
	m_faceTexCoords.swap (outTexUV);
	m_faceHasTex.swap (outFlags);
	m_faceRemoved.swap (outRemoved);
	m_faceOffsets.swap (outOffsets);
	m_faceCorners.swap (outCorners);
	m_faceArity = 0;

	// La concatenation peut laisser l'arite uniforme, et il ne faut pas payer les
	// offsets pour rien. Le descripteur etant derivable, on le re-derive.
	CompactFaceArity ();

	IncrementRevision ();
	return 1;
}

void Mesh::KeepFaces (const std::vector<unsigned int> &keep)
{
	const bool hasTexIdx  = !m_faceTexIndices.empty ();
	const bool hasTexUV   = !m_faceTexCoords.empty ();
	const bool hasFlags   = !m_faceHasTex.empty ();

	std::vector<unsigned int> outVerts, outOffsets, outCorners, outMaterial, outTexIdx;
	std::vector<float>        outTexUV;
	std::vector<uint8_t>      outFlags;
	outOffsets.reserve (keep.size ());
	outCorners.reserve (keep.size ());
	outMaterial.reserve (keep.size ());

	for (unsigned int fi : keep)
	{
		if (fi >= m_faceMaterial.size ())
			continue;
		const unsigned int n = FaceArity (fi);
		const unsigned int b = FaceBegin (fi);

		outOffsets.push_back ((unsigned int)outVerts.size ());
		outCorners.push_back (n);
		for (unsigned int k = 0; k < n; k++)
			outVerts.push_back (m_faceVertices[b+k]);
		outMaterial.push_back (m_faceMaterial[fi]);
		if (hasTexIdx)
			for (unsigned int k = 0; k < n; k++)
				outTexIdx.push_back (m_faceTexIndices[b+k]);
		if (hasTexUV)
			for (unsigned int k = 0; k < n; k++)
			{
				outTexUV.push_back (m_faceTexCoords[2*((size_t)b+k)]);
				outTexUV.push_back (m_faceTexCoords[2*((size_t)b+k)+1]);
			}
		if (hasFlags)
			outFlags.push_back (m_faceHasTex[fi]);
	}

	m_faceVertices.swap (outVerts);
	m_faceOffsets.swap (outOffsets);
	m_faceCorners.swap (outCorners);
	m_faceMaterial.swap (outMaterial);
	m_faceTexIndices.swap (outTexIdx);
	m_faceTexCoords.swap (outTexUV);
	m_faceHasTex.swap (outFlags);
	m_faceArity = 0;
	// Une compaction retire tous les trous par construction.
	m_faceRemoved.clear ();

	// Les normales par face sont indexees par la numerotation des faces, que cette
	// compaction change : les garder les rendrait fausses.
	m_faceNormals.assign (3 * m_faceMaterial.size (), 0.0f);

	CompactFaceArity ();
	AssertFaceStorageFull ();
}

// Repasse a la forme uniforme quand toutes les faces ont la meme arite et que les
// tranches sont contigues. Toujours legitime : le descripteur est derivable.
void Mesh::CompactFaceArity (void)
{
	if (m_faceCorners.empty ())
		return;                    // deja uniforme

	const size_t nf = m_faceCorners.size ();
	if (nf == 0)
	{
		m_faceOffsets.clear ();
		m_faceCorners.clear ();
		m_faceArity = 0;
		return;
	}

	const unsigned int k = m_faceCorners[0];
	if (k == 0)
		return;                    // arite uniforme 0 : reservee a « mixte »
	for (size_t i = 0; i < nf; i++)
		if (m_faceCorners[i] != k || m_faceOffsets[i] != (unsigned int)(i * k))
			return;
	// Les tranches orphelines rendent le pool plus long que nf*k : on le tronque.
	m_faceVertices.resize (nf * (size_t)k);
	if (!m_faceTexIndices.empty ()) m_faceTexIndices.resize (nf * (size_t)k);
	if (!m_faceTexCoords.empty ())  m_faceTexCoords.resize (2 * nf * (size_t)k);

	m_faceOffsets.clear ();
	m_faceCorners.clear ();
	m_faceArity = k;
	AssertFaceStorageFull ();
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

			m_vertexNormals[3*nVertices]   = m_vertexNormals[3*i];
			m_vertexNormals[3*nVertices+1] = m_vertexNormals[3*i+1];
			m_vertexNormals[3*nVertices+2] = m_vertexNormals[3*i+2];
			if (!m_vertexColors.empty())
			{
				m_vertexColors[3*nVertices]   = m_vertexColors[3*i];
				m_vertexColors[3*nVertices+1] = m_vertexColors[3*i+1];
				m_vertexColors[3*nVertices+2] = m_vertexColors[3*i+2];
			}

			nVertices++;
		}
	}
	m_nVertices = nVertices;

	// Les trois tableaux ont ete COMPACTES en place mais gardaient leur ancienne
	// taille. Sans ce redimensionnement, size() != 3*m_nVertices, et les
	// operations ulterieures qui testent ce parallelisme a l'egalite exacte
	// (MergeVertices, SplitVerticesByUVSeams) abandonnent silencieusement les
	// couleurs et les normales.
	m_pVertices.resize (3*m_nVertices);
	if (!m_vertexNormals.empty())
		m_vertexNormals.resize (3*m_nVertices);
	if (!m_vertexColors.empty())
		m_vertexColors.resize (3*m_nVertices);

	IncrementRevision ();
	return 0;
}

/*
bool Mesh::ColorizeVerticesDensity_Traverse (Octree &o, void *_data)
{
	if (o.m_nIndices)
	{
		float *data = (float*)_data;
		Vector3f v0, v;
		v0.Set (data[0], data[1], data[2]);
		float k = data[3];
		for (int i=0; i<o.m_nIndices; i++)
		{
			GetVertex (i, v);
			if ((v0).getDistance (v) < k)
				data[4+i]++;
		}
		
	}
	return true;
}
*/
// Treatments on vertices
int Mesh::ColorizeVerticesDensity (float k)
{
	if (m_vertexColors.empty())
		InitVertexColors (m_nVertices);

	unsigned int *neighbours = (unsigned int*)malloc(m_nVertices*sizeof(unsigned int));
	//unsigned int neighbours[m_nVertices];
	memset (neighbours, 0, m_nVertices*sizeof(unsigned int));
/*
	Vector3f v, vtmp;
	for (int i=0; i<m_nVertices; i++)
	{
		printf ("%d / %d\n", i, m_nVertices);
		GetVertex (i, v);
		for (int j=0; j<m_nVertices; j++)
		{
			GetVertex (j, vtmp);
			if ((v).getDistance (vtmp) < k)
				neighbours[i]++;
		}
	}
*/

	Octree *pOctree = new Octree ();
	pOctree->Build (m_pVertices.data(), m_nVertices, 200, 20);
	Vector3f pt;
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
			   &m_vertexColors[3*i], &m_vertexColors[3*i+1], &m_vertexColors[3*i+2]);
		
		//m_vertexColors[3*i]   = (float)neighbours[i]/max;
		//m_vertexColors[3*i+1] = (float)neighbours[i]/max;
		//m_vertexColors[3*i+2] = (float)neighbours[i]/max;
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
		Vector3f disp;
		disp.Set (gasdev(&idum), gasdev(&idum), gasdev(&idum));
		m_pVertices[3*i]   += variance*disp[0];
		m_pVertices[3*i+1] += variance*disp[1];
		m_pVertices[3*i+2] += variance*disp[2];
	}
	IncrementRevision();
}

// topology
void Mesh::GetTopologicIssues(std::vector<unsigned int>& nonManifoldEdges, std::vector<unsigned int>& borders) const
{
	nonManifoldEdges.clear();
	borders.clear();

	std::map<unsigned int, std::map<unsigned int, unsigned int>> occurences;
	for (int i = 0; i < GetNFaces (); i++)
	{
		auto pFace = FaceAt (i);
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
bool Mesh::GetIntersectionBboxWithRay (const Vector3f &o, const Vector3f &d)
{
	float bbox_min[3];
	float bbox_max[3];
	m_bbox.GetMinMax(bbox_min, bbox_max);
	AABox box (bbox_min[0], bbox_min[1], bbox_min[2]);
	box.AddVertex (bbox_max[0], bbox_max[1], bbox_max[2]);
	Ray r (Vector3 (o[0], o[1], o[2]), Vector3 (d[0], d[1], d[2]));
	return box.intersection (r, 0., 100.);
}

int Mesh::GetIntersectionWithRay (const Vector3f &vOrig, const Vector3f &vDirection, float *_t, Vector3f &vIntersection, Vector3f &vNormal)
{
	// Chemin non accelere, environ 3,4 fois plus lent que le chemin a octree sur
	// un maillage de 662 triangles. Voir mesh.h pour la raison de son existence.
	return GetIntersectionWithRayBruteForce (*this, vOrig, vDirection, _t, vIntersection, vNormal);
}

int Mesh::GetIntersectionWithSegment (const Vector3f &vStart, const Vector3f &vEnd, float *_t, Vector3f &i, Vector3f &n)
{
	return 0;
}

const void* Mesh::GetMaterial (void) const
{
	if (m_materials.empty())
		return nullptr;
	return GetMaterial (0u);
}

//
// Split vertices along UV seams -> vertex-parallel UVs (see header).
void Mesh::SplitVerticesByUVSeams (void)
{
	if (m_nTexCoords == 0)
		return;                                            // no UV pool: nothing to do
	if (m_texCoords.size() == 2u * (size_t)m_nVertices)
		return;                                            // already vertex-parallel

	const bool hasC = m_vertexColors.size()  == 3u * (size_t)m_nVertices;
	const bool hasN = m_vertexNormals.size() == 3u * (size_t)m_nVertices;
	const unsigned int NO_UV = 0xFFFFFFFFu;                // sentinel for corners without UV

	std::map<std::pair<unsigned int, unsigned int>, unsigned int> remap; // (vertex, uvIndex) -> new vertex
	std::vector<float> newV, newUV, newC, newN;
	unsigned int newNv = 0;

	for (unsigned int f = 0; f < GetNFaces (); f++)
	{
		auto face = FaceAt (f);
		if (!face) continue;
		const bool faceUV = face->UsesTextureCoordinates () && face->HasTexCoordIndices ();

		for (unsigned int c = 0; c < face->GetNVertices (); c++)
		{
			const unsigned int vi = face->GetVertex (c);
			const unsigned int ti = faceUV ? face->GetTexCoordIndex (c) : NO_UV;

			auto key = std::make_pair(vi, ti);
			auto it = remap.find(key);
			unsigned int ni;
			if (it == remap.end())
			{
				ni = newNv++;
				remap[key] = ni;

				newV.push_back(m_pVertices[3*vi]);
				newV.push_back(m_pVertices[3*vi+1]);
				newV.push_back(m_pVertices[3*vi+2]);

				if (faceUV && ti < m_nTexCoords)
				{
					newUV.push_back(m_texCoords[2*ti]);
					newUV.push_back(m_texCoords[2*ti+1]);
				}
				else
				{
					newUV.push_back(0.f);
					newUV.push_back(0.f);
				}

				if (hasC) { newC.push_back(m_vertexColors[3*vi]); newC.push_back(m_vertexColors[3*vi+1]); newC.push_back(m_vertexColors[3*vi+2]); }
				if (hasN) { newN.push_back(m_vertexNormals[3*vi]); newN.push_back(m_vertexNormals[3*vi+1]); newN.push_back(m_vertexNormals[3*vi+2]); }
			}
			else
			{
				ni = it->second;
			}

			face->SetVertex (c, ni);
			if (face->HasTexCoordIndices ())
				face->SetTexCoord (c, (unsigned int)(ni)); // UV index now == vertex index
		}
	}

	m_pVertices = std::move(newV);
	m_nVertices = newNv;
	m_texCoords = std::move(newUV);
	m_nTexCoords = newNv;                         // vertex-parallel: one UV per vertex
	if (hasC) m_vertexColors  = std::move(newC);
	if (hasN) m_vertexNormals = std::move(newN);

	IncrementRevision();
}

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
	const bool uvParallel    = !m_texCoords.empty()
	                        && m_texCoords.size() == 2u * m_nVertices;
	const bool normParallel  = !m_vertexNormals.empty()
	                        && m_vertexNormals.size()      == 3u * m_nVertices;
	const bool colorParallel = !m_vertexColors.empty()
	                        && m_vertexColors.size()       == 3u * m_nVertices;

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

				// Attribute checks against the winner (newOrig[j]). Vertex
				// normals are deliberately NOT a criterion: callers recompute
				// them after merging, so gating on them would only block welds
				// (e.g. at every facet boundary of an STL). Only authored data
				// a weld would corrupt — UV seams and colour islands — is kept.
				const unsigned int jOrig = newOrig[j];

				if (uvParallel)
				{
					const float du = m_texCoords[2 * jOrig    ] - m_texCoords[2 * i    ];
					const float dv = m_texCoords[2 * jOrig + 1] - m_texCoords[2 * i + 1];
					if (du * du + dv * dv > uvTol2) continue;
				}
				if (colorParallel)
				{
					const float dr = m_vertexColors[3 * jOrig    ] - m_vertexColors[3 * i    ];
					const float dg = m_vertexColors[3 * jOrig + 1] - m_vertexColors[3 * i + 1];
					const float db = m_vertexColors[3 * jOrig + 2] - m_vertexColors[3 * i + 2];
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
			newUVs[2 * ni    ] = m_texCoords[2 * orig    ];
			newUVs[2 * ni + 1] = m_texCoords[2 * orig + 1];
		}
		m_texCoords = std::move(newUVs);
		m_nTexCoords = nNewVertices;
	}

	if (normParallel)
	{
		std::vector<float> newNormals(3 * nNewVertices);
		for (unsigned int ni = 0; ni < nNewVertices; ++ni)
		{
			const unsigned int orig = newOrig[ni];
			newNormals[3 * ni    ] = m_vertexNormals[3 * orig    ];
			newNormals[3 * ni + 1] = m_vertexNormals[3 * orig + 1];
			newNormals[3 * ni + 2] = m_vertexNormals[3 * orig + 2];
		}
		m_vertexNormals = std::move(newNormals);
	}
	else if (!m_vertexNormals.empty())
	{
		// Best-effort: shrink to new size. Caller typically follows with
		// ComputeNormals() which fully overwrites this.
		m_vertexNormals.assign(3 * nNewVertices, 0.0f);
	}

	if (colorParallel)
	{
		std::vector<float> newColors(3 * nNewVertices);
		for (unsigned int ni = 0; ni < nNewVertices; ++ni)
		{
			const unsigned int orig = newOrig[ni];
			newColors[3 * ni    ] = m_vertexColors[3 * orig    ];
			newColors[3 * ni + 1] = m_vertexColors[3 * orig + 1];
			newColors[3 * ni + 2] = m_vertexColors[3 * orig + 2];
		}
		m_vertexColors = std::move(newColors);
	}
	else if (!m_vertexColors.empty())
	{
		// Size mismatch case: try a best-effort remap and clamp.
		std::vector<float> newColors(3 * nNewVertices, 0.0f);
		const size_t srcSize = m_vertexColors.size();
		for (unsigned int i = 0; i < m_nVertices; i++)
		{
			if (3u * i + 2 >= srcSize) continue;
			unsigned int ni = remap[i];
			newColors[3 * ni    ] = m_vertexColors[3 * i    ];
			newColors[3 * ni + 1] = m_vertexColors[3 * i + 1];
			newColors[3 * ni + 2] = m_vertexColors[3 * i + 2];
		}
		m_vertexColors = std::move(newColors);
	}

	unsigned int nOldVertices = m_nVertices;
	m_nVertices = nNewVertices;

	// Remap face indices and remove degenerate faces
	const unsigned int nf0 = GetNFaces ();
	std::vector<unsigned int> keep;
	keep.reserve (nf0);
	for (unsigned int i = 0; i < nf0; i++)
	{
		auto f = FaceAt (i);
		if (!f) continue;                     // trou : disparait avec la compaction

		for (unsigned int v = 0; v < (unsigned int)f->GetNVertices (); v++)
			f->SetVertex (v, remap[f->GetVertex (v)]);

		// Check for degenerate face (duplicate vertex indices)
		bool degenerate = false;
		for (unsigned int a = 0; a < (unsigned int)f->GetNVertices () && !degenerate; a++)
			for (unsigned int b = a + 1; b < (unsigned int)f->GetNVertices () && !degenerate; b++)
				if (f->GetVertex (a) == f->GetVertex (b))
					degenerate = true;

		if (!degenerate)
			keep.push_back (i);
	}
	KeepFaces (keep);

	delete[] remap;

	IncrementRevision();

	return (int)(nOldVertices - nNewVertices);
}

