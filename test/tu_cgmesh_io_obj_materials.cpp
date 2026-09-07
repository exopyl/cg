// ===========================================================================
//  L'export OBJ d'une page GABARIT porte ses materiaux
// ===========================================================================
//
// La page svg.html telecharge l'archive que MeshIO::export_obj_zip_bytes rend a
// partir du maillage produit par la chaine file.ref -> svg.extrude.colored ->
// mesh.color. C'est CETTE chaine qui est montee ici, et non un maillage fabrique
// a la main : le sujet du test est le trajet complet des couleurs du document
// jusqu'aux octets telecharges, et un maillage de laboratoire n'en eprouverait
// que la derniere marche.
//
// Ce qui se verifie sans navigateur :
//   1. l'archive compte DEUX entrees, le .obj et son .mtl ;
//   2. le .mtl declare une couleur par materiau, et le .obj les emploie ;
//   3. chaque `usemtl` du .obj a son `newmtl` dans le .mtl -- l'alignement des
//      noms, deja casse une fois (objMaterialName, mesh_io_obj.cpp) ;
//   4. relire l'archive rend le MEME nombre de materiaux et la MEME affectation
//      par face ;
//   5. un maillage MONO-MATERIAU s'exporte toujours, sans rien perdre.
//
// Ce qui ne s'y verifie pas : le bouton, le nom du fichier telecharge et ce que
// le navigateur en fait. C'est la sonde maker/probes/graph_obj_zip.js.
//
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "../src/cggraph/core/evaluator.h"
#include "../src/cggraph/nodes/catalog_factory.h"
#include "../src/cggraph/nodes/io/file_ref.h"
#include "../src/cggraph/nodes/value_types.h"
#include "../src/cgmesh/material.h"
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_io.h"
#include "../src/cgmesh/zip_manager.h"

using namespace cggraph;
using namespace cggraph_nodes;

namespace {

const char* kTiger = "./test/data/svg/Ghostscript_Tiger.svg";
const char* kRose  = "./test/data/svg/rose.svg";

// La chaine de la page GABARIT, montee a la main -- meme montage que
// tu_cggraph_svg_k5_colored.cpp, dont ce fichier prolonge les criteres jusqu'au
// fichier ecrit.
struct Chain
{
    Graph  graph;
    NodeId file = 0;
    NodeId svg = 0;
    NodeId color = 0;

    explicit Chain(const char* path)
    {
        file  = graph.AddNode(MakeNode("file.ref"));
        svg   = graph.AddNode(MakeNode("svg.extrude.colored"));
        color = graph.AddNode(MakeNode("mesh.color"));
        static_cast<FileRefNode*>(graph.FindNode(file))->SetPath(path);
        graph.Connect(file, 0, svg, 0);
        graph.Connect(svg, 0, color, 0);
    }

    ParamSet& SvgParams() { return graph.FindNode(svg)->GetParams(); }

