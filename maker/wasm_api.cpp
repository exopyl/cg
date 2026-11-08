// ===========================================================================
//  maker — pont Embind moteur natif <-> JavaScript (Emscripten)
// ===========================================================================
//
// Etape 2 (cf. maker/ANALYSE.md sec.4) : facade minimale exposant la logique
// geometrique du moteur natif au navigateur.
//
//   listShapes(category)       -> JSON : catalogue d'une categorie ("" = tout)
//   createShape(name)          -> id entier (registre cote C++), -1 si inconnu
//   createSvgExtrusion(svg)    -> id, a partir du texte SVG (via MEMFS)
//   createTextExtrusion(fontBytes, filename, text)
//                              -> id, texte 3D : police en Uint8Array, texte en string
//   getParams(id)              -> JSON : parametres typees (id/name/type/value/min/max/choices)
//   setParam(id, paramId, value)
//                              -> bool : ecrit la valeur (par Parameter::Id, jamais de pointeur cru)
//   setParamString(id, paramId, s)
//                              -> bool : idem pour un parametre TEXTUEL
//   regenerate(id)             -> string OBJ : Regenerate() puis export du maillage
//   destroyShape(id)           -> libere l'objet
//
// Garde d'impression d'une Gothic Window (gothic_implementation.md, M4-d2) :
//   setPrintConstraints(json)  -> JSON : lecture d'un document print-constraints/v1
//                                 (l'imprimante), etat garde cote WASM
//   printConstraints()         -> JSON : document de l'imprimante en vigueur
//   setPrintedWidthMm(id, mm)  -> bool : largeur imprimee de la piece `id`
//   gothicPrintReport(id)      -> { refused?, warning?, print?, printFailure? }
//
// UN SEUL ECRIVAIN OBJ, celui du moteur natif, sur les deux usages :
//   regenerate(id) -> MeshIO::export_obj_bytes, pour l'AMORCAGE du viseur ;
//   exportObj(id)  -> MeshIO::export_obj_zip_bytes, l'archive OBJ+MTL, donc les
//                     couleurs. C'est l'export utilisateur.
//
// ⚠ La chaine d'amorcage porte `mtllib` des que le maillage a des materiaux, et
// le .mtl ne l'accompagne pas : o3dv recense le fichier absent de sa FileList et
// affiche le modele sans ses couleurs, SANS erreur.
//
// Le GLB a son propre couple de points d'entree, exportGlb(id) et
// graphExportGlb(port) : un glTF binaire embarque ses materiaux, donc UN seul
// fichier et aucune archive.
//
// mesh_io.cpp entier reste hors du build WASM (dependances 3DS/PLY) ; seul
// mesh_io_obj.cpp, extrait pour cela, y est compile.
//
// Les Parameter du moteur natif referencent des pointeurs crus vers les membres de
// l'objet ; ils ne traversent jamais la frontiere JS : on serialise en JSON en
// sortie et on applique par nom en entree (ANALYSE sec.2/4).
//
// ===========================================================================

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "parameterized_shapes.h"   // IParameterized + formes a ressource
#include <cgarchi/parameterized_classical.h>   // ParameterizedClassicalColonnade (shapeWarnings)
#include <cgarchi/parameterized_gothic.h>   // ParameterizedGothicWindow (createGothicFromJson, exportGothicJson)
#include "print_validation_json.h"  // print-constraints/v1
#include <cgarchi/gothic_print_json.h>      // GothicPrintVerdictToJson : print-report/v1 de la garde d'impression
#include <cgarchi/parameterized_gothic_microarchitecture.h>   // ParameterizedGothicMicroarchitecture (createMicroarchFromJson)
#include <cgarchi/gothic_microarchitecture_json.h>            // ReportToJson
#include "image_pixel_blocks.h"      // image_to_pixel_blocks_per_component
#include "material.h"                // Material::GetName (nommage des groupes OBJ)
#include "mesh.h"                    // GetNVertices / GetVertex / GetTriangles
#include "mesh_io.h"                 // MeshIO::export_obj (serialisation OBJ+MTL)
#include "mesh_payload.h"
#include "graph_host.h"              // maker_graph::host () -- resultat du graphe (graphExportObj)
#include "../src/cggraph/nodes/shapes/shape_catalog.h"   // fabriques des formes sans ressource (cgmesh + gothique + classique)
#include "../src/cggraph/nodes/value_types.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
#include <emscripten/val.h>
#endif

