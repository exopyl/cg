#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/vmeshes.h"
#include "../src/cgmesh/surface_basic.h"   // CreateCube
#include "../src/cgmesh/vmeshes_io.h"

// ===========================================================================
//  vmeshes_io.cpp — la moitié EXPORT
// ===========================================================================
//
// Seul `VMeshesIO::load` était testé (16 sites d'appel dans tu_cgmesh_io.cpp,
// tous des imports). Ni `save`, ni aucun export.
//
// Deux constats faits en écrivant ces tests, qui déterminent ce qui EST testable :
//
//  1. Tous les `export_*` sont PRIVÉS. La surface publique se réduit à `load` et
//     `save`, qui ne dispatche que `.obj` et `.stl`. `save(.stl)` produit
//     désormais du **STL BINAIRE** (correctif) : la variante binaire était écrite
//     et testée mais inatteignable, tandis que l'ASCII, seul exposé, produisait
//     des fichiers cinq fois plus gros.
//  2. Sur les cinq exports déclarés, **trois sont des stubs** : `export_obj`,
//     `export_ply` et `export_3ds` sont des `return false` nus. `save()` sur un
//     `.obj` échoue donc silencieusement, bien que le dispatch reconnaisse
//     l'extension.
//
// Les tests couvrent le chemin qui marche par ALLER-RETOUR, et figent les stubs
// pour qu'une implémentation future soit un changement reconnu.
//
// ===========================================================================

namespace
{
    // Cube unité de 12 triangles, décalé — même construction que
    // tu_cgmesh_vmodels.cpp, pour rester comparable.
    Mesh* makeCube(float ox = 0.f, float oy = 0.f, float oz = 0.f)
    {
        Mesh* m = new Mesh();
        m->Init(8, 12);
        const float h = 0.5f;
        const float V[8][3] = {{-h,-h,-h},{h,-h,-h},{h,h,-h},{-h,h,-h},
                               {-h,-h,h},{h,-h,h},{h,h,h},{-h,h,h}};
        for (int i = 0; i < 8; ++i) m->SetVertex(i, V[i][0]+ox, V[i][1]+oy, V[i][2]+oz);
        const int F[12][3] = {{0,1,2},{0,2,3},{4,5,6},{4,6,7},{0,1,5},{0,5,4},
                              {3,2,6},{3,6,7},{0,3,7},{0,7,4},{1,2,6},{1,6,5}};
        for (int i = 0; i < 12; ++i) m->SetFace(i, F[i][0], F[i][1], F[i][2]);
        m->ComputeNormals();
        return m;
    }

    // Deux cubes disjoints : de quoi vérifier qu'un export agrège TOUS les
    // maillages du VMeshes, et pas seulement le premier.
    void fillTwoCubes(VMeshes& vm)
    {
        vm.AddMesh(makeCube());
        vm.AddMesh(makeCube(10.f, 0.f, 0.f));
    }

    unsigned int totalTriangles(VMeshes& vm)
    {
        unsigned int n = 0;
        for (Mesh* m : vm.GetMeshes())
            n += (unsigned int)(m->GetTriangles().size() / 3);
        return n;
    }

    bool bboxOf(VMeshes& vm, float mn[3], float mx[3])
    {
        bool any = false;
        for (Mesh* m : vm.GetMeshes())
        {
            const unsigned int n = m->GetNVertices();
            for (unsigned int i = 0; i < n; ++i)
            {
                float v[3];
                m->GetVertex(i, v);
                for (int k = 0; k < 3; ++k)
                {
                    if (!any) { mn[k] = mx[k] = v[k]; }
                    else { mn[k] = std::min(mn[k], v[k]); mx[k] = std::max(mx[k], v[k]); }
                }
                any = true;
            }
        }
        return any;
    }

    size_t fileSize(const char* path)
    {
        std::error_code ec;
        const auto s = std::filesystem::file_size(path, ec);
        return ec ? 0u : (size_t)s;
    }
}

