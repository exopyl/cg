#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../src/cgmesh/architecture_gothic.h"
#include "../src/cgmesh/contour_validation.h"
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "../src/cgmesh/parameterized.h"
#include "../src/cgmesh/parameterized_gothic.h"
#include "../src/cgmesh/polygon2.h"

// ===========================================================================
//  Fenetre gothique -- cas de reference geometrique
// ===========================================================================
//
// Ces tests figent la sortie de ParameterizedGothicWindow telle qu'elle est, par
// le validateur INDEPENDANT du generateur (ValidateMesh, mesh_validation.h).
// Deux familles :
//
//   - les proprietes saines (coque unique, fermee, orientee, sans
//     auto-intersection) sont des contrats ;
//   - les defauts connus sont figes a leur valeur ACTUELLE, sous un nom qui
//     enonce le defaut. Le jour ou il est corrige, le test casse et doit etre
//     inverse deliberement : une correction ne passe pas inapercue.
//
// La fenetre est faite de pieces a sommets partages, separees le long de la
// couture face/paroi : chaque position y est dupliquee, et les bords bruts sont
// les coutures. Toute la topologie se lit donc APRES soudure. La tolerance de
// soudure est ABSOLUE et fixee ici : une tolerance relative a la boite
// englobante changerait de sens avec l'echelle du modele, ce qu'un test de
// covariance d'echelle ne peut pas se permettre.

namespace
{
    // Tolerance de soudure de la fenetre, en unites monde (largeur par defaut 200).
    constexpr double kGothicWeldTol = 5e-4;

    // Valeurs de reference de la fenetre par defaut.
    constexpr double kDefaultVolume = 1159848.4;
    constexpr double kVolumeRelTol  = 1e-5;
    constexpr double kBBoxTol       = 1e-3;

    // Ecrit un parametre par son nom, quel que soit son type. Un nom inconnu est
    // une faute du test, pas un no-op : sinon une variante mal nommee reproduirait
    // silencieusement la fenetre par defaut.
    void setParam(IParameterized &shape, const std::string &name, double value)
    {
        for (Parameter &p : shape.GetParameters())
        {
            if (p.GetName() != name)
                continue;
            switch (p.GetType())
            {
            case Parameter::FLOAT: p.SetFloat(static_cast<float>(value)); return;
            case Parameter::INT:
            case Parameter::ENUM:  p.SetInt(static_cast<int>(value)); return;
            case Parameter::BOOL:  p.SetBool(value != 0.0); return;
            case Parameter::STRING: break;
            }
            ADD_FAILURE() << "parametre non numerique : " << name;
            return;
        }
        ADD_FAILURE() << "parametre inconnu : " << name;
    }

    using Config = std::function<void(ParameterizedGothicWindow &)>;

    std::unique_ptr<Mesh> generate(const Config &cfg = {})
    {
        ParameterizedGothicWindow shape;
        if (cfg)
            cfg(shape);
        shape.Regenerate();
        return std::unique_ptr<Mesh>(shape.TakeMesh());
    }

    MeshValidationReport validate(const Mesh &mesh, bool selfIntersection = true)
    {
        MeshValidationOptions options;
        options.weldTolerance         = kGothicWeldTol;
        options.checkSelfIntersection = selfIntersection;
        return ValidateMesh(mesh, options);
    }

    std::size_t count(const MeshValidationReport &r, MeshDefect kind) { return r.Count(kind); }

    // Aretes de bord sur les indices BRUTS, avant toute soudure.
    std::size_t rawBorderEdges(const Mesh &mesh)
    {
        std::vector<unsigned> nonManifold, borders;
        mesh.GetTopologicIssues(nonManifold, borders);
        return borders.size() / 2;
    }

    // Publie les compteurs dans la sortie XML de gtest : les tests de defauts
    // figes n'assertent qu'un seuil, la valeur mesuree reste ainsi lisible.
    void recordReport(const MeshValidationReport &r)
    {
        for (std::size_t k = 0; k < kMeshDefectCount; ++k)
            ::testing::Test::RecordProperty(MeshDefectName(static_cast<MeshDefect>(k)),
                                            static_cast<int>(r.counts[k]));
        ::testing::Test::RecordProperty("welded", static_cast<int>(r.weldedVertices));
        ::testing::Test::RecordProperty("pairs_tested", static_cast<int>(r.selfIntersectionPairsTested));
        ::testing::Test::RecordProperty("components", static_cast<int>(r.components));
        std::ostringstream volume;
        volume.precision(10);
        volume << r.signedVolume;
        ::testing::Test::RecordProperty("volume", volume.str());
    }

    // Aucune categorie de defaut, faces degenerees comprises : les triangles
    // d'aire nulle que le balayage de glutess laisse sur les capots sont
    // bascules par la tessellation (Polygon2::tesselate).
    void expectNoMeshDefect(const MeshValidationReport &r)
    {
        for (std::size_t k = 0; k < kMeshDefectCount; ++k)
        {
            const auto kind = static_cast<MeshDefect>(k);
            EXPECT_EQ(r.Count(kind), 0u) << MeshDefectName(kind);
        }
        EXPECT_FALSE(r.selfIntersectionTruncated);
        EXPECT_TRUE(r.IsValid());
    }

    void expectDefaultBBox(const MeshValidationReport &r)
    {
        const double lo[3] = {-116.0, -260.0, 0.0};
        const double hi[3] = {116.0, 191.4576, 20.0};
        for (int k = 0; k < 3; ++k)
        {
            EXPECT_NEAR(r.bbMin[k], lo[k], kBBoxTol) << "axe " << k;
            EXPECT_NEAR(r.bbMax[k], hi[k], kBBoxTol) << "axe " << k;
        }
    }

    // Meme maillage, sommets numerotes en ordre inverse.
    Mesh reversedVertexOrder(const Mesh &mesh)
    {
        const unsigned n = mesh.GetNVertices();
        const std::vector<float> &V = mesh.GetVertices();
        std::vector<float> v(V.size());
        for (unsigned i = 0; i < n; ++i)
            for (int k = 0; k < 3; ++k)
                v[3 * (n - 1 - i) + k] = V[3 * i + k];
        std::vector<unsigned> t;
        t.reserve(3 * mesh.GetNFaces());
        for (unsigned f = 0; f < mesh.GetNFaces(); ++f)
        {
            const auto face = mesh.FaceAt(f);
            EXPECT_TRUE(face && face->GetNVertices() == 3) << "face " << f << " non triangulaire";
            if (!face || face->GetNVertices() != 3)
                continue;
            for (unsigned k = 0; k < 3; ++k)
                t.push_back(n - 1 - static_cast<unsigned>(face->GetVertex(k)));
        }
        Mesh m;
        m.SetVertices(n, v.data());
        m.SetFaces(static_cast<unsigned>(t.size() / 3), 3, t.data());
        return m;
    }

    // Remonte depuis le repertoire courant jusqu'a la racine du depot (test/data
    // et src/cgmath presents).
    std::filesystem::path findProjectRoot()
    {
        std::filesystem::path root = std::filesystem::current_path();
        for (int i = 0; i < 6; ++i)
        {
            if (std::filesystem::exists(root / "test" / "data") &&
                std::filesystem::exists(root / "src" / "cgmath"))
                return root;
            root = root.parent_path();
        }
        return std::filesystem::current_path();
    }

    std::string readFile(const std::string &path)
    {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }
}

// ---------------------------------------------------------------------------
//  Fenetre par defaut : contrats
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_gothic_baseline, default_window_is_a_single_closed_oriented_shell)
{
    const auto mesh = generate();
    ASSERT_TRUE(mesh);
    const MeshValidationReport r = validate(*mesh);
    recordReport(r);
    EXPECT_EQ(mesh->GetNVertices(), 7268u);
    EXPECT_EQ(mesh->GetNFaces(), 7312u);
    EXPECT_EQ(r.weldedVertices, 3634u);
    EXPECT_TRUE(r.IsValid());
    EXPECT_EQ(count(r, MeshDefect::SelfIntersection), 0u);
    EXPECT_EQ(r.components, 1u);
    EXPECT_NEAR(r.signedVolume, kDefaultVolume, kVolumeRelTol * kDefaultVolume);
    expectDefaultBBox(r);
}