// ---------------------------------------------------------------------------
// Registre d'objets : le JS ne manipule que des id entiers.
// ---------------------------------------------------------------------------
namespace {

std::map<int, std::unique_ptr<IParameterized>> g_objects;
int g_nextId = 1;

// Fichiers MEMFS ecrits par les fabriques a partir d'octets (SVG, image) :
// l'objet natif en garde le CHEMIN et le relit a chaque Regenerate(), donc on ne
// peut pas le supprimer apres chargement -- il doit vivre aussi longtemps que
// l'objet. Sans ce registre, chaque import laissait sa copie dans le heap WASM
// pour la duree de la page (verifie : /tmp accumulait maker_img_0/1/2...).
std::map<int, std::string> g_tempFiles;

// Imprimante : seuils et enveloppe, poses par setPrintConstraints. Jamais
// d'echelle : elle vient de la largeur imprimee de chaque piece.
PrintConstraints g_printer;

// Largeur imprimee (mm) par objet : elle appartient a la piece, pas a
// l'imprimante. Absente : echelle non fixee. Liberee par destroyShape().
std::map<int, double> g_printedWidthMm;

void removeTempFile(int id)
{
    auto it = g_tempFiles.find(id);
    if (it == g_tempFiles.end()) return;
    std::remove(it->second.c_str());
    g_tempFiles.erase(it);
}

// Une forme de la page : son entree au catalogue reuni de la couche nodale
// (cggraph_nodes::CatalogShapes : formes de cgmesh, puis formes gothiques, puis
// formes classiques), qui porte le nom, la CATEGORIE et la fabrique, et son
// groupe de liste, seule donnee propre a maker. La categorie partitionne le
// catalogue entre les pages de maker/web/ : chacune n'expose que ses propres
// formes (listShapes(category)).
struct CatalogEntry {
    const ParametricShapeEntry* shape;   // jamais nul
    std::string group;                   // en-tete <optgroup> dans la liste ("" = aucun)
};

// Categorie dont les formes recoivent l'option Extrude (epaississement) : les
// formes gothiques ont deja leur propre extrusion (zHeight, profils), et une
// colonne classique est un solide ferme.
const char* const kExtrudableCategory = "parametric";

std::unique_ptr<IParameterized> makeShape(const ParametricShapeEntry& shape)
{
    std::unique_ptr<IParameterized> made = shape.make();
    if (std::strcmp(shape.category, kExtrudableCategory) != 0) return made;
    return std::unique_ptr<IParameterized>(new ParameterizedExtruded(std::move(made)));
}

// Catalogue de la page, dans l'ordre du catalogue reuni. Les groupes designent
// la forme par son identifiant au catalogue, qui ne change pas quand le nom
// change ; une forme sans groupe a le groupe "". Un identifiant inconnu du
// catalogue est signale sur std::cerr (la console sous Emscripten).
const std::vector<CatalogEntry>& catalog()
{
    // Groupes : de simples en-tetes de liste, ils ne changent rien au comportement.
    // Chaque forme garde son entree et son nom -- on cherche « Klein Bottle » ou
    // « Trefoil Knot » par son nom, et les regrouper derriere une famille les rendait
    // introuvables.
    static const char* const SOL = "Solides";
    static const char* const SUR = "Surfaces";
    static const char* const FRA = "Fractales";
    struct GroupRow { const char* id; const char* group; };
    static const std::vector<GroupRow> groups = {
        {"shape.cube",                  SOL},
        {"shape.sphere",                SOL},
        {"shape.cylinder",              SOL},
        {"shape.cone",                  SOL},
        {"shape.capsule",               SOL},
        {"shape.torus",                 SOL},
        {"shape.seashell",              SUR},
        {"shape.seashell.von_seggern",  SUR},
        {"shape.klein_bottle",          SUR},
        {"shape.breather",              SUR},
        {"shape.hyperbolic_paraboloid", SUR},
        {"shape.monkey_saddle",         SUR},
        {"shape.blobs",                 SUR},
        {"shape.drop",                  SUR},
        {"shape.knot.torus",            SUR},
        {"shape.knot.cinquefoil",       SUR},
        {"shape.knot.trefoil",          SUR},
        {"shape.borromean_rings",       SUR},
        {"shape.helicoid",              SUR},
        {"shape.corkscrew",             SUR},
        {"shape.mobius_strip",          SUR},
        {"shape.radial_wave",           SUR},
        {"shape.guimard",               SUR},
        {"shape.menger_sponge",         FRA},
        {"shape.lsystem",               FRA},
    };
    static const std::vector<CatalogEntry> c = [] {
        for (const GroupRow& g : groups) {
            if (!cggraph_nodes::FindCatalogShapeById(g.id))
                std::cerr << "maker: group entry \"" << g.id
                          << "\" is unknown to the shape catalogue\n";
        }
        const std::vector<ParametricShapeEntry>& shapes = cggraph_nodes::CatalogShapes();
        std::vector<CatalogEntry> out;
        out.reserve(shapes.size());
        for (const ParametricShapeEntry& shape : shapes) {
            std::string group;
            for (const GroupRow& g : groups) {
                if (std::strcmp(shape.id, g.id) == 0) {
                    group = g.group;
                    break;
                }
            }
            out.push_back(CatalogEntry{&shape, group});
        }
        return out;
    }();
    return c;
}

IParameterized* find(int id)
{
    auto it = g_objects.find(id);
    return it == g_objects.end() ? nullptr : it->second.get();
}

int registerObject(std::unique_ptr<IParameterized> obj)
{
    int id = g_nextId++;
    g_objects[id] = std::move(obj);
    return id;
}

// --- serialisation JSON minimale (echappement des chaines) -----------------
// Tout octet < 0x20 est echappe (RFC 8259) : recopie tel quel, un caractere de
// controle rendrait le document illisible par JSON.parse. Les octets >= 0x80
// passent tels quels (UTF-8).
std::string jsonEscape(const std::string& s)
{
    std::string o;
    o.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    static const char hex[] = "0123456789abcdef";
                    o += "\\u00";
                    o += hex[(static_cast<unsigned char>(c) >> 4) & 0xF];
                    o += hex[static_cast<unsigned char>(c) & 0xF];
                } else {
                    o += c;
                }
                break;
        }
    }
    return o;
}

std::string num(double v)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.6g", v);
    return buf;
}

Mesh* meshOf(IParameterized* obj)
{
    ParameterizedMesh* pm = dynamic_cast<ParameterizedMesh*>(obj);
    return pm ? pm->GetMesh() : nullptr;
}

} // namespace

// ===========================================================================
//  API exposee
// ===========================================================================

// Catalogue des formes d'une CATEGORIE : listShapes("parametric") renvoie
// ["Cube","Sphere",...]. Une categorie vide renvoie le catalogue entier ; un nom
// de categorie inconnu renvoie [] (et non le tout, qui masquerait la faute de
// frappe en peuplant la page avec tout le catalogue).
std::string listShapes(const std::string& category)
{
    std::string j = "[";
    bool first = true;
    for (const auto& e : catalog()) {
        if (!category.empty() && std::strcmp(e.shape->category, category.c_str()) != 0) continue;
        if (!first) j += ',';
        first = false;
        // Objet et non simple chaine : le JS a besoin du groupe pour ses
        // <optgroup>. Le nom reste la cle -- createShape(name).
        j += "{\"name\":\""; j += jsonEscape(e.shape->name);
        j += "\",\"group\":\""; j += jsonEscape(e.group); j += "\"}";
    }
    j += ']';
    return j;
}

// Avertissements de la derniere generation de la forme `id` ("" s'il n'y en a
// pas, ou si la forme n'en rend pas) : taille du maillage d'une colonnade.
std::string shapeWarnings(int id)
{
    const auto* colonnade = dynamic_cast<const cgarchi::ParameterizedClassicalColonnade*>(find(id));
    return colonnade ? colonnade->Warnings() : std::string();
}

// Cree une forme du catalogue. Renvoie son id, ou -1 si le nom est inconnu. Les
// noms etant uniques toutes categories confondues, la categorie n'intervient pas.
int createShape(const std::string& name)
{
    for (const auto& e : catalog()) {
        if (e.shape->name == name)
            return registerObject(makeShape(*e.shape));
    }
    return -1;
}

// Cree une extrusion SVG a partir du contenu texte du fichier (fourni par un
// <input type=file> cote JS). Ecrit en MEMFS puis instancie l'importeur natif.
int createSvgExtrusion(const std::string& svgText)
{
    static int counter = 0;
    std::string path = "/tmp/maker_svg_" + std::to_string(counter++) + ".svg";
    {
        std::ofstream f(path, std::ios::binary);
        if (!f) return -1;
        f.write(svgText.data(), (std::streamsize)svgText.size());
    }
    auto obj = std::unique_ptr<IParameterized>(new ParameterizedSvgExtrusion(path));
    const int id = registerObject(std::move(obj));
    g_tempFiles[id] = path;   // libere par destroyShape()
    return id;
}

#ifdef __EMSCRIPTEN__
// Cree un relief colore a partir des OCTETS d'une image (PNG/JPG/BMP/TGA/PNM).
// Contrairement au SVG, une image est binaire : les octets arrivent en Uint8Array
// (convertJSArrayToNumberVector, pas de std::string qui passerait par une
// conversion UTF-8 destructrice). `filename` n'est utilise que pour son
// EXTENSION : Img::load dispatche le decodeur dessus.
int createImageRelief(emscripten::val bytes, const std::string& filename)
{
    std::vector<unsigned char> data =
        emscripten::convertJSArrayToNumberVector<unsigned char>(bytes);
    if (data.empty()) return -1;

    // Conserve l'extension d'origine (sinon Img::load ne sait pas decoder).
    std::string ext;
    const size_t dot = filename.find_last_of('.');
    if (dot != std::string::npos) ext = filename.substr(dot);
    if (ext.empty()) ext = ".png";

    static int counter = 0;
    const std::string path = "/tmp/maker_img_" + std::to_string(counter++) + ext;
    {
        std::ofstream f(path, std::ios::binary);
        if (!f) return -1;
        f.write((const char*)data.data(), (std::streamsize)data.size());
    }

    auto obj = std::unique_ptr<IParameterized>(new ParameterizedImageRelief(path));
    // Une image illisible ou non vectorisable ne produit aucun maillage : mieux
    // vaut le signaler que d'enregistrer un objet vide. Le fichier n'etant alors
    // rattache a aucun id, c'est ICI qu'il faut le supprimer.
    if (!meshOf(obj.get()))
    {
        std::remove(path.c_str());
        return -1;
    }
    const int id = registerObject(std::move(obj));
    g_tempFiles[id] = path;   // libere par destroyShape()
    return id;
}
#endif