// ---------------------------------------------------------------------------
//  save(.stl) — aller-retour complet
// ---------------------------------------------------------------------------
// Le STL ne porte pas la découpe en objets : les deux cubes ressortent en UN seul
// solide. C'est une perte inhérente au format, pas un défaut — le test la fige
// pour qu'elle ne passe pas un jour pour une régression.
TEST(TEST_cgmesh_vmeshes_io, save_stl_round_trips_all_meshes_as_one_solid)
{
    const char* path = "./tu_vmio_two_cubes.stl";

    VMeshes out;
    fillTwoCubes(out);
    ASSERT_EQ(out.GetMeshes().size(), 2u);
    const unsigned int tris = totalTriangles(out);
    ASSERT_EQ(tris, 24u);
    float mn[3], mx[3];
    ASSERT_TRUE(bboxOf(out, mn, mx));

    ASSERT_TRUE(VMeshesIO::save(out, path));
    ASSERT_GT(fileSize(path), 0u);

    // `save` produit du STL BINAIRE : taille exacte = 80 o d'en-tete + u32 de
    // compte + 50 o par triangle. Un ecart signale un en-tete ou un pas faux.
    EXPECT_EQ(fileSize(path), 84u + 50u * tris);

    VMeshes back;
    ASSERT_TRUE(VMeshesIO::load(back, path));
    EXPECT_EQ(back.GetMeshes().size(), 1u) << "STL concatene : un seul solide attendu";
    EXPECT_EQ(totalTriangles(back), tris) << "triangles perdus a l'aller-retour";

    // La géométrie revient au même endroit, aux arrondis du format près.
    float rmn[3], rmx[3];
    ASSERT_TRUE(bboxOf(back, rmn, rmx));
    for (int k = 0; k < 3; ++k)
    {
        EXPECT_NEAR(rmn[k], mn[k], 1e-3f);
        EXPECT_NEAR(rmx[k], mx[k], 1e-3f);
    }

    std::remove(path);
}

// L'export agrège bien : deux fois plus de maillages en entrée, deux fois plus de
// triangles en sortie. Attrape un export qui ne prendrait que le premier Mesh.
TEST(TEST_cgmesh_vmeshes_io, save_stl_aggregates_every_mesh)
{
    const char* onePath = "./tu_vmio_one.stl";
    const char* twoPath = "./tu_vmio_two.stl";

    VMeshes one;
    one.AddMesh(makeCube());
    ASSERT_TRUE(VMeshesIO::save(one, onePath));

    VMeshes two;
    fillTwoCubes(two);
    ASSERT_TRUE(VMeshesIO::save(two, twoPath));

    VMeshes backOne, backTwo;
    ASSERT_TRUE(VMeshesIO::load(backOne, onePath));
    ASSERT_TRUE(VMeshesIO::load(backTwo, twoPath));
    EXPECT_EQ(totalTriangles(backOne), 12u);
    EXPECT_EQ(totalTriangles(backTwo), 24u);

    std::remove(onePath);
    std::remove(twoPath);
}

// Scène vide : `save` réussit et produit un STL binaire VALIDE mais sans facette,
// soit exactement 84 octets — 80 d'en-tête + le u32 de compte à zéro. Un fichier
// plus court (ou vide) casserait les lecteurs tiers, qui exigent l'en-tête.
TEST(TEST_cgmesh_vmeshes_io, saving_an_empty_scene_succeeds_and_yields_no_triangle)
{
    const char* path = "./tu_vmio_empty.stl";

    VMeshes empty;
    ASSERT_TRUE(VMeshesIO::save(empty, path));
    ASSERT_TRUE(std::filesystem::exists(path));
    EXPECT_EQ(fileSize(path), 84u) << "en-tete binaire seul attendu, aucun triangle";

    // Et il se relit sans erreur.
    VMeshes back;
    EXPECT_TRUE(VMeshesIO::load(back, path));
    EXPECT_EQ(totalTriangles(back), 0u);

    std::remove(path);
}