// Chaque position est dupliquee le long de la couture face/paroi ; sur les
// indices bruts, les bords sont les coutures. La fermeture ne se lit qu'apres
// soudure.
TEST(TEST_cgmesh_gothic_baseline, default_window_raw_index_topology_is_open_by_construction)
{
    const auto mesh = generate();
    ASSERT_TRUE(mesh);
    const MeshValidationReport r = validate(*mesh, false);
    EXPECT_EQ(rawBorderEdges(*mesh), 7268u);
    EXPECT_EQ(mesh->GetNVertices(), 2 * r.weldedVertices);
}

TEST(TEST_cgmesh_gothic_baseline, default_window_is_deterministic)
{
    const auto a = generate();
    const auto b = generate();
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    EXPECT_EQ(a->GetVertices(), b->GetVertices());
    EXPECT_EQ(a->GetTriangles(), b->GetTriangles());
}

TEST(TEST_cgmesh_gothic_baseline, chamfer_profile_is_a_single_closed_shell)
{
    const auto mesh = generate([](ParameterizedGothicWindow &s) { setParam(s, "Profile", 1); });
    ASSERT_TRUE(mesh);
    const MeshValidationReport r = validate(*mesh);
    recordReport(r);
    constexpr double kChamferVolume = 1064144.3;
    EXPECT_EQ(mesh->GetNFaces(), 10946u);
    EXPECT_EQ(r.weldedVertices, 5451u);
    EXPECT_EQ(count(r, MeshDefect::BoundaryEdge), 0u);
    EXPECT_EQ(count(r, MeshDefect::NonManifoldEdge), 0u);
    EXPECT_EQ(count(r, MeshDefect::InconsistentOrientation), 0u);
    EXPECT_FALSE(r.selfIntersectionTruncated);
    EXPECT_EQ(count(r, MeshDefect::SelfIntersection), 0u);
    EXPECT_EQ(r.components, 1u);
    EXPECT_NEAR(r.signedVolume, kChamferVolume, kVolumeRelTol * kChamferVolume);
}

// Chanfrein dont l'anneau final n'est pas valide (apex replie, anneaux qui se
// touchent) : la largeur du profil est reduite sur les contours en cause. La
// coque est stricte -- une composante, fermee, manifold, sans auto-intersection
// ni face degeneree, soudure a 5e-4 x W / 200 -- et la reduction est nommee.
namespace
{
    void expectStrictReducedChamfer(const std::vector<std::pair<const char *, double>> &params)
    {
        ParameterizedGothicWindow shape;
        setParam(shape, "Profile", 1);
        for (const auto &[name, value] : params)
            setParam(shape, name, value);
        shape.Regenerate();
        const std::unique_ptr<Mesh> mesh(shape.TakeMesh());
        ASSERT_TRUE(mesh);
        ASSERT_GT(mesh->GetNFaces(), 1u) << "triangle de repli";

        MeshValidationOptions options;
        options.weldTolerance      = 5e-4 * shape.Settings().width / 200.0;
        options.expectedComponents = 1u;
        const MeshValidationReport r = ValidateMesh(*mesh, options);
        recordReport(r);
        EXPECT_EQ(r.components, 1u);
        expectNoMeshDefect(r);

        const GothicBuildReport &report = shape.LastBuildReport();
        EXPECT_FALSE(report.HasErrors());
        EXPECT_EQ(report.Count(GothicBuildWarning::ProfileFallback), 0u);
        EXPECT_GT(report.Count(GothicBuildWarning::ProfileReduced), 0u);
        for (const GothicDiagnostic &d : report.entries)
            if (!d.error && d.code == static_cast<int>(GothicBuildWarning::ProfileReduced))
            {
                EXPECT_EQ(d.parameter, "Profile");
                EXPECT_EQ(d.message.rfind("contour ", 0), 0u) << d.message;
            }
    }
}

TEST(TEST_cgmesh_gothic_baseline, chamfer_on_a_narrow_bay_is_a_single_closed_shell)
{
    expectStrictReducedChamfer({{"Width", 120}, {"Offset outer", 30}, {"Lancet drop", 50}});
}

TEST(TEST_cgmesh_gothic_baseline, chamfer_on_recursion_2_is_a_single_closed_shell)
{
    expectStrictReducedChamfer({{"Recursion", 2}});
}

TEST(TEST_cgmesh_gothic_baseline, chamfer_with_offset_inner_40_is_a_single_closed_shell)
{
    expectStrictReducedChamfer({{"Offset inner", 40}});
}

// ---------------------------------------------------------------------------
//  Defauts connus, figes a leur valeur actuelle
// ---------------------------------------------------------------------------

// Regenerate fusionne les barres du menu a la plaque : Roll, Keel et Ogee
// donnent une seule coque fermee, sans aucun defaut de ValidateMesh.
TEST(TEST_cgmesh_gothic_baseline, bar_profiles_regenerate_a_single_closed_shell)
{
    for (const int profile : {2, 3, 4})
    {
        SCOPED_TRACE("Profile " + std::to_string(profile));
        const auto mesh = generate([profile](ParameterizedGothicWindow &s) { setParam(s, "Profile", profile); });
        ASSERT_TRUE(mesh);
        MeshValidationOptions options;
        options.weldTolerance      = kGothicWeldTol;
        options.expectedComponents = 1u;
        const MeshValidationReport r = ValidateMesh(*mesh, options);
        recordReport(r);
        EXPECT_EQ(r.components, 1u);
        for (size_t q = 0; q < kMeshDefectCount; ++q)
            if ((MeshDefect)q != MeshDefect::UnexpectedComponentCount)
                EXPECT_EQ(r.counts[q], 0u) << MeshDefectName((MeshDefect)q);
        EXPECT_FALSE(r.selfIntersectionTruncated);
    }
}

// Barres de profil fusionnees a la plaque (BuildFusedBay) : une seule coque
// fermee, stricte (aucun defaut de ValidateMesh, soudure 5e-4 x largeur / 200),
// sans niveau saute ni retire, pour Roll, Keel et Ogee ; baie par defaut, deux
// baies de Recursion 2 dont la pierre et le pied des barres se touchent en sens
// opposes sans remplissage (rand107, rand280), et une ligne du tableau de
// couverture a niveaux serres (cov7_4).
TEST(TEST_cgmesh_gothic_baseline, bar_profiles_are_a_single_closed_shell)
{
    using Params = std::vector<std::pair<const char *, double>>;
    struct Bay { const char *name; double width; Params params; };
    const std::vector<Bay> bays = {
        {"defaut", 200.0, {}},
        {"rand107", 444.71189914711852,
         {{"Width", 444.71189914711852}, {"Excess (arch)", 1.8907876792804663}, {"Body height", 505.69444134219106},
          {"Offset outer", 4.4194249963705579}, {"Offset inner", 19.995677513365205}, {"Lancet head", 1},
          {"Lancet head foils", 6}, {"Recursion", 2}, {"Lancet drop", 38.956790284334673},
          {"Lancet excess", 1.9614216329292016}, {"Gap fraction", 0.141702122738119}, {"Lancet layout", 1},
          {"Fillets", 0}, {"Mouchettes", 0}, {"Mouchette type", 1}, {"Rosette", 1}, {"Rosette foils", 0},
          {"Rosette foil count", 18}, {"Rosette foil type", 0}, {"Rosette pointedness", 1.3500406287787405},
          {"Extrusion (zHeight)", 99.146461108469339}}},
        {"rand280", 486.1161507958073,
         {{"Width", 486.1161507958073}, {"Excess (arch)", 1.2170267610897076}, {"Body height", 171.34428444265822},
          {"Offset outer", 3.6231149510477856}, {"Offset inner", 17.467781883378191}, {"Lancet head", 1},
          {"Lancet head foils", 8}, {"Recursion", 2}, {"Lancet drop", 71.844044322737162},
          {"Lancet excess", 1.7972382130100362}, {"Gap fraction", 0.12644968239231505}, {"Lancet layout", 0},
          {"Fillets", 0}, {"Mouchettes", 0}, {"Mouchette type", 2}, {"Rosette", 1}, {"Rosette foils", 1},
          {"Rosette foil count", 4}, {"Rosette foil type", 0}, {"Rosette pointedness", 0.236235901377709},
          {"Extrusion (zHeight)", 96.879409525534896}}},
        {"cov7_4", 600.0,
         {{"Excess (arch)", 0.7}, {"Width", 600}, {"Body height", 0}, {"Offset outer", 40}, {"Offset inner", 0},
          {"Lancet head", 1}, {"Lancet head foils", 8}, {"Recursion", 2}, {"Lancet drop", 100},
          {"Lancet excess", 2.5}, {"Gap fraction", 0.2}, {"Lancet layout", 1}, {"Fillets", 0}, {"Mouchettes", 0},
          {"Mouchette type", 2}, {"Rosette", 1}, {"Rosette foils", 0}, {"Rosette foil count", 3},
          {"Rosette foil type", 0}, {"Rosette pointedness", 0.01}, {"Extrusion (zHeight)", 1}}},
    };
    for (const Bay &bay : bays)
        for (const int profile : {2, 3, 4})
        {
            SCOPED_TRACE(std::string(bay.name) + ", Profile " + std::to_string(profile));
            ParameterizedGothicWindow w;
            for (const auto &p : bay.params)
                setParam(w, p.first, p.second);
            setParam(w, "Profile", profile);
            Mesh m;
            FusedBarsResult r;
            ASSERT_TRUE(w.BuildFusedBay(m, &r));
            EXPECT_EQ(r.skippedLevels, 0);
            EXPECT_EQ(r.nesting.droppedLevels, 0);
            MeshValidationOptions options;
            options.weldTolerance      = kGothicWeldTol * bay.width / 200.0;
            options.expectedComponents = 1u;
            const MeshValidationReport v = ValidateMesh(m, options);
            EXPECT_EQ(v.components, 1u);
            for (size_t q = 0; q < kMeshDefectCount; ++q)
                if ((MeshDefect)q != MeshDefect::UnexpectedComponentCount)
                    EXPECT_EQ(v.counts[q], 0u) << MeshDefectName((MeshDefect)q);
            EXPECT_FALSE(v.selfIntersectionTruncated);
            EXPECT_GT(v.signedVolume, 0.0);
        }
}

