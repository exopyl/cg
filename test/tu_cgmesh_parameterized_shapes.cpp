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
#include "../src/cgmesh/lsysteminit.h"   // les valeurs d'enum LSYSTEM_*

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

    // Les formes du catalogue de maker (wasm_api.cpp), c'est-à-dire toutes
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
            { "Seashell",              make<ParameterizedSeashell>() },
            { "SeashellVonSeggern",    make<ParameterizedSeashellVonSeggern>() },
            { "KleinBottle",           make<ParameterizedKleinBottle>() },
            { "Breather",              make<ParameterizedBreather>() },
            { "HyperbolicParaboloid",  make<ParameterizedHyperbolicParaboloid>() },
            { "MonkeySaddle",          make<ParameterizedMonkeySaddle>() },
            { "Blobs",                 make<ParameterizedBlobs>() },
            { "Drop",                  make<ParameterizedDrop>() },
            { "TorusKnot",             make<ParameterizedTorusKnot>() },
            { "CinquefoilKnot",        make<ParameterizedCinquefoilKnot>() },
            { "TrefoilKnot",           make<ParameterizedTrefoilKnot>() },
            { "BorromeanRings",        make<ParameterizedBorromeanRings>() },
            { "Helicoid",              make<ParameterizedHelicoid>() },
            { "Corkscrew",             make<ParameterizedCorkscrew>() },
            { "MobiusStrip",           make<ParameterizedMobiusStrip>() },
            { "RadialWave",            make<ParameterizedRadialWave>() },
            { "Guimard",               make<ParameterizedGuimard>() },
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

// ---------------------------------------------------------------------------
//  ParameterizedLSystem : le paramètre Mode
// ---------------------------------------------------------------------------
// La classe rend le même tracé de deux façons. En Tube, un tube de rayon
// `Thickness` est balayé le long de la marche tortue -- de la géométrie 3D, y
// compris pour les systèmes 3D du catalogue. En Extrusion, le tracé est épaissi
// DANS SON PLAN puis extrudé sur `Height` : le résultat est un prisme droit, donc
// imprimable à plat. Les tests génériques ci-dessus ne poussent chaque paramètre
// que d'un cran ; ils passent bien par Mode = 1, mais ne vérifient rien de ce qui
// caractérise l'extrusion.

namespace
{
    // Les Parameter portent des pointeurs vers les membres : écrire par l'un
    // d'eux équivaut à affecter le membre, sans exposer l'interne.
    void setInt(ParameterizedMesh& s, const char* name, int v)
    {
        std::vector<Parameter> p = s.GetParameters();
        for (Parameter& q : p)
            if (q.GetName() == name) { q.SetInt(v); return; }
        FAIL() << "parametre absent : " << name;
    }

    void setFloat(ParameterizedMesh& s, const char* name, float v)
    {
        std::vector<Parameter> p = s.GetParameters();
        for (Parameter& q : p)
            if (q.GetName() == name) { q.SetFloat(v); return; }
        FAIL() << "parametre absent : " << name;
    }

    void bbox(Mesh& m, float mn[3], float mx[3])
    {
        for (int k = 0; k < 3; ++k) { mn[k] = 1e30f; mx[k] = -1e30f; }
        for (unsigned int i = 0; i < m.GetNVertices(); ++i)
        {
            float v[3];
            m.GetVertex(i, v);
            for (int k = 0; k < 3; ++k)
            {
                if (v[k] < mn[k]) mn[k] = v[k];
                if (v[k] > mx[k]) mx[k] = v[k];
            }
        }
    }

    // Volume signé (théorème de la divergence) et aire du capot supérieur -- même
    // oracle que tu_cgmesh_extrude_contours.cpp : pour un prisme FERMÉ le volume
    // vaut exactement aire x hauteur.
    void measurePrism(Mesh& m, float height, double& volume, double& topArea)
    {
        volume = 0.0; topArea = 0.0;
        for (unsigned int f = 0; f < m.GetNFaces(); ++f)
        {
            if (m.GetFaceNVertices(f) != 3) continue;
            float p[3][3];
            for (int k = 0; k < 3; ++k)
                m.GetVertex((unsigned int)m.GetFaceVertex(f, k), p[k]);
            volume += ((double)p[0][0]*((double)p[1][1]*p[2][2]-(double)p[2][1]*p[1][2])
                     - (double)p[0][1]*((double)p[1][0]*p[2][2]-(double)p[2][0]*p[1][2])
                     + (double)p[0][2]*((double)p[1][0]*p[2][1]-(double)p[2][0]*p[1][1])) / 6.0;
            if (std::fabs(p[0][2]-height) < 1e-5f && std::fabs(p[1][2]-height) < 1e-5f
             && std::fabs(p[2][2]-height) < 1e-5f)
                topArea += (((double)p[1][0]-p[0][0])*((double)p[2][1]-p[0][1])
                          - ((double)p[1][1]-p[0][1])*((double)p[2][0]-p[0][0])) / 2.0;
        }
    }