// ---------------------------------------------------------------------------
//  Faces non triangulaires
// ---------------------------------------------------------------------------
// Le STL n'admet que des triangles : une face a plus de trois sommets doit etre
// TRIANGULEE a l'export.
//
// Ce n'etait pas le cas : `collectTriangles` faisait `if (GetNVertices() != 3)
// continue;` et ignorait ces faces EN SILENCE. L'impact n'etait pas theorique --
// `CreateCube()` a `bTri = false` par defaut, donc produit des quads, et c'est la
// scene initiale de sinaia : l'enregistrer en STL donnait un fichier sans aucune
// facette. Corrige en s'appuyant sur `Mesh::GetTriangles()`, qui triangule
// (eventail pour les convexes, glutess pour les concaves).
TEST(TEST_cgmesh_vmeshes_io, quad_faces_are_triangulated_on_save)
{
    const char* path = "./tu_vmio_quads.stl";

    // SetFaces(nFaces, nVerticesPerFace, ...) et non Init+SetFace : Init prealloue
    // des faces TRIANGULAIRES, qu'un SetFace a quatre sommets ne peut pas elargir.
    Mesh* quad = new Mesh();
    float verts[] = { 0.f,0.f,0.f,  1.f,0.f,0.f,  1.f,1.f,0.f,  0.f,1.f,0.f };
    quad->SetVertices(4, verts);
    unsigned int f[] = { 0, 1, 2, 3 };
    quad->SetFaces(1, 4, f);
    quad->ComputeNormals();
    ASSERT_EQ(quad->GetNFaces(), 1u);
    ASSERT_EQ(quad->GetFaceNVertices(0), 4) << "la face doit bien etre un quad";
    ASSERT_EQ(quad->GetTriangles().size() / 3, 2u);

    VMeshes out;
    out.AddMesh(quad);
    ASSERT_TRUE(VMeshesIO::save(out, path));

    VMeshes back;
    ASSERT_TRUE(VMeshesIO::load(back, path));
    EXPECT_EQ(totalTriangles(back), 2u) << "un quad doit sortir en deux triangles";

    std::remove(path);
}

// Le cas qui motivait le correctif : la scene par defaut de sinaia est un
// CreateCube() a QUADS. Son enregistrement doit contenir toutes ses facettes.
TEST(TEST_cgmesh_vmeshes_io, the_default_quad_cube_exports_all_its_facets)
{
    const char* path = "./tu_vmio_default_cube.stl";

    Mesh* cube = CreateCube();          // bTri = false par defaut => quads
    ASSERT_NE(cube, nullptr);
    ASSERT_GT(cube->GetNFaces(), 0u);
    ASSERT_EQ(cube->GetFaceNVertices(0), 4) << "CreateCube() est bien a quads";
    const unsigned int tris = (unsigned int)(cube->GetTriangles().size() / 3);
    ASSERT_EQ(tris, 12u) << "6 quads => 12 triangles";

    VMeshes out;
    out.AddMesh(cube);
    ASSERT_TRUE(VMeshesIO::save(out, path));

    VMeshes back;
    ASSERT_TRUE(VMeshesIO::load(back, path));
    EXPECT_EQ(totalTriangles(back), tris) << "facettes perdues a l'export";

    std::remove(path);
}

