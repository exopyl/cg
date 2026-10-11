#include <gtest/gtest.h>

#include <algorithm>
#include <cfloat>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"

// ===========================================================================
//  mesh_validation.h -- validateur de maillage independant
// ===========================================================================
//
// Chaque categorie de defaut est prouvee CAPABLE D'ECHOUER : un cas construit ou
// elle compte au moins un defaut (exactement, quand le compte est defini), et un
// temoin ou elle compte zero. Un compteur valide sur le seul temoin ne prouve
// rien.

namespace
{
    constexpr double kTol = 1e-6;

    Mesh makeMesh(const std::vector<float> &vertices, const std::vector<unsigned> &faces,
                  unsigned verticesPerFace = 3)
    {
        Mesh m;
        std::vector<float>    v = vertices;
        std::vector<unsigned> f = faces;
        m.SetVertices(static_cast<unsigned>(v.size() / 3), v.data());
        m.SetFaces(static_cast<unsigned>(f.size() / verticesPerFace), verticesPerFace, f.data());
        return m;
    }

    MeshValidationReport validate(const Mesh &m, double tol = kTol)
    {
        MeshValidationOptions o;
        o.weldTolerance = tol;
        return ValidateMesh(m, o);
    }

    std::size_t count(const MeshValidationReport &r, MeshDefect kind) { return r.Count(kind); }

    // Tetraedre unite, faces orientees vers l'exterieur : volume signe +1/6.
    const std::vector<float>    kTetVertices  = {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
    const std::vector<unsigned> kTetTriangles = {0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3};

    // Deux tetraedres : le second translate de (dx, dy, dz).
    Mesh twoTetrahedra(float dx, float dy, float dz)
    {
        std::vector<float> v = kTetVertices;
        for (std::size_t i = 0; i < kTetVertices.size(); i += 3)
            v.insert(v.end(), {kTetVertices[i] + dx, kTetVertices[i + 1] + dy, kTetVertices[i + 2] + dz});
        std::vector<unsigned> t = kTetTriangles;
        for (unsigned idx : kTetTriangles)
            t.push_back(idx + 4);
        return makeMesh(v, t);
    }

    // Tetraedre a un sommet par coin de face : 12 sommets.
    Mesh splitTetrahedron(float offset = 0.f)
    {
        std::vector<float>    v;
        std::vector<unsigned> t;
        for (unsigned idx : kTetTriangles)
        {
            for (int k = 0; k < 3; ++k)
                v.push_back(kTetVertices[3 * idx + k] + offset);
            t.push_back(static_cast<unsigned>(t.size()));
        }
        return makeMesh(v, t);
    }

    Mesh reversedVertexOrder(const Mesh &mesh)
    {
        const unsigned n = mesh.GetNVertices();
        const std::vector<float> &V = mesh.GetVertices();
        std::vector<float> v(V.size());
        for (unsigned i = 0; i < n; ++i)
            for (int k = 0; k < 3; ++k)
                v[3 * (n - 1 - i) + k] = V[3 * i + k];
        std::vector<unsigned> t;
        for (unsigned f = 0; f < mesh.GetNFaces(); ++f)
            for (unsigned k = 0; k < 3; ++k)
                t.push_back(n - 1 - static_cast<unsigned>(mesh.FaceAt(f)->GetVertex(k)));
        return makeMesh(v, t);
    }

    // Cube unite triangule (deux triangles par face, orientation sortante).
    const std::vector<float> kCubeVertices = {0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0,
                                              0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1};
    std::vector<unsigned> cubeTriangles()
    {
        const unsigned quads[6][4] = {{0, 2, 3, 1}, {4, 5, 7, 6}, {0, 1, 5, 4},
                                      {2, 6, 7, 3}, {0, 4, 6, 2}, {1, 3, 7, 5}};
        std::vector<unsigned> t;
        for (const auto &q : quads)
            t.insert(t.end(), {q[0], q[1], q[2], q[0], q[2], q[3]});
        return t;
    }

    // Sphere UV de rayon 1 : 2 * slices * (stacks - 1) triangles.
    Mesh uvSphere(unsigned stacks, unsigned slices)
    {
        const double pi = std::acos(-1.0);
        std::vector<float> v = {0.f, 0.f, 1.f};
        for (unsigned i = 1; i < stacks; ++i)
        {
            const double theta = pi * i / stacks;
            for (unsigned j = 0; j < slices; ++j)
            {
                const double phi = 2.0 * pi * j / slices;
                v.insert(v.end(), {static_cast<float>(std::sin(theta) * std::cos(phi)),
                                   static_cast<float>(std::sin(theta) * std::sin(phi)),
                                   static_cast<float>(std::cos(theta))});
            }
        }
        v.insert(v.end(), {0.f, 0.f, -1.f});
        const unsigned south = static_cast<unsigned>(v.size() / 3 - 1);
        auto ring = [slices](unsigned i, unsigned j) { return 1 + (i - 1) * slices + j % slices; };
        std::vector<unsigned> t;
        for (unsigned j = 0; j < slices; ++j)
            t.insert(t.end(), {0u, ring(1, j), ring(1, j + 1)});
        for (unsigned i = 1; i + 1 < stacks; ++i)
            for (unsigned j = 0; j < slices; ++j)
            {
                t.insert(t.end(), {ring(i, j), ring(i + 1, j), ring(i + 1, j + 1)});
                t.insert(t.end(), {ring(i, j), ring(i + 1, j + 1), ring(i, j + 1)});
            }
        for (unsigned j = 0; j < slices; ++j)
            t.insert(t.end(), {south, ring(stacks - 1, j + 1), ring(stacks - 1, j)});
        return makeMesh(v, t);
    }
}

// ---------------------------------------------------------------------------
//  Cas sain, soudure
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_mesh_validation, accepts_a_closed_tetrahedron)
{
    const MeshValidationReport r = validate(makeMesh(kTetVertices, kTetTriangles));
    EXPECT_TRUE(r.IsValid());
    EXPECT_TRUE(r.selfIntersectionChecked);
    EXPECT_EQ(r.weldedVertices, 4u);
    EXPECT_EQ(r.components, 1u);
    EXPECT_NEAR(r.signedVolume, 1.0 / 6.0, 1e-9);
    EXPECT_NEAR(r.area, 1.5 + std::sqrt(3.0) / 2.0, 1e-6);
    EXPECT_EQ(r.bbMin, (std::array<double, 3>{0, 0, 0}));
    EXPECT_EQ(r.bbMax, (std::array<double, 3>{1, 1, 1}));
    EXPECT_TRUE(r.records.empty());
}