// La recursion est calculee par un seul booleen 2D (pierre = cadre - reunion des
// vides, architecture_gothic_region.h). Avec les autres parametres par defaut,
// le solide est une coque fermee, manifold, sans auto-intersection ni face
// degeneree, d'un seul tenant : le sous-remplage reste relie au cadre.
TEST(TEST_cgmesh_gothic_baseline, recursive_tracery_is_a_single_closed_manifold_shell)
{
    for (const int depth : {1, 2})
    {
        SCOPED_TRACE("Recursion = " + std::to_string(depth));
        const auto mesh = generate([depth](ParameterizedGothicWindow &s) { setParam(s, "Recursion", depth); });
        ASSERT_TRUE(mesh);
        MeshValidationOptions options;
        options.weldTolerance      = kGothicWeldTol;
        options.expectedComponents = 1u;
        const MeshValidationReport r = ValidateMesh(*mesh, options);
        EXPECT_EQ(r.components, 1u);
        EXPECT_FALSE(r.selfIntersectionTruncated);
        EXPECT_EQ(count(r, MeshDefect::SelfIntersection), 0u);
        expectNoMeshDefect(r);
    }
}

// Le validateur rend le meme rapport quel que soit l'ordre des sommets, sur un
// maillage reel de la taille d'une fenetre recursive.
TEST(TEST_cgmesh_gothic_baseline, validation_is_independent_of_vertex_order_on_recursion)
{
    const auto mesh = generate([](ParameterizedGothicWindow &s) { setParam(s, "Recursion", 1); });
    ASSERT_TRUE(mesh);
    const MeshValidationReport a = validate(*mesh);
    const MeshValidationReport b = validate(reversedVertexOrder(*mesh));
    EXPECT_EQ(a.counts, b.counts);
    EXPECT_EQ(a.weldedVertices, b.weldedVertices);
    EXPECT_EQ(a.components, b.components);
    EXPECT_NEAR(a.signedVolume, b.signedVolume, 1e-9 * std::fabs(a.signedVolume));
}

// Les mouchettes ne laissent aucun triangle degenere, et la coque reste fermee.
TEST(TEST_cgmesh_gothic_baseline, mouchettes_produce_no_degenerate_triangles)
{
    const auto mesh = generate([](ParameterizedGothicWindow &s) { setParam(s, "Mouchettes", 1); });
    ASSERT_TRUE(mesh);
    const MeshValidationReport r = validate(*mesh);
    recordReport(r);
    EXPECT_EQ(count(r, MeshDefect::DegenerateFace), 0u);
    EXPECT_EQ(count(r, MeshDefect::BoundaryEdge), 0u);
}

// ---------------------------------------------------------------------------
//  Covariance d'echelle
// ---------------------------------------------------------------------------
//
// La chaine calcule dans un repere canonique de largeur 200 et n'applique
// l'echelle W / 200 qu'en sortie. Les seuils d'elements (GothicFeatureThresholds)
// sont des longueurs monde : la fenetre reduite d'un facteur 10 est la fenetre
// par defaut reduite d'un facteur 10 quand ses seuils le sont aussi.

namespace
{
    constexpr double kDiv10 = 0.1;

    // Ecart relatif admis point a point : quatre ulp float, la region etant
    // rendue en float (Polygon2) puis mise a l'echelle.
    constexpr double kScaledPointRelTol = 4.0 * std::numeric_limits<float>::epsilon();

    // Rapport de volumes de la fenetre reduite d'un facteur 10, seuils monde
    // par defaut (MSVC Release).
    constexpr double kWorldThresholdsDiv10VolumeRatio = 1.104498739e-3;

    // Toutes les longueurs de la fenetre par defaut divisees par 10.
    void applyDiv10(ParameterizedGothicWindow &s)
    {
        setParam(s, "Width", 20);
        setParam(s, "Offset outer", 1.6);
        setParam(s, "Offset inner", 1.0);
        setParam(s, "Body height", 26);
        setParam(s, "Extrusion (zHeight)", 2);
    }

    double volumeRatio(const Mesh &scaled, const Mesh &reference)
    {
        const double ratio = validate(scaled, false).signedVolume / validate(reference, false).signedVolume;
        std::ostringstream measured;
        measured.precision(10);
        measured << ratio;
        ::testing::Test::RecordProperty("ratio", measured.str());
        return ratio;
    }
}

TEST(TEST_cgmesh_gothic_baseline, generator_is_scale_covariant)
{
    const auto scaled = generate([](ParameterizedGothicWindow &s) {
        applyDiv10(s);
        s.SetFeatureThresholds(GothicFeatureThresholds().Scaled(kDiv10));
    });
    const auto reference = generate();
    ASSERT_TRUE(scaled);
    ASSERT_TRUE(reference);
    EXPECT_EQ(scaled->GetNVertices(), reference->GetNVertices());
    EXPECT_EQ(scaled->GetNFaces(), reference->GetNFaces());
    EXPECT_NEAR(volumeRatio(*scaled, *reference) / 1e-3, 1.0, 1e-6);
}

// Les seuils restent en unites monde quand seule la fenetre change d'echelle :
// a la largeur 20, les planchers d'anneau de rosace et de retrait d'oeillet
// valent dix fois plus, relativement, qu'a la largeur 200. Comportement voulu.
TEST(TEST_cgmesh_gothic_baseline, world_feature_thresholds_do_not_follow_the_window_scale)
{
    const auto scaled = generate(applyDiv10);
    const auto reference = generate();
    ASSERT_TRUE(scaled);
    ASSERT_TRUE(reference);
    EXPECT_NEAR(volumeRatio(*scaled, *reference), kWorldThresholdsDiv10VolumeRatio, 1e-8);
}

