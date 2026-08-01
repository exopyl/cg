#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/parameterized.h"
#include "../src/cgmesh/parameterized_shapes.h"

// ===========================================================================
//  parameterized_shapes.cpp — le catalogue de formes de maker
// ===========================================================================
//
// Ce fichier n'avait AUCUN test : 1296 lignes, 31 classes, et c'est pourtant tout
// ce dont maker dépend pour produire de la géométrie. tu_cgmesh_shapes.cpp couvre
// les générateurs de dessous (surface_basic, surface_parametric), pas les
// enveloppes `ParameterizedXxx` ni le contrat `IParameterized`.
//
// La stratégie est de tester le CONTRAT sur TOUT le catalogue plutôt que la
// géométrie de chaque forme une à une : un test par forme se périmerait au
// premier réglage de paramètre, alors que « chaque forme se nomme, expose des
// bornes cohérentes, et régénère une géométrie finie » reste vrai par
// construction. Une forme ajoutée au catalogue est couverte sans écrire un test.
//
// Coût maîtrisé : on reste aux valeurs par défaut et on ne pousse un paramètre
// que d'un cran (value+1). Monter un paramètre à son MAX ferait exploser la
// durée -- l'éponge de Menger et les L-systèmes végétaux atteignent des
// centaines de milliers de sommets.
//
// ===========================================================================

namespace
{
    using Factory = std::function<std::unique_ptr<ParameterizedMesh>()>;

    template <typename T>
    Factory make() { return [] { return std::unique_ptr<ParameterizedMesh>(new T()); }; }

    struct Entry { const char* label; Factory make; };

    // Les 27 formes du catalogue de maker (wasm_api.cpp), c'est-à-dire toutes
    // celles constructibles sans argument. Les quatre autres
    // (SvgExtrusion / ImageRelief / ImagePixelBlocks / ImplicitFromPoints)
    // exigent un fichier et sont traitées séparément en fin de fichier.
    const std::vector<Entry>& catalog()
    {
        static const std::vector<Entry> c = {
            { "Cube",                  make<ParameterizedCube>() },
            { "Sphere",                make<ParameterizedSphere>() },
            { "Cylinder",              make<ParameterizedCylinder>() },
            { "Cone",                  make<ParameterizedCone>() },
            { "Capsule",               make<ParameterizedCapsule>() },
            { "Torus",                 make<ParameterizedTorus>() },
            { "KleinBottle",           make<ParameterizedKleinBottle>() },
            { "Helicoid",              make<ParameterizedHelicoid>() },
            { "Seashell",              make<ParameterizedSeashell>() },
            { "SeashellVonSeggern",    make<ParameterizedSeashellVonSeggern>() },
            { "Corkscrew",             make<ParameterizedCorkscrew>() },
            { "MobiusStrip",           make<ParameterizedMobiusStrip>() },
            { "RadialWave",            make<ParameterizedRadialWave>() },
            { "Breather",              make<ParameterizedBreather>() },
            { "HyperbolicParaboloid",  make<ParameterizedHyperbolicParaboloid>() },
            { "MonkeySaddle",          make<ParameterizedMonkeySaddle>() },
            { "Blobs",                 make<ParameterizedBlobs>() },
            { "Drop",                  make<ParameterizedDrop>() },
            { "Guimard",               make<ParameterizedGuimard>() },
            { "TorusKnot",             make<ParameterizedTorusKnot>() },
            { "CinquefoilKnot",        make<ParameterizedCinquefoilKnot>() },
            { "TrefoilKnot",           make<ParameterizedTrefoilKnot>() },
            { "BorromeanRings",        make<ParameterizedBorromeanRings>() },
            { "MengerSponge",          make<ParameterizedMengerSponge>() },
            { "LSystem",               make<ParameterizedLSystem>() },
            { "GothicBlock",           make<ParameterizedGothicBlock>() },
            { "GothicWindow",          make<ParameterizedGothicWindow>() },
        };
        return c;
    }

    bool allFinite(Mesh& m)
    {
        const unsigned int n = m.GetNVertices();
        for (unsigned int i = 0; i < n; ++i)
        {
            float v[3];
            m.GetVertex(i, v);
            for (int k = 0; k < 3; ++k)
                if (!std::isfinite(v[k])) return false;
        }
        return true;
    }
}

// ---------------------------------------------------------------------------
//  Contrat IParameterized, sur tout le catalogue
// ---------------------------------------------------------------------------

// GetName() alimente le menu de maker et sert de clé dans son catalogue : un nom
// vide ou dupliqué rendrait une forme inatteignable.
TEST(TEST_cgmesh_parameterized_shapes, every_shape_has_a_unique_non_empty_name)
{
    std::set<std::string> names;
    for (const Entry& e : catalog())
    {
        SCOPED_TRACE(e.label);
        auto shape = e.make();
        const std::string name = shape->GetName();
        EXPECT_FALSE(name.empty());
        EXPECT_TRUE(names.insert(name).second) << "nom deja pris : " << name;
    }
    EXPECT_EQ(names.size(), catalog().size());
}