// Un sommet par coin de face : fermee apres soudure seulement.
TEST(TEST_cgmesh_mesh_validation, welds_split_vertices)
{
    const MeshValidationReport r = validate(splitTetrahedron());
    EXPECT_EQ(r.weldedVertices, 4u);
    EXPECT_EQ(count(r, MeshDefect::BoundaryEdge), 0u);
    EXPECT_TRUE(r.IsValid());
    EXPECT_NEAR(r.signedVolume, 1.0 / 6.0, 1e-9);
}

// La soudure est transitive : une chaine de points espaces de 0.9 tol forme une
// seule grappe, meme si ses extremites sont a 1.8 tol.
TEST(TEST_cgmesh_mesh_validation, weld_is_transitive)
{
    const MeshValidationReport r = validate(makeMesh({0.f, 0, 0, 0.9e-3f, 0, 0, 1.8e-3f, 0, 0}, {0, 1, 2}), 1e-3);
    EXPECT_EQ(r.weldedVertices, 1u);
    EXPECT_EQ(count(r, MeshDefect::DegenerateFace), 1u);
}

// Deux points a plus de tol (1.98 tol, de part et d'autre de cellules) ne sont
// pas soudes.
TEST(TEST_cgmesh_mesh_validation, weld_respects_the_tolerance)
{
    const MeshValidationReport r = validate(makeMesh({-0.49e-3f, 0, 0, 1.49e-3f, 0, 0, 0, 1, 0}, {0, 1, 2}), 1e-3);
    EXPECT_EQ(r.weldedVertices, 3u);
}

TEST(TEST_cgmesh_mesh_validation, report_is_independent_of_vertex_order)
{
    const Mesh split = splitTetrahedron();
    const MeshValidationReport a = validate(split);
    const MeshValidationReport b = validate(reversedVertexOrder(split));
    EXPECT_EQ(a.counts, b.counts);
    EXPECT_EQ(a.weldedVertices, b.weldedVertices);
    EXPECT_EQ(a.components, b.components);
    EXPECT_DOUBLE_EQ(a.signedVolume, b.signedVolume);
}

// ---------------------------------------------------------------------------
//  Une categorie par test : cas fautif et temoin
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_mesh_validation, invalid_index)
{
    const MeshValidationReport bad = validate(makeMesh({0, 0, 0, 1, 0, 0, 0, 1, 0}, {0, 1, 7}));
    EXPECT_EQ(count(bad, MeshDefect::InvalidIndex), 1u);
    EXPECT_EQ(count(bad, MeshDefect::UnreferencedVertex), 3u) << "la face fautive est exclue";
    EXPECT_FALSE(bad.IsValid());
    EXPECT_EQ(count(validate(makeMesh(kTetVertices, kTetTriangles)), MeshDefect::InvalidIndex), 0u);
}

TEST(TEST_cgmesh_mesh_validation, non_triangle_face)
{
    const MeshValidationReport bad = validate(makeMesh({0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0}, {0, 1, 2, 3}, 4));
    EXPECT_EQ(count(bad, MeshDefect::NonTriangleFace), 1u);
    EXPECT_EQ(count(bad, MeshDefect::BoundaryEdge), 4u) << "le quad participe a la topologie";
    EXPECT_NEAR(bad.area, 1.0, 1e-9);
    EXPECT_EQ(count(validate(makeMesh(kTetVertices, kTetTriangles)), MeshDefect::NonTriangleFace), 0u);
}

TEST(TEST_cgmesh_mesh_validation, non_finite_vertex)
{
    std::vector<float> v = kTetVertices;
    v.insert(v.end(), {std::numeric_limits<float>::quiet_NaN(), 0.f, 0.f});
    v.insert(v.end(), {0.f, std::numeric_limits<float>::infinity(), 0.f});
    const MeshValidationReport bad = validate(makeMesh(v, kTetTriangles));
    EXPECT_EQ(count(bad, MeshDefect::NonFiniteVertex), 2u);
    EXPECT_EQ(bad.bbMax, (std::array<double, 3>{1, 1, 1})) << "la boite ignore les sommets non finis";
    EXPECT_EQ(count(validate(makeMesh(kTetVertices, kTetTriangles)), MeshDefect::NonFiniteVertex), 0u);
}

// Un sommet non fini CITE n'entre dans aucune mesure geometrique.
TEST(TEST_cgmesh_mesh_validation, referenced_non_finite_vertex_is_kept_out_of_geometry)
{
    std::vector<float> v = kTetVertices;
    v[9] = std::numeric_limits<float>::quiet_NaN();
    const MeshValidationReport bad = validate(makeMesh(v, kTetTriangles));
    EXPECT_EQ(count(bad, MeshDefect::NonFiniteVertex), 1u);
    EXPECT_TRUE(std::isfinite(bad.signedVolume));
    EXPECT_TRUE(std::isfinite(bad.area));
}

