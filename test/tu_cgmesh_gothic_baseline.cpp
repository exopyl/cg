#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "../src/cgmesh/parameterized.h"
#include "../src/cgmesh/parameterized_shapes.h"

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

// ---------------------------------------------------------------------------
//  Defauts connus, figes a leur valeur actuelle
// ---------------------------------------------------------------------------

// Defaut : les barres de profil sont des coques fermees separees, posees sur la
// plaque sans booleen, qui la traversent. Le solide n'est pas une coque unique.
TEST(TEST_cgmesh_gothic_baseline, roll_bar_profile_yields_separate_shells)
{
    const auto mesh = generate([](ParameterizedGothicWindow &s) { setParam(s, "Profile", 2); });
    ASSERT_TRUE(mesh);
    const MeshValidationReport r = validate(*mesh);
    recordReport(r);
    EXPECT_EQ(r.components, 5u) << "defaut fige : plaque + barres non fusionnees";
    EXPECT_EQ(count(r, MeshDefect::BoundaryEdge), 0u);
    EXPECT_EQ(count(r, MeshDefect::NonManifoldEdge), 0u);
    EXPECT_FALSE(r.selfIntersectionTruncated);
    EXPECT_GT(count(r, MeshDefect::SelfIntersection), 0u) << "defaut fige";
}

// Defaut : la recursion produit des aretes ouvertes, des aretes et sommets non
// manifold, des auto-intersections, et laisse des sommets orphelins.
TEST(TEST_cgmesh_gothic_baseline, recursion_produces_open_and_non_manifold_edges)
{
    const auto mesh = generate([](ParameterizedGothicWindow &s) { setParam(s, "Recursion", 1); });
    ASSERT_TRUE(mesh);
    const MeshValidationReport r = validate(*mesh);
    recordReport(r);
    EXPECT_GT(count(r, MeshDefect::BoundaryEdge), 0u) << "defaut fige";
    EXPECT_GT(count(r, MeshDefect::NonManifoldEdge), 0u) << "defaut fige";
    EXPECT_GT(count(r, MeshDefect::NonManifoldVertex), 0u) << "defaut fige";
    EXPECT_GT(count(r, MeshDefect::UnreferencedVertex), 0u) << "defaut fige";
    EXPECT_FALSE(r.selfIntersectionTruncated);
    EXPECT_GT(count(r, MeshDefect::SelfIntersection), 0u) << "defaut fige";
}

// Le validateur rend le meme rapport quel que soit l'ordre des sommets, y
// compris sur un maillage a sommets quasi coincidents.
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

// Defaut : les mouchettes produisent des triangles degeneres, sans ouvrir la coque.
TEST(TEST_cgmesh_gothic_baseline, mouchettes_produce_degenerate_triangles)
{
    const auto mesh = generate([](ParameterizedGothicWindow &s) { setParam(s, "Mouchettes", 1); });
    ASSERT_TRUE(mesh);
    const MeshValidationReport r = validate(*mesh);
    recordReport(r);
    EXPECT_GT(count(r, MeshDefect::DegenerateFace), 0u) << "defaut fige";
    EXPECT_EQ(count(r, MeshDefect::BoundaryEdge), 0u);
}

// Defaut : reduire toutes les longueurs d'un facteur 10 ne reduit pas le volume
// d'un facteur 1000. Un generateur covariant donnerait un rapport de 1e-3 ; il
// est fige au-dessus de 1.05e-3.
TEST(TEST_cgmesh_gothic_baseline, generator_is_not_scale_covariant)
{
    const auto scaled = generate([](ParameterizedGothicWindow &s) {
        setParam(s, "Width", 20);
        setParam(s, "Offset outer", 1.6);
        setParam(s, "Offset inner", 1.0);
        setParam(s, "Body height", 26);
        setParam(s, "Extrusion (zHeight)", 2);
    });
    const auto reference = generate();
    ASSERT_TRUE(scaled);
    ASSERT_TRUE(reference);
    const double ratio = validate(*scaled, false).signedVolume / validate(*reference, false).signedVolume;
    std::ostringstream measured;
    measured.precision(6);
    measured << ratio;
    RecordProperty("ratio", measured.str());
    EXPECT_GT(ratio, 1.05e-3) << "defaut fige : rapport mesure " << ratio;
}

// Defaut : une largeur non finie n'est pas rejetee ; le generateur rend en
// silence le triangle de repli.
TEST(TEST_cgmesh_gothic_baseline, non_finite_width_silently_yields_a_placeholder)
{
    const auto mesh = generate([](ParameterizedGothicWindow &s) {
        setParam(s, "Width", std::numeric_limits<double>::quiet_NaN());
    });
    ASSERT_TRUE(mesh);
    EXPECT_EQ(mesh->GetNVertices(), 3u) << "defaut fige";
    EXPECT_EQ(mesh->GetNFaces(), 1u) << "defaut fige";
}

// Defaut : LoadFromJson accepte un document v1, n'en lit rien et renvoie true.
TEST(TEST_cgmesh_gothic_baseline, v1_instance_json_is_silently_ignored)
{
    const std::string json = readFile("./test/data/gothic-window-instance.json");
    ASSERT_FALSE(json.empty()) << "./test/data/gothic-window-instance.json introuvable";

    ParameterizedGothicWindow loaded;
    EXPECT_TRUE(loaded.LoadFromJson(json)) << "defaut fige";
    loaded.Regenerate();
    const std::unique_ptr<Mesh> mesh(loaded.TakeMesh());
    const auto reference = generate();
    ASSERT_TRUE(mesh);
    ASSERT_TRUE(reference);
    EXPECT_EQ(mesh->GetVertices(), reference->GetVertices()) << "defaut fige";
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