    // Hilbert (le défaut) : tracé OUVERT, donc épaissi. Van Koch Snowflake est
    // marqué fermé, donc tessellé tel quel.
    std::unique_ptr<ParameterizedLSystem> extrudedLSystem(int system, int iterations,
                                                          float thickness, float height)
    {
        std::unique_ptr<ParameterizedLSystem> s(new ParameterizedLSystem());
        setInt  (*s, "Mode",       1);
        setInt  (*s, "System",     system);
        setInt  (*s, "Iterations", iterations);
        setFloat(*s, "Thickness",  thickness);
        setFloat(*s, "Height",     height);
        s->Regenerate();
        return s;
    }
}

// Le nombre de récursions est plafonné PAR SYSTÈME : les règles prolifiques
// dépassent le plafond MAX_SEG de 60 000 segments, qui tronquerait le tracé en
// silence -- figure incomplète, aucun signalement --, et les règles lentes ont au
// contraire besoin de plus de récursions pour prendre forme. Une valeur unique
// tronquerait les unes et brimerait les autres.
//
// Le contrôle est en deux volets. Au-delà du plafond, rien ne change : il
// s'applique. Juste en-dessous, tout change encore : il ne rogne pas une récursion
// de trop. Le second volet est indispensable -- sans lui, un plafond trop bas
// passerait inaperçu.
TEST(TEST_cgmesh_parameterized_shapes, lsystem_caps_iterations_per_system)
{
    struct Capped { int system; int cap; const char* label; };
    const std::vector<Capped> capped = {
        { LSYSTEM_HILBERT_CURVE_3D,        1, "Hilbert curve 3D" },
        { LSYSTEM_PLANT1,                  2, "Plant1" },
        { LSYSTEM_QUADRATIC_KOCH_ISLAND_B, 3, "Quadratic Koch island B" },
        { LSYSTEM_PLANT3,                  3, "Plant3" },
        { LSYSTEM_QUADRATIC_GOSPER,        3, "Quadratic Gosper" },
        { LSYSTEM_BOARD,                   4, "Board" },
        { LSYSTEM_KOCH_CURVE,              4, "Koch Curve" },
        { LSYSTEM_QUADRATIC_KOCH_ISLAND_A, 4, "Quadratic Koch island A" },
        { LSYSTEM_PEANO_CURVE,             4, "Peano curve" },
        { LSYSTEM_HEXAGONAL_GOSPER,        4, "Hexagonal Gosper" },
        { LSYSTEM_CROSS_A,                 4, "Cross A" },
        { LSYSTEM_RINGS,                   4, "Rings" },
        { LSYSTEM_BUSH1,                   4, "Bush1" },
        { LSYSTEM_BUSH2,                   4, "Bush2" },
        { LSYSTEM_BUSH3,                   4, "Bush3" },
        { LSYSTEM_CROSS_B,                 5, "Cross B" },
        { LSYSTEM_BUSH4,                   6, "Bush4" },
        { LSYSTEM_PLANT2,                  6, "Plant2" },
        // Plafonds RELEVÉS au-dessus du défaut de six : croissance en 3^n et 2^n.
        { LSYSTEM_SIERPINSKI_ARROWHEAD,    7, "Sierpinski Arrowhead" },
        { LSYSTEM_DRAGON_CURVE,            9, "Dragon curve" },
    };

    for (const Capped& c : capped)
    {
        SCOPED_TRACE(c.label);
        auto at = [&](int iterations)
        {
            ParameterizedLSystem s;
            setInt(s, "Mode", 0);
            setInt(s, "System", c.system);
            setInt(s, "Iterations", iterations);
            s.Regenerate();
            // Le plafond porte sur une COPIE : le réglage posé reste intact, sinon
            // redescendre vers un système moins prolifique ne le retrouverait pas.
            EXPECT_EQ(s.GetParameters()[2].GetInt(), iterations)
                << "le plafond a ecrase le reglage de l'utilisateur";
            // Et il est annoncé au panneau, qui construit son curseur avec.
            EXPECT_EQ(s.GetParameters()[2].GetMaxInt(), c.cap)
                << "borne annoncee differente du plafond applique";
            Mesh* m = s.GetMesh();
            return m ? m->GetNVertices() : 0u;
        };

        const unsigned int atCap = at(c.cap);
        EXPECT_GT(atCap, 0u);
        EXPECT_EQ(at(c.cap + 1), atCap) << "le plafond ne s'applique pas";
        EXPECT_NE(at(c.cap - 1), atCap) << "plafonne une recursion de trop";
    }

    // Les autres systèmes gardent le défaut de six.
    ParameterizedLSystem hilbert;
    setInt(hilbert, "System", LSYSTEM_HILBERT);
    EXPECT_EQ(hilbert.GetParameters()[2].GetMaxInt(), 6);
}