TEST(TEST_cgmesh_mesh_validation, unreferenced_vertex)
{
    std::vector<float> v = kTetVertices;
    v.insert(v.end(), {5.f, 5.f, 5.f});
    const MeshValidationReport bad = validate(makeMesh(v, kTetTriangles));
    EXPECT_EQ(count(bad, MeshDefect::UnreferencedVertex), 1u);
    EXPECT_EQ(bad.bbMax, (std::array<double, 3>{1, 1, 1})) << "la boite ignore les orphelins";
    EXPECT_EQ(count(validate(makeMesh(kTetVertices, kTetTriangles)), MeshDefect::UnreferencedVertex), 0u);
}

TEST(TEST_cgmesh_mesh_validation, degenerate_face)
{
    EXPECT_EQ(count(validate(makeMesh({0, 0, 0, 1, 0, 0, 2, 0, 0}, {0, 1, 2})), MeshDefect::DegenerateFace), 1u)
        << "triangle colineaire";
    EXPECT_EQ(count(validate(makeMesh({0, 0, 0, 1, 0, 0}, {0, 0, 1})), MeshDefect::DegenerateFace), 1u)
        << "indice repete";
    EXPECT_EQ(count(validate(makeMesh(kTetVertices, kTetTriangles)), MeshDefect::DegenerateFace), 0u);
}

// L'aire de degenerescence est reglable.
TEST(TEST_cgmesh_mesh_validation, degenerate_area_is_configurable)
{
    const Mesh tri = makeMesh({0, 0, 0, 1, 0, 0, 0, 1, 0}, {0, 1, 2});
    MeshValidationOptions o;
    o.weldTolerance  = kTol;
    o.degenerateArea = 0.6;
    EXPECT_EQ(count(ValidateMesh(tri, o), MeshDefect::DegenerateFace), 1u);
    o.degenerateArea = 0.4;
    EXPECT_EQ(count(ValidateMesh(tri, o), MeshDefect::DegenerateFace), 0u);
}

TEST(TEST_cgmesh_mesh_validation, duplicate_face)
{
    std::vector<unsigned> same = kTetTriangles;
    same.insert(same.end(), {1, 2, 3});
    const MeshValidationReport bad = validate(makeMesh(kTetVertices, same));
    EXPECT_EQ(count(bad, MeshDefect::DuplicateFace), 1u);

    std::vector<unsigned> reversed = kTetTriangles;
    reversed.insert(reversed.end(), {3, 2, 1});
    EXPECT_EQ(count(validate(makeMesh(kTetVertices, reversed)), MeshDefect::DuplicateFace), 1u)
        << "doublon d'orientation opposee";

    EXPECT_EQ(count(validate(makeMesh(kTetVertices, kTetTriangles)), MeshDefect::DuplicateFace), 0u);
}

TEST(TEST_cgmesh_mesh_validation, boundary_edge)
{
    const std::vector<unsigned> open(kTetTriangles.begin(), kTetTriangles.end() - 3);
    const MeshValidationReport bad = validate(makeMesh(kTetVertices, open));
    EXPECT_EQ(count(bad, MeshDefect::BoundaryEdge), 3u);
    EXPECT_EQ(count(bad, MeshDefect::NonManifoldEdge), 0u);
    EXPECT_EQ(count(bad, MeshDefect::InconsistentOrientation), 0u);
    EXPECT_EQ(count(validate(splitTetrahedron()), MeshDefect::BoundaryEdge), 0u);
}

TEST(TEST_cgmesh_mesh_validation, non_manifold_edge)
{
    // Trois triangles autour de l'arete (0, 1).
    const Mesh fin = makeMesh({0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, -1, 0}, {0, 1, 2, 1, 0, 3, 0, 1, 4});
    const MeshValidationReport bad = validate(fin);
    EXPECT_EQ(count(bad, MeshDefect::NonManifoldEdge), 1u);
    EXPECT_EQ(count(bad, MeshDefect::BoundaryEdge), 6u);
    EXPECT_EQ(count(validate(makeMesh(kTetVertices, kTetTriangles)), MeshDefect::NonManifoldEdge), 0u);
}

TEST(TEST_cgmesh_mesh_validation, non_manifold_vertex)
{
    // Deux tetraedres fermes qui ne partagent que le sommet 0.
    const Mesh pinch = makeMesh({0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, -1, -1, 0, 0, 0, -1, 0},
                                {0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3, 0, 4, 5, 0, 5, 6, 0, 6, 4, 4, 6, 5});
    const MeshValidationReport bad = validate(pinch);
    EXPECT_EQ(count(bad, MeshDefect::NonManifoldVertex), 1u);
    EXPECT_EQ(bad.components, 2u);
    EXPECT_EQ(count(bad, MeshDefect::BoundaryEdge), 0u);
    EXPECT_EQ(count(bad, MeshDefect::NonManifoldEdge), 0u);

    const MeshValidationReport disjoint = validate(twoTetrahedra(3.f, 0.f, 0.f));
    EXPECT_EQ(count(disjoint, MeshDefect::NonManifoldVertex), 0u);
    EXPECT_EQ(disjoint.components, 2u);
}

TEST(TEST_cgmesh_mesh_validation, inconsistent_orientation)
{
    std::vector<unsigned> t = kTetTriangles;
    std::swap(t[10], t[11]);
    const MeshValidationReport bad = validate(makeMesh(kTetVertices, t));
    EXPECT_EQ(count(bad, MeshDefect::InconsistentOrientation), 3u);
    EXPECT_EQ(count(bad, MeshDefect::BoundaryEdge), 0u);
    EXPECT_EQ(count(bad, MeshDefect::InvertedShell), 0u) << "une coque mal orientee n'a pas de volume signifiant";
    EXPECT_EQ(count(validate(makeMesh(kTetVertices, kTetTriangles)), MeshDefect::InconsistentOrientation), 0u);
}