// Le panneau de maker construit ses widgets depuis ces bornes : une valeur hors
// [min, max] positionne le curseur ailleurs que sur la valeur réelle, et un
// min >= max donne un curseur mort. Un indice d'enum hors des choix afficherait
// une entrée vide.
TEST(TEST_cgmesh_parameterized_shapes, every_parameter_is_self_consistent)
{
    for (const Entry& e : catalog())
    {
        SCOPED_TRACE(e.label);
        auto shape = e.make();
        std::vector<Parameter> params = shape->GetParameters();
        EXPECT_FALSE(params.empty()) << "aucun parametre expose";

        std::set<std::string> seen;
        for (Parameter& p : params)
        {
            SCOPED_TRACE(p.GetName());
            EXPECT_FALSE(p.GetName().empty());
            EXPECT_TRUE(seen.insert(p.GetName()).second) << "nom de parametre duplique";

            switch (p.GetType())
            {
            case Parameter::INT:
                EXPECT_LT(p.GetMinInt(), p.GetMaxInt()) << "intervalle vide ou inverse";
                EXPECT_GE(p.GetInt(), p.GetMinInt()) << "defaut sous le minimum";
                EXPECT_LE(p.GetInt(), p.GetMaxInt()) << "defaut au-dessus du maximum";
                break;
            case Parameter::FLOAT:
                EXPECT_LT(p.GetMinFloat(), p.GetMaxFloat()) << "intervalle vide ou inverse";
                EXPECT_GE(p.GetFloat(), p.GetMinFloat()) << "defaut sous le minimum";
                EXPECT_LE(p.GetFloat(), p.GetMaxFloat()) << "defaut au-dessus du maximum";
                break;
            case Parameter::ENUM:
                EXPECT_FALSE(p.GetChoices().empty());
                EXPECT_GE(p.GetInt(), 0);
                EXPECT_LT(p.GetInt(), (int)p.GetChoices().size()) << "indice hors des choix";
                for (const std::string& c : p.GetChoices())
                    EXPECT_FALSE(c.empty()) << "choix d'enum vide";
                break;
            case Parameter::BOOL:
                break;   // rien a borner
            }
        }
    }
}

// Chaque constructeur appelle Regenerate() : la forme doit être affichable dès
// l'instanciation, sans réglage préalable.
TEST(TEST_cgmesh_parameterized_shapes, every_shape_builds_a_finite_mesh_on_construction)
{
    for (const Entry& e : catalog())
    {
        SCOPED_TRACE(e.label);
        auto shape = e.make();
        Mesh* m = shape->GetMesh();
        ASSERT_NE(m, nullptr) << "aucun maillage produit par le constructeur";
        EXPECT_GT(m->GetNVertices(), 0u);
        EXPECT_GT(m->GetNFaces(), 0u);
        // NaN / inf : piege classique des surfaces implicites et des paramétrages
        // à singularité. Invisible sur un compteur de sommets, fatal au rendu.
        EXPECT_TRUE(allFinite(*m)) << "coordonnee non finie";
    }
}

// Regenerate() REMPLACE, il n'accumule pas. Un appel de plus ne doit rien changer
// à réglages constants -- c'est exactement le bug que porte encore
// buildBayMoulding sur son chemin dégénéré.
TEST(TEST_cgmesh_parameterized_shapes, regenerating_twice_is_idempotent)
{
    for (const Entry& e : catalog())
    {
        SCOPED_TRACE(e.label);
        auto shape = e.make();
        ASSERT_NE(shape->GetMesh(), nullptr);
        const unsigned int nv = shape->GetMesh()->GetNVertices();
        const unsigned int nf = shape->GetMesh()->GetNFaces();

        shape->Regenerate();
        ASSERT_NE(shape->GetMesh(), nullptr);
        EXPECT_EQ(shape->GetMesh()->GetNVertices(), nv);
        EXPECT_EQ(shape->GetMesh()->GetNFaces(),    nf);
    }
}