// Profils du menu : Chamfer (1) et Roll bar (2), Offset inner 40 pour que les
// termes additifs soient actifs (chanfrein min(0,6 x 40, 0,45 x 40 + 2) = 20,
// barre min(0,5 x 40, 0,45 x 40 + 1,5) = 19,5). Reduits d'un facteur 10 avec
// leurs seuils, ils donnent le solide reduit.
TEST(TEST_cgmesh_gothic_baseline, menu_profiles_are_scale_covariant_with_scaled_thresholds)
{
    for (const int profile : {1, 2})
    {
        SCOPED_TRACE("Profile = " + std::to_string(profile));
        const auto reference = generate([profile](ParameterizedGothicWindow &s) {
            setParam(s, "Profile", profile);
            setParam(s, "Offset inner", 40);
        });
        const auto scaled = generate([profile](ParameterizedGothicWindow &s) {
            applyDiv10(s);
            setParam(s, "Profile", profile);
            setParam(s, "Offset inner", 4);
            s.SetFeatureThresholds(GothicFeatureThresholds().Scaled(kDiv10));
        });
        ASSERT_TRUE(reference);
        ASSERT_TRUE(scaled);
        EXPECT_EQ(scaled->GetNVertices(), reference->GetNVertices());
        EXPECT_NEAR(volumeRatio(*scaled, *reference) / 1e-3, 1.0, 1e-6);
    }
}

// Scaled multiplie chaque longueur de la structure.
TEST(TEST_cgmesh_gothic_baseline, scaled_thresholds_multiply_every_length)
{
    constexpr double k = 0.37;
    const GothicFeatureThresholds d;
    const GothicFeatureThresholds t = d.Scaled(k);
    EXPECT_DOUBLE_EQ(t.minRosetteRadius,     k * d.minRosetteRadius);
    EXPECT_DOUBLE_EQ(t.minRingWidth,         k * d.minRingWidth);
    EXPECT_DOUBLE_EQ(t.minEyeletInset,       k * d.minEyeletInset);
    EXPECT_DOUBLE_EQ(t.minSubRosetteRadius,  k * d.minSubRosetteRadius);
    EXPECT_DOUBLE_EQ(t.minMouchetteRadius,   k * d.minMouchetteRadius);
    EXPECT_DOUBLE_EQ(t.minFilletInnerOffset, k * d.minFilletInnerOffset);
    EXPECT_DOUBLE_EQ(t.maxMouchetteInset,    k * d.maxMouchetteInset);
    EXPECT_DOUBLE_EQ(t.chamferWidthAdd,      k * d.chamferWidthAdd);
    EXPECT_DOUBLE_EQ(t.minChamferWidth,      k * d.minChamferWidth);
    EXPECT_DOUBLE_EQ(t.barRadiusAdd,         k * d.barRadiusAdd);
    EXPECT_DOUBLE_EQ(t.minBarRadius,         k * d.minBarRadius);
    EXPECT_DOUBLE_EQ(t.minBand,              k * d.minBand);
}

// Le rapport de construction compte les elements supprimes par un seuil : a la
// largeur 20, seuils monde, les trois champs d'ecoincon sont trop petits pour
// porter une mouchette (rayon inscrit < 1,5) et restent de simples vides.
TEST(TEST_cgmesh_gothic_baseline, build_report_records_features_dropped_by_world_thresholds)
{
    for (const bool scaledThresholds : {false, true})
    {
        SCOPED_TRACE(scaledThresholds ? "seuils / 10" : "seuils monde");
        ParameterizedGothicWindow shape;
        applyDiv10(shape);
        setParam(shape, "Mouchettes", 1);
        if (scaledThresholds)
            shape.SetFeatureThresholds(GothicFeatureThresholds().Scaled(kDiv10));
        GothicBuildReport report;
        shape.BuildStonePolygon(&report);
        EXPECT_EQ(report.Count(GothicBuildWarning::FeatureDropped), scaledThresholds ? 0u : 3u);
    }
}

// Les profils explicites sont donnes en unites monde : reduits avec la baie,
// ils donnent le solide par defaut reduit.
TEST(TEST_cgmesh_gothic_baseline, explicit_profiles_are_in_world_units)
{
    const Profile2D splay = chamferSplayProfile(6.0, 10.0);
    const Profile2D bar   = rollBarProfile(5.0);
    for (const bool useBar : {false, true})
    {
        SCOPED_TRACE(useBar ? "barre" : "ebrasement");
        const Profile2D &profile = useBar ? bar : splay;
        Profile2D small = profile;
        for (Vector2d &q : small.points) { q.x *= kDiv10; q.y *= kDiv10; }
        auto setProfile = [useBar](ParameterizedGothicWindow &s, const Profile2D &p) {
            if (useBar) s.SetBarProfile(&p); else s.SetSplayProfile(&p);
        };
        const auto reference = generate([&](ParameterizedGothicWindow &s) { setProfile(s, profile); });
        const auto scaled = generate([&](ParameterizedGothicWindow &s) {
            applyDiv10(s);
            s.SetFeatureThresholds(GothicFeatureThresholds().Scaled(kDiv10));
            setProfile(s, small);
        });
        ASSERT_TRUE(reference);
        ASSERT_TRUE(scaled);
        EXPECT_EQ(scaled->GetNVertices(), reference->GetNVertices());
        EXPECT_NEAR(volumeRatio(*scaled, *reference) / 1e-3, 1.0, 1e-6);
    }
}

// Point a point : la region de pierre reduite, seuils compris, est la region
// par defaut multipliee par 0,1, a l'arrondi float pres.
TEST(TEST_cgmesh_gothic_baseline, stone_polygon_is_scale_covariant_point_to_point)
{
    for (const auto &[name, params] : std::vector<std::pair<const char *, std::vector<std::pair<const char *, double>>>>{
             {"default", {}}, {"recursion_1", {{"Recursion", 1}}}, {"mouchettes", {{"Mouchettes", 1}}}})
    {
        SCOPED_TRACE(name);
        ParameterizedGothicWindow reference, scaled;
        for (const auto &[n, v] : params)
        {
            setParam(reference, n, v);
            setParam(scaled, n, v);
        }
        applyDiv10(scaled);
        scaled.SetFeatureThresholds(GothicFeatureThresholds().Scaled(kDiv10));
        const Polygon2 a = reference.BuildStonePolygon();
        const Polygon2 b = scaled.BuildStonePolygon();
        ASSERT_EQ(b.get_n_contours(), a.get_n_contours());
        double worst = 0.0;
        for (int c = 0; c < a.get_n_contours(); ++c)
        {
            ASSERT_EQ(b.get_n_points(c), a.get_n_points(c)) << "contour " << c;
            const float *pa = a.get_points(c);
            const float *pb = b.get_points(c);
            for (unsigned i = 0; i < 2 * a.get_n_points(c); ++i)
            {
                const double expected = kDiv10 * pa[i];
                const double tol = kScaledPointRelTol * std::max(1.0, std::fabs(expected));
                worst = std::max(worst, std::fabs(pb[i] - expected) / std::max(1.0, std::fabs(expected)));
                EXPECT_NEAR(pb[i], expected, tol) << "contour " << c << ", coordonnee " << i;
            }
        }
        std::ostringstream measured;
        measured.precision(3);
        measured << worst;
        RecordProperty(std::string("worst_rel_") + name, measured.str());
    }
}

// Un reglage flottant non fini est refuse avant tout calcul : triangle de repli,
// et le rapport porte une seule erreur invalid_parameter, nommee par le
// parametre ; BuildStonePolygon leve apres l'avoir enregistree.
namespace
{
    void expectNonFiniteParameterReported(const std::string &name, double value)
    {
        SCOPED_TRACE(name + " = " + std::to_string(value));
        ParameterizedGothicWindow shape;
        setParam(shape, name, value);
        shape.Regenerate();
        const std::unique_ptr<Mesh> mesh(shape.TakeMesh());
        ASSERT_TRUE(mesh);
        EXPECT_EQ(mesh->GetNVertices(), 3u);
        EXPECT_EQ(mesh->GetNFaces(), 1u);
        const GothicBuildReport &r = shape.LastBuildReport();
        EXPECT_EQ(r.Count(GothicBuildError::InvalidParameter), 1u);
        EXPECT_EQ(r.Count(GothicBuildError::ConstructionFailed), 0u);
        ASSERT_EQ(r.entries.size(), 1u);
        EXPECT_TRUE(r.entries[0].error);
        EXPECT_EQ(r.entries[0].code, static_cast<int>(GothicBuildError::InvalidParameter));
        EXPECT_EQ(r.entries[0].parameter, name);

        GothicBuildReport direct;
        EXPECT_THROW(shape.BuildStonePolygon(&direct), std::invalid_argument);
        EXPECT_EQ(direct.Count(GothicBuildError::InvalidParameter), 1u);
    }
}