#ifdef __EMSCRIPTEN__
// Cree un texte 3D a partir des OCTETS d'une police et d'une chaine.
//
// Deux protocoles DIFFERENTS dans le meme appel, et c'est voulu :
//
//   - la POLICE est binaire -> Uint8Array (convertJSArrayToNumberVector). Une
//     std::string la ferait passer par la conversion UTF-8 d'embind, qui
//     DETRUIRAIT les octets -- meme piege que pour les octets d'image.
//   - le TEXTE est du texte -> std::string, et c'est justement cette conversion
//     UTF-8 que l'on veut : la mise en page decode l'UTF-8 (text_layout.cpp).
//
// Contrairement au SVG et a l'image, le fichier MEMFS peut etre supprime
// AUSSITOT : ParameterizedText3D garde la police EN MEMOIRE et ne relit jamais
// son fichier. C'est aussi ce qui rend chaque mouvement de curseur bon marche.
int createTextExtrusion(emscripten::val fontBytes, const std::string& filename,
                        const std::string& text)
{
    std::vector<unsigned char> data =
        emscripten::convertJSArrayToNumberVector<unsigned char>(fontBytes);
    if (data.empty() || text.empty()) return -1;

    // L'extension n'a aucune importance pour le chargement (le format est
    // reconnu a la signature, cf. font.cpp) ; on la conserve pour la lisibilite
    // des messages d'erreur.
    std::string ext;
    const size_t dot = filename.find_last_of('.');
    if (dot != std::string::npos) ext = filename.substr(dot);
    if (ext.empty()) ext = ".ttf";

    static int counter = 0;
    const std::string path = "/tmp/maker_font_" + std::to_string(counter++) + ext;
    {
        std::ofstream f(path, std::ios::binary);
        if (!f) return -1;
        f.write((const char*)data.data(), (std::streamsize)data.size());
    }

    auto obj = std::unique_ptr<IParameterized>(new ParameterizedText3D(path, text));
    std::remove(path.c_str());   // la police est deja en memoire

    if (!meshOf(obj.get())) return -1;
    return registerObject(std::move(obj));
}
#endif

#ifdef __EMSCRIPTEN__
// Cree des blocs pixelises a partir des OCTETS d'une image. Meme protocole que
// createImageRelief : Uint8Array (une image est binaire), extension conservee
// pour que Img::load sache dispatcher son decodeur, fichier MEMFS garde vivant
// aussi longtemps que l'objet.
int createImagePixelBlocks(emscripten::val bytes, const std::string& filename)
{
    std::vector<unsigned char> data =
        emscripten::convertJSArrayToNumberVector<unsigned char>(bytes);
    if (data.empty()) return -1;

    std::string ext;
    const size_t dot = filename.find_last_of('.');
    if (dot != std::string::npos) ext = filename.substr(dot);
    if (ext.empty()) ext = ".png";

    static int counter = 0;
    const std::string path = "/tmp/maker_pix_" + std::to_string(counter++) + ext;
    {
        std::ofstream f(path, std::ios::binary);
        if (!f) return -1;
        f.write((const char*)data.data(), (std::streamsize)data.size());
    }

    auto obj = std::unique_ptr<IParameterized>(new ParameterizedImagePixelBlocks(path));
    if (!meshOf(obj.get()))
    {
        std::remove(path.c_str());
        return -1;
    }
    const int id = registerObject(std::move(obj));
    g_tempFiles[id] = path;   // libere par destroyShape()
    return id;
}
#endif

#ifdef __EMSCRIPTEN__
// Octets du dernier fichier exporte -- archive ZIP OBJ+MTL, ou GLB. Un tampon
// statique, comme pour meshData : la vue typee rendue au JS pointe DANS le tas
// WASM et ne reste valide que jusqu'au prochain appel -- le JS doit copier
// aussitot. UN SEUL tampon pour tous les formats, deux exports n'etant jamais en
// vol en meme temps.
//
// Pourquoi une vue et non une std::string : embind convertit une std::string en
// chaine JS avec decodage UTF-8, ce qui DETRUIRAIT des octets binaires. Meme
// piege que pour les octets d'image a l'import.
static std::string g_exportBytes;

// Export par BLOC CONNEXE : renvoie { obj, mtl, name, blocks }.
//
// Les blocs sont FUSIONNES en un seul Mesh via Mesh::Append, qui decale les
// indices de faces ET les identifiants de materiaux -- chaque bloc arrivant avec
// son unique materiau, le maillage resultant en porte un PAR BLOC. La
// serialisation est ensuite entierement deleguee au moteur natif
// (MeshIO::export_obj, emitObjectGroups=true) : `o block_NNNN_color_NN` et
// `usemtl` a chaque frontiere de bloc, plus le .mtl compagnon qui donne les
// couleurs. Rien n'est ecrit a la main ici.
//
// NB : nous sommes le premier appelant de Mesh::Append (son commentaire notait
// qu'elle n'en avait aucun). Elle TRANSFERE la propriete des materiaux au
// maillage cible et vide celle de la source, donc les Mesh sources ne doivent
// etre detruits qu'apres la fusion -- ce que fait la boucle ci-dessous.
emscripten::val exportPixelBlocks(int id, const std::string& base)
{
    using namespace emscripten;
    val out = val::object();
    out.set("name", std::string());
    out.set("blocks", 0);
    g_exportBytes.clear();

    IParameterized* obj = find(id);
    ParameterizedImagePixelBlocks* pb = dynamic_cast<ParameterizedImagePixelBlocks*>(obj);
    if (pb)
    {
        std::vector<Mesh*> meshes =
            image_to_pixel_blocks_per_component(pb->GetFilename(), pb->GetOptions());
        Mesh merged;
        int nBlocks = 0;
        for (Mesh* m : meshes)
        {
            if (m) { merged.Append(m); ++nBlocks; }
            delete m;                  // l'appelant possede chaque Mesh
        }
        if (merged.GetNFaces() > 0)
        {
            std::string safe;
            for (char c : base)
                if (isalnum((unsigned char)c) || c == '-' || c == '_') safe += c;
            if (safe.empty()) safe = "blocks";

            g_exportBytes = MeshIO::export_obj_zip_bytes(merged, safe,
                                                      /*emitObjectGroups=*/true);
            if (!g_exportBytes.empty())
            {
                out.set("name", safe);
                out.set("blocks", nBlocks);
            }
        }
    }
    out.set("bytes", val(typed_memory_view(g_exportBytes.size(),
                                           (const unsigned char*)g_exportBytes.data())));
    return out;
}