// Les Parameter référencent des POINTEURS CRUS vers les membres de l'objet
// (cf. parameterized.h) : écrire par le Parameter doit se voir dans la relecture.
// Un pointeur mal câblé écrirait à côté sans que rien ne le signale.
TEST(TEST_cgmesh_parameterized_shapes, writing_a_parameter_is_read_back)
{
    for (const Entry& e : catalog())
    {
        SCOPED_TRACE(e.label);
        auto shape = e.make();
        std::vector<Parameter> params = shape->GetParameters();

        for (size_t i = 0; i < params.size(); ++i)
        {
            SCOPED_TRACE(params[i].GetName());
            switch (params[i].GetType())
            {
            case Parameter::INT:
            case Parameter::ENUM:
            {
                const int target = params[i].GetMinInt() != params[i].GetInt()
                                 ? params[i].GetMinInt() : params[i].GetMaxInt();
                params[i].SetInt(target);
                EXPECT_EQ(shape->GetParameters()[i].GetInt(), target);
                break;
            }
            case Parameter::FLOAT:
            {
                const float target = params[i].GetMinFloat();
                params[i].SetFloat(target);
                EXPECT_FLOAT_EQ(shape->GetParameters()[i].GetFloat(), target);
                break;
            }
            case Parameter::BOOL:
            {
                const bool target = !params[i].GetBool();
                params[i].SetBool(target);
                EXPECT_EQ(shape->GetParameters()[i].GetBool(), target);
                break;
            }
            }
        }
    }
}

// Pousser CHAQUE paramètre d'un cran, un à la fois, puis régénérer. On ne monte
// pas au max : l'éponge de Menger et les L-systèmes végétaux exploseraient la
// durée du test. Un cran suffit à faire passer le paramètre dans le générateur.
TEST(TEST_cgmesh_parameterized_shapes, nudging_any_parameter_keeps_the_mesh_valid)
{
    for (const Entry& e : catalog())
    {
        SCOPED_TRACE(e.label);
        const size_t nParams = e.make()->GetParameters().size();

        for (size_t i = 0; i < nParams; ++i)
        {
            // Instance NEUVE par paramètre : sinon les réglages se cumulent et un
            // échec ne désigne plus un paramètre en particulier.
            auto shape = e.make();
            std::vector<Parameter> params = shape->GetParameters();
            SCOPED_TRACE(params[i].GetName());

            switch (params[i].GetType())
            {
            case Parameter::INT:
            case Parameter::ENUM:
                if (params[i].GetInt() < params[i].GetMaxInt())
                    params[i].SetInt(params[i].GetInt() + 1);
                else if (params[i].GetInt() > params[i].GetMinInt())
                    params[i].SetInt(params[i].GetInt() - 1);
                break;
            case Parameter::FLOAT:
            {
                // Un dixième de l'intervalle, borné : assez pour changer la forme,
                // trop peu pour atteindre un régime dégénéré.
                const float step = 0.1f * (params[i].GetMaxFloat() - params[i].GetMinFloat());
                const float target = std::min(params[i].GetFloat() + step, params[i].GetMaxFloat());
                params[i].SetFloat(target);
                break;
            }
            case Parameter::BOOL:
                params[i].SetBool(!params[i].GetBool());
                break;
            }

            ASSERT_NO_THROW(shape->Regenerate());
            Mesh* m = shape->GetMesh();
            ASSERT_NE(m, nullptr);
            EXPECT_GT(m->GetNVertices(), 0u);
            EXPECT_TRUE(allFinite(*m)) << "coordonnee non finie apres reglage";
        }
    }
}

// TakeMesh() TRANSFÈRE la propriété : l'objet ne doit plus la détenir, et une
// régénération doit repartir de zéro. maker s'appuie sur ce contrat, et un double
// `delete` se paierait par un crash à la destruction.
TEST(TEST_cgmesh_parameterized_shapes, take_mesh_transfers_ownership_and_regenerate_rebuilds)
{
    ParameterizedTorus shape;
    Mesh* first = shape.GetMesh();
    ASSERT_NE(first, nullptr);

    Mesh* taken = shape.TakeMesh();
    EXPECT_EQ(taken, first);
    EXPECT_EQ(shape.GetMesh(), nullptr) << "l'objet detient encore le maillage cede";

    shape.Regenerate();
    Mesh* rebuilt = shape.GetMesh();
    ASSERT_NE(rebuilt, nullptr);
    EXPECT_NE(rebuilt, taken) << "Regenerate a rendu le maillage deja cede";

    delete taken;   // l'appelant en est proprietaire
}

// ---------------------------------------------------------------------------
//  Un paramètre de résolution agit bien sur la densité
// ---------------------------------------------------------------------------
// Les tests ci-dessus vérifient qu'un réglage ne casse rien ; celui-ci vérifie
// qu'il SERT à quelque chose. Sur une sphère, `nu`/`nv` sont des résolutions de
// grille : les augmenter doit augmenter le nombre de sommets.
TEST(TEST_cgmesh_parameterized_shapes, sphere_resolution_drives_vertex_count)
{
    ParameterizedSphere coarse, fine;

    std::vector<Parameter> p = fine.GetParameters();
    int changed = 0;
    for (Parameter& q : p)
        if (q.GetType() == Parameter::INT && q.GetInt() * 2 <= q.GetMaxInt())
        {
            q.SetInt(q.GetInt() * 2);
            ++changed;
        }
    ASSERT_GT(changed, 0) << "aucun parametre de resolution trouve sur la sphere";
    fine.Regenerate();

    ASSERT_NE(coarse.GetMesh(), nullptr);
    ASSERT_NE(fine.GetMesh(), nullptr);
    EXPECT_GT(fine.GetMesh()->GetNVertices(), coarse.GetMesh()->GetNVertices());
}