    std::shared_ptr<const Mesh> Evaluate()
    {
        Evaluator evaluator(graph);
        EvalContext ctx;
        ValueList outputs;
        if (!evaluator.Evaluate(color, outputs, ctx).IsOk() || outputs.empty())
            return nullptr;
        return outputs[0].Share<Mesh>(Types().mesh);
    }
};

// Lecteur d'archive STORED, reduit a ce dont ces tests ont besoin : parcours du
// repertoire central, verification du CRC declare, extraction. ZipManager
// n'ecrit que des entrees non compressees, donc aucun decodeur n'est necessaire.
// Rend une table vide si l'archive est illisible.
std::map<std::string, std::string> readStoredZip(const std::string& z)
{
    std::map<std::string, std::string> out;
    if (z.size() < 22) return out;

    auto rd16 = [&](size_t o) { return (unsigned int)((unsigned char)z[o] | ((unsigned char)z[o+1] << 8)); };
    auto rd32 = [&](size_t o) { return (unsigned int)((unsigned char)z[o] | ((unsigned char)z[o+1] << 8)
                                                    | ((unsigned char)z[o+2] << 16) | ((unsigned char)z[o+3] << 24)); };
    size_t eocd = std::string::npos;
    for (size_t i = z.size() - 22; i != (size_t)-1; --i)
        if (rd32(i) == 0x06054b50u) { eocd = i; break; }
    if (eocd == std::string::npos) return out;

    const unsigned int nEntries = rd16(eocd + 10);
    size_t p = rd32(eocd + 16);
    for (unsigned int k = 0; k < nEntries; ++k)
    {
        if (p + 46 > z.size() || rd32(p) != 0x02014b50u) { out.clear(); return out; }
        const unsigned int crc   = rd32(p + 16);
        const unsigned int usize = rd32(p + 24);
        const unsigned int nlen  = rd16(p + 28);
        const size_t       lho   = rd32(p + 42);
        const std::string  name  = z.substr(p + 46, nlen);

        const size_t dataStart = lho + 30 + rd16(lho + 26) + rd16(lho + 28);
        if (dataStart + usize > z.size()) { out.clear(); return out; }
        const std::string data = z.substr(dataStart, usize);
        if (ZipManager::Crc32(data.data(), data.size()) != crc) { out.clear(); return out; }
        out[name] = data;
        p += 46 + nlen + rd16(p + 30) + rd16(p + 32);
    }
    return out;
}

// Arguments d'une directive en debut de ligne : `usemtl X` -> "X".
std::vector<std::string> directiveArgs(const std::string& text, const std::string& keyword)
{
    std::vector<std::string> names;
    size_t pos = 0;
    while ((pos = text.find(keyword, pos)) != std::string::npos)
    {
        if (pos != 0 && text[pos - 1] != '\n') { pos += keyword.size(); continue; }
        const size_t b = pos + keyword.size();
        const size_t e = text.find_first_of("\r\n", b);
        names.push_back(text.substr(b, e - b));
        pos = b;
    }
    return names;
}

std::string materialNameOfFace(const Mesh& m, unsigned int f)
{
    const int id = m.GetFaceMaterialId(f);
    const Material* mat = (id < 0) ? nullptr : m.GetMaterial((unsigned int)id);
    return mat ? mat->GetName() : std::string("<aucun>");
}

void writeFile(const char* path, const std::string& bytes)
{
    std::ofstream f(path, std::ios::binary);
    f.write(bytes.data(), (std::streamsize)bytes.size());
}

} // namespace

// ---------------------------------------------------------------------------
//  Le Tigre, couleurs du fichier COCHEES
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_io_obj_materials, the_tiger_archive_declares_and_uses_every_material)
{
    Chain chain(kTiger);
    chain.SvgParams().SetBool("useSvgColors", true);
    std::shared_ptr<const Mesh> mesh = chain.Evaluate();
    ASSERT_NE(mesh, nullptr);
    ASSERT_GE(mesh->GetNMaterials(), 30u) << "le document doit porter sa palette";

    const std::string bytes = MeshIO::export_obj_zip_bytes(*mesh, "tigre");
    ASSERT_FALSE(bytes.empty());

    const std::map<std::string, std::string> entries = readStoredZip(bytes);
    ASSERT_EQ(entries.size(), 2u) << "attendu : le .obj ET son .mtl";
    ASSERT_EQ(entries.count("tigre.obj"), 1u);
    ASSERT_EQ(entries.count("tigre.mtl"), 1u);

    const std::string& obj = entries.at("tigre.obj");
    const std::string& mtl = entries.at("tigre.mtl");

    // Le .obj designe le .mtl par le nom que porte l'entree de l'archive :
    // apres extraction cote a cote, la ligne resout telle quelle.
    EXPECT_NE(obj.find("mtllib tigre.mtl"), std::string::npos);

    const std::vector<std::string> declared = directiveArgs(mtl, "newmtl ");
    const std::vector<std::string> used     = directiveArgs(obj, "usemtl ");
    const std::set<std::string> usedDistinct(used.begin(), used.end());
    const std::set<std::string> declaredSet(declared.begin(), declared.end());

    EXPECT_EQ(declared.size(), mesh->GetNMaterials())
        << "un newmtl par materiau de la table";
    EXPECT_GE(declared.size(), 30u);
    EXPECT_GE(usedDistinct.size(), 30u);

    // ALIGNEMENT DES NOMS. Un usemtl sans newmtl correspondant ne provoque
    // aucune erreur chez un lecteur : le modele sort simplement sans couleur.
    for (const std::string& u : usedDistinct)
        EXPECT_EQ(declaredSet.count(u), 1u)
            << "usemtl \"" << u << "\" n'a pas de newmtl correspondant";

    // Des COULEURS, et non trente fois la meme.
    const std::vector<std::string> kdLines = directiveArgs(mtl, "Kd ");
    const std::set<std::string> kd(kdLines.begin(), kdLines.end());
    EXPECT_GE(kd.size(), 30u) << "la palette doit rester distincte dans le .mtl";

    std::cout << "[G3] archive du tigre : " << bytes.size() << " octets, "
              << declared.size() << " newmtl, " << usedDistinct.size()
              << " usemtl distincts, " << kd.size() << " Kd distincts" << std::endl;
    RecordProperty("newmtl", (int)declared.size());
    RecordProperty("usemtl", (int)usedDistinct.size());
    RecordProperty("zipBytes", (int)bytes.size());
}

