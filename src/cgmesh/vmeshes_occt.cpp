// STEP (ISO 10303) and IGES (ANSI Y14.26M) importers via OpenCASCADE
// Technology.
//
// Strategy:
//   - Parse with the format-specific reader (STEPControl_Reader or
//     IGESControl_Reader) → one TopoDS_Shape.
//   - Compute a bbox and derive an *adaptive* linear deflection ≈ 0.1% of
//     the bbox diagonal so the tessellation density scales with the
//     model regardless of its absolute size.
//   - Run BRepMesh_IncrementalMesh on the whole shape.
//   - Walk the FACES (TopAbs_FACE) and emit one cgmesh::Mesh per face —
//     preserves the CAD feature decomposition so callers can address each
//     cylinder / fillet / planar patch individually instead of dealing
//     with a single fused triangle soup.
//
// Out of scope (deliberate, for now):
//   - Colors and materials. Would need STEPCAFControl_Reader /
//     IGESCAFControl_Reader plus XCAF libraries (TKLCAF / TKXCAF /
//     TKXDESTEP). The plain readers used here are enough for geometry.
//   - Native unit preservation. OCCT's machinery translates file
//     coordinates to its working unit (default: millimeters). The
//     resulting Mesh therefore comes out in mm regardless of whether
//     the file declared INCH, M, etc. — that's still "the file's units"
//     in the sense that no extra normalization is applied on top.

#include "vmeshes.h"
#include "mesh.h"

// <cstdio> is used both by the active OCCT path and by the CG_HAS_OCCT=Off
// stubs at the bottom of the file — keep it outside the #ifdef so the
// stubs still compile when OCCT support is disabled.
#include <cstdio>

#ifdef CG_HAS_OCCT

#include <STEPControl_Reader.hxx>
#include <IGESControl_Reader.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <GCPnts_UniformDeflection.hxx>
#include <Poly_Triangulation.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
#include <cmath>

namespace {

// Common post-read pipeline for a freshly-imported OCCT shape. Emits:
//   - one cgmesh Mesh per TopoDS_Face (tessellated surface), then
//   - one extra Mesh gathering the FREE wireframe — edges that bound no face
//     (discretised into polyline segments) and standalone vertices (explicit
//     points) — so a file mixing solids and curves imports BOTH forms.
// Returns the count of meshes appended (0 if the shape carried neither a
// usable triangulation nor any free curve/point).
//
// Shared by STEP and IGES importers — only the reader-specific lines
// (the few that produce the TopoDS_Shape from a filename) live in the
// VMeshes::import_* members.
int tessellateAndAppend(const TopoDS_Shape& shape, std::vector<Mesh*>& out)
{
    if (shape.IsNull()) return 0;

    // Adaptive deflection: tighter than the model isn't useful (sub-
    // pixel triangulation), looser blurs out features. 0.1% of the
    // diagonal is a common starting point — exposes ~1000 triangles per
    // smooth surface. Bnd_Box::Get on a Void box yields sentinel values
    // that would make the math NaN-prone, so guard explicitly.
    Bnd_Box bbox;
    BRepBndLib::Add(shape, bbox);
    double linearDeflection = 0.01;
    if (!bbox.IsVoid()) {
        Standard_Real xmin, ymin, zmin, xmax, ymax, zmax;
        bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);
        const double dx = xmax - xmin;
        const double dy = ymax - ymin;
        const double dz = zmax - zmin;
        const double diagonal = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (diagonal > 0.0) linearDeflection = 0.001 * diagonal;
    }
    const double angularDeflection = 0.5;  // radians (~28°), OCCT default

    // The shape-taking BRepMesh_IncrementalMesh ctor runs Perform()
    // internally — no need to call it ourselves (doing so would
    // re-tessellate the whole shape).
    BRepMesh_IncrementalMesh mesher(shape, linearDeflection,
                                    /*isRelative*/ Standard_False,
                                    angularDeflection,
                                    /*isInParallel*/ Standard_True);