// Garde d'export d'une Gothic Window : un rapport de construction qui porte
// une erreur refuse l'export -- octets vides, motif dans `refused` ; un maillage
// non strict est exporte, ses defauts dans `warning`. Le controle d'impression
// (CheckGothicExportAndPrintAtWidth : imprimante g_printer, largeur imprimee de
// `id`) AVERTIT seulement : rapport print-report/v1 dans `print`. Une exception
// du controle ou de son ecriture JSON ne change pas le verdict : motif dans
// `printFailure`, et le verdict geometrique est recalcule seul s'il manque.
// Une exception de CheckGothicExport lui-meme laisse la geometrie NON JUGEE :
// refus, motif « geometry not judged: ... » dans `refused`, comme pour une
// erreur de construction. Sans effet sur les autres formes. Rend false sur
// refus.
bool gothicExportGuard(int id, IParameterized* obj, const Mesh& m, emscripten::val& out)
{
    const auto* gw = dynamic_cast<const cgarchi::ParameterizedGothicWindow*>(obj);
    if (!gw) return true;
    std::optional<cgarchi::GothicExportCheck> geometry;
    try
    {
        const auto width = g_printedWidthMm.find(id);
        const std::optional<double> printedWidthMm =
            (width == g_printedWidthMm.end()) ? std::nullopt : std::optional<double>(width->second);
        const cgarchi::GothicPrintVerdict verdict = cgarchi::CheckGothicExportAndPrintAtWidth(*gw, m, g_printer, printedWidthMm);
        geometry = verdict.check.geometry;
        out.set("print", cgarchi::GothicPrintVerdictToJson(verdict));
    }
    catch (const std::exception& e)
    {
        out.set("printFailure", emscripten::val(e.what()));
    }
    catch (...)
    {
        out.set("printFailure", emscripten::val("unknown exception"));
    }
    if (!geometry)
    {
        try
        {
            geometry = cgarchi::CheckGothicExport(*gw, m);
        }
        catch (const std::exception& e)
        {
            out.set("refused", emscripten::val(std::string("geometry not judged: ") + e.what()));
            return false;
        }
        catch (...)
        {
            out.set("refused", emscripten::val("geometry not judged: unknown exception"));
            return false;
        }
    }
    const cgarchi::GothicExportCheck check = *geometry;
    if (!check.allowed)
    {
        out.set("refused", check.message);
        return false;
    }
    if (!check.strict)
        out.set("warning", check.message);
    return true;
}

// Garde des exports generiques pour la microarchitecture gothique : son maillage
// n'est qu'un affichage (tubes qui se traversent aux jonctions, ni coque ni solide,
// sommets relatifs a une origine). Pose `refused` et rend false pour elle ; true
// pour toute autre forme.
bool microarchExportGuard(IParameterized* obj, emscripten::val& out)
{
    if (!dynamic_cast<cgarchi::ParameterizedGothicMicroarchitecture*>(obj)) return true;
    out.set("refused", emscripten::val(std::string(
        "display mesh only: tubes cross each other at the junctions, neither a shell nor a solid. "
        "Export the network (JSON), the curves (OBJ) or the tubes (OBJ, origin applied) from the microarchitecture panel")));
    return false;
}

// Verdict de la garde d'export de `id`, sans octets : Regenerate, puis
// gothicExportGuard. Sert au controle a la demande et a la garde du STL, dont
// l'encodeur est en JS. `refused` pour une microarchitecture
// (microarchExportGuard) ; objet vide pour une autre forme qui n'est pas une
// Gothic Window, ou un id inconnu.
emscripten::val gothicPrintReport(int id)
{
    emscripten::val out = emscripten::val::object();
    IParameterized* obj = find(id);
    if (obj && !microarchExportGuard(obj, out)) return out;
    if (!obj || !dynamic_cast<cgarchi::ParameterizedGothicWindow*>(obj)) return out;
    obj->Regenerate();
    Mesh* m = meshOf(obj);
    if (m) gothicExportGuard(id, obj, *m, out);
    return out;
}

// Export OBJ de la forme courante, materiaux compris : delegue a
// MeshIO::export_obj_zip_bytes, qui ecrit aussi le .mtl compagnon et met les
// deux dans une archive.
//
// Renvoie { name, bytes }. L'archive n'a qu'UNE entree quand le maillage ne
// porte aucun materiau (formes parametriques).
//
// Distinct de regenerate() par l'USAGE, pas par le format : regenerate() alimente
// l'amorcage du viseur, qui prend une chaine et non une archive.
//
// Passe par MEMFS parce que MeshIO ecrit par NOM DE FICHIER -- donc exactement
// le meme code qu'en natif, couvert par les memes tests d'E/S.
//
// Une Gothic Window passe par gothicExportGuard : `refused` ou `warning`.
emscripten::val exportObj(int id, const std::string& base)
{
    using namespace emscripten;
    val out = val::object();
    out.set("name", std::string());
    g_exportBytes.clear();

    IParameterized* obj = find(id);
    if (obj && microarchExportGuard(obj, out))
    {
        obj->Regenerate();
        Mesh* m = meshOf(obj);
        if (m && gothicExportGuard(id, obj, *m, out))
        {
            // `base` nomme les entrees INTERNES a l'archive, donc aussi la ligne
            // mtllib que le moteur natif derive du chemin de sortie : on ne garde que des
            // caracteres sans danger dans un nom de fichier.
            std::string safe;
            for (char c : base)
                if (isalnum((unsigned char)c) || c == '-' || c == '_') safe += c;
            if (safe.empty()) safe = "model";

            g_exportBytes = MeshIO::export_obj_zip_bytes(*m, safe);
            if (g_exportBytes.empty())
                std::fprintf(stderr, "exportObj: zip failed\n");
            else
                out.set("name", safe);
        }
    }
    out.set("bytes", val(typed_memory_view(g_exportBytes.size(),
                                           (const unsigned char*)g_exportBytes.data())));
    return out;
}

// Export GLB de la forme courante. UN SEUL fichier, la ou l'OBJ en demande deux :
// le glTF binaire embarque ses materiaux, il n'y a pas d'archive a batir. Meme
// garde que l'OBJ (gothicExportGuard).
emscripten::val exportGlb(int id, const std::string& base)
{
    using namespace emscripten;
    val out = val::object();
    out.set("name", std::string());
    g_exportBytes.clear();

    IParameterized* obj = find(id);
    if (obj && microarchExportGuard(obj, out))
    {
        obj->Regenerate();
        Mesh* m = meshOf(obj);
        if (m && gothicExportGuard(id, obj, *m, out))
        {
            // `base` ne nomme ici que le FICHIER telecharge : un GLB ne
            // reference aucun compagnon. Meme filtrage tout de meme, le nom
            // partant dans un attribut download.
            std::string safe;
            for (char c : base)
                if (isalnum((unsigned char)c) || c == '-' || c == '_') safe += c;
            if (safe.empty()) safe = "model";

            g_exportBytes = MeshIO::export_glb_bytes(*m);
            if (g_exportBytes.empty())
                std::fprintf(stderr, "exportGlb: glb failed\n");
            else
                out.set("name", safe);
        }
    }
    out.set("bytes", val(typed_memory_view(g_exportBytes.size(),
                                           (const unsigned char*)g_exportBytes.data())));
    return out;
}