TEST(TEST_cgmesh_gothic_baseline, non_finite_parameters_are_reported_as_errors)
{
    ParameterizedGothicWindow proto;
    std::size_t floats = 0;
    for (const Parameter &p : proto.GetParameters())
    {
        if (p.GetType() != Parameter::FLOAT)
            continue;
        ++floats;
        for (const double value : {std::numeric_limits<double>::quiet_NaN(),
                                   std::numeric_limits<double>::infinity(),
                                   -std::numeric_limits<double>::infinity()})
        {
            if (p.GetName() == "Rosette pointedness" && std::isnan(value))
                continue;   // rosette_pointedness_nan_is_reported_as_an_error
            expectNonFiniteParameterReported(p.GetName(), value);
        }
    }
    EXPECT_EQ(floats, 10u);
}

// Isole : sans le controle des non-finis, ce cas fait planter Regenerate (point
// NaN dans la region, acces invalide dans la tessellation).
TEST(TEST_cgmesh_gothic_baseline, rosette_pointedness_nan_is_reported_as_an_error)
{
    expectNonFiniteParameterReported("Rosette pointedness", std::numeric_limits<double>::quiet_NaN());
}

// Le descripteur v1 de la page gothique est converti en v2 : meme maillage que
// son equivalent v2 ecrit a la main, et chaque chemin perdu ou converti est
// liste, nomme par son chemin v1.
TEST(TEST_cgmesh_gothic_baseline, v1_instance_is_converted_to_v2)
{
    const std::string json = readFile("./test/data/gothic-window-instance.json");
    ASSERT_FALSE(json.empty()) << "./test/data/gothic-window-instance.json introuvable";

    ParameterizedGothicWindow loaded;
    ASSERT_TRUE(loaded.LoadFromJson(json));
    const GothicBuildReport &report = loaded.LastLoadReport();
    EXPECT_FALSE(report.HasErrors()) << GothicBuildReportToJson(report);
    loaded.Regenerate();
    const std::unique_ptr<Mesh> mesh(loaded.TakeMesh());
    ASSERT_TRUE(mesh);
    EXPECT_EQ(mesh->GetNVertices(), 3790u);
    EXPECT_EQ(mesh->GetNFaces(), 4592u);

    const char *const v2Equivalent = R"({
        "schema": "gothic-window/v2",
        "geometry": {"width": 200, "excess": 1.0, "bdOuter": 16, "bdInner": 10,
                     "heightBott": -180, "kseg": 72,
                     "subwindows": {"count": 2, "arcDown": 0, "excess": 1.0, "layout": "uniform",
                                    "gap": {"mode": "fraction", "gapFraction": 0.11}}},
        "style": {"mainArch": {"profile": {"type": "chamfer"},
                               "trefoil": {"enabled": false, "splitParameter": 0.45, "foilRadiusFactor": 0.30}},
                  "subArch": {"head": {"type": "foiled", "foils": 3}},
                  "rosette": {"present": true, "foils": {"count": 6, "type": "round"}},
                  "fillet": {"fill": "plain"}},
        "recursion": {"depth": 0}
    })";
    ParameterizedGothicWindow hand;
    ASSERT_TRUE(hand.LoadFromJson(v2Equivalent)) << GothicBuildReportToJson(hand.LastLoadReport());
    EXPECT_FALSE(hand.LastLoadReport().HasWarnings()) << GothicBuildReportToJson(hand.LastLoadReport());
    EXPECT_TRUE(hand.Settings() == loaded.Settings());
    hand.Regenerate();
    const std::unique_ptr<Mesh> handMesh(hand.TakeMesh());
    ASSERT_TRUE(handMesh);
    EXPECT_EQ(mesh->GetVertices(), handMesh->GetVertices());

    std::vector<std::string> ignored, converted;
    for (const GothicDiagnostic &d : report.entries)
    {
        if (d.error)
            continue;
        if (d.code == static_cast<int>(GothicBuildWarning::FieldIgnored))
            ignored.push_back(d.parameter);
        else if (d.code == static_cast<int>(GothicBuildWarning::FieldConverted))
            converted.push_back(d.parameter);
    }
    std::sort(ignored.begin(), ignored.end());
    const std::vector<std::string> expectedIgnored = {
        "variants", "wall", "window.arch.profile.depth", "window.arch.profile.segments",
        "window.fillets.subdivide", "window.fillets.subdivisionDepth", "window.rosette.hub.radiusFactor",
        "window.style.glass", "window.style.recursive.scaleFactor", "window.style.stone",
        "window.style.subdivisionSurface", "window.style.windowStyle",
        "window.tessellation.adaptiveSubdivision", "window.tessellation.manifold",
        "window.tessellation.targetEdgeLength"};
    EXPECT_EQ(ignored, expectedIgnored);
    EXPECT_EQ(converted, std::vector<std::string>{"window.subwindows.foils"});
    EXPECT_EQ(report.Count(GothicBuildWarning::FieldIgnored), expectedIgnored.size());
    EXPECT_EQ(report.Count(GothicBuildWarning::FieldConverted), 1u);
}

// ---------------------------------------------------------------------------
//  Autres contrats
// ---------------------------------------------------------------------------

// Sans extrusion, la fenetre est une feuille plane : un seul cycle de bord par
// sommet, volume nul.
TEST(TEST_cgmesh_gothic_baseline, flat_mode_is_an_open_sheet)
{
    const auto mesh = generate([](ParameterizedGothicWindow &s) { setParam(s, "Extrusion (zHeight)", 0); });
    ASSERT_TRUE(mesh);
    const MeshValidationReport r = validate(*mesh);
    recordReport(r);
    EXPECT_EQ(count(r, MeshDefect::BoundaryEdge), 1817u);
    EXPECT_FALSE(r.selfIntersectionTruncated);
    EXPECT_EQ(count(r, MeshDefect::SelfIntersection), 0u);
    EXPECT_NEAR(r.signedVolume, 0.0, 1e-6);
}

TEST(TEST_cgmesh_gothic_baseline, binary_stl_round_trip_preserves_shell)
{
    const auto mesh = generate();
    ASSERT_TRUE(mesh);
    const std::filesystem::path dir = findProjectRoot() / "tmp";
    std::filesystem::create_directories(dir);
    const std::string path = (dir / "gothic-baseline-default.stl").string();
    if (std::filesystem::exists(path))
        std::filesystem::remove(path);
    ASSERT_EQ(mesh->export_stl_binary(path.c_str()), 0);

    Mesh back;
    ASSERT_EQ(back.load(path.c_str()), 0);
    const MeshValidationReport r = validate(back);
    recordReport(r);
    EXPECT_EQ(r.weldedVertices, 3634u);
    EXPECT_EQ(count(r, MeshDefect::BoundaryEdge), 0u);
    EXPECT_TRUE(r.IsValid());
    EXPECT_NEAR(r.signedVolume, kDefaultVolume, kVolumeRelTol * kDefaultVolume);
    expectDefaultBBox(r);
}

// ---------------------------------------------------------------------------
//  Region de pierre et solide extrude : matrice de variantes
// ---------------------------------------------------------------------------
//
// Contrat a deux etages, sur les memes variantes :
//   - la region 2D que Regenerate extrude (BuildStonePolygon) n'a aucun defaut
//     de contour a la tolerance par defaut : contours simples, deux a deux
//     disjoints, orientes selon leur niveau (precondition d'extrusion de
//     Havemann, these p. 246) ;
//   - le maillage extrude est une coque fermee, manifold, orientee, sans
//     auto-intersection ni face degeneree, d'un seul tenant -- Recursion = 2
//     sans bande de cadre ou sans corps compris : la sous-rosace repose sur
//     l'apex des sous-lancettes effectivement percees. La variante a l'echelle
//     1/10 reduit aussi les seuils d'elements (GothicVariant::thresholdScale) :
//     elle est la fenetre par defaut reduite.

namespace
{
    struct GothicVariant
    {
        const char *name;
        std::vector<std::pair<const char *, double>> params;
        double      thresholdScale = 1.0; // seuils d'elements = seuils par defaut x thresholdScale
    };

