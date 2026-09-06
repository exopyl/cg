// ============================================================================
//  K2 -- palette par couleur (svg_couleurs_faisabilite.md, section 12)
// ============================================================================
//
// K2 pose l'etage sur lequel K3 se posera : chaque region du document devient un
// Append portant le materiau de SA couleur, les couleurs identiques partageant
// un materiau. Le recouvrement n'est pas resolu -- les capots restent
// coplanaires -- et ce fichier ne juge donc rien de l'aspect a l'ecran.
//
// Deux invariants le tiennent :
//
//   - a `perShapeMaterials` faux, le maillage est CELUI DE K1, aux memes
//     comptes, et toutes ses faces sortent a MATERIAL_NONE ;
//   - a vrai, la palette compte les couleurs DISTINCTES, aucune face ne reste
//     sans materiau, et chaque materiau de la table porte au moins une face.
//
// Aucun compte du Tigre n'est ecrit en dur : les oracles sont des RELATIONS
// (le maillage a false est celui que produit svg_to_contours suivi d'un Append
// unique ; la palette compte les couleurs que portent les groupes), et les
// valeurs absolues sont imprimees. Un nombre fige ici lierait la suite a la
// tessellation d'un compilateur.
//
// ============================================================================

#include <gtest/gtest.h>

#include "../src/cgmesh/import_svg.h"
#include "../src/cgmesh/material.h"
#include "../src/cgmesh/mesh.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

const char* kTiger  = "./test/data/svg/Ghostscript_Tiger.svg";
const char* kSquare = "./test/data/svg/square.svg";

// Couleurs de REMPLISSAGE distinctes du Tigre, mesurees par K0 (section 16).
// La palette en compte davantage : les regions issues d'un trait portent la
// couleur de leur stroke, et l'une d'elles n'existe dans aucun remplissage.
const unsigned int kTigerFillColors = 38u;

SvgExtrudeOptions defaultOptions()
{
    SvgExtrudeOptions opt;
    opt.height       = 0.1f;
    opt.flattenTol   = 0.005f;
    opt.centerAndFit = true;
    opt.invertY      = true;
    return opt;
}

// Faces que personne n'a peintes, au sens exact du noeud de peinture
// (cggraph/nodes/mesh/color.cpp : GetFaceMaterialId(f) < 0). C'est l'indicateur
// `paintedFaces` de ce noeud, mesure ici au niveau du maillage.
unsigned int unpaintedFaces(const Mesh& m)
{
    unsigned int n = 0;
    for (unsigned int f = 0; f < m.GetNFaces(); ++f)
        if (m.GetFaceMaterialId(f) < 0) ++n;
    return n;
}

void writeSvg(const char* path, const char* body)
{
    std::ofstream f(path);
    f << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">\n" << body << "</svg>\n";
}

const MaterialColor* colorMaterial(const Mesh& m, unsigned int id)
{
    const Material* mat = m.GetMaterial(id);
    if (!mat || mat->GetType() != MATERIAL_COLOR) return nullptr;
    return static_cast<const MaterialColor*>(mat);
}

} // namespace

// ============================================================================
//  Critere 1 -- a false, rien ne bouge
// ============================================================================

// L'oracle n'est pas un compte mais le maillage de K1 lui-meme, reconstruit
// ici : la liste plate de contours, une seule tessellation, aucun materiau.
TEST(TEST_cgmesh_svg_k2, disabled_materials_reproduce_the_k1_mesh)
{
    for (const char* file : { kTiger, kSquare })
    {
        SvgExtrudeOptions opt = defaultOptions();
        ASSERT_FALSE(opt.perShapeMaterials);

        std::vector<ExtrudeContour> contours;
        ASSERT_TRUE(svg_to_contours(file, opt, contours)) << file;

        ExtrudeAppendOptions ao;
        ao.zBottom              = 0.0f;
        ao.zTop                 = opt.height;
        ao.winding              = ExtrudeWinding::NonZero;
        ao.normalizeOrientation = false;

        ExtrudedMeshBuilder builder;
        ASSERT_TRUE(builder.Append(contours, ao)) << file;
        std::unique_ptr<Mesh> expected(builder.Build());
        ASSERT_NE(expected, nullptr) << file;

        std::unique_ptr<Mesh> actual(import_svg_extruded(file, opt));
        ASSERT_NE(actual, nullptr) << file;

        ASSERT_EQ(actual->GetNVertices(), expected->GetNVertices()) << file;
        ASSERT_EQ(actual->GetNFaces(),    expected->GetNFaces())    << file;
        EXPECT_EQ(actual->GetNMaterials(), 0u) << file;

        for (unsigned int v = 0; v < expected->GetNVertices(); ++v)
        {
            float a[3], b[3];
            expected->GetVertex(v, a);
            actual->GetVertex(v, b);
            ASSERT_FLOAT_EQ(a[0], b[0]) << file << " sommet " << v;
            ASSERT_FLOAT_EQ(a[1], b[1]) << file << " sommet " << v;
            ASSERT_FLOAT_EQ(a[2], b[2]) << file << " sommet " << v;
        }
        for (unsigned int f = 0; f < expected->GetNFaces(); ++f)
        {
            ASSERT_EQ(expected->GetFaceNVertices(f), actual->GetFaceNVertices(f)) << file;
            for (int k = 0; k < expected->GetFaceNVertices(f); ++k)
                ASSERT_EQ(expected->GetFaceVertex(f, k), actual->GetFaceVertex(f, k))
                    << file << " face " << f;
        }
    }
}