// L'aller-retour : ce que l'archive rend a qui la relit. C'est l'oracle qui
// juge le contenu des fichiers plutot que leur apparence.
TEST(TEST_cgmesh_io_obj_materials, the_tiger_archive_survives_a_round_trip)
{
    Chain chain(kTiger);
    chain.SvgParams().SetBool("useSvgColors", true);
    std::shared_ptr<const Mesh> mesh = chain.Evaluate();
    ASSERT_NE(mesh, nullptr);

    const std::string bytes = MeshIO::export_obj_zip_bytes(*mesh, "tigre_rt");
    const std::map<std::string, std::string> entries = readStoredZip(bytes);
    ASSERT_EQ(entries.size(), 2u);

    // Extraction COTE A COTE, comme le ferait un utilisateur : c'est ce qui rend
    // la ligne mtllib resoluble.
    writeFile("./tigre_rt.obj", entries.at("tigre_rt.obj"));
    writeFile("./tigre_rt.mtl", entries.at("tigre_rt.mtl"));

    Mesh back;
    ASSERT_EQ(MeshIO::import_obj(back, "./tigre_rt.obj"), 0);
    EXPECT_EQ(back.GetNMaterials(), mesh->GetNMaterials());
    ASSERT_EQ(back.GetNFaces(), mesh->GetNFaces());

    unsigned int mismatched = 0;
    for (unsigned int f = 0; f < mesh->GetNFaces(); ++f)
        if (materialNameOfFace(*mesh, f) != materialNameOfFace(back, f))
            ++mismatched;
    EXPECT_EQ(mismatched, 0u) << "affectation par face perdue sur " << mismatched
                              << " faces / " << mesh->GetNFaces();

    std::remove("./tigre_rt.obj");
    std::remove("./tigre_rt.mtl");
}

// ---------------------------------------------------------------------------
//  Non-regression : le maillage MONO-MATERIAU
// ---------------------------------------------------------------------------
// Le chemin qui marchait avant ce chantier -- une piece d'un seul tenant, peinte
// par mesh.color -- doit rendre exactement la meme chose.
TEST(TEST_cgmesh_io_obj_materials, a_single_material_mesh_still_exports_its_colour)
{
    Chain chain(kRose);
    chain.SvgParams().SetBool("useSvgColors", false);
    std::shared_ptr<const Mesh> mesh = chain.Evaluate();
    ASSERT_NE(mesh, nullptr);
    ASSERT_EQ(mesh->GetNMaterials(), 1u) << "bascule decochee : une seule couleur";

    const std::string bytes = MeshIO::export_obj_zip_bytes(*mesh, "piece");
    const std::map<std::string, std::string> entries = readStoredZip(bytes);
    ASSERT_EQ(entries.size(), 2u);

    EXPECT_EQ(directiveArgs(entries.at("piece.mtl"), "newmtl ").size(), 1u);
    const std::vector<std::string> used = directiveArgs(entries.at("piece.obj"), "usemtl ");
    ASSERT_EQ(used.size(), 1u);
    EXPECT_EQ(used[0], directiveArgs(entries.at("piece.mtl"), "newmtl ")[0]);

    // Les deux entrees reprennent LEUR nom d'archive : c'est de lui que la ligne
    // mtllib est derivee, donc le renommer romprait la resolution.
    writeFile("./piece.obj", entries.at("piece.obj"));
    writeFile("./piece.mtl", entries.at("piece.mtl"));
    Mesh back;
    ASSERT_EQ(MeshIO::import_obj(back, "./piece.obj"), 0);
    EXPECT_EQ(back.GetNMaterials(), 1u);
    EXPECT_EQ(back.GetNFaces(), mesh->GetNFaces());
    std::remove("./piece.obj");
    std::remove("./piece.mtl");
}