    std::ostream &operator<<(std::ostream &os, const GothicVariant &v) { return os << v.name; }

    void applyVariant(ParameterizedGothicWindow &shape, const GothicVariant &v)
    {
        for (const auto &[name, value] : v.params)
            setParam(shape, name, value);
        if (v.thresholdScale != 1.0)
            shape.SetFeatureThresholds(GothicFeatureThresholds().Scaled(v.thresholdScale));
    }

    const std::vector<GothicVariant> &stoneVariants()
    {
        static const std::vector<GothicVariant> variants = {
            {"default", {}},
            {"recursion_1", {{"Recursion", 1}}},
            {"recursion_2", {{"Recursion", 2}}},
            {"mouchettes", {{"Mouchettes", 1}}},
            {"foils_24_pointedness_2", {{"Rosette foil count", 24}, {"Rosette pointedness", 2}}},
            {"excess_0_7", {{"Excess (arch)", 0.7}}},
            {"offset_inner_0", {{"Offset inner", 0}}},
            {"offset_inner_40", {{"Offset inner", 40}}},
            {"scale_div_10", {{"Width", 20}, {"Offset outer", 1.6}, {"Offset inner", 1.0},
                              {"Body height", 26}, {"Extrusion (zHeight)", 2}},
             kDiv10},
            {"scale_div_10_world_thresholds", {{"Width", 20}, {"Offset outer", 1.6}, {"Offset inner", 1.0},
                                               {"Body height", 26}, {"Extrusion (zHeight)", 2}}},
            {"lancet_head_plain", {{"Lancet head", 0}}},
            {"gap_fraction_0", {{"Gap fraction", 0}}},
            {"recursion_2_offset_outer_0", {{"Recursion", 2}, {"Offset outer", 0}}},
            {"recursion_2_body_height_0", {{"Recursion", 2}, {"Body height", 0}}},
        };
        return variants;
    }

    std::string variantName(const ::testing::TestParamInfo<GothicVariant> &info) { return info.param.name; }

    void recordContourReport(const ContourValidationReport &r)
    {
        for (std::size_t k = 0; k < kContourDefectCount; ++k)
            ::testing::Test::RecordProperty(std::string("2d_") + ContourDefectName(static_cast<ContourDefect>(k)),
                                            static_cast<int>(r.counts[k]));
        ::testing::Test::RecordProperty("contours", static_cast<int>(r.contourCount));
        ::testing::Test::RecordProperty("points", static_cast<int>(r.pointCount));
        ::testing::Test::RecordProperty("contact_contour_pairs", static_cast<int>(r.contactContourPairs));
    }
}

class GothicStoneRegion : public ::testing::TestWithParam<GothicVariant> {};

TEST_P(GothicStoneRegion, stone_contours_are_simple_disjoint_and_oriented)
{
    ParameterizedGothicWindow shape;
    applyVariant(shape, GetParam());
    const Polygon2 stone = shape.BuildStonePolygon();
    const ContourValidationReport r = ValidateContours(stone);
    recordContourReport(r);
    EXPECT_GT(r.contourCount, 1u);
    EXPECT_FALSE(r.truncated);
    EXPECT_TRUE(r.IsValid()) << "contact " << r.Count(ContourDefect::ContourContact)
                             << ", auto-intersection " << r.Count(ContourDefect::SelfIntersection)
                             << ", arete courte " << r.Count(ContourDefect::ShortEdge)
                             << ", orientation " << r.Count(ContourDefect::WrongOrientation);
}

INSTANTIATE_TEST_SUITE_P(TEST_cgmesh_gothic_baseline, GothicStoneRegion,
                         ::testing::ValuesIn(stoneVariants()), variantName);

class GothicExtrudedWindow : public ::testing::TestWithParam<GothicVariant> {};

TEST_P(GothicExtrudedWindow, extruded_window_is_a_closed_manifold_shell)
{
    const auto mesh = generate([](ParameterizedGothicWindow &s) { applyVariant(s, GetParam()); });
    ASSERT_TRUE(mesh);
    ASSERT_GT(mesh->GetNFaces(), 1u) << "triangle de repli";
    MeshValidationOptions options;
    options.weldTolerance      = kGothicWeldTol;
    options.expectedComponents = 1u;
    const MeshValidationReport r = ValidateMesh(*mesh, options);
    recordReport(r);
    RecordProperty("vertices", static_cast<int>(mesh->GetNVertices()));
    RecordProperty("faces", static_cast<int>(mesh->GetNFaces()));
    EXPECT_EQ(r.components, 1u);
    expectNoMeshDefect(r);
}

INSTANTIATE_TEST_SUITE_P(TEST_cgmesh_gothic_baseline, GothicExtrudedWindow,
                         ::testing::ValuesIn(stoneVariants()), variantName);

// ---------------------------------------------------------------------------
//  Rapport de construction
// ---------------------------------------------------------------------------
//
// Le generateur enregistre, sans en changer le resultat, les evenements qui
// modifient la baie : ici le repli d'une unite recursive dont la subdivision
// echoue (l'ouverture entiere de l'unite est percee). A Recursion = 2, chacune
// des quatre sous-lancettes de la fenetre par defaut s'y replie.

TEST(TEST_cgmesh_gothic_baseline, build_report_records_recursion_depth_fallbacks)
{
    for (const auto &[depth, fallbacks] : {std::pair<int, std::size_t>{0, 0}, {1, 0}, {2, 4}})
    {
        SCOPED_TRACE("Recursion = " + std::to_string(depth));
        ParameterizedGothicWindow shape;
        setParam(shape, "Recursion", depth);
        GothicBuildReport report;
        shape.BuildStonePolygon(&report);
        EXPECT_EQ(report.Count(GothicBuildWarning::DepthFallback), fallbacks);

        shape.Regenerate();
        EXPECT_EQ(shape.LastBuildReport().counts, report.counts);
    }
}

// ---------------------------------------------------------------------------
//  Rosace sans lobes
// ---------------------------------------------------------------------------
//
// La rosace passe par l'apex des lancettes. Sans lobes, son vide est evide en
// retrait de l'anneau du motif lobe (largeur max(3, 0,1 R)) : il ne touche pas
// les vides des lancettes (Havemann, « contiguous border plane »).

TEST(TEST_cgmesh_gothic_baseline, plain_oculus_does_not_touch_the_lancets)
{
    for (const int depth : {0, 1})
    {
        SCOPED_TRACE("Recursion = " + std::to_string(depth));
        ParameterizedGothicWindow shape;
        setParam(shape, "Rosette foils", 0);
        setParam(shape, "Recursion", depth);
        const ContourValidationReport r = ValidateContours(shape.BuildStonePolygon());
        EXPECT_TRUE(r.IsValid()) << "contact " << r.Count(ContourDefect::ContourContact)
                                 << ", auto-intersection " << r.Count(ContourDefect::SelfIntersection);
    }
}

// ---------------------------------------------------------------------------
//  Mouchettes en goutte
// ---------------------------------------------------------------------------
//
// La goutte de cgmath est rendue en trois arcs qui ne se suivent pas bout a
// bout (base T1 -> T2, cote T1 -> apex, cote apex -> T2). Le generateur les
// chaine par leurs extremites : le vide est un contour simple, sans lame
// d'epaisseur nulle le long de la corde T1T2. Le Soufflet (type 2), dont les
// arcs se suivent deja (buildSouffletMouchette), est le temoin d'un trace
// deja chaine : le chainage ne doit pas l'alterer.

TEST(TEST_cgmesh_gothic_baseline, teardrop_mouchette_voids_are_simple_contours)
{
    for (const int type : {1, 2})
        for (const int depth : {0, 1})
        {
            SCOPED_TRACE("Mouchette type = " + std::to_string(type) + ", Recursion = " + std::to_string(depth));
            ParameterizedGothicWindow shape;
            setParam(shape, "Mouchettes", 1);
            setParam(shape, "Mouchette type", type);
            setParam(shape, "Recursion", depth);
            const ContourValidationReport r = ValidateContours(shape.BuildStonePolygon());
            EXPECT_TRUE(r.IsValid()) << "contact " << r.Count(ContourDefect::ContourContact)
                                     << ", auto-intersection " << r.Count(ContourDefect::SelfIntersection)
                                     << ", aire negligeable " << r.Count(ContourDefect::NegligibleArea);
        }
}