// L'indicateur dont le grisage du selecteur se servira : a false, mesh.color
// peint TOUT.
TEST(TEST_cgmesh_svg_k2, disabled_materials_leave_every_face_unpainted)
{
    std::unique_ptr<Mesh> m(import_svg_extruded(kTiger, defaultOptions()));
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(unpaintedFaces(*m), m->GetNFaces());
}

// ============================================================================
//  Critere 2 -- a true, une palette de couleurs distinctes
// ============================================================================

TEST(TEST_cgmesh_svg_k2, tiger_palette_holds_one_material_per_distinct_color)
{
    SvgExtrudeOptions opt = defaultOptions();
    opt.perShapeMaterials = true;

    std::unique_ptr<Mesh> m(import_svg_extruded(kTiger, opt));
    ASSERT_NE(m, nullptr);

    // La palette compte les couleurs que portent les REGIONS produites, pas les
    // seules couleurs de remplissage du document : les 13 regions issues d'un
    // trait portent la couleur de leur stroke (strokeUsesStrokeColor).
    std::vector<SvgShapeGroup> groups;
    ASSERT_TRUE(svg_to_shape_groups(kTiger, opt, groups));

    std::set<unsigned int> fillColors, groupColors;
    for (const SvgShapeGroup& g : groups)
    {
        const unsigned int c = g.paint.hasFill ? g.paint.fillRGBA : g.paint.strokeRGBA;
        groupColors.insert(c);
        if (g.paint.hasFill) fillColors.insert(g.paint.fillRGBA);
    }
    EXPECT_EQ((unsigned int)fillColors.size(), kTigerFillColors);
    EXPECT_EQ(m->GetNMaterials(), (unsigned int)groupColors.size());

    // Une plage de rendu par materiau, et aucune plage pour MATERIAL_NONE.
    const Mesh::PolygonRenderData rd = m->BuildPolygonRenderData();
    EXPECT_EQ((unsigned int)rd.materialRanges.size(), m->GetNMaterials());
    for (const auto& r : rd.materialRanges)
        EXPECT_NE(r.materialId, (unsigned int)MATERIAL_NONE);

    EXPECT_EQ(unpaintedFaces(*m), 0u);

    // Chaque face pointe sur un materiau de la table, et chaque materiau porte
    // au moins une face : une palette plus large que ce qui est peint
    // publierait des lots de rendu vides.
    std::set<unsigned int> used;
    for (unsigned int f = 0; f < m->GetNFaces(); ++f)
    {
        const int id = m->GetFaceMaterialId(f);
        ASSERT_GE(id, 0);
        ASSERT_LT((unsigned int)id, m->GetNMaterials());
        used.insert((unsigned int)id);
    }
    EXPECT_EQ((unsigned int)used.size(), m->GetNMaterials());
}

// ============================================================================
//  Critere 3 -- square.svg : une couleur, une geometrie inchangee
// ============================================================================

TEST(TEST_cgmesh_svg_k2, square_keeps_its_geometry_and_gets_one_material)
{
    SvgExtrudeOptions opt = defaultOptions();
    opt.height            = 0.5f;
    opt.perShapeMaterials = true;

    std::unique_ptr<Mesh> m(import_svg_extruded(kSquare, opt));
    ASSERT_NE(m, nullptr);

    EXPECT_EQ(m->GetNVertices(),  16u);
    EXPECT_EQ(m->GetNFaces(),     12u);
    EXPECT_EQ(m->GetNMaterials(), 1u);
    EXPECT_EQ(unpaintedFaces(*m), 0u);
}

// ============================================================================
//  Predictions K2 (section 19) -- mesure, pas rattrapage
// ============================================================================