// ---------------------------------------------------------------------------
// Microarchitecture gothique. Le resultat est le reseau de courbes ; le
// maillage n'est que son affichage tubulaire (exports generiques refuses par
// microarchExportGuard).
// ---------------------------------------------------------------------------

// { id, report, network [, failure] }
//   id      : id du registre, -1 si la specification ou le reseau est refuse
//   report  : ReportToJson(LoadReport()) -- lecture et reseau
//   network : texte gothic-microarchitecture-network/v1, "" sur refus. Copie en
//             chaine JS dans le rappel de Load, donc AVANT la construction des
//             tubes, et liberee du tas WASM au retour du rappel.
//   failure : what() d'une exception (epuisement de la memoire), id = -1
emscripten::val createMicroarchFromJson(const std::string& specText)
{
    using namespace emscripten;
    val out = val::object();
    out.set("id", -1);
    out.set("network", std::string());
    out.set("report", std::string());
    try
    {
        auto obj = std::make_unique<cgarchi::ParameterizedGothicMicroarchitecture>();
        const bool ok = obj->Load(specText, [&](const std::string& json) { out.set("network", val(json)); });
        out.set("report", cgarchi::microarch::ReportToJson(obj->LoadReport()));
        if (ok) out.set("id", registerObject(std::move(obj)));
    }
    catch (const std::exception& e)
    {
        out.set("failure", std::string(e.what()));
    }
    return out;
}

// ReportToJson du dernier maillage tente ; "" si `id` n'est pas une microarchitecture.
std::string microarchTubeReport(int id)
{
    auto* m = dynamic_cast<cgarchi::ParameterizedGothicMicroarchitecture*>(find(id));
    return m ? cgarchi::microarch::ReportToJson(m->TubeReport()) : std::string();
}

// [x, y, z] en doubles (point du dessin = sommet + origine), ou null sans maillage
// ou si `id` n'est pas une microarchitecture.
emscripten::val microarchOrigin(int id)
{
    using namespace emscripten;
    auto* m = dynamic_cast<cgarchi::ParameterizedGothicMicroarchitecture*>(find(id));
    if (!m || !m->GetMesh()) return val::null();
    val a = val::array();
    a.call<void>("push", m->MeshOrigin().x);
    a.call<void>("push", m->MeshOrigin().y);
    a.call<void>("push", m->MeshOrigin().z);
    return a;
}

// { name, bytes, report } ; kind = "curves" (polylignes du reseau) | "tubes"
// (maillage, origine appliquee). Un seul fichier .obj, sans materiau ; bytes vide
// sur refus, motif dans report (ReportToJson).
emscripten::val exportMicroarchObj(int id, const std::string& base, const std::string& kind)
{
    using namespace emscripten;
    val out = val::object();
    out.set("name", std::string());
    out.set("report", std::string());
    g_exportBytes.clear();
    auto* m = dynamic_cast<cgarchi::ParameterizedGothicMicroarchitecture*>(find(id));
    if (m && (kind == "curves" || kind == "tubes"))
    {
        std::string safe;
        for (char c : base)
            if (isalnum((unsigned char)c) || c == '-' || c == '_') safe += c;
        if (safe.empty()) safe = "microarchitecture";
        cgarchi::microarch::MicroarchReport report;
        if (kind == "curves")
            g_exportBytes = m->NetworkObj(report);
        else
        {
            m->Regenerate();
            g_exportBytes = m->TubeObj(report);
            // Sans maillage, le motif est celui du dernier maillage tente.
            if (!m->GetMesh()) report = m->TubeReport();
        }
        out.set("report", cgarchi::microarch::ReportToJson(report));
        if (!g_exportBytes.empty()) out.set("name", safe + "_" + kind);
    }
    out.set("bytes", val(typed_memory_view(g_exportBytes.size(),
                                           (const unsigned char*)g_exportBytes.data())));
    return out;
}

// Solide imprimable (J7-d, decisions M29 point 5 et M33), construit au clic puis libere : ni
// affiche, ni garde. { name, bytes, verdict, triangles, threshold, extentMm, report [, failure] }
//   bytes     : STL binaire en mm (centre en x et y, base a z = 0), Uint8Array COPIE dans le tas
//               JS ; le tampon du tas WASM est rendu avant le retour. Vide si refuse.
//   verdict   : "printable" | "not_printable" | "unverified" (F > threshold, STL exporte sans
//               verdict) | "refused" (parametres, plafonds dont le plafond exact de triangles
//               atteint pendant l'extraction, ou forme qui n'est pas une microarchitecture)
//   report    : ReportToJson -- construction et verdict, codes et positions
//   failure   : what() d'une exception (epuisement de la memoire), verdict "refused"
// Rayon en permil de D, q entier (jamais arrondi : refus sinon), echelle en mm par unite du
// dessin ; paroi minimale et enveloppe : imprimante en vigueur (g_printer).
emscripten::val exportMicroarchStl(int id, const std::string& base, double radiusPermil, double cellsPerRadius,
                                   double mmPerUnit)
{
    using namespace emscripten;
    // Le tampon du dernier export OBJ ou GLB est rendu avant de construire le solide.
    std::string().swap(g_exportBytes);
    val out = val::object();
    out.set("name", std::string());
    out.set("verdict", std::string("refused"));
    out.set("triangles", 0);
    out.set("report", std::string());
    std::size_t size = 0;
    val bytes = val::global("Uint8Array").new_(0);
    auto* m = dynamic_cast<cgarchi::ParameterizedGothicMicroarchitecture*>(find(id));
    if (m)
    {
        out.set("threshold", static_cast<double>(m->Limits().max_verified_triangles));
        try
        {
            cgarchi::MicroarchSolidExport e;
            if (!(std::isfinite(cellsPerRadius) && std::floor(cellsPerRadius) == cellsPerRadius
                  && std::fabs(cellsPerRadius) < 1e6))
                e.report.Record(cgarchi::microarch::MicroarchError::SolidInvalidParameter, "solid.cells_per_radius",
                                "the number of cells per radius must be an integer");
            else
            {
                cgarchi::microarch::SolidParams params;
                params.radius_ratio     = radiusPermil / 1000.0;
                params.cells_per_radius = static_cast<int>(cellsPerRadius);
                e = m->ExportSolidStl(params, mmPerUnit, g_printer);
            }
            out.set("verdict", std::string(cgarchi::SolidExportVerdictName(e.verdict)));
            out.set("triangles", static_cast<double>(e.triangles));
            out.set("report", cgarchi::microarch::ReportToJson(e.report));
            val extent = val::array();
            for (double x : e.extent_mm) extent.call<void>("push", x);
            out.set("extentMm", extent);
            size = e.stl.size();
            if (size)
            {
                bytes = val::global("Uint8Array").new_(size);
                bytes.call<void>("set", val(typed_memory_view(size, (const unsigned char*)e.stl.data())));
                std::string safe;
                for (char c : base)
                    if (isalnum((unsigned char)c) || c == '-' || c == '_') safe += c;
                if (safe.empty()) safe = "microarchitecture";
                out.set("name", safe + "_solid");
            }
        }
        catch (const std::exception& e)
        {
            out.set("verdict", std::string("refused"));
            out.set("failure", std::string(e.what()));
        }
    }
    out.set("bytes", bytes);
    return out;
}
#endif

