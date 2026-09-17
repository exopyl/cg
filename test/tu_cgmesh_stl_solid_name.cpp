#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <iterator>
#include <fstream>
#include <string>
#include <vector>

#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_io.h"
#include "../src/cgmesh/surface_basic.h"   // CreateCube

// ===========================================================================
//  Noms dérivés du chemin de sortie, écrits dans un fichier GÉNÉRÉ
// ===========================================================================
//
// Les deux exportateurs STL ASCII — `MeshIO::export_stl` (mesh_io.cpp) et
// `VMeshesIO::export_stl` (vmeshes_io.cpp) — fabriquent le nom du solide à
// partir du CHEMIN DE SORTIE, donc d'une donnée fournie par l'utilisateur, et
// l'écrivaient tel quel dans `solid <nom>` / `endsolid <nom>`.
//
// Un seul des deux est testé ici, et c'est délibéré : `VMeshesIO::export_stl`
// est privé ET sans appelant — `vmeshes_io.cpp:45` le documente lui-même
// (« reste implemente mais n'a plus d'appelant »), `save(.stl)` produisant du
// STL binaire. Il a reçu la même correction, par cohérence, mais il n'y a rien
// à y exercer tant qu'il reste mort.
//
// Un caractère de contrôle dans ce nom ne casse pas la mémoire : il casse la
// STRUCTURE du fichier produit. Un saut de ligne clôt le solide et en ouvre un
// autre, de sorte qu'un simple nom de fichier suffit à fabriquer un STL à
// plusieurs solides — que le consommateur suivant lira comme une géométrie
// qu'on ne lui a jamais demandé d'écrire.
//
// ⚠ PORTÉE RÉELLE DU DÉFAUT, à ne pas surestimer : sous Windows, Win32 refuse
// déjà les caractères de contrôle dans un nom de fichier, donc ce chemin n'y
// est pas atteignable. Sous Linux, tout octet sauf '/' et NUL est permis dans
// un nom : il l'est. Le second test ci-dessous est donc conditionné, et c'est
// la CI Linux qui l'exécute — pas la machine de développement Windows.
//
// ===========================================================================

namespace {

std::vector<std::string> linesOf (const std::string& path)
{
    std::vector<std::string> lines;
    std::ifstream ifs (path);
    std::string line;
    while (std::getline (ifs, line))
    {
        if (!line.empty () && line.back () == '\r') line.pop_back ();
        lines.push_back (line);
    }
    return lines;
}

// Préfixe strict : « endsolid » ne doit PAS compter comme « solid ».
bool startsWith (const std::string& s, const std::string& prefix)
{
    return s.rfind (prefix, 0) == 0;
}

size_t countPrefixed (const std::vector<std::string>& lines, const std::string& prefix)
{
    size_t n = 0;
    for (const std::string& l : lines)
        if (startsWith (l, prefix)) ++n;
    return n;
}

} // namespace

// Non-régression : un nom ordinaire doit ressortir INTACT. C'est la moitié qui
// compte le plus au quotidien — un nettoyage trop large écraserait les accents
// des noms de fichiers français, et personne ne s'en apercevrait avant de lire
// un STL nommé « mod__le ».
TEST(TEST_cgmesh_stl_solid_name, an_ordinary_name_is_written_verbatim)
{
    const std::string stem = "tu_stl_piece-de-reference";
    const std::string path = "./" + stem + ".stl";

    Mesh* cube = CreateCube (true);
    ASSERT_NE(cube, nullptr);
    cube->ComputeNormals ();
    ASSERT_EQ(MeshIO::save (*cube, path.c_str ()), 0);

    const std::vector<std::string> lines = linesOf (path);
    ASSERT_FALSE(lines.empty ());
    EXPECT_EQ(lines.front (), "solid " + stem);
    EXPECT_EQ(countPrefixed (lines, "solid "), 1u);
    EXPECT_EQ(countPrefixed (lines, "endsolid "), 1u);

    delete cube;
    std::remove (path.c_str ());
}

#ifndef _WIN32
// Le cœur du correctif. Sans lui, ce fichier contient DEUX solides : le nom
// injecte « endsolid x » puis « solid y » avant même le premier facet.
//
// Non exécuté sous Windows (Win32 refuse le caractère de contrôle dans le nom
// de fichier, donc `fopen` échouerait avant d'écrire quoi que ce soit).
TEST(TEST_cgmesh_stl_solid_name, control_characters_cannot_break_the_solid_structure)
{
    std::string stem = "tu_stl_injecte";
    stem += '\n';
    stem += "endsolid x";
    stem += '\n';
    stem += "solid y";
    const std::string path = "./" + stem + ".stl";

    Mesh* cube = CreateCube (true);
    ASSERT_NE(cube, nullptr);
    cube->ComputeNormals ();
    ASSERT_EQ(MeshIO::save (*cube, path.c_str ()), 0)
        << "le systeme de fichiers doit accepter ce nom pour que le test ait un sens";

    const std::vector<std::string> lines = linesOf (path);
    ASSERT_FALSE(lines.empty ());
    EXPECT_EQ(countPrefixed (lines, "solid "), 1u)
        << "le nom a ouvert un second solide : la structure du STL est injectable";
    EXPECT_EQ(countPrefixed (lines, "endsolid "), 1u);

    delete cube;
    std::remove (path.c_str ());
}
#endif


