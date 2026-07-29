// ===========================================================================
//  maker — pont Embind cgmesh <-> JavaScript (Emscripten)
// ===========================================================================
//
// Etape 2 (cf. maker/ANALYSE.md sec.4) : facade minimale exposant la logique
// geometrique de cgmesh au navigateur.
//
//   listShapes()               -> JSON : catalogue des formes disponibles
//   createShape(name)          -> id entier (registre cote C++), -1 si inconnu
//   createSvgExtrusion(svg)    -> id, a partir du texte SVG (via MEMFS)
//   getParams(id)              -> JSON : parametres typees (name/type/value/min/max/choices)
//   setParam(id, name, value)  -> bool : ecrit la valeur (par nom, jamais de pointeur cru)
//   regenerate(id)             -> string OBJ : Regenerate() puis export du maillage
//   destroyShape(id)           -> libere l'objet
//
// Le maillage est renvoye en OBJ texte (chemin le plus court, cf. ANALYSE sec.5) ;
// l'export est fait ici depuis Mesh (GetNVertices/GetVertex/GetTriangles) pour ne
// PAS tirer mesh_io.cpp et ses formats (3ds/ply/...) dans le core WASM.
//
// Les Parameter de cgmesh referencent des pointeurs crus vers les membres de
// l'objet ; ils ne traversent jamais la frontiere JS : on serialise en JSON en
// sortie et on applique par nom en entree (ANALYSE sec.2/4).
//
// ===========================================================================

#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "parameterized_shapes.h"   // IParameterized + catalogue de formes
#include "mesh.h"                    // GetNVertices / GetVertex / GetTriangles

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
// l'objet cgmesh en garde le CHEMIN et le relit a chaque Regenerate(), donc on ne
// peut pas le supprimer apres chargement -- il doit vivre aussi longtemps que
// l'objet. Sans ce registre, chaque import laissait sa copie dans le heap WASM
// pour la duree de la page (verifie : /tmp accumulait maker_img_0/1/2...).
std::map<int, std::string> g_tempFiles;

void removeTempFile(int id)
{
    auto it = g_tempFiles.find(id);
    if (it == g_tempFiles.end()) return;
    std::remove(it->second.c_str());
    g_tempFiles.erase(it);
}

using Factory = std::function<std::unique_ptr<IParameterized>()>;

template <typename T>
Factory make() { return [] { return std::unique_ptr<IParameterized>(new T()); }; }