// Rapport JSON (GothicBuildReportToJson) du dernier createGothicFromJson,
// accepte ou refuse : sur un refus, l'objet n'existe plus pour le porter.
namespace { std::string g_lastGothicLoadReport; }

// Cree une Gothic Window a partir d'un fichier de description JSON (schema
// gothic-window-v2, ou v1 converti). Parse cote natif
// (ParameterizedGothicWindow::LoadFromJson), puis la geometrie est generee par
// l'engine via Regenerate() (regenerate/meshData). Renvoie l'id, ou -1 si le
// JSON est refuse ; le motif est dans lastGothicLoadReport().
int createGothicFromJson(const std::string& jsonText)
{
    auto obj = std::unique_ptr<cgarchi::ParameterizedGothicWindow>(new cgarchi::ParameterizedGothicWindow());
    const bool loaded = obj->LoadFromJson(jsonText);
    g_lastGothicLoadReport = cgarchi::GothicBuildReportToJson(obj->LastLoadReport());
    if (!loaded) return -1;
    return registerObject(std::move(obj));
}

std::string lastGothicLoadReport()
{
    return g_lastGothicLoadReport;
}

// Rapports de la Gothic Window `id` : {"load": chargement JSON, "build": dernier
// Regenerate}, chacun au format GothicBuildReportToJson. "" si l'objet n'est pas
// une Gothic Window.
std::string gothicReport(int id)
{
    const cgarchi::ParameterizedGothicWindow* gw = dynamic_cast<cgarchi::ParameterizedGothicWindow*>(find(id));
    if (!gw) return std::string();
    return "{\"load\":" + cgarchi::GothicBuildReportToJson(gw->LastLoadReport())
         + ",\"build\":" + cgarchi::GothicBuildReportToJson(gw->LastBuildReport()) + "}";
}

// Pose l'imprimante : document print-constraints/v1, lecture stricte
// (PrintConstraintsFromJson). Refus : imprimante en vigueur inchangee. Un
// mmPerUnit du document est IGNORE (l'echelle vient de la largeur imprimee de
// chaque piece) et nomme dans `ignored`. Rend
//   {"accepted": bool, "errors": [{"path", "message"}], "ignored": [...],
//    "constraints": document de l'imprimante en vigueur apres l'appel}.
std::string setPrintConstraints(const std::string& jsonText)
{
    PrintConstraints read = g_printer;
    const std::vector<PrintJsonError> errors = PrintConstraintsFromJson(jsonText, read);
    const bool ignoredScale = errors.empty() && read.mmPerUnit.has_value();
    if (errors.empty())
    {
        read.mmPerUnit.reset();
        g_printer = read;
    }
    std::string j = "{\"accepted\":";
    j += errors.empty() ? "true" : "false";
    j += ",\"errors\":[";
    for (std::size_t i = 0; i < errors.size(); ++i)
    {
        if (i) j += ',';
        j += "{\"path\":\"" + jsonEscape(errors[i].path) + "\",\"message\":\"" + jsonEscape(errors[i].message) + "\"}";
    }
    j += "],\"ignored\":[";
    if (ignoredScale) j += "\"mmPerUnit\"";
    j += "],\"constraints\":" + PrintConstraintsToJson(g_printer) + "}";
    return j;
}

// Document print-constraints/v1 de l'imprimante en vigueur, sans mmPerUnit.
std::string printConstraints()
{
    return PrintConstraintsToJson(g_printer);
}

// Largeur imprimee (mm) de la piece `id`, d'ou la garde derive l'echelle a
// chaque controle (GothicMmPerUnitForPrintedWidth). NaN l'efface (echelle non
// fixee) ; toute autre valeur est gardee telle quelle : une valeur hors domaine
// est rendue par la garde dans le rapport, sans bloquer l'export. Rend false
// pour un id inconnu.
bool setPrintedWidthMm(int id, double mm)
{
    if (!find(id)) return false;
    if (std::isnan(mm)) g_printedWidthMm.erase(id);
    else g_printedWidthMm[id] = mm;
    return true;
}

// Exporte la Gothic Window `id` en JSON de description (schema gothic-window-v2),
// reflet des valeurs courantes de l'UI. Renvoie "" si l'objet n'est pas une
// Gothic Window.
std::string exportGothicJson(int id)
{
    IParameterized* obj = find(id);
    cgarchi::ParameterizedGothicWindow* gw = dynamic_cast<cgarchi::ParameterizedGothicWindow*>(obj);
    return gw ? gw->ExportJson() : std::string();
}

// Parametres typees d'un objet, serialises en JSON. "id" est la cle de
// setParam, setParamString et des liens profonds ; "name" n'est qu'un libelle.
std::string getParams(int id)
{
    IParameterized* obj = find(id);
    if (!obj) return "[]";

    std::vector<Parameter> params = obj->GetParameters();
    std::string j = "[";
    bool first = true;
    for (Parameter& p : params) {
        if (!first) j += ',';
        first = false;
        j += "{\"id\":\""; j += jsonEscape(p.Id()); j += "\",";
        j += "\"name\":\""; j += jsonEscape(p.GetName()); j += "\",";
        switch (p.GetType()) {
            case Parameter::INT:
                j += "\"type\":\"int\",\"value\":" + std::to_string(p.GetInt());
                j += ",\"min\":" + std::to_string(p.GetMinInt());
                j += ",\"max\":" + std::to_string(p.GetMaxInt());
                break;
            case Parameter::FLOAT:
                j += "\"type\":\"float\",\"value\":" + num(p.GetFloat());
                j += ",\"min\":" + num(p.GetMinFloat());
                j += ",\"max\":" + num(p.GetMaxFloat());
                break;
            case Parameter::BOOL:
                j += "\"type\":\"bool\",\"value\":";
                j += (p.GetBool() ? "true" : "false");
                break;
            case Parameter::STRING:
                j += "\"type\":\"string\",\"value\":\"";
                j += jsonEscape(p.GetString());
                j += "\",\"multiline\":";
                j += (p.IsMultiline() ? "true" : "false");
                break;
            case Parameter::ENUM: {
                j += "\"type\":\"enum\",\"value\":" + std::to_string(p.GetInt());
                j += ",\"choices\":[";
                bool f2 = true;
                for (const std::string& c : p.GetChoices()) {
                    if (!f2) j += ',';
                    f2 = false;
                    j += '"'; j += jsonEscape(c); j += '"';
                }
                j += ']';
                break;
            }
        }
        j += '}';
    }
    j += ']';
    return j;
}