    int producedMeshes = 0;
    for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next()) {
        TopoDS_Face face = TopoDS::Face(exp.Current());
        TopLoc_Location loc;
        Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
        if (tri.IsNull() || tri->NbTriangles() == 0) continue;

        const gp_Trsf& trsf       = loc.Transformation();
        const bool     reversed   = (face.Orientation() == TopAbs_REVERSED);
        const bool     hasNormals = tri->HasNormals();

        Mesh* m = new Mesh();
        m->Init(static_cast<unsigned int>(tri->NbNodes()),
                static_cast<unsigned int>(tri->NbTriangles()));

        // Nodes — OCCT indices are 1-based; translate to cgmesh's 0-based.
        for (Standard_Integer i = 1; i <= tri->NbNodes(); ++i) {
            gp_Pnt p = tri->Node(i).Transformed(trsf);
            m->m_pVertices[3 * (i - 1) + 0] = static_cast<float>(p.X());
            m->m_pVertices[3 * (i - 1) + 1] = static_cast<float>(p.Y());
            m->m_pVertices[3 * (i - 1) + 2] = static_cast<float>(p.Z());
        }

        // Normals — OCCT can carry them on Poly_Triangulation. If the
        // face is topologically reversed we flip the per-vertex normal
        // so the resulting Mesh's normals always point outward in the
        // shape's intended orientation.
        if (hasNormals) {
            for (Standard_Integer i = 1; i <= tri->NbNodes(); ++i) {
                gp_Dir n(tri->Normal(i));
                if (reversed) n.Reverse();
                m->m_pVertexNormals[3 * (i - 1) + 0] = static_cast<float>(n.X());
                m->m_pVertexNormals[3 * (i - 1) + 1] = static_cast<float>(n.Y());
                m->m_pVertexNormals[3 * (i - 1) + 2] = static_cast<float>(n.Z());
            }
        }
        // If absent, m_pVertexNormals stays at the zero values set by
        // Mesh::InitVertices — the consumer typically falls back to
        // Mesh::ComputeNormals before rendering.

        // Triangles — flip winding when the face is reversed so the
        // (b - a) × (c - a) cross product is consistent with the
        // outward normal stored above.
        for (Standard_Integer t = 1; t <= tri->NbTriangles(); ++t) {
            Standard_Integer n1, n2, n3;
            tri->Triangle(t).Get(n1, n2, n3);
            if (reversed) std::swap(n2, n3);
            Face* f = m->m_pFaces[t - 1];
            f->SetVertex(0, static_cast<unsigned int>(n1 - 1));
            f->SetVertex(1, static_cast<unsigned int>(n2 - 1));
            f->SetVertex(2, static_cast<unsigned int>(n3 - 1));
        }

        out.push_back(m);
        ++producedMeshes;
    }

    // --- Wireframe geometry --------------------------------------------------
    //
    // Besides surfaces, a CAD file can carry free wireframe: IGES polylines /
    // lines / arcs / splines (types 106/110/100/104...) and standalone points
    // (type 116) that bound no surface. These survive as free TopoDS_Edge /
    // TopoDS_Vertex in the shape. We collect them into ONE extra Mesh so a file
    // that mixes solids and curves imports BOTH — surfaces as face meshes above,
    // curves/points as this wireframe mesh.
    //
    // "Free" is the key filter: an edge that bounds a face, or a vertex that is
    // an edge endpoint, is already covered by the surface / its own curve and
    // must be excluded (otherwise we would redraw every surface's boundary).
    // MapShapesAndAncestors gives each edge its owner faces (empty list => free)
    // and each vertex its owner edges (empty list => explicit point).
    //
    // Line vertices go into m_pVertices (indexed by m_pLines); only genuinely
    // standalone points go into m_pPoints — same convention as the OBJ l/p path.
    std::vector<float>        wireVerts;   // xyz, flat
    std::vector<unsigned int> wireLines;   // two vertex indices per segment
    std::vector<unsigned int> wirePoints;  // one vertex index per explicit point

    TopTools_IndexedDataMapOfShapeListOfShape edgeFaces;
    TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, edgeFaces);
    for (Standard_Integer i = 1; i <= edgeFaces.Extent(); ++i)
    {
        if (!edgeFaces(i).IsEmpty()) continue;   // boundary of a face -> skip
        const TopoDS_Edge edge = TopoDS::Edge(edgeFaces.FindKey(i));

        Standard_Real first = 0., last = 0.;
        if (BRep_Tool::Curve(edge, first, last).IsNull()) continue;  // no 3D curve

        BRepAdaptor_Curve curve(edge);
        GCPnts_UniformDeflection discretizer(curve, linearDeflection);
        if (!discretizer.IsDone() || discretizer.NbPoints() < 2) continue;

        const unsigned int base = static_cast<unsigned int>(wireVerts.size() / 3);
        for (Standard_Integer k = 1; k <= discretizer.NbPoints(); ++k)
        {
            const gp_Pnt p = discretizer.Value(k);
            wireVerts.push_back(static_cast<float>(p.X()));
            wireVerts.push_back(static_cast<float>(p.Y()));
            wireVerts.push_back(static_cast<float>(p.Z()));
        }
        for (Standard_Integer k = 0; k < discretizer.NbPoints() - 1; ++k)
        {
            wireLines.push_back(base + k);
            wireLines.push_back(base + k + 1);
        }
    }

    TopTools_IndexedDataMapOfShapeListOfShape vertexEdges;
    TopExp::MapShapesAndAncestors(shape, TopAbs_VERTEX, TopAbs_EDGE, vertexEdges);
    for (Standard_Integer i = 1; i <= vertexEdges.Extent(); ++i)
    {
        if (!vertexEdges(i).IsEmpty()) continue;   // edge endpoint -> not explicit
        const TopoDS_Vertex v = TopoDS::Vertex(vertexEdges.FindKey(i));
        const gp_Pnt p = BRep_Tool::Pnt(v);

        wirePoints.push_back(static_cast<unsigned int>(wireVerts.size() / 3));
        wireVerts.push_back(static_cast<float>(p.X()));
        wireVerts.push_back(static_cast<float>(p.Y()));
        wireVerts.push_back(static_cast<float>(p.Z()));
    }

    if (!wireLines.empty() || !wirePoints.empty())
    {
        const unsigned int nv = static_cast<unsigned int>(wireVerts.size() / 3);
        Mesh* w = new Mesh();
        w->Init(nv, 0);   // 0 faces: a pure wireframe / point mesh
        for (size_t k = 0; k < wireVerts.size(); ++k)
            w->m_pVertices[k] = wireVerts[k];
        w->m_pLines  = std::move(wireLines);
        w->m_pPoints = std::move(wirePoints);
        w->computebbox();

        out.push_back(w);
        ++producedMeshes;
    }

    return producedMeshes;
}

} // anonymous namespace