// Un prisme droit : toute la géométrie tient entre z = 0 et z = Height. C'est ce
// qui distingue le mode -- le tube, lui, occupe de l'épaisseur sur les trois axes.
TEST(TEST_cgmesh_parameterized_shapes, lsystem_extrusion_is_a_flat_prism)
{
    const float h = 0.05f;
    auto s = extrudedLSystem(LSYSTEM_HILBERT, 3, 0.04f, h);
    Mesh* m = s->GetMesh();
    ASSERT_NE(m, nullptr);
    ASSERT_GT(m->GetNVertices(), 0u);
    EXPECT_TRUE(allFinite(*m));

    float mn[3], mx[3];
    bbox(*m, mn, mx);
    EXPECT_NEAR(mn[2], 0.f, 1e-6f);
    EXPECT_NEAR(mx[2], h,   1e-6f);
    // Le tracé est normalisé sur une diagonale de bbox valant 4 : son étendue XY
    // est donc de l'ordre de l'unité, sans commune mesure avec la hauteur.
    EXPECT_GT(mx[0] - mn[0], 10.f * h);
    EXPECT_GT(mx[1] - mn[1], 10.f * h);
}

// Le mode Tube, lui, n'est pas plan : le tube monte de part et d'autre du tracé.
TEST(TEST_cgmesh_parameterized_shapes, lsystem_tube_mode_is_not_flat)
{
    ParameterizedLSystem s;
    setInt(s, "Mode", 0);
    setInt(s, "System", LSYSTEM_HILBERT);
    setInt(s, "Iterations", 3);
    s.Regenerate();
    ASSERT_NE(s.GetMesh(), nullptr);

    float mn[3], mx[3];
    bbox(*s.GetMesh(), mn, mx);
    // Un tracé 2D balayé par un tube occupe de l'épaisseur en z. La valeur exacte
    // dépend de l'orientation de la section : CreateTubes la construit à 6 côtés,
    // donc un hexagone de rayon circonscrit r, qui mesure 2r de sommet à sommet et
    // r*sqrt(3) de plat à plat. L'encadrement est le seul énoncé qui ne dépende pas
    // de cette orientation.
    const float r = 0.04f;
    EXPECT_GE(mx[2] - mn[2], std::sqrt(3.f) * r - 1e-3f);
    EXPECT_LE(mx[2] - mn[2], 2.f * r + 1e-3f);
    EXPECT_LT(mn[2], 0.f) << "le tube devrait descendre sous le plan du trace";
}

// Height agit, et proportionnellement : deux hauteurs -> deux volumes dans le même
// rapport, l'empreinte étant inchangée.
TEST(TEST_cgmesh_parameterized_shapes, lsystem_extrusion_height_scales_the_volume)
{
    auto thin  = extrudedLSystem(LSYSTEM_HILBERT, 3, 0.04f, 0.02f);
    auto thick = extrudedLSystem(LSYSTEM_HILBERT, 3, 0.04f, 0.08f);
    ASSERT_NE(thin->GetMesh(), nullptr);
    ASSERT_NE(thick->GetMesh(), nullptr);

    double vThin = 0, aThin = 0, vThick = 0, aThick = 0;
    measurePrism(*thin->GetMesh(),  0.02f, vThin,  aThin);
    measurePrism(*thick->GetMesh(), 0.08f, vThick, aThick);

    ASSERT_GT(std::fabs(aThin), 1e-6);
    EXPECT_NEAR(std::fabs(aThick), std::fabs(aThin), 1e-4) << "l'empreinte a bouge";
    EXPECT_NEAR(std::fabs(vThick), 4.0 * std::fabs(vThin), 1e-3);
}