// ---------------------------------------------------------------------------
//  Baies recursives tirees au hasard
// ---------------------------------------------------------------------------
//
// Baies issues d'un tirage aleatoire de parametres (graine 7). Les contrats :
// une baie a mouchettes en goutte, a rosace sans lobes, ou dont la region
// calculee sans garde se pince, est une coque fermee d'un seul tenant. Les
// defauts connus restants sont figes a leur valeur actuelle, sous un nom qui
// les enonce.

namespace
{
    // Baie recursive a mouchettes en goutte : les vides de goutte, chaines par
    // leurs extremites, ne touchent aucun autre vide.
    const GothicVariant kRecursiveTeardropBay = {
        "recursive_teardrop_bay",
        {{"Width", 109.02}, {"Excess (arch)", 2.40595}, {"Body height", 78.3755},
         {"Offset outer", 6.56033}, {"Offset inner", 6.13813}, {"Lancet head", 1},
         {"Lancet head foils", 5}, {"Recursion", 1}, {"Lancet drop", 29.5081},
         {"Lancet excess", 1.07387}, {"Gap fraction", 0.154198}, {"Lancet layout", 1},
         {"Fillets", 1}, {"Mouchettes", 1}, {"Mouchette type", 1}, {"Rosette", 1},
         {"Rosette foils", 1}, {"Rosette foil count", 4}, {"Rosette foil type", 0},
         {"Extrusion (zHeight)", 62.6254}}};

    // Baie recursive a rosace sans lobes dont un sous-vide passe a moins d'un
    // pas de grille du cercle de la rosace : le vide de l'oculus, en retrait de
    // l'anneau, ne l'approche pas.
    const GothicVariant kRecursivePlainOculusBay = {
        "recursive_plain_oculus_bay",
        {{"Width", 225.578}, {"Excess (arch)", 1.47843}, {"Body height", 332.594},
         {"Offset outer", 29.4862}, {"Offset inner", 34.6999}, {"Lancet head", 1},
         {"Lancet head foils", 6}, {"Recursion", 1}, {"Lancet drop", 48.5871},
         {"Lancet excess", 0.868648}, {"Gap fraction", 0.104381}, {"Lancet layout", 1},
         {"Fillets", 0}, {"Mouchettes", 1}, {"Mouchette type", 2}, {"Rosette", 1},
         {"Rosette foils", 0}, {"Rosette foil count", 3}, {"Rosette foil type", 1},
         {"Rosette pointedness", 0.131639}, {"Extrusion (zHeight)", 28.677}}};

    // Baie recursive dont la region calculee sans garde se pince (six
    // auto-intersections 2D) : la garde morphologique la rend valide. Ni
    // l'ouverture seule ni la fermeture seule n'y suffisent.
    const GothicVariant kGuardedRecursiveBay = {
        "guarded_recursive_bay",
        {{"Width", 353.542}, {"Excess (arch)", 2.02632}, {"Body height", 570.923},
         {"Offset outer", 20.6714}, {"Offset inner", 15.0616}, {"Lancet head", 0},
         {"Lancet head foils", 8}, {"Recursion", 1}, {"Lancet drop", 64.3023},
         {"Lancet excess", 1.24135}, {"Gap fraction", 0.0277447}, {"Lancet layout", 1},
         {"Fillets", 1}, {"Mouchettes", 0}, {"Mouchette type", 2}, {"Rosette", 0},
         {"Rosette foils", 0}, {"Rosette foil count", 13}, {"Rosette foil type", 0},
         {"Rosette pointedness", 0.319251}, {"Extrusion (zHeight)", 64.0417}}};

    // Baie recursive dont le drop des lancettes (62,1) depasse le corps (48,4) :
    // sans relevement du corps, le sous-remplage sortirait du cadre en pieces
    // separees.
    const GothicVariant kDropBeyondBodyBay = {
        "drop_beyond_body_bay",
        {{"Width", 444.181}, {"Excess (arch)", 1.02188}, {"Body height", 48.3602},
         {"Offset outer", 32.3003}, {"Offset inner", 1.17648}, {"Lancet head", 0},
         {"Lancet head foils", 6}, {"Recursion", 2}, {"Lancet drop", 62.0804},
         {"Lancet excess", 1.4122}, {"Gap fraction", 0.131001}, {"Lancet layout", 1},
         {"Fillets", 1}, {"Mouchettes", 0}, {"Mouchette type", 2}, {"Rosette", 1},
         {"Rosette foils", 0}, {"Rosette foil count", 20}, {"Rosette foil type", 1},
         {"Rosette pointedness", 0.120066}, {"Extrusion (zHeight)", 29.2894}}};

    // Baie recursive dont une fleur de sous-rosace touche un sous-vide ;
    // l'ouverture suivie de la fermeture ne les separe pas, la fermeture seule si.
    const GothicVariant kSubRosetteContactBay = {
        "sub_rosette_contact_bay",
        {{"Width", 443.878}, {"Excess (arch)", 2.42181}, {"Body height", 233.397},
         {"Offset outer", 22.0919}, {"Offset inner", 5.3312}, {"Lancet head", 0},
         {"Lancet head foils", 6}, {"Recursion", 2}, {"Lancet drop", 25.8889},
         {"Lancet excess", 1.37079}, {"Gap fraction", 0.189201}, {"Lancet layout", 1},
         {"Fillets", 0}, {"Mouchettes", 0}, {"Mouchette type", 0}, {"Rosette", 0},
         {"Rosette foils", 1}, {"Rosette foil count", 4}, {"Rosette foil type", 0},
         {"Rosette pointedness", 0.429834}, {"Extrusion (zHeight)", 98.2938}}};

    struct VariantReports
    {
        ContourValidationReport contours;
        MeshValidationReport    mesh;
    };

    VariantReports validateVariant(const GothicVariant &v)
    {
        ParameterizedGothicWindow shape;
        applyVariant(shape, v);
        VariantReports out;
        out.contours = ValidateContours(shape.BuildStonePolygon());
        recordContourReport(out.contours);
        const auto mesh = generate([&v](ParameterizedGothicWindow &s) { applyVariant(s, v); });
        EXPECT_TRUE(mesh);
        if (mesh)
        {
            out.mesh = validate(*mesh);
            recordReport(out.mesh);
        }
        return out;
    }
}

// Une baie recursive a mouchettes en goutte donne une region de pierre sans
// defaut de contour et une coque fermee, manifold, d'un seul tenant.
TEST(TEST_cgmesh_gothic_baseline, recursive_teardrop_bay_is_a_single_closed_shell)
{
    ParameterizedGothicWindow shape;
    applyVariant(shape, kRecursiveTeardropBay);
    GothicBuildReport report;
    shape.BuildStonePolygon(&report);
    EXPECT_EQ(report.Count(GothicBuildWarning::MorphologicalGuard), 0u)
        << "la region doit etre valide sans la garde";

    const VariantReports r = validateVariant(kRecursiveTeardropBay);
    EXPECT_TRUE(r.contours.IsValid()) << "contact " << r.contours.Count(ContourDefect::ContourContact)
                                      << ", auto-intersection " << r.contours.Count(ContourDefect::SelfIntersection);
    EXPECT_EQ(r.mesh.components, 1u);
    EXPECT_EQ(count(r.mesh, MeshDefect::NonManifoldEdge), 0u);
    EXPECT_EQ(count(r.mesh, MeshDefect::NonManifoldVertex), 0u);
    EXPECT_EQ(count(r.mesh, MeshDefect::BoundaryEdge), 0u);
    EXPECT_EQ(count(r.mesh, MeshDefect::SelfIntersection), 0u);
    EXPECT_FALSE(r.mesh.selfIntersectionTruncated);
}