// Contre-epreuve : le MEME cube deja triangule donne exactement le meme nombre de
// facettes. La triangulation a l'export n'ajoute ni ne retire rien.
TEST(TEST_cgmesh_vmeshes_io, the_quad_cube_and_the_triangulated_cube_export_alike)
{
    const char* quadPath = "./tu_vmio_cube_quads.stl";
    const char* triPath  = "./tu_vmio_cube_tris.stl";

    VMeshes quads;  quads.AddMesh(CreateCube(false));
    VMeshes tris;   tris.AddMesh(CreateCube(true));
    ASSERT_TRUE(VMeshesIO::save(quads, quadPath));
    ASSERT_TRUE(VMeshesIO::save(tris,  triPath));

    // Meme nombre de triangles => meme taille de fichier binaire.
    EXPECT_EQ(fileSize(quadPath), fileSize(triPath));

    VMeshes backQuads, backTris;
    ASSERT_TRUE(VMeshesIO::load(backQuads, quadPath));
    ASSERT_TRUE(VMeshesIO::load(backTris,  triPath));
    EXPECT_EQ(totalTriangles(backQuads), totalTriangles(backTris));

    std::remove(quadPath);
    std::remove(triPath);
}

// ---------------------------------------------------------------------------
//  save() — refus francs
// ---------------------------------------------------------------------------
// Toute extension hors .obj / .stl échoue SANS écrire : mieux vaut un refus net
// qu'un fichier au contenu inattendu.
TEST(TEST_cgmesh_vmeshes_io, save_refuses_an_unknown_extension_without_writing)
{
    const char* path = "./tu_vmio_unknown.xyz";

    VMeshes out;
    fillTwoCubes(out);
    EXPECT_FALSE(VMeshesIO::save(out, path));
    EXPECT_FALSE(std::filesystem::exists(path)) << "un fichier a ete cree malgre l'echec";
}

// Garde-fou de save() sur les noms trop courts pour porter une extension
// (`if (size < 4) return false;`) : sans lui, la lecture de filename[size-3]
// sortirait du tampon.
TEST(TEST_cgmesh_vmeshes_io, save_refuses_a_name_too_short_to_carry_an_extension)
{
    VMeshes out;
    fillTwoCubes(out);
    EXPECT_FALSE(VMeshesIO::save(out, "ab"));
    EXPECT_FALSE(VMeshesIO::save(out, ""));
}

// Répertoire inexistant : l'échec doit être rapporté, pas avalé.
TEST(TEST_cgmesh_vmeshes_io, save_reports_failure_on_an_unwritable_path)
{
    VMeshes out;
    fillTwoCubes(out);
    EXPECT_FALSE(VMeshesIO::save(out, "./tu_vmio_no_such_dir/x.stl"));
}

// ---------------------------------------------------------------------------
//  État figé de ce qui n'est PAS implémenté
// ---------------------------------------------------------------------------
// `export_obj` est un `return false` nu, alors que `save()` reconnaît `.obj`.
// C'est le piège le plus probable pour un appelant : l'extension est acceptée par
// le dispatch, l'enregistrement échoue quand même, et aucun fichier n'apparaît.
//
// Ce test ne valide pas ce comportement, il le DOCUMENTE : le jour où
// `export_obj` sera écrit, il échouera et forcera une mise à jour consciente.
TEST(TEST_cgmesh_vmeshes_io, save_obj_still_fails_because_the_exporter_is_a_stub)
{
    const char* path = "./tu_vmio_saved.obj";

    VMeshes out;
    fillTwoCubes(out);
    EXPECT_FALSE(VMeshesIO::save(out, path))
        << "save(.obj) reussit desormais : export_obj a ete implemente, mettre ce test a jour";
    EXPECT_FALSE(std::filesystem::exists(path));
}

// Contre-épreuve du dispatch : la MÊME scène s'enregistre en .stl. L'échec du .obj
// vient donc bien de l'exportateur, pas de la scène ni de save() lui-même.
TEST(TEST_cgmesh_vmeshes_io, the_same_scene_that_fails_as_obj_saves_as_stl)
{
    const char* objPath = "./tu_vmio_pair.obj";
    const char* stlPath = "./tu_vmio_pair.stl";

    VMeshes out;
    fillTwoCubes(out);
    EXPECT_FALSE(VMeshesIO::save(out, objPath));
    EXPECT_TRUE (VMeshesIO::save(out, stlPath));

    std::remove(stlPath);
}