TEST(TEST_cgmesh_mesh_validation, inverted_shell)
{
    std::vector<unsigned> t = kTetTriangles;
    for (std::size_t i = 0; i < t.size(); i += 3)
        std::swap(t[i + 1], t[i + 2]);
    const MeshValidationReport bad = validate(makeMesh(kTetVertices, t));
    EXPECT_EQ(count(bad, MeshDefect::InvertedShell), 1u);
    EXPECT_EQ(count(bad, MeshDefect::InconsistentOrientation), 0u);
    EXPECT_NEAR(bad.signedVolume, -1.0 / 6.0, 1e-9);

    const MeshValidationReport good = validate(makeMesh(kTetVertices, kTetTriangles));
    EXPECT_EQ(count(good, MeshDefect::InvertedShell), 0u);
    EXPECT_NEAR(good.signedVolume, 1.0 / 6.0, 1e-9);
}

// Evalue par composante : une coque inversee parmi deux saines est signalee
// meme si le volume global reste positif.
TEST(TEST_cgmesh_mesh_validation, inverted_shell_is_detected_per_component)
{
    std::vector<float> v = kTetVertices;
    std::vector<unsigned> t = kTetTriangles;
    for (int copy = 1; copy <= 2; ++copy)
    {
        const unsigned base = static_cast<unsigned>(v.size() / 3);
        for (std::size_t i = 0; i < kTetVertices.size(); i += 3)
            v.insert(v.end(), {kTetVertices[i] + 3.f * copy, kTetVertices[i + 1], kTetVertices[i + 2]});
        for (std::size_t i = 0; i < kTetTriangles.size(); i += 3)
        {
            const bool flip = copy == 2;
            t.insert(t.end(), {base + kTetTriangles[i], base + kTetTriangles[i + (flip ? 2 : 1)],
                               base + kTetTriangles[i + (flip ? 1 : 2)]});
        }
    }
    const MeshValidationReport r = validate(makeMesh(v, t));
    EXPECT_EQ(count(r, MeshDefect::InvertedShell), 1u);
    EXPECT_GT(r.signedVolume, 0.0);
}

TEST(TEST_cgmesh_mesh_validation, unexpected_component_count)
{
    const Mesh two = twoTetrahedra(3.f, 0.f, 0.f);
    MeshValidationOptions o;
    o.weldTolerance = kTol;
    o.expectedComponents = 1;
    EXPECT_EQ(count(ValidateMesh(two, o), MeshDefect::UnexpectedComponentCount), 1u);
    o.expectedComponents = 2;
    EXPECT_EQ(count(ValidateMesh(two, o), MeshDefect::UnexpectedComponentCount), 0u);
    o.expectedComponents.reset();
    EXPECT_EQ(count(ValidateMesh(two, o), MeshDefect::UnexpectedComponentCount), 0u);
}

// ---------------------------------------------------------------------------
//  Auto-intersection
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_mesh_validation, self_intersection_of_interpenetrating_tetrahedra)
{
    const MeshValidationReport r = validate(twoTetrahedra(0.25f, 0.25f, 0.25f));
    EXPECT_GE(count(r, MeshDefect::SelfIntersection), 1u);
    EXPECT_FALSE(r.IsValid());
    RecordProperty("pairs", static_cast<int>(count(r, MeshDefect::SelfIntersection)));
}

// Un sommet du cube pousse a travers la face opposee : ses triangles traversent
// la face du bas, avec laquelle ils ne partagent aucun sommet.
TEST(TEST_cgmesh_mesh_validation, self_intersection_of_a_folded_shell)
{
    std::vector<float> v = kCubeVertices;
    v[21] = 0.6f;
    v[22] = 0.3f;
    v[23] = -1.f;
    const MeshValidationReport r = validate(makeMesh(v, cubeTriangles()));
    EXPECT_GE(count(r, MeshDefect::SelfIntersection), 1u);
    EXPECT_EQ(count(r, MeshDefect::BoundaryEdge), 0u);
    RecordProperty("pairs", static_cast<int>(count(r, MeshDefect::SelfIntersection)));
    EXPECT_EQ(count(validate(makeMesh(kCubeVertices, cubeTriangles())), MeshDefect::SelfIntersection), 0u);
}

// Deux triangles coplanaires sans sommet commun dont les interieurs se
// recouvrent : une paire signalee.
TEST(TEST_cgmesh_mesh_validation, self_intersection_of_coplanar_overlapping_triangles)
{
    const Mesh m = makeMesh({0, 0, 0, 1, 0, 0, 0, 1, 0, 0.2f, 0.2f, 0, 1.2f, 0.2f, 0, 0.2f, 1.2f, 0},
                            {0, 1, 2, 3, 4, 5});
    EXPECT_EQ(count(validate(m), MeshDefect::SelfIntersection), 1u);
}

// Triangles coplanaires adjacents d'une meme face plane : pas de recouvrement.
TEST(TEST_cgmesh_mesh_validation, adjacent_coplanar_triangles_do_not_intersect)
{
    const Mesh square = makeMesh({0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0}, {0, 1, 2, 0, 2, 3});
    EXPECT_EQ(count(validate(square), MeshDefect::SelfIntersection), 0u);
}

// Repli a plat : deux triangles coplanaires a arete commune, du meme cote.
TEST(TEST_cgmesh_mesh_validation, self_intersection_of_a_flat_fold)
{
    const Mesh fold = makeMesh({0, 0, 0, 1, 0, 0, 0, 1, 0, 0.3f, 0.3f, 0}, {0, 1, 2, 1, 0, 3});
    EXPECT_EQ(count(validate(fold), MeshDefect::SelfIntersection), 1u);
}