// Thickness est la DEMI-largeur du trait : l'épaissir élargit l'empreinte, donc
// l'aire du capot. Même grandeur que le rayon du tube dans l'autre mode.
TEST(TEST_cgmesh_parameterized_shapes, lsystem_extrusion_thickness_widens_the_footprint)
{
    auto slim = extrudedLSystem(LSYSTEM_HILBERT, 3, 0.02f, 0.05f);
    auto fat  = extrudedLSystem(LSYSTEM_HILBERT, 3, 0.06f, 0.05f);

    double v = 0, aSlim = 0, aFat = 0;
    measurePrism(*slim->GetMesh(), 0.05f, v, aSlim);
    measurePrism(*fat->GetMesh(),  0.05f, v, aFat);
    EXPECT_GT(std::fabs(aFat), std::fabs(aSlim) * 1.5)
        << "tripler la largeur n'a pas elargi l'empreinte";
}

// Le tracé épaissi doit sortir ÉTANCHE : le volume signé vaut aire x hauteur.
// C'est l'oracle du correctif de tessellateContours (points de contour confondus
// écartés par glutess, cf. tu_cgmesh_extrude_contours.cpp) : sans lui, l'écart se
// comptait en dizaines de pour cent sur ce genre de tracé dense.
TEST(TEST_cgmesh_parameterized_shapes, lsystem_extrusion_is_watertight)
{
    const float h = 0.05f;
    auto s = extrudedLSystem(LSYSTEM_HILBERT, 3, 0.04f, h);
    ASSERT_NE(s->GetMesh(), nullptr);

    double volume = 0, topArea = 0;
    measurePrism(*s->GetMesh(), h, volume, topArea);
    ASSERT_GT(std::fabs(topArea), 1e-6);
    EXPECT_NEAR(std::fabs(volume) / (std::fabs(topArea) * h), 1.0, 1e-3)
        << "volume " << volume << " au lieu de " << (topArea * h)
        << " : des parois manquent";
}

// Un système 3D du catalogue reste PLAN en Extrusion : une extrusion se fait dans
// un plan, projeter la marche tortue 3D n'aurait pas de sens. Regenerate bascule
// donc sur l'interprétation 2D quel que soit le système.
TEST(TEST_cgmesh_parameterized_shapes, lsystem_extrusion_flattens_a_3d_system)
{
    const float h = 0.05f;
    // Une seule itération : la marche d'un système 3D projetée en 2D se replie sur
    // elle-même, et l'union Clipper2 d'un tracé aussi redondant coûte cher -- 15 s
    // à l'itération 2, une fraction de seconde ici, pour le même énoncé.
    auto s = extrudedLSystem(LSYSTEM_HILBERT_CURVE_3D, 1, 0.04f, h);
    Mesh* m = s->GetMesh();
    ASSERT_NE(m, nullptr);
    ASSERT_GT(m->GetNVertices(), 0u);

    float mn[3], mx[3];
    bbox(*m, mn, mx);
    EXPECT_NEAR(mn[2], 0.f, 1e-6f);
    EXPECT_NEAR(mx[2], h,   1e-6f);
}

// Un tracé marqué FERMÉ délimite une surface : elle est tessellée telle quelle, pas
// épaissie -- sinon on n'obtiendrait que son contour creux. Le contrôle est que
// Thickness, la demi-largeur du trait, n'a alors aucun effet sur l'empreinte.
TEST(TEST_cgmesh_parameterized_shapes, lsystem_extrusion_fills_a_closed_trace)
{
    const float h = 0.05f;
    auto slim = extrudedLSystem(LSYSTEM_VAN_KOCH_SNOWFLAKE, 3, 0.01f, h);
    auto fat  = extrudedLSystem(LSYSTEM_VAN_KOCH_SNOWFLAKE, 3, 0.20f, h);
    ASSERT_NE(slim->GetMesh(), nullptr);
    ASSERT_NE(fat->GetMesh(),  nullptr);

    double v = 0, aSlim = 0, aFat = 0;
    measurePrism(*slim->GetMesh(), h, v, aSlim);
    measurePrism(*fat->GetMesh(),  h, v, aFat);
    ASSERT_GT(std::fabs(aSlim), 1e-6);
    EXPECT_NEAR(std::fabs(aFat), std::fabs(aSlim), 1e-6)
        << "Thickness a joue sur un trace ferme : il a donc ete epaissi";

    // Et c'est bien un plein, pas un anneau : le flocon de Van Koch à l'itération 3
    // remplit largement sa boîte englobante.
    float mn[3], mx[3];
    bbox(*slim->GetMesh(), mn, mx);
    const double boxArea = (double)(mx[0]-mn[0]) * (mx[1]-mn[1]);
    EXPECT_GT(std::fabs(aSlim), 0.4 * boxArea) << "empreinte creuse";
}