TEST(TEST_cgmesh_svg_k2, tiger_volumetry_with_per_shape_materials)
{
    SvgExtrudeOptions opt = defaultOptions();

    std::unique_ptr<Mesh> mono(import_svg_extruded(kTiger, opt));
    ASSERT_NE(mono, nullptr);

    opt.perShapeMaterials = true;
    std::unique_ptr<Mesh> perShape(import_svg_extruded(kTiger, opt));
    ASSERT_NE(perShape, nullptr);

    const unsigned int nv0 = mono->GetNVertices(),     nf0 = mono->GetNFaces();
    const unsigned int nv1 = perShape->GetNVertices(), nf1 = perShape->GetNFaces();

    std::cout << "  Tigre : monolithique -> par forme\n"
              << "    sommets   : " << nv0 << " -> " << nv1 << "\n"
              << "    faces     : " << nf0 << " -> " << nf1 << "\n"
              << "    materiaux : " << mono->GetNMaterials() << " -> "
              << perShape->GetNMaterials() << std::endl;
    RecordProperty("nVerticesMono",     (int)nv0);
    RecordProperty("nFacesMono",        (int)nf0);
    RecordProperty("nVerticesPerShape", (int)nv1);
    RecordProperty("nFacesPerShape",    (int)nf1);

    // Chaque Append emet quatre blocs de n sommets.
    EXPECT_EQ(nv1 % 4u, 0u);

    // La tessellation par forme ne resout plus les intersections ENTRE formes :
    // le polygone glutess unique creait un sommet de reprise (COMBINE) a chacune
    // d'elles, et les triangles qui les portaient.
    EXPECT_LT(nv1, nv0);
    EXPECT_LT(nf1, nf0);
}

// ============================================================================
//  overlapPolicy : les deux reglages produisent un maillage
// ============================================================================
//
// Subtract etait DECLARE et refuse tant que la marqueterie n'existait pas : un
// maillage non decoupe rendu sous ce reglage serait passe pour decoupe. Il est
// desormais honore, et ce test le constate au niveau ou le refus se produisait.
// La marqueterie elle-meme est verifiee par tu_cgmesh_svg_k3_subtract.cpp.
//
// square.svg n'a qu'une forme : elle ne recouvre rien, la geometrie est donc la
// meme sous les deux politiques -- c'est ce qui fait de ce fichier le garde-fou
// du chemin, et non de son effet.
TEST(TEST_cgmesh_svg_k2, subtract_overlap_policy_is_honoured)
{
    SvgExtrudeOptions opt = defaultOptions();
    opt.height            = 0.5f;

    std::unique_ptr<Mesh> none(import_svg_extruded(kSquare, opt));
    ASSERT_NE(none, nullptr);

    for (const bool perShape : { false, true })
    {
        opt.perShapeMaterials = perShape;
        opt.overlapPolicy     = SvgExtrudeOptions::OverlapPolicy::Subtract;

        SvgExtrudeMapping mapping;
        std::unique_ptr<Mesh> cut(import_svg_extruded(kSquare, opt, &mapping));
        ASSERT_NE(cut, nullptr) << "perShapeMaterials=" << perShape;

        EXPECT_EQ(cut->GetNVertices(), none->GetNVertices()) << perShape;
        EXPECT_EQ(cut->GetNFaces(),    none->GetNFaces())    << perShape;
        EXPECT_EQ(mapping.overlap.fullyCoveredGroups, 0u)    << perShape;
        EXPECT_NEAR(mapping.overlap.removedArea, 0.0, 1e-9)  << perShape;
    }
}

// ============================================================================
//  Les deux correspondances (section 11.5)
// ============================================================================

TEST(TEST_cgmesh_svg_k2, the_mapping_covers_every_group_and_every_face)
{
    SvgExtrudeOptions opt = defaultOptions();
    opt.perShapeMaterials = true;

    std::vector<SvgShapeGroup> groups;
    ASSERT_TRUE(svg_to_shape_groups(kTiger, opt, groups));

    SvgExtrudeMapping mapping;
    std::unique_ptr<Mesh> m(import_svg_extruded(kTiger, opt, &mapping));
    ASSERT_NE(m, nullptr);

    ASSERT_EQ(mapping.materialOfGroup.size(), groups.size());
    ASSERT_EQ(mapping.facesOfGroup.size(),    groups.size());

    // Les plages se suivent sans trou ni recouvrement et couvrent tout le
    // maillage : un Append par groupe, dans l'ordre des groupes.
    unsigned int next = 0;
    for (size_t g = 0; g < groups.size(); ++g)
    {
        const unsigned int first = mapping.facesOfGroup[g].first;
        const unsigned int count = mapping.facesOfGroup[g].second;
        ASSERT_EQ(first, next) << "groupe " << g;
        next += count;

        // Chaque face de la plage porte le materiau annonce pour le groupe.
        if (count > 0)
        {
            EXPECT_EQ((unsigned int)m->GetFaceMaterialId(first),
                      mapping.materialOfGroup[g]) << "groupe " << g;
            EXPECT_EQ((unsigned int)m->GetFaceMaterialId(first + count - 1),
                      mapping.materialOfGroup[g]) << "groupe " << g;
        }
    }
    EXPECT_EQ(next, m->GetNFaces());

    // La deduplication est visible dans la correspondance : moins de materiaux
    // que de groupes sur un fichier a couleurs repetees.
    const std::set<unsigned int> distinct(mapping.materialOfGroup.begin(),
                                          mapping.materialOfGroup.end());
    EXPECT_EQ((unsigned int)distinct.size(), m->GetNMaterials());
    EXPECT_LT(distinct.size(), groups.size());
}