// Catalogue : source unique de verite (nom affiche == GetName()). Les formes
// necessitant un fichier (SVG, nuage de points implicite) ont leur propre
// fabrique dediee et ne figurent pas ici.
const std::vector<std::pair<std::string, Factory>>& catalog()
{
    static const std::vector<std::pair<std::string, Factory>> c = {
        {"Cube",                  make<ParameterizedCube>()},
        {"Sphere",                make<ParameterizedSphere>()},
        {"Cylinder",              make<ParameterizedCylinder>()},
        {"Cone",                  make<ParameterizedCone>()},
        {"Capsule",               make<ParameterizedCapsule>()},
        {"Torus",                 make<ParameterizedTorus>()},
        {"Klein Bottle",          make<ParameterizedKleinBottle>()},
        {"Helicoid",              make<ParameterizedHelicoid>()},
        {"Seashell",              make<ParameterizedSeashell>()},
        {"Seashell (von Seggern)",make<ParameterizedSeashellVonSeggern>()},
        {"Corkscrew",             make<ParameterizedCorkscrew>()},
        {"Mobius Strip",          make<ParameterizedMobiusStrip>()},
        {"Radial Wave",           make<ParameterizedRadialWave>()},
        {"Breather",              make<ParameterizedBreather>()},
        {"Hyperbolic Paraboloid", make<ParameterizedHyperbolicParaboloid>()},
        {"Monkey Saddle",         make<ParameterizedMonkeySaddle>()},
        {"Blobs",                 make<ParameterizedBlobs>()},
        {"Drop",                  make<ParameterizedDrop>()},
        {"Guimard",               make<ParameterizedGuimard>()},
        {"Torus Knot",            make<ParameterizedTorusKnot>()},
        {"Cinquefoil Knot",       make<ParameterizedCinquefoilKnot>()},
        {"Trefoil Knot",          make<ParameterizedTrefoilKnot>()},
        {"Borromean Rings",       make<ParameterizedBorromeanRings>()},
        {"Menger Sponge",         make<ParameterizedMengerSponge>()},
        {"L-system",              make<ParameterizedLSystem>()},
        {"Gothic Window",         make<ParameterizedGothicWindow>()},
        {"Gothic Block",          make<ParameterizedGothicBlock>()},
    };
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
            default:   o += c;      break;
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

// --- export OBJ direct depuis Mesh -----------------------------------------
std::string meshToObj(Mesh* m)
{
    if (!m) return std::string();
    std::string obj = "# generated by maker (cgmesh wasm)\n";
    const int nv = m->GetNVertices();
    obj.reserve(nv * 24 + 64);
    float v[3];
    for (int i = 0; i < nv; ++i) {
        m->GetVertex((unsigned int)i, v);
        obj += "v ";
        obj += num(v[0]); obj += ' ';
        obj += num(v[1]); obj += ' ';
        obj += num(v[2]); obj += '\n';
    }
    std::vector<unsigned int> tris = m->GetTriangles();
    for (size_t t = 0; t + 2 < tris.size(); t += 3) {
        // OBJ est indexe a partir de 1.
        obj += "f ";
        obj += std::to_string(tris[t] + 1);     obj += ' ';
        obj += std::to_string(tris[t + 1] + 1); obj += ' ';
        obj += std::to_string(tris[t + 2] + 1); obj += '\n';
    }
    return obj;
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

// Catalogue des formes : ["Cube","Sphere",...]
std::string listShapes()
{
    std::string j = "[";
    bool first = true;
    for (const auto& e : catalog()) {
        if (!first) j += ',';
        first = false;
        j += '"'; j += jsonEscape(e.first); j += '"';
    }
    j += ']';
    return j;
}

// Cree une forme du catalogue. Renvoie son id, ou -1 si le nom est inconnu.
int createShape(const std::string& name)
{
    for (const auto& e : catalog()) {
        if (e.first == name)
            return registerObject(e.second());
    }
    return -1;
}

// Cree une extrusion SVG a partir du contenu texte du fichier (fourni par un
// <input type=file> cote JS). Ecrit en MEMFS puis instancie l'importeur cgmesh.
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

// Cree une Gothic Window a partir d'un fichier de description JSON (schema
// gothic-window-v2). Parse cote cgmesh (ParameterizedGothicWindow::LoadFromJson),
// puis la geometrie est generee par l'engine via Regenerate() (regenerate/meshData).
// Renvoie l'id, ou -1 si le JSON est illisible.
int createGothicFromJson(const std::string& jsonText)
{
    auto obj = std::unique_ptr<ParameterizedGothicWindow>(new ParameterizedGothicWindow());
    if (!obj->LoadFromJson(jsonText)) return -1;
    return registerObject(std::move(obj));
}

// Exporte la Gothic Window `id` en JSON de description (schema gothic-window-v2),
// reflet des valeurs courantes de l'UI. Renvoie "" si l'objet n'est pas une
// Gothic Window.
std::string exportGothicJson(int id)
{
    IParameterized* obj = find(id);
    ParameterizedGothicWindow* gw = dynamic_cast<ParameterizedGothicWindow*>(obj);
    return gw ? gw->ExportJson() : std::string();
}

// Parametres typees d'un objet, serialises en JSON.
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
        j += "{\"name\":\""; j += jsonEscape(p.GetName()); j += "\",";
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

// Ecrit une valeur de parametre par nom (aucun pointeur cru cote JS).
bool setParam(int id, const std::string& name, double value)
{
    IParameterized* obj = find(id);
    if (!obj) return false;

    std::vector<Parameter> params = obj->GetParameters();
    for (Parameter& p : params) {
        if (p.GetName() != name) continue;
        switch (p.GetType()) {
            case Parameter::INT:
            case Parameter::ENUM:  p.SetInt((int)value); break;
            case Parameter::FLOAT: p.SetFloat((float)value); break;
            case Parameter::BOOL:  p.SetBool(value != 0.0); break;
        }
        return true;
    }
    return false;
}

// Reconstruit le maillage depuis les parametres courants et renvoie l'OBJ.
std::string regenerate(int id)
{
    IParameterized* obj = find(id);
    if (!obj) return std::string();
    obj->Regenerate();
    return meshToObj(meshOf(obj));
}

// Libere l'objet, et le fichier MEMFS dont il etait le seul lecteur.
void destroyShape(int id)
{
    g_objects.erase(id);      // d'abord l'objet : plus personne ne relira le chemin
    removeTempFile(id);
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
    g_pos.clear();
    g_idx.clear();
    g_col.clear();

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

EMSCRIPTEN_BINDINGS(maker)
{
    emscripten::function("listShapes",        &listShapes);
    emscripten::function("createShape",       &createShape);
    emscripten::function("createSvgExtrusion",&createSvgExtrusion);
    emscripten::function("createImageRelief", &createImageRelief);
    emscripten::function("createGothicFromJson",&createGothicFromJson);
    emscripten::function("exportGothicJson",  &exportGothicJson);
    emscripten::function("getParams",         &getParams);
    emscripten::function("setParam",          &setParam);
    emscripten::function("regenerate",        &regenerate);
    emscripten::function("meshData",          &meshData);
    emscripten::function("destroyShape",      &destroyShape);
}
#endif