// Paire a sommet commun : signalee si l'arete opposee de l'une perce l'autre,
// pas si elles ne font que se toucher en ce sommet.
TEST(TEST_cgmesh_mesh_validation, shared_vertex_pair_is_reported_only_when_pierced)
{
    const Mesh pierced = makeMesh({0, 0, 0, 1, 0, 0, 0, 1, 0, 0.3f, 0.3f, 1, 0.3f, 0.3f, -1},
                                  {0, 1, 2, 0, 3, 4});
    EXPECT_EQ(count(validate(pierced), MeshDefect::SelfIntersection), 1u);

    const Mesh touching = makeMesh({0, 0, 0, 1, 0, 0, 0, 1, 0, -1, 0, 1, 0, -1, 1}, {0, 1, 2, 0, 3, 4});
    EXPECT_EQ(count(validate(touching), MeshDefect::SelfIntersection), 0u);
}

// Traversee franche dont les deux extremites tombent sur des cotes : croix
// perpendiculaire a cordes identiques.
TEST(TEST_cgmesh_mesh_validation, self_intersection_of_a_perpendicular_cross)
{
    const Mesh cross = makeMesh({-1, -1, 0, 1, -1, 0, 0, 1, 0, -1, 0, -1, 1, 0, -1, 0, 0, 1}, {0, 1, 2, 3, 4, 5});
    EXPECT_EQ(count(validate(cross), MeshDefect::SelfIntersection), 1u);
}

// Deux lames a sommet commun qui se traversent au-dela de ce sommet ; temoins :
// lames de part et d'autre du sommet, contact au seul sommet, arete commune.
TEST(TEST_cgmesh_mesh_validation, self_intersection_of_blades_sharing_a_vertex)
{
    const Mesh blades = makeMesh({0, 0, 0, 2, -1, 0, 2, 1, 0, 2, 0, -1, 2, 0, 1}, {0, 1, 2, 0, 3, 4});
    EXPECT_EQ(count(validate(blades), MeshDefect::SelfIntersection), 1u);

    const Mesh opposite = makeMesh({0, 0, 0, 2, -1, 0, 2, 1, 0, -2, 0, -1, -2, 0, 1}, {0, 1, 2, 0, 3, 4});
    EXPECT_EQ(count(validate(opposite), MeshDefect::SelfIntersection), 0u) << "lames opposees";

    const Mesh vertexOnly = makeMesh({0, 0, 0, 1, 0, 0, 0, 1, 0, -1, 0, 1, 0, -1, 1}, {0, 1, 2, 0, 3, 4});
    EXPECT_EQ(count(validate(vertexOnly), MeshDefect::SelfIntersection), 0u) << "contact au seul sommet";

    const Mesh hinge = makeMesh({0, 0, 0, 1, 0, 0, 0, 1, 0, 0.5f, -1, 0.5f}, {0, 1, 2, 1, 0, 3});
    EXPECT_EQ(count(validate(hinge), MeshDefect::SelfIntersection), 0u) << "arete commune";
}

// Recouvrement coplanaire dont chaque sommet est un point de contact : A et B
// partagent (1, 1) et se recouvrent sur une aire 0.5.
TEST(TEST_cgmesh_mesh_validation, self_intersection_of_coplanar_overlap_bounded_by_contacts)
{
    const Mesh m = makeMesh({2, 1, 0, 2, 3, 0, 1, 1, 0, 2, 2, 0, 3, 1, 0}, {0, 1, 2, 3, 2, 4});
    EXPECT_EQ(count(validate(m), MeshDefect::SelfIntersection), 1u);

    const Mesh touching = makeMesh({0, 0, 0, 1, 0, 0, 0, 1, 0, -1, 0, 0, 0, -1, 0}, {0, 1, 2, 0, 3, 4});
    EXPECT_EQ(count(validate(touching), MeshDefect::SelfIntersection), 0u) << "contact au seul sommet";
}

namespace
{
    // SplitMix64 : graine fixe, suite reproductible sur toute plateforme.
    struct SplitMix64
    {
        std::uint64_t state;
        std::uint64_t next()
        {
            std::uint64_t z = (state += 0x9E3779B97F4A7C15ull);
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            return z ^ (z >> 31);
        }
        int coordinate() { return static_cast<int>(next() % 4); }
    };

    using P3 = std::array<double, 3>;
    P3 sub3(const P3 &a, const P3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
    P3 cross3(const P3 &a, const P3 &b)
    {
        return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    }
    double dot3(const P3 &a, const P3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

    enum class Truth { Crossing, Disjoint, Undecided };

    // Reference exacte, coordonnees entieres, triangles non coplanaires : il y
    // a traversee si chacun a des sommets strictement des deux cotes du plan de
    // l'autre et si leurs intervalles sur D = nA x nB se recouvrent. Au-dela de
    // 0.05 : traversee franche ; recouvrement <= 0 : aucune traversee.
    Truth crossingTruth(const std::array<P3, 6> &q)
    {
        const P3 nA = cross3(sub3(q[1], q[0]), sub3(q[2], q[0]));
        const P3 nB = cross3(sub3(q[4], q[3]), sub3(q[5], q[3]));
        const P3 D  = cross3(nA, nB);
        if (dot3(nA, nA) == 0 || dot3(nB, nB) == 0 || dot3(D, D) == 0)
            return Truth::Undecided;
        double dA[3], dB[3];
        for (int k = 0; k < 3; ++k)
        {
            dA[k] = dot3(sub3(q[k], q[3]), nB);
            dB[k] = dot3(sub3(q[3 + k], q[0]), nA);
        }
        auto straddles = [](const double *d) {
            return *std::max_element(d, d + 3) > 0 && *std::min_element(d, d + 3) < 0;
        };
        if (!straddles(dA) || !straddles(dB))
            return Truth::Disjoint;
        const double dn = std::sqrt(dot3(D, D));
        auto interval = [&](int o, const double *d, double &lo, double &hi) {
            lo = 1e300;
            hi = -1e300;
            for (int i = 0; i < 3; ++i)
            {
                const int j = (i + 1) % 3;
                const P3 &a = q[o + i], &b = q[o + j];
                auto take = [&](const P3 &x) {
                    const double s = dot3(x, D) / dn;
                    lo = std::min(lo, s);
                    hi = std::max(hi, s);
                };
                if (d[i] == 0)
                    take(a);
                if ((d[i] > 0 && d[j] < 0) || (d[i] < 0 && d[j] > 0))
                {
                    const double t = d[i] / (d[i] - d[j]);
                    take({a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1]), a[2] + t * (b[2] - a[2])});
                }
            }
        };
        double la, ha, lb, hb;
        interval(0, dA, la, ha);
        interval(3, dB, lb, hb);
        const double overlap = std::min(ha, hb) - std::max(la, lb);
        if (overlap > 0.05)
            return Truth::Crossing;
        return overlap <= 0 ? Truth::Disjoint : Truth::Undecided;
    }