// ===========================================================================
//  export_cpp : le stem devient un IDENTIFIANT C++
// ===========================================================================
//
// `MeshIO::save(*.cpp)` génère un fichier source. Le stem y sert d'identifiant
// et le chemin complet y était écrit dans un commentaire `/* ... */`.

// Exerçable partout : l'espace et le tiret sont des caractères légaux dans un
// nom de fichier sous Windows comme sous Linux, et produisaient un identifiant
// C++ invalide — donc un fichier généré qui ne compile pas, sans que l'export
// signale quoi que ce soit.
TEST(TEST_cgmesh_generated_cpp, stem_becomes_a_valid_cpp_identifier)
{
    const std::string path = "./tu_cpp piece-de test.cpp";

    Mesh* cube = CreateCube (true);
    ASSERT_NE(cube, nullptr);
    cube->ComputeNormals ();
    ASSERT_EQ(MeshIO::save (*cube, path.c_str ()), 0);

    std::ifstream ifs (path);
    ASSERT_TRUE(ifs.good ());
    const std::string body ((std::istreambuf_iterator<char>(ifs)),
                             std::istreambuf_iterator<char>());

    EXPECT_NE(body.find ("tu_cpp_piece_de_test_n_vertices"), std::string::npos)
        << "l'espace et le tiret doivent devenir des soulignes";
    // On vise l'IDENTIFIANT, pas le fichier entier : le chemin brut figure
    // legitimement dans le commentaire d'en-tete.
    EXPECT_EQ(body.find ("piece-de test_n_vertices"), std::string::npos)
        << "aucun caractere invalide ne doit subsister dans un identifiant";

    // Le chemin est désormais dans un commentaire de LIGNE : plus de `/*`
    // ouvrant qu'un « */ » venu du chemin pourrait refermer.
    EXPECT_EQ(body.find ("/*"), std::string::npos)
        << "un commentaire de bloc est refermable depuis le chemin";

    ifs.close ();
    delete cube;
    std::remove (path.c_str ());
}

#ifndef _WIN32
// L'injection proprement dite. Un répertoire nommé « a* » est légal sous Linux ;
// « a*/ » refermait le commentaire de bloc et faisait passer la suite du chemin
// pour du code dans un fichier destiné à être compilé.
//
// Non exécuté sous Windows, qui interdit « * » dans un nom de fichier.
TEST(TEST_cgmesh_generated_cpp, a_star_in_the_path_cannot_close_a_comment)
{
    const std::string dir  = "./tu_cpp_a*";
    const std::string path = dir + "/piece.cpp";

    std::error_code ec;
    std::filesystem::create_directories (dir, ec);
    ASSERT_FALSE(ec) << "le systeme de fichiers doit accepter ce nom : " << ec.message ();

    Mesh* cube = CreateCube (true);
    ASSERT_NE(cube, nullptr);
    cube->ComputeNormals ();
    ASSERT_EQ(MeshIO::save (*cube, path.c_str ()), 0);

    std::ifstream ifs (path);
    ASSERT_TRUE(ifs.good ());
    const std::string body ((std::istreambuf_iterator<char>(ifs)),
                             std::istreambuf_iterator<char>());
    // L'invariant est « aucun commentaire de bloc n'est ouvert », pas « la
    // sequence */ est absente » : le chemin en porte une, legitimement, et elle
    // est inerte tant qu'il n'y a rien a refermer.
    EXPECT_EQ(body.find ("/*"), std::string::npos)
        << "un commentaire de bloc est refermable depuis le chemin";

    // Et le chemin reste confine dans SA ligne de commentaire.
    const std::vector<std::string> lines = linesOf (path);
    size_t seen = 0;
    for (const std::string& l : lines)
        if (l.find (dir) != std::string::npos)
        {
            ++seen;
            EXPECT_TRUE(startsWith (l, "//"))
                << "le chemin est sorti de son commentaire de ligne : " << l;
        }
    EXPECT_NE(seen, 0u) << "le chemin doit figurer dans l'en-tete generee";

    ifs.close ();
    delete cube;
    std::filesystem::remove_all (dir, ec);
}
#endif