// Ecrit une valeur de parametre designe par son identifiant (Parameter::Id), sans
// pointeur cru cote JS. Refuse une valeur non finie (NaN, infinie), y compris une
// fois convertie en float (1e39) : aucun parametre n'en admet.
bool setParam(int id, const std::string& paramId, double value)
{
    IParameterized* obj = find(id);
    if (!obj) return false;
    if (!std::isfinite(value) || std::fabs(value) > std::numeric_limits<float>::max()) return false;

    std::vector<Parameter> params = obj->GetParameters();
    for (Parameter& p : params) {
        if (p.Id() != paramId) continue;
        switch (p.GetType()) {
            case Parameter::INT:
            case Parameter::ENUM:  p.SetInt((int)value); break;
            case Parameter::FLOAT: p.SetFloat((float)value); break;
            case Parameter::BOOL:  p.SetBool(value != 0.0); break;
            // Un parametre TEXTUEL n'a pas de valeur numerique : l'ecrire par ce
            // chemin serait une erreur d'appel cote JS, pas une conversion a
            // deviner. On refuse plutot que d'ecraser la chaine par du vide.
            case Parameter::STRING: return false;
        }
        return true;
    }
    return false;
}

// Ecrit un parametre TEXTUEL. Fonction distincte et non une surcharge : embind
// resout par arite et par type, et une surcharge double/string sur le meme nom
// exposerait au JS un appel dont le comportement dependrait du type dynamique de
// l'argument -- exactement le genre d'ambiguite que le pont evite ailleurs.
//
// La chaine traverse en std::string, donc en UTF-8 : c'est bien ce que l'on veut
// ici (contrairement aux OCTETS d'une police ou d'une image, que cette meme
// conversion detruirait).
bool setParamString(int id, const std::string& paramId, const std::string& value)
{
    IParameterized* obj = find(id);
    if (!obj) return false;

    std::vector<Parameter> params = obj->GetParameters();
    for (Parameter& p : params) {
        if (p.Id() != paramId) continue;
        if (p.GetType() != Parameter::STRING) return false;
        p.SetString(value);
        return true;
    }
    return false;
}

// Reconstruit le maillage depuis les parametres courants et renvoie l'OBJ
// d'AMORCAGE du viseur (cf. l'en-tete : son `mtllib` ne resout pas). Le shell
// rebascule aussitot sur meshData(), qui porte la geometrie non soudee ET les
// materiaux. "" pour une microarchitecture, ou sans maillage.
std::string regenerate(int id)
{
    IParameterized* obj = find(id);
    if (!obj) return std::string();
    obj->Regenerate();
    // Microarchitecture : pas d'OBJ texte d'amorcage. Au plafond de triangles, il
    // porterait le tas au-dela de 1 Gio ; le shell affiche alors par meshData.
    if (dynamic_cast<cgarchi::ParameterizedGothicMicroarchitecture*>(obj)) return std::string();
    Mesh* m = meshOf(obj);
    return m ? MeshIO::export_obj_bytes(*m, "model") : std::string();
}

// Libere l'objet, et le fichier MEMFS dont il etait le seul lecteur.
void destroyShape(int id)
{
    g_objects.erase(id);      // d'abord l'objet : plus personne ne relira le chemin
    removeTempFile(id);
    g_printedWidthMm.erase(id);
}

#ifdef __EMSCRIPTEN__
// Regenere puis renvoie la geometrie brute (positions + indices) en vues typees
// sur le tas WASM, pour une mise a jour three.js EN PLACE cote JS (evite le
// reload de fichier d'o3dv, source du flickering). Les buffers statiques restent
// valides jusqu'au prochain appel : le JS doit copier les vues immediatement.
static std::vector<float>        g_pos;
static std::vector<unsigned int> g_idx;
static std::vector<float>        g_col;

// Couleurs par sommet deduites du material id des faces, pour les maillages
// MULTI-MATERIAUX (le relief d'image : une couleur par region). Renvoie false
// quand le maillage n'a pas de MaterialColor exploitable -> le JS garde alors sa
// couleur globale (color picker) pour les formes parametriques classiques.
//
// Aucun sommet n'est partage entre deux materiaux differents dans les maillages
// produits par ExtrudedMeshBuilder (blocs de sommets disjoints par Append), donc
// une couleur par sommet est exacte, sans duplication.
bool fillVertexColors(Mesh* m, std::vector<float>& out)
{
    const unsigned int nMat = m->GetNMaterials();
    if (nMat == 0) return false;

    out.assign((size_t)m->GetNVertices() * 3, 0.7f);
    bool any = false;
    for (unsigned int f = 0; f < m->GetNFaces(); ++f) {
        MaterialColor* mc =
            dynamic_cast<MaterialColor*>(m->GetMaterial((unsigned int)m->GetFaceMaterialId(f)));
        if (!mc) continue;
        const float rgb[3] = { mc->GetFloatRed(), mc->GetFloatGreen(), mc->GetFloatBlue() };
        const int nfv = m->GetFaceNVertices(f);
        for (int k = 0; k < nfv; ++k) {
            const int vi = m->GetFaceVertex(f, k);
            if (vi < 0) continue;
            out[(size_t)vi * 3 + 0] = rgb[0];
            out[(size_t)vi * 3 + 1] = rgb[1];
            out[(size_t)vi * 3 + 2] = rgb[2];
            any = true;
        }
    }
    if (!any) out.clear();
    return any;
}

emscripten::val meshData(int id)
{
    using namespace emscripten;
    val out = val::object();
    // Tampons rendus au tas AVANT Regenerate, pas seulement vides : une capacite
    // gardee fragmente le tas pendant la reconstruction, qui croit alors a chaque
    // changement de curseur.
    std::vector<float>().swap(g_pos);
    std::vector<unsigned int>().swap(g_idx);
    std::vector<float>().swap(g_col);

    IParameterized* obj = find(id);
    if (obj) {
        obj->Regenerate();
        Mesh* m = meshOf(obj);
        if (m) {
            const int nv = m->GetNVertices();
            g_pos.reserve((size_t)nv * 3);
            float v[3];
            for (int i = 0; i < nv; ++i) {
                m->GetVertex((unsigned int)i, v);
                g_pos.push_back(v[0]);
                g_pos.push_back(v[1]);
                g_pos.push_back(v[2]);
            }
            g_idx = m->GetTriangles();
            fillVertexColors(m, g_col);
        }
    }
    out.set("positions", val(typed_memory_view(g_pos.size(), g_pos.data())));
    out.set("indices",   val(typed_memory_view(g_idx.size(), g_idx.data())));
    // Vide quand le maillage est mono-couleur : le JS retombe sur le picker.
    out.set("colors",    val(typed_memory_view(g_col.size(), g_col.data())));
    out.set("nv", (int)(g_pos.size() / 3));
    out.set("nf", (int)(g_idx.size() / 3));
    return out;
}