    using P2 = std::array<double, 2>;
    double area2(const std::vector<P2> &p)
    {
        double a = 0;
        for (std::size_t i = 0; i < p.size(); ++i)
            a += p[i][0] * p[(i + 1) % p.size()][1] - p[i][1] * p[(i + 1) % p.size()][0];
        return a / 2;
    }

    // Reference exacte, triangles du plan z = 0 : aire du recouvrement par
    // decoupe de A par B. Au-dela de 0.01 : recouvrement ; nulle : disjoints.
    Truth coplanarTruth(const std::array<P3, 6> &q)
    {
        std::vector<P2> A = {{q[0][0], q[0][1]}, {q[1][0], q[1][1]}, {q[2][0], q[2][1]}};
        std::vector<P2> B = {{q[3][0], q[3][1]}, {q[4][0], q[4][1]}, {q[5][0], q[5][1]}};
        if (area2(A) == 0 || area2(B) == 0)
            return Truth::Undecided;
        if (area2(A) < 0)
            std::reverse(A.begin(), A.end());
        if (area2(B) < 0)
            std::reverse(B.begin(), B.end());
        std::vector<P2> sa = A, sb = B;
        std::sort(sa.begin(), sa.end());
        std::sort(sb.begin(), sb.end());
        if (sa == sb)
            return Truth::Undecided;
        std::vector<P2> poly = A;
        for (std::size_t i = 0; i < 3 && !poly.empty(); ++i)
        {
            const P2 a = B[i], b = B[(i + 1) % 3];
            auto side = [&](const P2 &p) { return (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0]); };
            std::vector<P2> out;
            for (std::size_t k = 0; k < poly.size(); ++k)
            {
                const P2 p = poly[k], r = poly[(k + 1) % poly.size()];
                const double sp = side(p), sr = side(r);
                if (sp >= 0)
                    out.push_back(p);
                if ((sp > 0 && sr < 0) || (sp < 0 && sr > 0))
                {
                    const double t = sp / (sp - sr);
                    out.push_back({p[0] + t * (r[0] - p[0]), p[1] + t * (r[1] - p[1])});
                }
            }
            poly = out;
        }
        const double a = poly.size() >= 3 ? std::fabs(area2(poly)) : 0.0;
        if (a > 0.01)
            return Truth::Crossing;
        return a < 1e-12 ? Truth::Disjoint : Truth::Undecided;
    }

    std::size_t selfIntersections(const std::array<P3, 6> &q, double tol)
    {
        std::vector<float> v;
        for (const P3 &p : q)
            for (double x : p)
                v.push_back(static_cast<float>(x));
        return count(validate(makeMesh(v, {0, 1, 2, 3, 4, 5}), tol), MeshDefect::SelfIntersection);
    }

    struct FuzzTally
    {
        int crossing = 0, missed = 0, disjoint = 0, falsePositive = 0;
    };

    FuzzTally fuzz(std::uint64_t seed, int pairs, bool coplanar, double tol = kTol)
    {
        SplitMix64 rng {seed};
        FuzzTally t;
        for (int it = 0; it < pairs; ++it)
        {
            std::array<P3, 6> q;
            for (P3 &p : q)
                p = {double(rng.coordinate()), double(rng.coordinate()), coplanar ? 0.0 : double(rng.coordinate())};
            const Truth truth = coplanar ? coplanarTruth(q) : crossingTruth(q);
            if (truth == Truth::Undecided)
                continue;
            const bool reported = selfIntersections(q, tol) > 0;
            if (truth == Truth::Crossing)
            {
                ++t.crossing;
                t.missed += reported ? 0 : 1;
            }
            else
            {
                ++t.disjoint;
                t.falsePositive += reported ? 1 : 0;
            }
        }
        return t;
    }
}

// Paires aleatoires sur une grille entiere, comparees a une reference exacte :
// aucune traversee franche manquee, aucune paire disjointe signalee.
TEST(TEST_cgmesh_mesh_validation, self_intersection_matches_exact_reference_on_random_pairs)
{
    const FuzzTally crossing = fuzz(0x5EEDu, 4000, false);
    EXPECT_GT(crossing.crossing, 300);
    EXPECT_GT(crossing.disjoint, 1500);
    EXPECT_EQ(crossing.missed, 0);
    EXPECT_EQ(crossing.falsePositive, 0);
    RecordProperty("crossing_cases", crossing.crossing);
    RecordProperty("non_crossing_cases", crossing.disjoint);

    const FuzzTally coplanar = fuzz(0xC0FFEEu, 4000, true);
    EXPECT_GT(coplanar.crossing, 500);
    EXPECT_GT(coplanar.disjoint, 300);
    EXPECT_EQ(coplanar.missed, 0);
    EXPECT_EQ(coplanar.falsePositive, 0);
    RecordProperty("coplanar_overlap_cases", coplanar.crossing);
    RecordProperty("coplanar_disjoint_cases", coplanar.disjoint);
}