// ---------------------------------------------------------------------------
//  Les formes construites depuis un FICHIER
// ---------------------------------------------------------------------------
// Elles ne sont pas dans le catalogue (elles exigent un chemin) et n'étaient
// couvertes que par leurs fonctions libres, jamais par l'enveloppe
// `Parameterized*`. Les fichiers d'exemple sont écrits par le test : pas d'actif
// binaire à maintenir, et pas d'échec dû à une donnée manquante.

namespace
{
    // Un SVG minimal : un seul contour fermé, ce qu'attend l'extrusion.
    bool writeMinimalSvg(const char* path)
    {
        std::ofstream f(path);
        if (!f) return false;
        f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
             "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\""
             " viewBox=\"0 0 100 100\">\n"
             "  <path d=\"M 20 20 L 80 20 L 80 80 L 20 80 Z\" fill=\"black\"/>\n"
             "</svg>\n";
        return true;
    }

    // PPM (P6) binaire : fond blanc, carré rouge centré. Deux régions de couleur,
    // le minimum pour que la quantification puis la vectorisation aient prise.
    bool writeMinimalPpm(const char* path)
    {
        const int W = 24, H = 24;
        std::ofstream f(path, std::ios::binary);
        if (!f) return false;
        f << "P6\n" << W << " " << H << "\n255\n";
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const bool red = (x >= 6 && x < 18 && y >= 6 && y < 18);
                const char px[3] = { (char)(red ? 255 : 255), (char)(red ? 0 : 255), (char)(red ? 0 : 255) };
                f.write(px, 3);
            }
        return true;
    }
}

TEST(TEST_cgmesh_parameterized_shapes, svg_extrusion_wrapper_builds_from_a_minimal_file)
{
    const char* svg = "./tu_param_minimal.svg";
    ASSERT_TRUE(writeMinimalSvg(svg));

    ParameterizedSvgExtrusion shape(svg);
    EXPECT_FALSE(shape.GetName().empty());
    EXPECT_FALSE(shape.GetParameters().empty());

    Mesh* m = shape.GetMesh();
    ASSERT_NE(m, nullptr) << "l'extrusion n'a rien produit depuis un SVG a un contour";
    EXPECT_GT(m->GetNVertices(), 0u);
    EXPECT_GT(m->GetNFaces(), 0u);
    EXPECT_TRUE(allFinite(*m));

    std::remove(svg);
}

TEST(TEST_cgmesh_parameterized_shapes, image_wrappers_build_from_a_minimal_file)
{
    const char* ppm = "./tu_param_minimal.ppm";
    ASSERT_TRUE(writeMinimalPpm(ppm));

    {
        ParameterizedImageRelief relief(ppm);
        EXPECT_FALSE(relief.GetName().empty());
        EXPECT_FALSE(relief.GetParameters().empty());
        Mesh* m = relief.GetMesh();
        ASSERT_NE(m, nullptr);
        EXPECT_GT(m->GetNVertices(), 0u);
        EXPECT_TRUE(allFinite(*m));
    }
    {
        ParameterizedImagePixelBlocks blocks(ppm);
        EXPECT_FALSE(blocks.GetName().empty());
        EXPECT_FALSE(blocks.GetParameters().empty());
        Mesh* m = blocks.GetMesh();
        ASSERT_NE(m, nullptr);
        EXPECT_GT(m->GetNVertices(), 0u);
        EXPECT_TRUE(allFinite(*m));
    }

    std::remove(ppm);
}

// Un chemin inexistant ne doit ni lever ni produire un maillage bancal : les
// enveloppes sont construites depuis un `<input type=file>` côté maker, donc un
// fichier illisible est un cas de DONNÉE, pas un bug.
TEST(TEST_cgmesh_parameterized_shapes, file_based_wrappers_survive_a_missing_file)
{
    EXPECT_NO_THROW({
        ParameterizedSvgExtrusion svg("./tu_param_does_not_exist.svg");
        EXPECT_FALSE(svg.GetName().empty());
    });
    EXPECT_NO_THROW({
        ParameterizedImageRelief relief("./tu_param_does_not_exist.ppm");
        EXPECT_EQ(relief.GetMesh(), nullptr);
    });
    EXPECT_NO_THROW({
        ParameterizedImagePixelBlocks blocks("./tu_param_does_not_exist.ppm");
        EXPECT_EQ(blocks.GetMesh(), nullptr);
    });
}