// Une baie recursive a rosace sans lobes donne une region de pierre sans defaut
// de contour et une coque fermee d'un seul tenant.
TEST(TEST_cgmesh_gothic_baseline, recursive_plain_oculus_bay_is_a_single_closed_shell)
{
    ParameterizedGothicWindow shape;
    applyVariant(shape, kRecursivePlainOculusBay);
    GothicBuildReport report;
    shape.BuildStonePolygon(&report);
    EXPECT_EQ(report.Count(GothicBuildWarning::MorphologicalGuard), 0u)
        << "la region doit etre valide sans la garde";

    const VariantReports r = validateVariant(kRecursivePlainOculusBay);
    EXPECT_TRUE(r.contours.IsValid()) << "contact " << r.contours.Count(ContourDefect::ContourContact)
                                      << ", auto-intersection " << r.contours.Count(ContourDefect::SelfIntersection);
    EXPECT_EQ(r.mesh.components, 1u);
    EXPECT_EQ(count(r.mesh, MeshDefect::NonManifoldEdge), 0u);
    EXPECT_EQ(count(r.mesh, MeshDefect::BoundaryEdge), 0u);
    EXPECT_EQ(count(r.mesh, MeshDefect::SelfIntersection), 0u);
    EXPECT_FALSE(r.mesh.selfIntersectionTruncated);
}

TEST(TEST_cgmesh_gothic_baseline, guarded_recursive_bay_is_a_single_closed_shell)
{
    ParameterizedGothicWindow shape;
    applyVariant(shape, kGuardedRecursiveBay);
    GothicBuildReport report;
    shape.BuildStonePolygon(&report);
    EXPECT_EQ(report.Count(GothicBuildWarning::MorphologicalGuard), 1u);

    const VariantReports r = validateVariant(kGuardedRecursiveBay);
    EXPECT_TRUE(r.contours.IsValid()) << "contact " << r.contours.Count(ContourDefect::ContourContact)
                                      << ", auto-intersection " << r.contours.Count(ContourDefect::SelfIntersection);
    EXPECT_EQ(r.mesh.components, 1u);
    EXPECT_EQ(count(r.mesh, MeshDefect::NonManifoldEdge), 0u);
    EXPECT_EQ(count(r.mesh, MeshDefect::BoundaryEdge), 0u);
    EXPECT_EQ(count(r.mesh, MeshDefect::SelfIntersection), 0u);
    EXPECT_FALSE(r.mesh.selfIntersectionTruncated);
}

// Un drop de lancette qui ferait passer le pied des lancettes sous l'appui
// (drop > corps - offset exterieur) porte le corps a drop + offset exterieur :
// le sous-remplage reste dans le cadre, d'un seul tenant ; le clamp est
// enregistre sur « Body height ».
TEST(TEST_cgmesh_gothic_baseline, lancet_drop_beyond_the_body_is_clamped)
{
    ParameterizedGothicWindow shape;
    applyVariant(shape, kDropBeyondBodyBay);
    GothicBuildReport report;
    shape.BuildStonePolygon(&report);
    std::size_t bodyClamps = 0;
    for (const GothicDiagnostic &d : report.entries)
        bodyClamps += (!d.error && d.code == static_cast<int>(GothicBuildWarning::ParameterClamped)
                       && d.parameter == "Body height");
    EXPECT_EQ(bodyClamps, 1u);

    const VariantReports r = validateVariant(kDropBeyondBodyBay);
    EXPECT_TRUE(r.contours.IsValid());
    EXPECT_EQ(r.mesh.components, 1u);
    EXPECT_EQ(count(r.mesh, MeshDefect::NonManifoldEdge), 0u);
    EXPECT_EQ(count(r.mesh, MeshDefect::BoundaryEdge), 0u);
    EXPECT_EQ(count(r.mesh, MeshDefect::SelfIntersection), 0u);
}

// Meme regle au niveau 0 : corps nul, ou drop au-dela du corps moins l'offset
// exterieur. Le pied de la fenetre descend a -(drop + offset exterieur) ; sous
// la regle (drop <= corps - offset exterieur), le corps est inchange.
TEST(TEST_cgmesh_gothic_baseline, level_0_body_is_raised_to_the_lancet_drop_plus_the_frame)
{
    struct Case { double body, drop, expectedBottom; bool raised; };
    for (const Case &c : {Case{0, 0, -16, true},
                          Case{20, 21, -37, true},
                          Case{40, 10, -40, false}})
    {
        SCOPED_TRACE("Body height = " + std::to_string(c.body) + ", Lancet drop = " + std::to_string(c.drop));
        const GothicVariant v = {"level_0_body", {{"Body height", c.body}, {"Lancet drop", c.drop}}};
        ParameterizedGothicWindow shape;
        applyVariant(shape, v);
        GothicBuildReport report;
        shape.BuildStonePolygon(&report);
        std::size_t bodyClamps = 0;
        for (const GothicDiagnostic &d : report.entries)
            bodyClamps += (!d.error && d.code == static_cast<int>(GothicBuildWarning::ParameterClamped)
                           && d.parameter == "Body height");
        EXPECT_EQ(bodyClamps, c.raised ? 1u : 0u);

        const VariantReports r = validateVariant(v);
        EXPECT_TRUE(r.contours.IsValid()) << "contact " << r.contours.Count(ContourDefect::ContourContact);
        expectNoMeshDefect(r.mesh);
        EXPECT_EQ(r.mesh.components, 1u);
        EXPECT_NEAR(r.mesh.bbMin[1], c.expectedBottom, kBBoxTol);
    }
}

// Une fleur de sous-rosace touche un sous-vide ; l'ouverture suivie de la
// fermeture laisse le contact (l'ouverture le recree), la fermeture seule les
// separe : la garde la retient, et l'enregistre.
TEST(TEST_cgmesh_gothic_baseline, closing_fallback_separates_the_sub_rosette_flower)
{
    ParameterizedGothicWindow shape;
    applyVariant(shape, kSubRosetteContactBay);
    GothicBuildReport report;
    shape.BuildStonePolygon(&report);
    EXPECT_EQ(report.Count(GothicBuildWarning::MorphologicalGuard), 1u);
    EXPECT_EQ(report.Count(GothicBuildWarning::GuardClosingFallback), 1u);
    EXPECT_EQ(report.Count(GothicBuildWarning::GuardInsufficient), 0u);

    const VariantReports r = validateVariant(kSubRosetteContactBay);
    EXPECT_TRUE(r.contours.IsValid()) << "contact " << r.contours.Count(ContourDefect::ContourContact)
                                      << ", auto-intersection " << r.contours.Count(ContourDefect::SelfIntersection);
    EXPECT_EQ(r.mesh.components, 1u);
    EXPECT_EQ(count(r.mesh, MeshDefect::NonManifoldEdge), 0u);
    EXPECT_EQ(count(r.mesh, MeshDefect::NonManifoldVertex), 0u);
    EXPECT_EQ(count(r.mesh, MeshDefect::BoundaryEdge), 0u);
    EXPECT_EQ(count(r.mesh, MeshDefect::SelfIntersection), 0u);
    EXPECT_FALSE(r.mesh.selfIntersectionTruncated);
}

// Recursion 2 a la largeur maximale de l'interface : la grille canonique est
// plus grossiere en monde, la region calculee se pince ; l'ouverture suivie de
// la fermeture laisse des auto-intersections, la fermeture seule non.
TEST(TEST_cgmesh_gothic_baseline, recursion_2_at_width_600_is_a_single_closed_shell)
{
    const GothicVariant wide = {"recursion_2_width_600", {{"Recursion", 2}, {"Width", 600}}};
    ParameterizedGothicWindow shape;
    applyVariant(shape, wide);
    GothicBuildReport report;
    shape.BuildStonePolygon(&report);
    EXPECT_EQ(report.Count(GothicBuildWarning::GuardClosingFallback), 1u);
    EXPECT_EQ(report.Count(GothicBuildWarning::GuardInsufficient), 0u);

    const VariantReports r = validateVariant(wide);
    EXPECT_TRUE(r.contours.IsValid()) << "contact " << r.contours.Count(ContourDefect::ContourContact)
                                      << ", auto-intersection " << r.contours.Count(ContourDefect::SelfIntersection);
    EXPECT_EQ(r.mesh.components, 1u);
    EXPECT_EQ(count(r.mesh, MeshDefect::NonManifoldEdge), 0u);
    EXPECT_EQ(count(r.mesh, MeshDefect::BoundaryEdge), 0u);
    EXPECT_EQ(count(r.mesh, MeshDefect::SelfIntersection), 0u);
}