bool VMeshes::import_step(const char* filename)
{
    if (!filename) return false;

    // OCCT throws Standard_Failure (derives from std::exception) on
    // pathological geometry — self-intersecting surfaces, malformed
    // edges, etc. Wrap the whole pipeline so the failure surfaces as a
    // `return false` instead of unwinding through the wx caller's
    // wxBusyInfo and the surrounding GUI code.
    try {
        STEPControl_Reader reader;
        if (reader.ReadFile(filename) != IFSelect_RetDone) {
            std::fprintf(stderr, "VMeshes::import_step: read failed for %s\n", filename);
            return false;
        }
        reader.TransferRoots();
        return tessellateAndAppend(reader.OneShape(), m_Meshes) > 0;
    }
    catch (const Standard_Failure& e) {
        std::fprintf(stderr,
            "VMeshes::import_step: OCCT failure on %s: %s\n",
            filename, e.GetMessageString());
        return false;
    }
}

bool VMeshes::import_iges(const char* filename)
{
    if (!filename) return false;

    try {
        IGESControl_Reader reader;
        if (reader.ReadFile(filename) != IFSelect_RetDone) {
            std::fprintf(stderr, "VMeshes::import_iges: read failed for %s\n", filename);
            return false;
        }
        // IGES is laxer than STEP: TransferRoots() can silently drop
        // roots it doesn't know how to convert. Walk every root
        // explicitly so a bad entity doesn't abort the rest of the
        // file (and so OCCT's per-root failures don't bubble up).
        const Standard_Integer nbRoots = reader.NbRootsForTransfer();
        for (Standard_Integer i = 1; i <= nbRoots; ++i) {
            try { reader.TransferOneRoot(i); }
            catch (const Standard_Failure& e) {
                std::fprintf(stderr,
                    "VMeshes::import_iges: skipping root %d of %s: %s\n",
                    (int)i, filename, e.GetMessageString());
            }
        }
        return tessellateAndAppend(reader.OneShape(), m_Meshes) > 0;
    }
    catch (const Standard_Failure& e) {
        std::fprintf(stderr,
            "VMeshes::import_iges: OCCT failure on %s: %s\n",
            filename, e.GetMessageString());
        return false;
    }
}

#else  // !CG_HAS_OCCT

bool VMeshes::import_step(const char* /*filename*/)
{
    std::fprintf(stderr,
        "VMeshes::import_step: OCCT support not built in (CG_HAS_OCCT undefined)\n");
    return false;
}

bool VMeshes::import_iges(const char* /*filename*/)
{
    std::fprintf(stderr,
        "VMeshes::import_iges: OCCT support not built in (CG_HAS_OCCT undefined)\n");
    return false;
}

#endif  // CG_HAS_OCCT