// Le recouvrement coplanaire reste exact a tolerance tres fine : l'aire du
// polygone de recouvrement ne doit pas porter de bruit d'arrondi proportionnel
// a la taille du triangle.
TEST(TEST_cgmesh_mesh_validation, coplanar_overlap_matches_exact_reference_at_fine_tolerance)
{
    const FuzzTally coplanar = fuzz(0xC0FFEEu, 40000, true, 1e-9);
    EXPECT_GT(coplanar.crossing, 5000);
    EXPECT_GT(coplanar.disjoint, 3000);
    EXPECT_EQ(coplanar.missed, 0);
    EXPECT_EQ(coplanar.falsePositive, 0);
}

TEST(TEST_cgmesh_mesh_validation, separated_and_adjacent_triangles_do_not_intersect)
{
    EXPECT_EQ(count(validate(twoTetrahedra(1.1f, 0.f, 0.f)), MeshDefect::SelfIntersection), 0u)
        << "deux tetraedres a 0.1";
    EXPECT_EQ(count(validate(makeMesh(kTetVertices, kTetTriangles)), MeshDefect::SelfIntersection), 0u)
        << "faces adjacentes d'un tetraedre ferme";
}

TEST(TEST_cgmesh_mesh_validation, self_intersection_check_can_be_disabled)
{
    MeshValidationOptions o;
    o.weldTolerance = kTol;
    o.checkSelfIntersection = false;
    const MeshValidationReport r = ValidateMesh(twoTetrahedra(0.25f, 0.25f, 0.25f), o);
    EXPECT_FALSE(r.selfIntersectionChecked);
    EXPECT_EQ(count(r, MeshDefect::SelfIntersection), 0u);
}

namespace
{
    // Double cone ferme a n fuseaux : toutes les faces d'un cone partagent
    // l'apex, donc toutes leurs boites se recouvrent -- pire cas de la phase
    // large, sans aucune auto-intersection.
    Mesh bicone(unsigned n)
    {
        const double pi = std::acos(-1.0);
        std::vector<float> v = {0.f, 0.f, 1.f, 0.f, 0.f, -1.f};
        for (unsigned i = 0; i < n; ++i)
            v.insert(v.end(), {static_cast<float>(std::cos(2 * pi * i / n)),
                               static_cast<float>(std::sin(2 * pi * i / n)), 0.f});
        std::vector<unsigned> t;
        for (unsigned i = 0; i < n; ++i)
        {
            const unsigned a = 2 + i, b = 2 + (i + 1) % n;
            t.insert(t.end(), {0u, a, b});
            t.insert(t.end(), {1u, b, a});
        }
        return makeMesh(v, t);
    }
}

// Un test d'auto-intersection tronque par le budget ne certifie rien :
// IsValid() est faux, meme sans auto-intersection trouvee.
TEST(TEST_cgmesh_mesh_validation, truncated_self_intersection_check_is_not_valid)
{
    const Mesh cone = bicone(400);
    MeshValidationOptions o;
    o.weldTolerance = kTol;

    o.maxSelfIntersectionPairs = 1000;
    const MeshValidationReport truncated = ValidateMesh(cone, o);
    EXPECT_TRUE(truncated.selfIntersectionTruncated);
    EXPECT_EQ(truncated.selfIntersectionPairsTested, 1000u);
    EXPECT_EQ(count(truncated, MeshDefect::SelfIntersection), 0u);
    EXPECT_FALSE(truncated.IsValid());

    o.maxSelfIntersectionPairs.reset();
    const MeshValidationReport complete = ValidateMesh(cone, o);
    EXPECT_FALSE(complete.selfIntersectionTruncated);
    EXPECT_GE(complete.selfIntersectionPairBudget, kMinSelfIntersectionPairs);
    EXPECT_TRUE(complete.IsValid());
    RecordProperty("pairs_tested", static_cast<int>(complete.selfIntersectionPairsTested));
}

// Le budget par defaut croit avec le nombre de triangles.
TEST(TEST_cgmesh_mesh_validation, default_pair_budget_scales_with_triangle_count)
{
    const Mesh sphere = uvSphere(201, 250);
    MeshValidationOptions o;
    const MeshValidationReport r = ValidateMesh(sphere, o);
    EXPECT_EQ(r.selfIntersectionPairBudget,
              std::max(kMinSelfIntersectionPairs, kSelfIntersectionPairsPerTriangle * sphere.GetNFaces()));
    EXPECT_FALSE(r.selfIntersectionTruncated);
}

// 20 000 copies disjointes d'un meme triangle : 60 000 sommets en trois points.
// La soudure les reunit en temps lineaire.
TEST(TEST_cgmesh_mesh_validation, many_coincident_vertices_weld_in_linear_time)
{
    constexpr unsigned kCopies = 20000;
    std::vector<float>    v;
    std::vector<unsigned> t;
    for (unsigned i = 0; i < kCopies; ++i)
    {
        v.insert(v.end(), {0, 0, 0, 1, 0, 0, 0, 1, 0});
        t.insert(t.end(), {3 * i, 3 * i + 1, 3 * i + 2});
    }
    MeshValidationOptions o;
    o.weldTolerance = kTol;
    o.checkSelfIntersection = false;
    const MeshValidationReport r = ValidateMesh(makeMesh(v, t), o);
    EXPECT_EQ(r.weldedVertices, 3u);
    EXPECT_EQ(count(r, MeshDefect::DuplicateFace), kCopies - 1);
}