// Le chemin monolithique ne rattache aucune face a un groupe : la correspondance
// de faces est VIDE plutot qu'inventee.
TEST(TEST_cgmesh_svg_k2, without_materials_the_face_mapping_is_empty)
{
    const SvgExtrudeOptions opt = defaultOptions();

    std::vector<SvgShapeGroup> groups;
    ASSERT_TRUE(svg_to_shape_groups(kSquare, opt, groups));

    SvgExtrudeMapping mapping;
    std::unique_ptr<Mesh> m(import_svg_extruded(kSquare, opt, &mapping));
    ASSERT_NE(m, nullptr);

    EXPECT_EQ(mapping.materialOfGroup.size(), groups.size());
    EXPECT_TRUE(mapping.facesOfGroup.empty());
    for (unsigned int id : mapping.materialOfGroup)
        EXPECT_EQ(id, (unsigned int)MATERIAL_NONE);
}

// ============================================================================
//  strokeUsesStrokeColor (section 11.4)
// ============================================================================
//
// Le parametre ne joue que sur une forme SANS remplissage porteuse d'un trait :
// le trait d'une forme fermee et remplie n'est pas epaissi. Il ne deplace aucun
// sommet, donc les deux reglages rendent la meme geometrie.

TEST(TEST_cgmesh_svg_k2, a_stroke_region_takes_its_stroke_color_by_default)
{
    const char* path = "k2_stroke_color.svg";
    writeSvg(path,
             "  <rect x=\"0\" y=\"0\" width=\"40\" height=\"40\" fill=\"#ff0000\"/>\n"
             "  <polyline points=\"60,0 80,20 60,40\" fill=\"none\" stroke=\"#0000ff\""
             " stroke-width=\"4\"/>\n");

    SvgExtrudeOptions opt = defaultOptions();
    opt.perShapeMaterials = true;
    ASSERT_TRUE(opt.strokeUsesStrokeColor);

    std::unique_ptr<Mesh> withStroke(import_svg_extruded(path, opt));
    ASSERT_NE(withStroke, nullptr);
    ASSERT_EQ(withStroke->GetNMaterials(), 2u);

    // Ordre des materiaux = ordre de premiere rencontre : le remplissage rouge,
    // puis le ruban bleu.
    const MaterialColor* fill   = colorMaterial(*withStroke, 0);
    const MaterialColor* ribbon = colorMaterial(*withStroke, 1);
    ASSERT_NE(fill,   nullptr);
    ASSERT_NE(ribbon, nullptr);
    EXPECT_FLOAT_EQ(fill->GetFloatRed(),    1.0f);
    EXPECT_FLOAT_EQ(fill->GetFloatBlue(),   0.0f);
    EXPECT_FLOAT_EQ(ribbon->GetFloatRed(),  0.0f);
    EXPECT_FLOAT_EQ(ribbon->GetFloatBlue(), 1.0f);

    // Reglage inverse : le ruban prend la couleur de son `fill`, absent donc
    // noir transparent.
    opt.strokeUsesStrokeColor = false;
    std::unique_ptr<Mesh> withFill(import_svg_extruded(path, opt));
    ASSERT_NE(withFill, nullptr);
    EXPECT_EQ(withFill->GetNVertices(), withStroke->GetNVertices());
    EXPECT_EQ(withFill->GetNFaces(),    withStroke->GetNFaces());
    ASSERT_EQ(withFill->GetNMaterials(), 2u);

    const MaterialColor* ribbon2 = colorMaterial(*withFill, 1);
    ASSERT_NE(ribbon2, nullptr);
    EXPECT_FLOAT_EQ(ribbon2->GetFloatBlue(),  0.0f);
    EXPECT_FLOAT_EQ(ribbon2->GetFloatAlpha(), 0.0f);

    std::remove(path);
}