// ---------------------------------------------------------------------------
//  Resultat d'un graphe -> OBJ, pour les PAGES TEMPLATEES
// ---------------------------------------------------------------------------
//
// Les pages de formes affichent un objet en donnant son OBJ a Online3DViewer.
// Une page pilotee par un GRAPHE a le meme besoin, et rien ne le servait : la
// facade nodale ne sait rendre que des vues typees (graphMeshView), faites pour
// le renderer WebGL du worker.
//
// Les deux facades partagent le meme artefact et, sur le thread UI, la meme
// instance : lire host () d'ici est donc lire le document que la page vient
// d'evaluer, pas un autre.
//
// DEUX POINTS D'ENTREE, un par usage, et le meme ecrivain derriere :
//   graphExportObj(port)          -> la chaine .obj, pour l'AMORCAGE du viseur ;
//   graphExportObjZip(port, base) -> l'archive OBJ+MTL, pour le TELECHARGEMENT.
// Geometrie du resultat d'un graphe en vues typees, COULEURS COMPRISES.
//
// C'est le pendant de meshData(id) pour une page GABARIT. Il ne double pas l'OBJ
// d'amorcage : celui-ci est SOUDE par le lecteur d'o3dv, ce qui lisse les
// normales a travers les aretes vives, et il ne traverse qu'une fois. La mise a
// jour en place rejoue a chaque pas de curseur -- d'ou des vues typees plutot
// qu'un fichier texte.
//
// Les couleurs passent par le meme chemin que pour les formes : un attribut
// `color` par sommet, que viewer.js branche sur three.js. fillVertexColors rend
// un tableau VIDE pour un maillage mono-materiau, et le JS retombe alors sur le
// selecteur de couleur -- exactement comme pour une forme parametrique.
// Geometrie du resultat nodal, telle que three.js la veut.
//
// Le corps vit dans mesh_payload.cpp, PARTAGE avec graphMeshView : les deux vues
// 3D de maker -- celle des pages et celle de l'editeur nodal -- lisaient le meme
// maillage de deux facons differentes, et l'editeur n'en tirait que positions et
// indices. Voir l'en-tete de mesh_payload.h.
static maker::MeshPayloadBuffers g_graphBufs;

emscripten::val graphMeshData(unsigned int port)
{
    const cggraph::ValueList& outputs = maker_graph::host().model.GetLastOutputs();
    const std::shared_ptr<const Mesh> mesh =
        (port < outputs.size())
            ? outputs[port].Share<Mesh>(cggraph_nodes::Types().mesh)
            : nullptr;
    return maker::BuildMeshPayload(mesh.get(), g_graphBufs);
}

// Maillage porte par un port de sortie du graphe, ou nullptr.
static std::shared_ptr<const Mesh> graphOutputMesh(unsigned int port)
{
    const cggraph::ValueList& outputs = maker_graph::host().model.GetLastOutputs();
    if (port >= outputs.size()) return nullptr;
    return outputs[port].Share<Mesh>(cggraph_nodes::Types().mesh);
}

// OBJ d'AMORCAGE du viseur, pendant nodal de regenerate() : meme `mtllib` qui ne
// resout pas, et meme bascule de la page vers graphMeshData.
std::string graphExportObj(unsigned int port)
{
    const std::shared_ptr<const Mesh> mesh = graphOutputMesh(port);
    return mesh ? MeshIO::export_obj_bytes(*mesh, "model") : std::string();
}

// Export UTILISATEUR d'une page GABARIT : l'archive OBJ+MTL, donc les materiaux.
// Calque de exportObj(id), a ceci pres que le maillage vient d'un port de sortie
// de graphe et non d'un objet identifie.
emscripten::val graphExportObjZip(unsigned int port, const std::string& base)
{
    using namespace emscripten;
    val out = val::object();
    out.set("name", std::string());
    g_exportBytes.clear();

    const std::shared_ptr<const Mesh> mesh = graphOutputMesh(port);
    if (mesh)
    {
        // `base` nomme les entrees INTERNES a l'archive, donc aussi la ligne
        // mtllib : on ne garde que des caracteres sans danger dans un nom.
        std::string safe;
        for (char c : base)
            if (isalnum((unsigned char)c) || c == '-' || c == '_') safe += c;
        if (safe.empty()) safe = "model";

        g_exportBytes = MeshIO::export_obj_zip_bytes(*mesh, safe);
        if (!g_exportBytes.empty())
            out.set("name", safe);
    }
    out.set("bytes", val(typed_memory_view(g_exportBytes.size(),
                                           (const unsigned char*)g_exportBytes.data())));
    return out;
}

// Le meme GLB, source nodale : `port` est un port de sortie du graphe et non un
// identifiant d'objet.
emscripten::val graphExportGlb(unsigned int port, const std::string& base)
{
    using namespace emscripten;
    val out = val::object();
    out.set("name", std::string());
    g_exportBytes.clear();

    const std::shared_ptr<const Mesh> mesh = graphOutputMesh(port);
    if (mesh)
    {
        std::string safe;
        for (char c : base)
            if (isalnum((unsigned char)c) || c == '-' || c == '_') safe += c;
        if (safe.empty()) safe = "model";

        g_exportBytes = MeshIO::export_glb_bytes(*mesh);
        if (!g_exportBytes.empty())
            out.set("name", safe);
    }
    out.set("bytes", val(typed_memory_view(g_exportBytes.size(),
                                           (const unsigned char*)g_exportBytes.data())));
    return out;
}

EMSCRIPTEN_BINDINGS(maker)
{
    emscripten::function("listShapes",        &listShapes);
    emscripten::function("shapeWarnings",     &shapeWarnings);
    emscripten::function("createShape",       &createShape);
    emscripten::function("createSvgExtrusion",&createSvgExtrusion);
    emscripten::function("createImageRelief", &createImageRelief);
    emscripten::function("createImagePixelBlocks", &createImagePixelBlocks);
    emscripten::function("createTextExtrusion", &createTextExtrusion);
    emscripten::function("exportPixelBlocks",      &exportPixelBlocks);
    emscripten::function("createGothicFromJson",&createGothicFromJson);
    emscripten::function("exportGothicJson",  &exportGothicJson);
    emscripten::function("gothicReport",      &gothicReport);
    emscripten::function("lastGothicLoadReport", &lastGothicLoadReport);
    emscripten::function("setPrintConstraints", &setPrintConstraints);
    emscripten::function("printConstraints",  &printConstraints);
    emscripten::function("setPrintedWidthMm", &setPrintedWidthMm);
    emscripten::function("gothicPrintReport", &gothicPrintReport);
    emscripten::function("getParams",         &getParams);
    emscripten::function("setParam",          &setParam);
    emscripten::function("setParamString",    &setParamString);
    emscripten::function("regenerate",        &regenerate);
    emscripten::function("graphExportObj",    &graphExportObj);
    emscripten::function("graphExportObjZip", &graphExportObjZip);
    emscripten::function("graphExportGlb",    &graphExportGlb);
    emscripten::function("graphMeshData",     &graphMeshData);
    emscripten::function("exportObj",         &exportObj);
    emscripten::function("exportGlb",         &exportGlb);
    emscripten::function("meshData",          &meshData);
    emscripten::function("destroyShape",      &destroyShape);
    emscripten::function("createMicroarchFromJson", &createMicroarchFromJson);
    emscripten::function("microarchTubeReport",     &microarchTubeReport);
    emscripten::function("microarchOrigin",         &microarchOrigin);
    emscripten::function("exportMicroarchObj",      &exportMicroarchObj);
    emscripten::function("exportMicroarchStl",      &exportMicroarchStl);
}
#endif