// ---------------------------------------------------------------------------
//  Enregistrements, noms, options
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_mesh_validation, records_are_capped_but_counts_are_not)
{
    std::vector<float>    v;
    std::vector<unsigned> t;
    for (unsigned i = 0; i < 10; ++i)
    {
        const float x = 3.f * i;
        v.insert(v.end(), {x, 0, 0, x + 1, 0, 0, x, 1, 0});
        t.insert(t.end(), {3 * i, 3 * i + 1, 3 * i + 2});
    }
    MeshValidationOptions o;
    o.weldTolerance = kTol;
    o.maxRecordsPerKind = 5;
    const MeshValidationReport r = ValidateMesh(makeMesh(v, t), o);
    EXPECT_EQ(count(r, MeshDefect::BoundaryEdge), 30u);
    const auto boundaryRecords = std::count_if(r.records.begin(), r.records.end(),
        [](const MeshDefectRecord &rec) { return rec.kind == MeshDefect::BoundaryEdge; });
    EXPECT_EQ(boundaryRecords, 5);
}

TEST(TEST_cgmesh_mesh_validation, records_locate_the_defect)
{
    const std::vector<unsigned> open(kTetTriangles.begin(), kTetTriangles.end() - 3);
    const MeshValidationReport r = validate(makeMesh(kTetVertices, open));
    ASSERT_EQ(r.records.size(), 3u);
    for (const MeshDefectRecord &rec : r.records)
    {
        EXPECT_EQ(rec.kind, MeshDefect::BoundaryEdge);
        ASSERT_EQ(rec.vertices.size(), 2u);
        ASSERT_EQ(rec.faces.size(), 1u);
        EXPECT_NE(rec.vertices[0], 0u) << "l'arete ouverte borde la face retiree (1, 2, 3)";
        EXPECT_NE(rec.vertices[1], 0u);
    }

    const MeshValidationReport si = validate(twoTetrahedra(0.25f, 0.25f, 0.25f));
    ASSERT_GT(count(si, MeshDefect::SelfIntersection), 0u);
    for (const MeshDefectRecord &rec : si.records)
        if (rec.kind == MeshDefect::SelfIntersection)
        {
            ASSERT_EQ(rec.faces.size(), 2u);
            EXPECT_LT(rec.faces[0], 4u) << "une face de chaque tetraedre";
            EXPECT_GE(rec.faces[1], 4u);
        }
}

TEST(TEST_cgmesh_mesh_validation, defect_names_are_stable_and_unique)
{
    std::set<std::string> names;
    for (std::size_t k = 0; k < kMeshDefectCount; ++k)
        names.insert(MeshDefectName(static_cast<MeshDefect>(k)));
    EXPECT_EQ(names.size(), kMeshDefectCount);
    EXPECT_STREQ(MeshDefectName(MeshDefect::BoundaryEdge), "boundary_edge");
    EXPECT_STREQ(MeshDefectName(MeshDefect::SelfIntersection), "self_intersection");
    EXPECT_STREQ(MeshDefectName(MeshDefect::UnexpectedComponentCount), "unexpected_component_count");
}

TEST(TEST_cgmesh_mesh_validation, rejects_invalid_tolerances)
{
    const Mesh tet = makeMesh(kTetVertices, kTetTriangles);
    MeshValidationOptions o;
    o.weldTolerance = 0.0;
    EXPECT_THROW(ValidateMesh(tet, o), std::invalid_argument);
    o.weldTolerance = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(ValidateMesh(tet, o), std::invalid_argument);
    o.weldTolerance = kTol;
    o.degenerateArea = -1.0;
    EXPECT_THROW(ValidateMesh(tet, o), std::invalid_argument);
}

// Loin de l'origine, deux copies d'un meme sommet calculees separement
// different d'un ulp float, bien plus que 1e-6 * diagonale. La tolerance par
// defaut doit les souder ; une tolerance purement relative ne le fait pas.
TEST(TEST_cgmesh_mesh_validation, default_tolerance_welds_one_ulp_apart_far_from_origin)
{
    constexpr float kOffset = 1.0e6f;
    Mesh m = splitTetrahedron(kOffset);
    std::vector<float> v = m.GetVertices();
    for (std::size_t i = 3; i < v.size(); i += 6)
        v[i] = std::nextafter(v[i], std::numeric_limits<float>::max());
    m.SetVertices(static_cast<unsigned>(v.size() / 3), v.data());

    const MeshValidationReport byDefault = ValidateMesh(m);
    EXPECT_GE(byDefault.weldTolerance, 4.0 * FLT_EPSILON * kOffset);
    EXPECT_EQ(byDefault.weldedVertices, 4u);
    EXPECT_EQ(count(byDefault, MeshDefect::BoundaryEdge), 0u);

    MeshValidationOptions relativeOnly;
    relativeOnly.weldTolerance = 1e-6 * std::sqrt(3.0);
    EXPECT_GT(count(ValidateMesh(m, relativeOnly), MeshDefect::BoundaryEdge), 0u);
}

TEST(TEST_cgmesh_mesh_validation, default_tolerance_follows_model_scale_near_origin)
{
    const Mesh tet = makeMesh(kTetVertices, kTetTriangles);
    EXPECT_NEAR(DefaultWeldTolerance(tet), 1e-6 * std::sqrt(3.0), 1e-12);
}

// Une sphere fine (>= 1e5 triangles) est une coque valide.
TEST(TEST_cgmesh_mesh_validation, fine_sphere_is_valid)
{
    const Mesh sphere = uvSphere(201, 250);
    ASSERT_GE(sphere.GetNFaces(), 100000u);
    const MeshValidationReport r = ValidateMesh(sphere);
    EXPECT_TRUE(r.IsValid());
    EXPECT_EQ(r.components, 1u);
    EXPECT_NEAR(r.signedVolume, 4.0 / 3.0 * std::acos(-1.0), 1e-3);
}
