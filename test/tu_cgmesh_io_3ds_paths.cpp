#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "../src/cgmesh/vmeshes.h"
#include "../src/cgmesh/vmeshes_io.h"

// ===========================================================================
//  Import 3DS — chemin d'entrée long, et fermeture du descripteur
// ===========================================================================
//
// Deux régressions, sur la même fonction `Load3DSFile`, toutes deux atteignables
// depuis l'API publique `VMeshesIO::load` avec un chemin fourni par l'appelant :
//
//  1. `strcpy(pModel->strPathToModel, szFile)` dans un `char[255]`, sans borne.
//     Le champ n'était PAS le dernier de la structure : il précédait un INT32
//     puis un `std::vector`, donc le dépassement écrasait les pointeurs internes
//     du vecteur — corruption de tas depuis un fichier .3ds parfaitement valide.
//
//  2. Les retours d'erreur postérieurs au `fopen` ne fermaient pas le fichier.
//     `CleanUp_3DS()` est le seul endroit qui le fait, et il n'était appelé que
//     sur le chemin nominal.
//
// ⚠ LA FENÊTRE UTILE EST ÉTROITE, et deux tentatives ont échoué avant celle-ci.
// Il faut un chemin que `fopen` accepte ENCORE et qui dépasse DÉJÀ 254
// caractères :
//
//   - au-delà de 259, `fopen` de la CRT refuse (MAX_PATH = 260) et le test ne
//     mesure plus que le refus de la CRT ;
//   - un préfixe « ./ » répété ne sert à rien : la CRT ajoute le répertoire
//     courant AVANT de normaliser, donc un relatif de 258 caractères est déjà
//     hors limite (mesuré — c'est ce qui a fait échouer la première version).
//
// Reste l'arborescence réelle avec chemin ABSOLU, calibrée pour atterrir entre
// 255 et 259. Si le répertoire de travail est trop long pour que cette fenêtre
// soit atteignable, le test s'exclut plutôt que de prétendre avoir mesuré.
//
// ===========================================================================

namespace {

// Chemin absolu de `total` caracteres exactement, sous `root`, se terminant par
// « .3ds » pour que le dispatcher d'extension le route vers l'import 3DS.
//
// C'est le NOM DE FICHIER qu'on allonge, pas l'arborescence : creer un
// repertoire est plafonne a MAX_PATH - 12 = 248 caracteres sous Windows (mesure
// -- « Nom de fichier ou extension trop long »), tandis qu'un composant de nom
// accepte 255 caracteres et que seul le total doit rester sous MAX_PATH.
// Rend un chemin vide si `total` est inatteignable.
std::filesystem::path pathOfLength (const std::filesystem::path& root, size_t total)
{
    const size_t parentLen = root.string ().size ();
    if (parentLen + 1 + 5 > total) return {};   // pas la place d'un « x.3ds »
    const size_t nameLen = total - parentLen - 1;
    if (nameLen > 255) return {};               // composant de nom trop long
    return root / (std::string (nameLen - 4, 'x') + ".3ds");
}

} // namespace

// Un chemin absolu de 259 caractères : le fichier s'ouvre encore (on est sous
// MAX_PATH), et l'écriture du chemin — 260 octets avec le NUL — dans le tampon
// de 255 déborde de 5 octets.
//
// Le HAUT de la fenêtre est choisi délibérément. À 255 caractères le
// dépassement ne serait que d'un octet, qui peut tomber dans le bourrage
// d'alignement et ne rien casser : le test passerait alors même sur le code
// fautif, et ne vaudrait rien. À 259 il couvre l'INT32 `numOfUnknownChunks`
// (offset 256 après le bourrage d'alignement) et mord sur le `std::vector`
// qui suit.
TEST(TEST_cgmesh_io_3ds_paths, long_input_path_does_not_overflow_the_model_struct)
{
    const std::string fixture = "test/data/sink.3ds";
    ASSERT_TRUE(std::filesystem::exists(fixture)) << "fixture absente";

    const std::filesystem::path root = std::filesystem::current_path() / "tu3ds";

    // 259 : le maximum que `fopen` accepte encore (MAX_PATH = 260, NUL compris).
    const std::filesystem::path target = pathOfLength(root, 259);
    if (target.empty())
        GTEST_SKIP() << "repertoire de travail trop long : 259 caracteres sont "
                        "inatteignables sans depasser MAX_PATH";

    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    ASSERT_FALSE(ec) << "creation du repertoire : " << ec.message();
    std::filesystem::copy_file(fixture, target,
                               std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << "copie de la fixture : " << ec.message();

    const std::string longPath = target.string();
    ASSERT_GT(longPath.size(), 254u) << "le chemin doit depasser le tampon d'origine";

    VMeshes vm;
    ASSERT_TRUE(VMeshesIO::load(vm, longPath.c_str()))
        << "chemin de " << longPath.size() << " caracteres";

    // Le modele est reellement charge : le debordement ecrasait precisement la
    // structure qui porte ce contenu, donc un chargement correct est l'oracle.
    EXPECT_GT(vm.GetNMeshes(), 0u);

    // Et le meme fichier par son chemin court donne le meme compte : la
    // longueur du chemin ne doit RIEN changer au resultat.
    VMeshes reference;
    ASSERT_TRUE(VMeshesIO::load(reference, fixture.c_str()));
    EXPECT_EQ(vm.GetNMeshes(), reference.GetNMeshes());

    std::filesystem::remove_all(root, ec);
}

// Le chemin d'erreur « ce n'est pas un 3DS » laissait le descripteur ouvert.
// On le prend plus de fois qu'il n'y a de flux disponibles (512 par défaut dans
// la CRT Windows, 1024 pour `ulimit -n` sous Linux), puis on vérifie qu'un
// fichier valide se charge encore. Avec la fuite, `fopen` finit par rendre
// nullptr et ce dernier chargement échoue.
//
// Si la limite de flux du processus est relevée au-delà de la boucle, le test
// passe sans rien exercer — c'est un filet de non-régression, pas une preuve.
TEST(TEST_cgmesh_io_3ds_paths, failed_loads_do_not_leak_file_descriptors)
{
    const std::string bad = "tu_3ds_not_really_a_3ds.3ds";
    {
        std::ofstream ofs(bad, std::ios::binary);
        ASSERT_TRUE(ofs.good());
        ofs << "ceci n'est pas un fichier 3DS";   // premier chunk != M3DMAGIC
    }

    for (int i = 0; i < 1200; ++i)
    {
        VMeshes vm;
        EXPECT_FALSE(VMeshesIO::load(vm, bad.c_str()))
            << "iteration " << i << " : un fichier invalide ne doit pas se charger";
    }

    VMeshes vm;
    EXPECT_TRUE(VMeshesIO::load(vm, "test/data/sink.3ds"))
        << "1200 echecs ont epuise les flux du processus : les descripteurs fuient";

    std::remove(bad.c_str());
}
