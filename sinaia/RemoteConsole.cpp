#include "RemoteConsole.h"

#include "SinaiaFrame.h"
#include "wxOpenGLCanvas.h"

#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/material.h"
#include "../src/cgmesh/material_pbr.h"
#include "../src/cgmesh/vmeshes.h"

#include <wx/app.h>
#include <wx/string.h>

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <future>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace {

// ----- main-thread marshaling -----------------------------------------------

// Runs `fn` on the main wx thread and blocks the calling (worker) thread
// until it returns. Template so we can return whatever type `fn` returns.
template <typename Fn>
auto callOnMain(Fn&& fn) -> decltype(fn())
{
    using R = decltype(fn());
    std::promise<R> prom;
    auto fut = prom.get_future();
    wxTheApp->CallAfter([&prom, &fn]() {
        if constexpr (std::is_void_v<R>) {
            fn();
            prom.set_value();
        } else {
            prom.set_value(fn());
        }
    });
    return fut.get();
}

std::string trim(const std::string& s)
{
    size_t a = 0;
    size_t b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
    while (b > a && (s[b-1] == ' ' || s[b-1] == '\t')) --b;
    return s.substr(a, b - a);
}

// ----- handler helpers ------------------------------------------------------

// CIBLE des commandes qui inspectent ou modifient UN fichier.
//
// Regle (MyGLCanvas::GetTargetModel) : un seul modele dans la vue -> c'est lui,
// sans selection prealable ; plusieurs -> le modele selectionne. Plusieurs sans
// selection est un REFUS explicite, et non un repli sur le modele 0 : une
// commande mal ciblee doit se voir, pas rendre des compteurs plausibles.
struct TargetMeshes
{
    VMeshes*    meshes = nullptr;
    const char* err    = nullptr;
    explicit operator bool() const { return meshes != nullptr; }
};

TargetMeshes getTargetVMeshes(MyFrame* frame)
{
    if (!frame) return { nullptr, "ERR frame unavailable\n" };
    MyGLCanvas* canvas = frame->GetActiveCanvas();
    if (!canvas) return { nullptr, "ERR no active view\n" };
    if (VMeshes* vm = canvas->GetTargetMeshes())
        return { vm, nullptr };
    if (canvas->IsTargetAmbiguous())
        return { nullptr, "ERR several models loaded and none selected"
                          " -- use 'select N' to choose the target\n" };
    return { nullptr, "ERR no model loaded\n" };
}

std::string bboxLine(const BoundingBox& bb)
{
    if (bb.IsEmpty())
        return std::string("bbox empty\n");
    char buf[256];
    std::snprintf(buf, sizeof(buf), "bbox %g %g %g %g %g %g\n",
                  bb.GetMinX(), bb.GetMinY(), bb.GetMinZ(),
                  bb.GetMaxX(), bb.GetMaxY(), bb.GetMaxZ());
    return buf;
}

// ----- command handlers -----------------------------------------------------

std::string cmdInfo(MyFrame* frame)
{
    const TargetMeshes target = getTargetVMeshes(frame);
    if (!target) return target.err;
    VMeshes* vm = target.meshes;

    BoundingBox total;
    for (auto* m : vm->GetMeshes())
        if (m) total.AddBoundingBox(m->bbox());

    std::ostringstream os;
    os << "meshes " << vm->GetNMeshes() << "\n";
    os << "vertices " << vm->GetNVertices() << "\n";
    os << "faces " << vm->GetNFaces() << "\n";
    os << bboxLine(total);
    os << "OK\n";
    return os.str();
}

std::string cmdMesh(MyFrame* frame, int n)
{
    const TargetMeshes target = getTargetVMeshes(frame);
    if (!target) return target.err;
    VMeshes* vm = target.meshes;
    auto& meshes = vm->GetMeshes();
    if (n < 0 || (size_t)n >= meshes.size())
        return "ERR mesh index out of range\n";
    Mesh* m = meshes[n];
    if (!m) return "ERR mesh slot empty\n";

    std::ostringstream os;
    os << "name \"" << m->GetName () << "\"\n";
    os << "vertices " << m->GetNVertices() << "\n";
    os << "faces " << m->GetNFaces() << "\n";
    os << "materials " << m->GetNMaterials() << "\n";
    os << bboxLine(m->bbox());
    os << "OK\n";
    return os.str();
}

std::string cmdFace(MyFrame* frame, int n, int f)
{
    const TargetMeshes target = getTargetVMeshes(frame);
    if (!target) return target.err;
    VMeshes* vm = target.meshes;
    auto& meshes = vm->GetMeshes();
    if (n < 0 || (size_t)n >= meshes.size())
        return "ERR mesh index out of range\n";
    Mesh* m = meshes[n];
    if (!m || f < 0 || (unsigned)f >= m->GetNFaces ())
        return "ERR face index out of range\n";

    auto face = m->FaceAt (f);
    if (!face || face->GetNVertices() < 3)
        return "ERR face is not a triangle\n";

    const unsigned int a = face->GetVertex(0);
    const unsigned int b = face->GetVertex(1);
    const unsigned int c = face->GetVertex(2);

    std::ostringstream os;
    os << "vertices " << a << " " << b << " " << c << "\n";
    os << "positions\n";
    for (unsigned int vi : {a, b, c})
    {
        if (vi >= m->GetNVertices ()) { os << "  invalid\n"; continue; }
        char buf[128];
        std::snprintf(buf, sizeof(buf), "  %g %g %g\n",
                      m->GetVertices ()[3*vi], m->GetVertices ()[3*vi+1], m->GetVertices ()[3*vi+2]);
        os << buf;
    }
    if (!m->GetFaceNormals ().empty() && (unsigned)f * 3 + 2 < m->GetFaceNormals ().size())
    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "face_normal %g %g %g\n",
                      m->GetFaceNormals ()[3*f], m->GetFaceNormals ()[3*f+1], m->GetFaceNormals ()[3*f+2]);
        os << buf;
    }
    os << "material " << static_cast<int>(face->GetMaterialId()) << "\n";
    os << "OK\n";
    return os.str();
}

std::string cmdVertex(MyFrame* frame, int n, int v)
{
    const TargetMeshes target = getTargetVMeshes(frame);
    if (!target) return target.err;
    VMeshes* vm = target.meshes;
    auto& meshes = vm->GetMeshes();
    if (n < 0 || (size_t)n >= meshes.size())
        return "ERR mesh index out of range\n";
    Mesh* m = meshes[n];
    if (!m || v < 0 || (unsigned)v >= m->GetNVertices ())
        return "ERR vertex index out of range\n";

    std::ostringstream os;
    char buf[160];
    std::snprintf(buf, sizeof(buf), "position %g %g %g\n",
                  m->GetVertices ()[3*v], m->GetVertices ()[3*v+1], m->GetVertices ()[3*v+2]);
    os << buf;
    if (!m->GetVertexNormals ().empty() && (unsigned)v * 3 + 2 < m->GetVertexNormals ().size())
    {
        std::snprintf(buf, sizeof(buf), "normal %g %g %g\n",
                      m->GetVertexNormals ()[3*v], m->GetVertexNormals ()[3*v+1], m->GetVertexNormals ()[3*v+2]);
        os << buf;
    }
    if (!m->GetTextureCoordinates ().empty() && (unsigned)v * 2 + 1 < m->GetTextureCoordinates ().size())
    {
        std::snprintf(buf, sizeof(buf), "uv %g %g\n",
                      m->GetTextureCoordinates ()[2*v], m->GetTextureCoordinates ()[2*v+1]);
        os << buf;
    }
    os << "OK\n";
    return os.str();
}

std::string cmdMaterial(MyFrame* frame, int n, int matId)
{
    const TargetMeshes target = getTargetVMeshes(frame);
    if (!target) return target.err;
    VMeshes* vm = target.meshes;
    auto& meshes = vm->GetMeshes();
    if (n < 0 || (size_t)n >= meshes.size())
        return "ERR mesh index out of range\n";
    Mesh* m = meshes[n];
    if (!m || matId < 0 || (unsigned)matId >= m->GetNMaterials())
        return "ERR material index out of range\n";
    Material* mat = m->GetMaterial(matId);
    if (!mat) return "ERR material slot empty\n";

    std::ostringstream os;
    os << "name \"" << mat->GetName() << "\"\n";
    const MaterialType t = mat->GetType();
    const char* tname = "MATERIAL_NONE";
    if (t == MATERIAL_COLOR)     tname = "MATERIAL_COLOR";
    if (t == MATERIAL_COLOR_ADV) tname = "MATERIAL_COLOR_ADV";
    if (t == MATERIAL_TEXTURE)   tname = "MATERIAL_TEXTURE";
    if (t == MATERIAL_PBR)       tname = "MATERIAL_PBR";
    os << "type " << tname << "\n";

    // MATERIAL_PBR : decrit par ses PROPRES champs, et non par sa projection
    // Phong, AVANT la cascade de dynamic_cast qui suit -- celle-ci ne connait
    // que les classes historiques et rendrait « MATERIAL_NONE », faux et non
    // vide. Ce bloc rend ce que le fichier dit, facteurs metallique et rugosite
    // compris, precisement ce que la projection Phong jette.
    if (const MaterialPbr* pbr = dynamic_cast<const MaterialPbr*>(mat))
    {
        const cgpbr::Factors& f = pbr->GetFactors();
        char buf[220];
        std::snprintf(buf, sizeof(buf), "baseColor %g %g %g %g\n",
                      f.baseColor[0], f.baseColor[1], f.baseColor[2], f.baseColor[3]);
        os << buf;
        std::snprintf(buf, sizeof(buf), "emissive %g %g %g\n",
                      f.emissive[0], f.emissive[1], f.emissive[2]);
        os << buf;
        std::snprintf(buf, sizeof(buf), "metallic %g\nroughness %g\n",
                      f.metallic, f.roughness);
        os << buf;
        std::snprintf(buf, sizeof(buf),
                      "normalScale %g\nocclusionStrength %g\nalphaCutoff %g\n",
                      f.normalScale, f.occlusionStrength, f.alphaCutoff);
        os << buf;
        const char* am = (pbr->GetAlphaMode() == cgpbr::AlphaMode::mask)  ? "MASK"
                       : (pbr->GetAlphaMode() == cgpbr::AlphaMode::blend) ? "BLEND"
                                                                          : "OPAQUE";
        os << "alphaMode " << am << "\n";
        os << "doubleSided " << (pbr->IsDoubleSided() ? 1 : 0) << "\n";

        static const char* const kSlotNames[] = {
            "base_color", "normal", "metallic_roughness", "occlusion", "emissive"
        };
        // Table INDEXEE PAR MapSlot : un emplacement ajoute a l'enumeration
        // (clearcoat, specular...) ferait lire kSlotNames hors bornes et
        // passerait un pointeur indetermine a %s, sans un mot du compilateur.
        //
        // ⚠ CE QUE LA GARDE NE COUVRE PAS : elle protege le CARDINAL, pas
        // l'ORDRE. Reordonner MapSlot laisse `count == 5` et fait afficher les
        // mauvais libelles -- defaut cosmetique, invisible a la compilation.
        // L'ordre de cette table doit suivre celui de l'enumeration.
        static_assert (std::size (kSlotNames)
                           == static_cast<std::size_t>(cgpbr::MapSlot::count),
                       "kSlotNames doit couvrir exactement cgpbr::MapSlot");
        for (int s = 0; s < static_cast<int>(cgpbr::MapSlot::count); ++s)
        {
            const cgpbr::MapSlot slot = static_cast<cgpbr::MapSlot>(s);
            if (!pbr->HasMap(slot))
                continue;
            const cgpbr::TextureRef& ref = pbr->GetMap(slot);
            std::snprintf(buf, sizeof(buf), "map %s \"%s\" %s uv%u\n",
                          kSlotNames[s], ref.name.c_str(),
                          (ref.colorSpace == cgpbr::ColorSpace::srgb) ? "sRGB" : "linear",
                          (unsigned)ref.uvSet);
            os << buf;
        }
        os << "OK\n";
        return os.str();
    }

    if (auto* ext = dynamic_cast<MaterialColorExt*>(mat))
    {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "ambient %g %g %g %g\n",
                      ext->m_fAmbient[0], ext->m_fAmbient[1], ext->m_fAmbient[2], ext->m_fAmbient[3]);
        os << buf;
        std::snprintf(buf, sizeof(buf), "diffuse %g %g %g %g\n",
                      ext->m_fDiffuse[0], ext->m_fDiffuse[1], ext->m_fDiffuse[2], ext->m_fDiffuse[3]);
        os << buf;
        std::snprintf(buf, sizeof(buf), "specular %g %g %g %g\n",
                      ext->m_fSpecular[0], ext->m_fSpecular[1], ext->m_fSpecular[2], ext->m_fSpecular[3]);
        os << buf;
        std::snprintf(buf, sizeof(buf), "emission %g %g %g %g\n",
                      ext->m_fEmission[0], ext->m_fEmission[1], ext->m_fEmission[2], ext->m_fEmission[3]);
        os << buf;
        std::snprintf(buf, sizeof(buf), "shininess %g\n", ext->m_fShininess[0]);
        os << buf;
    }
    else if (auto* col = dynamic_cast<MaterialColor*>(mat))
    {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "color %g %g %g %g\n",
                      col->GetFloatRed(), col->GetFloatGreen(), col->GetFloatBlue(), col->GetFloatAlpha());
        os << buf;
    }
    os << "OK\n";
    return os.str();
}

std::string cmdFlip(MyFrame* frame, int n)
{
    if (!frame) return "ERR frame unavailable\n";

    struct Res { int faces; const char* err; };

    const Res result = callOnMain([frame, n]() -> Res {
        const TargetMeshes target = getTargetVMeshes(frame);
        if (!target) return { 0, target.err };
        auto& meshes = target.meshes->GetMeshes();
        if (n < 0 || (size_t)n >= meshes.size()) return { 0, "ERR mesh index out of range\n" };
        Mesh* m = meshes[n];
        if (!m) return { 0, "ERR mesh index out of range\n" };

        for (unsigned int i = 0; i < m->GetNFaces (); ++i)
        {
            auto face = m->FaceAt (i);
            if (!face || face->GetNVertices() != 3) continue;
            const unsigned int v1 = face->GetVertex(1);
            const unsigned int v2 = face->GetVertex(2);
            face->SetVertex(1, v2);
            face->SetVertex(2, v1);
        }
        m->ComputeNormals();
        m->IncrementRevision();

        if (MyGLCanvas* c = frame->GetActiveCanvas())
            c->Refresh(false);

        return Res{ static_cast<int>(m->GetNFaces ()), nullptr };
    });

    if (result.err) return result.err;

    std::ostringstream os;
    os << "flipped " << result.faces << " faces of mesh " << n << "\n";
    os << "OK\n";
    return os.str();
}

// BASE DE COUPE : bascule / interroge l'affichage du tapis quadrille.
//
// On passe par MyFrame::SetCuttingMat plutot que par MyGLCanvas::ChangeCuttingMat.
// Le detour n'est pas gratuit : c'est la fenetre qui remet a jour le bouton de la
// barre d'outils ET la case du menu. Basculer le canvas directement changerait
// bien le rendu, mais laisserait les deux commandes annoncer l'etat inverse --
// une console de mise au point qui desynchronise l'interface qu'elle sert a
// examiner.
std::string cmdCuttingMat(MyFrame* frame, const std::string& arg)
{
    if (!frame) return "ERR frame unavailable\n";

    int want = -1;                      // -1 : bascule
    if (arg == "on")       want = 1;
    else if (arg == "off") want = 0;
    else if (!arg.empty()) return "ERR usage: cuttingmat [on|off]\n";

    struct State { int on; float level; };
    const State st = callOnMain([frame, want]() -> State {
        MyGLCanvas* c = frame->GetActiveCanvas();
        if (!c) return { -1, 0.f };
        frame->SetCuttingMat(want < 0 ? !c->GetCuttingMat() : (want != 0));
        return { c->GetCuttingMat() ? 1 : 0, c->GetCuttingMatLevel() };
    });

    if (st.on < 0) return "ERR no active view\n";

    // La COTE est rapportee avec l'etat : c'est elle qui dit que le tapis suit le
    // modele, et elle doit valoir le minimum Z que `info` annonce.
    std::ostringstream os;
    os << "cuttingmat " << (st.on ? "on" : "off")
       << " (level Z = " << st.level << " mm)\nOK\n";
    return os.str();
}

// RAMENER SUR Z = 0 : deplace le Model N (indice du panneau « Models ») pour que
// le minimum Z de sa bbox tombe sur zero. Rend le deplacement applique, ce qui en
// fait un oracle utilisable depuis un script : la cote annoncee doit valoir
// l'oppose du minimum Z d'avant.
//
// Sans rapport avec la base de coupe : celle-ci se pose d'elle-meme sur le minimum
// Z de la scene visible, donc le modele repose dessus quoi qu'il arrive.
std::string cmdDrop(MyFrame* frame, int n)
{
    if (!frame) return "ERR frame unavailable\n";

    struct Res { int code; float dz; };
    const Res r = callOnMain([frame, n]() -> Res {
        MyGLCanvas* c = frame->GetActiveCanvas();
        if (!c || !c->GetVModels()) return { -1, 0.f };
        Model* mdl = c->GetVModels()->GetModel((size_t)n);
        if (!mdl) return { -2, 0.f };
        return { 0, c->MoveModelToZeroLevel(mdl) };
    });

    if (r.code == -1) return "ERR no model loaded\n";
    if (r.code == -2) return "ERR model index out of range\n";

    std::ostringstream os;
    os << "moved model " << n << " by " << r.dz << " mm in Z\nOK\n";
    return os.str();
}

// NORMALISER : recentre le modele et amene sa plus grande dimension a la taille
// visee par VMeshes::kNormalizedSize. Passe par la fenetre, qui rafraichit en plus
// le panneau des cotes.
std::string cmdNormalize(MyFrame* frame)
{
    if (!frame) return "ERR frame unavailable\n";

    // Normalize porte sur la VUE ENTIERE et non sur un fichier (voir
    // MyGLCanvas::ApplyNormalization) : la garde est donc celle de la scene, et
    // cette commande n'a pas de cible a designer.
    const bool ok = callOnMain([frame]() -> bool {
        MyGLCanvas* c = frame->GetActiveCanvas();
        if (!c || !c->GetVModels() || c->GetVModels()->GetNModels() == 0) return false;
        frame->NormalizeActiveModel();
        return true;
    });

    if (!ok) return "ERR no model loaded\n";
    return "normalized\nOK\n";
}

// SELECTION : designe le modele que les commandes « un fichier » adressent --
// info, mesh, face, vertex, material, flip, et les traitements du menu. Sans
// argument, rend la selection courante. C'est l'equivalent scripte du clic dans
// le panneau « Models », et le seul moyen de lever l'ambiguite d'une vue
// multi-fichiers sans souris.
//
// Passe par MyFrame::SelectModelByIndex plutot que par MyGLCanvas::SetSelectedModel :
// la fenetre remet a jour « Models » (ligne en gras) et « Model information », que
// le canvas seul laisserait sur l'ancienne selection.
std::string cmdSelect(MyFrame* frame, const std::string& arg)
{
    if (!frame) return "ERR frame unavailable\n";

    struct Res { int code; long index; std::size_t count; std::string name; };

    if (arg.empty())
    {
        const Res r = callOnMain([frame]() -> Res {
            MyGLCanvas* c = frame->GetActiveCanvas();
            if (!c || !c->GetVModels()) return { -1, -1, 0, std::string() };
            VModels* scene = c->GetVModels();
            const Model* target = c->GetTargetModel();
            long idx = -1;
            for (std::size_t i = 0; i < scene->GetNModels(); ++i)
                if (scene->GetModel(i) == target) { idx = (long)i; break; }
            return { 0, idx, scene->GetNModels(),
                     target ? target->m_name : std::string() };
        });

        if (r.code == -1) return "ERR no active view\n";

        std::ostringstream os;
        os << "models " << r.count << "\n";
        if (r.index < 0)
            os << "selected none"
               << (r.count > 1 ? "  (several models loaded -- use 'select N')" : "")
               << "\n";
        else
            os << "selected " << r.index << " \"" << r.name << "\"\n";
        os << "OK\n";
        return os.str();
    }

    // VIDER la selection. C'est le seul moyen, sans souris, de placer la vue dans
    // le cas AMBIGU -- plusieurs fichiers, aucun selectionne -- et donc de verifier
    // que les commandes « un fichier » refusent au lieu de designer le modele 0.
    if (arg == "none")
    {
        const bool ok = callOnMain([frame]() -> bool {
            MyGLCanvas* c = frame->GetActiveCanvas();
            if (!c) return false;
            c->SetSelectedModel(nullptr);
            frame->RefreshModelSelection();   // « Models » et « Model information » suivent
            return true;
        });
        if (!ok) return "ERR no active view\n";
        return "selected none\nOK\n";
    }

    char* end = nullptr;
    const long n = std::strtol(arg.c_str(), &end, 10);
    if (!end || *end != '\0')
        return "ERR usage: select [N|none]   (N = model index, as in 'drop N')\n";

    const Res r = callOnMain([frame, n]() -> Res {
        MyGLCanvas* c = frame->GetActiveCanvas();
        if (!c || !c->GetVModels()) return { -1, n, 0, std::string() };
        const std::size_t count = c->GetVModels()->GetNModels();
        if (n < 0 || !frame->SelectModelByIndex((std::size_t)n))
            return { -2, n, count, std::string() };
        const Model* sel = c->GetSelectedModel();
        return { 0, n, count, sel ? sel->m_name : std::string() };
    });

    if (r.code == -1) return "ERR no model loaded\n";
    if (r.code == -2)
    {
        std::ostringstream os;
        os << "ERR model index " << n << " out of range (scene has " << r.count
           << (r.count == 1 ? " model" : " models") << ", valid 0.."
           << (r.count ? r.count - 1 : 0) << ")\n";
        return os.str();
    }

    std::ostringstream os;
    os << "selected " << r.index << " \"" << r.name << "\"\nmodels " << r.count << "\nOK\n";
    return os.str();
}

std::string cmdOpen(MyFrame* frame, const std::string& path)
{
    if (!frame) return "ERR frame unavailable\n";

    bool ok = callOnMain([frame, &path]() -> bool {
        const wxString wxPath = wxString::FromUTF8(path.c_str());
        if (!wxFileExists(wxPath))
            return false;
        frame->LoadModelFile(wxPath);
        return true;
    });

    if (!ok)
        return "ERR open failed (file not found?): " + path + "\n";
    return "opened " + path + "\nOK\n";
}

// AJOUTER un fichier a la vue COURANTE, sans remplacer la scene ni renormaliser :
// le pendant scripte du glisser-deposer depuis le panneau des fichiers.
//
// C'est l'instrument qui manquait : sans lui, une scene multi-fichiers -- le cas
// d'usage meme du pivot par modele -- n'etait pas constructible depuis la console,
// donc « la bonne cible est-elle choisie ? » n'etait pas mesurable. `open` ouvre
// un ONGLET, il ne superpose pas.
std::string cmdAppend(MyFrame* frame, const std::string& path)
{
    if (!frame) return "ERR frame unavailable\n";

    struct Res { int code; std::size_t count; };
    const Res r = callOnMain([frame, &path]() -> Res {
        MyGLCanvas* c = frame->GetActiveCanvas();
        if (!c) return { -1, 0 };
        const wxString wxPath = wxString::FromUTF8(path.c_str());
        if (!wxFileExists(wxPath))
            return { -2, 0 };
        if (!c->AppendModel(wxPath))
            return { -3, 0 };
        return { 0, c->GetVModels() ? c->GetVModels()->GetNModels() : 0 };
    });

    if (r.code == -1) return "ERR no active view (open a model first)\n";
    if (r.code == -2) return "ERR append failed (file not found): " + path + "\n";
    if (r.code == -3) return "ERR append failed (unsupported or empty file): " + path + "\n";

    std::ostringstream os;
    os << "appended " << path << "\nmodels " << r.count << "\nOK\n";
    return os.str();
}

std::string cmdScreenshot(MyFrame* frame, const std::string& path)
{
    if (!frame) return "ERR frame unavailable\n";

    struct Res { bool ok; int w; int h; };

    Res r = callOnMain([frame, &path]() -> Res {
        MyGLCanvas* c = frame->GetActiveCanvas();
        if (!c) return { false, 0, 0 };
        int w = 0, h = 0;
        c->GetClientSize(&w, &h);
        const wxString wxPath = wxString::FromUTF8(path.c_str());
        const bool ok = c->SaveScreenshot(wxPath);
        return { ok, w, h };
    });

    if (!r.ok)
    {
        std::ostringstream os;
        os << "ERR screenshot failed (path=" << path << ")\n";
        return os.str();
    }
    std::ostringstream os;
    os << "written " << r.w << "x" << r.h << " RGB -> " << path << "\n";
    os << "OK\n";
    return os.str();
}

// ---------------------------------------------------------------------------
//  HARNAIS DE CAPTURES : camera deterministe, bascules d'affichage, controle GL
// ---------------------------------------------------------------------------
// Une capture de reference n'est un oracle que si elle est reproductible. Il
// faut donc pouvoir poser exactement le meme point de vue et les memes options
// d'affichage d'une execution a l'autre -- ce que la souris et les menus ne
// permettent pas. Avec `open`, `screenshot` et ces trois commandes, un script
// peut balayer N modeles x M reglages et comparer aux images de reference.
//
// C'est aussi le seul filet de ce module : cgre n'a aucun test, et la seule
// facon de juger un rendu reste de le regarder.

std::string cmdCamera(MyFrame* frame, const std::string& args)
{
    if (!frame) return "ERR frame unavailable\n";

    std::istringstream iss(args);
    std::string sub;
    iss >> sub;

    if (sub.empty() || sub == "show")
    {
        MyGLCanvas::CameraInfo info;
        const bool ok = callOnMain([frame, &info]() -> bool {
            MyGLCanvas* c = frame->GetActiveCanvas();
            return c && c->GetCameraInfo(info);
        });
        if (!ok) return "ERR no active view\n";

        // Une valeur par ligne, « cle valeurs » : un script lit un champ sans
        // decouper une phrase. L'ancienne forme (« camera zoom = Z ») n'avait
        // aucun consommateur -- verifie sur les scripts du depot.
        std::ostringstream os;
        os << "pivot "        << info.pivot[0] << " " << info.pivot[1] << " " << info.pivot[2] << "\n";
        os << "eye "          << info.eye[0]   << " " << info.eye[1]   << " " << info.eye[2]   << "\n";
        os << "distance "     << info.distance << "\n";
        os << "zoom "         << -info.distance << "\n";
        os << "near "         << info.zNear << "\n";
        os << "far "          << info.zFar  << "\n";
        // Les deux spheres : celle de PROFONDEUR (centre + rayon) fixe near/far,
        // celle de CADRAGE (rayon seul, centree sur le pivot) borne le dolly.
        // Le centre de profondeur n'est pas l'origine du monde : sans lui, un
        // script ne peut pas verifier que les plans encadrent bien la scene.
        os << "scene_center "   << info.sceneCenter[0] << " " << info.sceneCenter[1]
                                << " " << info.sceneCenter[2] << "\n";
        os << "scene_radius "   << info.sceneRadius << "\n";
        os << "framing_radius " << info.framingRadius << "\n";
        os << "fov_y_deg "      << info.fovYDeg << "\n";
        os << "viewport "     << info.viewportWidth << " " << info.viewportHeight << "\n";
        os << "OK\n";
        return os.str();
    }

    if (sub == "reset")
    {
        const bool ok = callOnMain([frame]() -> bool {
            MyGLCanvas* c = frame->GetActiveCanvas();
            if (!c) return false;
            c->ResetCamera();
            return true;
        });
        return ok ? "camera reset\nOK\n" : "ERR no active view\n";
    }

    if (sub == "azel")
    {
        float az = 0.f, el = 0.f;
        iss >> az >> el;
        if (iss.fail()) return "ERR usage: camera azel AZ EL   (degrees)\n";
        const bool ok = callOnMain([frame, az, el]() -> bool {
            MyGLCanvas* c = frame->GetActiveCanvas();
            if (!c) return false;
            c->SetCameraOrientation(az, el);
            return true;
        });
        if (!ok) return "ERR no active view\n";
        std::ostringstream os;
        os << "camera azel " << az << " " << el << "\nOK\n";
        return os.str();
    }

    if (sub == "zoom")
    {
        float z = 0.f;
        iss >> z;
        if (iss.fail()) return "ERR usage: camera zoom Z   (negative = further away)\n";
        const bool ok = callOnMain([frame, z]() -> bool {
            MyGLCanvas* c = frame->GetActiveCanvas();
            if (!c) return false;
            c->SetCameraZoom(z);
            return true;
        });
        if (!ok) return "ERR no active view\n";
        std::ostringstream os;
        os << "camera zoom " << z << "\nOK\n";
        return os.str();
    }

    // Forme POSITIVE de `camera zoom`. Les deux coexistent : le signe de `zoom`
    // est un contrat documente (negatif = plus loin), et le changer casserait
    // tout appelant ecrit contre lui.
    if (sub == "dist")
    {
        float d = 0.f;
        iss >> d;
        if (iss.fail() || !(d > 0.f))
            return "ERR usage: camera dist D   (D > 0, world units; same as 'camera zoom -D')\n";

        // La distance appliquee peut differer de celle demandee : set_zoom la
        // borne contre le rayon de scene. C'est elle qu'on rapporte.
        const float applied = callOnMain([frame, d]() -> float {
            MyGLCanvas* c = frame->GetActiveCanvas();
            if (!c) return 0.f;
            c->SetCameraZoom(-d);
            return -c->GetCameraZoom();
        });
        if (applied <= 0.f) return "ERR no active view\n";
        std::ostringstream os;
        os << "camera dist " << applied << "\nOK\n";
        return os.str();
    }

    // PIVOT : le centre d'orbite. `camera pivot X Y Z` le pose sans toucher a la
    // distance -- choisir autour de quoi on tourne n'est pas choisir de combien
    // on recule. `camera pivot auto` recadre sur la bbox agregee visible, ce que
    // fait aussi `camera reset`, mais SANS remettre l'orientation a zero.
    if (sub == "pivot")
    {
        std::string mode;
        iss >> mode;

        if (mode == "auto")
        {
            const bool ok = callOnMain([frame]() -> bool {
                MyGLCanvas* c = frame->GetActiveCanvas();
                return c && c->FrameVisibleScene();
            });
            if (!ok) return "ERR no active view, or no visible model to frame\n";
            return "camera pivot auto\nOK\n";
        }

        // RECENTRER SUR UN MODELE, par son indice dans la scene -- le meme que
        // `drop N` et que le panneau "Models". Pendant scripte de la touche F.
        //
        // Un indice hors bornes est une ERREUR explicite et non un repli sur la
        // scene entiere : un script qui se trompe d'indice doit s'en apercevoir,
        // pas recevoir un cadrage plausible.
        if (mode == "model")
        {
            long   n     = -1;
            bool   parsed = false;
            {
                std::string tok;
                iss >> tok;
                if (!tok.empty())
                {
                    char* end = nullptr;
                    n = std::strtol(tok.c_str(), &end, 10);
                    parsed = (end && *end == '\0');
                }
            }
            if (!parsed)
                return "ERR usage: camera pivot model N   (N = model index, as in 'drop N')\n";

            struct Res { int code; std::size_t count; float r; };
            const Res r = callOnMain([frame, n]() -> Res {
                MyGLCanvas* c = frame->GetActiveCanvas();
                if (!c || !c->GetVModels()) return { -1, 0, 0.f };
                const std::size_t count = c->GetVModels()->GetNModels();
                if (n < 0 || (std::size_t)n >= count) return { -2, count, 0.f };
                if (!c->FrameModelByIndex((std::size_t)n)) return { -3, count, 0.f };
                MyGLCanvas::CameraInfo info;
                c->GetCameraInfo(info);
                return { 0, count, info.framingRadius };
            });

            if (r.code == -1) return "ERR no model loaded\n";
            if (r.code == -2)
            {
                std::ostringstream os;
                os << "ERR model index " << n << " out of range (scene has " << r.count
                   << (r.count == 1 ? " model" : " models") << ", valid 0.."
                   << (r.count ? r.count - 1 : 0) << ")\n";
                return os.str();
            }
            if (r.code == -3) return "ERR model has no geometry to frame (empty or degenerate bbox)\n";

            std::ostringstream os;
            os << "camera pivot model " << n << "\nframing_radius " << r.r << "\nOK\n";
            return os.str();
        }

        // CADRER SUR LA BASE DE COUPE. Le tapis reste hors de la sphere de
        // cadrage : on le vise, on ne l'y fait pas entrer (Q-2).
        if (mode == "mat")
        {
            struct Res { int code; float r; };
            const Res r = callOnMain([frame]() -> Res {
                MyGLCanvas* c = frame->GetActiveCanvas();
                if (!c) return { -1, 0.f };
                if (!c->FrameCuttingMat()) return { -2, 0.f };
                MyGLCanvas::CameraInfo info;
                c->GetCameraInfo(info);
                return { 0, info.framingRadius };
            });
            if (r.code == -1) return "ERR no active view\n";
            if (r.code == -2) return "ERR cutting mat has no extent\n";
            std::ostringstream os;
            os << "camera pivot mat\nframing_radius " << r.r << "\nOK\n";
            return os.str();
        }

        // POSER LE PIVOT SUR LA SURFACE SOUS UN PIXEL -- le pendant scripte de
        // Alt + clic gauche.
        //
        // Elle appelle SetPivotFromPixel, le point d'entree du geste, et NON une
        // replique de son calcul : une replique mesurerait sa propre exactitude,
        // pas celle du geste. Le `t` et le point d'impact sont rendus parce que
        // le critere les demande -- « le pivot est sur le rayon de
        // `camera unproject PX PY`, a la distance t » ne se verifie pas sans eux.
        if (mode == "pick")
        {
            float px = 0.f, py = 0.f;
            iss >> px >> py;
            if (iss.fail())
                return "ERR usage: camera pivot pick PX PY   (viewport pixels, y downwards)\n";

            // Le nom du modele touche est lu DANS le lambda, sur le thread
            // principal : la scene peut etre remplacee entre deux commandes, et
            // un Model* rapporte ici serait deja pendouillant.
            struct Reply { MyGLCanvas::PivotPick pick; std::string hit; };
            const Reply r = callOnMain([frame, px, py]() -> Reply {
                Reply out;
                MyGLCanvas* c = frame->GetActiveCanvas();
                if (!c) return out;                    // status reste NoView
                out.pick = c->SetPivotFromPixel(px, py);
                if (out.pick.model) out.hit = out.pick.model->m_name;
                return out;
            });

            std::ostringstream os;
            switch (r.pick.status)
            {
            case MyGLCanvas::PivotPickStatus::NoView:
                return "ERR no active view, or viewport is empty\n";
            case MyGLCanvas::PivotPickStatus::OutOfViewport:
                os << "ERR pixel " << px << " " << py << " is outside the viewport (0.."
                   << r.pick.viewportWidth << " x 0.." << r.pick.viewportHeight << ")\n";
                return os.str();
            case MyGLCanvas::PivotPickStatus::NoHit:
                os << "ERR no surface under pixel " << px << " " << py
                   << " (pivot unchanged)\n";
                return os.str();
            default:
                break;
            }

            os << "pixel " << std::fixed << std::setprecision(3) << px << " " << py << "\n";
            os.unsetf(std::ios::floatfield);
            os << std::setprecision(6);
            os << "pivot " << r.pick.point[0] << " " << r.pick.point[1] << " "
               << r.pick.point[2] << "\n";
            os << "t " << r.pick.t << "\n";
            os << "hit " << (r.hit.empty() ? std::string("unnamed") : r.hit) << "\n";
            os << "viewport " << r.pick.viewportWidth << " " << r.pick.viewportHeight << "\n";
            os << "OK\n";
            return os.str();
        }

        // Relecture depuis le debut de l'argument : `mode` a deja consomme X.
        std::istringstream coords(args);
        std::string skip;
        float x = 0.f, y = 0.f, z = 0.f;
        coords >> skip >> x >> y >> z;
        if (coords.fail())
            return "ERR usage: camera pivot X Y Z | camera pivot auto | "
                   "camera pivot model N | camera pivot mat | "
                   "camera pivot pick PX PY\n";

        const bool ok = callOnMain([frame, x, y, z]() -> bool {
            MyGLCanvas* c = frame->GetActiveCanvas();
            return c && c->SetCameraPivot(x, y, z);
        });
        if (!ok) return "ERR no active view\n";
        std::ostringstream os;
        os << "camera pivot " << x << " " << y << " " << z << "\nOK\n";
        return os.str();
    }

    // PANORAMIQUE scripte, en PIXELS ecran. Il emprunte le meme point d'entree
    // que le glisser du bouton du milieu, donc le critere « le point sous le
    // curseur y reste » porte sur la loi reelle et non sur une replique.
    if (sub == "pan")
    {
        float dx = 0.f, dy = 0.f;
        iss >> dx >> dy;
        if (iss.fail())
            return "ERR usage: camera pan DX DY   (screen pixels, y downwards)\n";

        const bool ok = callOnMain([frame, dx, dy]() -> bool {
            MyGLCanvas* c = frame->GetActiveCanvas();
            return c && c->PanCamera(dx, dy);
        });
        if (!ok) return "ERR no active view\n";
        std::ostringstream os;
        os << "camera pan " << dx << " " << dy << "\nOK\n";
        return os.str();
    }

    // PROJECTION monde -> pixels : l'instrument des criteres exprimes en pixels.
    // Origine en HAUT A GAUCHE, comme les evenements souris.
    if (sub == "project")
    {
        float x = 0.f, y = 0.f, z = 0.f;
        iss >> x >> y >> z;
        if (iss.fail())
            return "ERR usage: camera project X Y Z   (world point -> viewport pixels)\n";

        const float world[3] = { x, y, z };
        const MyGLCanvas::ProjectedPoint p = callOnMain([frame, &world]() -> MyGLCanvas::ProjectedPoint {
            MyGLCanvas* c = frame->GetActiveCanvas();
            return c ? c->ProjectPoint(world) : MyGLCanvas::ProjectedPoint();
        });

        std::ostringstream os;
        switch (p.status)
        {
        case MyGLCanvas::ProjectStatus::NoCamera:
            return "ERR no active view\n";
        case MyGLCanvas::ProjectStatus::EmptyViewport:
            return "ERR viewport is empty (window minimised?)\n";
        case MyGLCanvas::ProjectStatus::BehindEye:
            os << "ERR point is behind the eye (clip w = " << p.eyeDepth << ")\n";
            return os.str();
        case MyGLCanvas::ProjectStatus::ClippedByNear:
            os << "ERR point is in front of the near plane (eye depth = " << p.eyeDepth
               << ", ndc z = " << p.ndcZ << ")\n";
            return os.str();
        default:
            break;
        }

        os << "world " << x << " " << y << " " << z << "\n";
        os << "pixel " << std::fixed << std::setprecision(3)
           << p.pixelX << " " << p.pixelY << "\n";
        os << "center " << 0.5f * p.viewportWidth << " " << 0.5f * p.viewportHeight << "\n";
        os.unsetf(std::ios::floatfield);
        os << std::setprecision(6);
        os << "ndc " << p.ndcX << " " << p.ndcY << " " << p.ndcZ << "\n";
        os << "depth " << p.eyeDepth << "\n";
        os << "viewport " << p.viewportWidth << " " << p.viewportHeight << "\n";
        if (p.status == MyGLCanvas::ProjectStatus::OkBeyondFar)
            os << "note beyond the far plane (not drawn)\n";
        os << "OK\n";
        return os.str();
    }

    // DEPROJECTION pixels -> rayon monde : l'INVERSE de `camera project`, et le
    // seul moyen d'exercer le picking sans souris.
    //
    // Deux criteres reposent dessus :
    //   - l'aller-retour : le rayon rendu pour le pixel que `camera project`
    //     donne d'un point monde doit repasser par ce point. Controle NUMERIQUE,
    //     scriptable, qui ne depend d'aucun pixel de rendu ;
    //   - la fraicheur : une orientation posee par `camera azel` ne declenche
    //     qu'un Refresh(false), qui PLANIFIE la peinture sans l'executer. Un
    //     rayon construit depuis l'etat GL lirait l'image precedente.
    if (sub == "unproject")
    {
        float px = 0.f, py = 0.f;
        iss >> px >> py;
        if (iss.fail())
            return "ERR usage: camera unproject PX PY   (viewport pixels, y downwards)\n";

        // Le nom du modele touche est lu SUR LE THREAD PRINCIPAL, avec le rayon :
        // la scene peut etre remplacee entre deux commandes, et un Model* rendu
        // ici serait deja pendouillant.
        struct Reply { MyGLCanvas::PickedRay ray; std::string hit; };
        const Reply r = callOnMain([frame, px, py]() -> Reply {
            Reply out;
            MyGLCanvas* c = frame->GetActiveCanvas();
            if (!c) return out;
            out.ray = c->UnprojectPixel(px, py);
            if (out.ray.model) out.hit = out.ray.model->m_name;
            return out;
        });

        if (!r.ray.valid)
            return "ERR no active view, or viewport is empty\n";

        std::ostringstream os;
        os << "pixel " << std::fixed << std::setprecision(3) << px << " " << py << "\n";
        os.unsetf(std::ios::floatfield);
        os << std::setprecision(6);
        os << "origin "    << r.ray.origin[0]    << " " << r.ray.origin[1]    << " " << r.ray.origin[2]    << "\n";
        os << "direction " << r.ray.direction[0] << " " << r.ray.direction[1] << " " << r.ray.direction[2] << "\n";
        os << "hit " << (r.hit.empty() ? std::string("none") : r.hit) << "\n";
        os << "viewport " << r.ray.viewportWidth << " " << r.ray.viewportHeight << "\n";
        os << "OK\n";
        return os.str();
    }

    return "ERR usage: camera [show|reset|azel AZ EL|zoom Z|dist D|pivot X Y Z|pivot auto|"
           "pivot model N|pivot mat|pivot pick PX PY|pan DX DY|project X Y Z|"
           "unproject PX PY]\n";
}

std::string cmdShading(MyFrame* frame, const std::string& arg)
{
    if (!frame) return "ERR frame unavailable\n";

    // -1 : pas de changement, on se contente de rapporter.
    int want = -1;
    if      (arg == "materials")    want = (int)CG_shading_mode::Materials;
    else if (arg == "neutral")      want = (int)CG_shading_mode::Neutral;
    else if (arg == "vertexcolors") want = (int)CG_shading_mode::VertexColors;
    else if (!arg.empty())
        return "ERR usage: shading [materials|neutral|vertexcolors]\n";

    const int mode = callOnMain([frame, want]() -> int {
        MyGLCanvas* c = frame->GetActiveCanvas();
        if (!c) return -1;
        if (want >= 0) c->SetShadingMode((CG_shading_mode)want);
        return (int)c->GetShadingMode();
    });

    if (mode < 0) return "ERR no active view\n";

    const char* name = (mode == (int)CG_shading_mode::Neutral)      ? "neutral"
                     : (mode == (int)CG_shading_mode::VertexColors) ? "vertexcolors"
                                                                    : "materials";
    return std::string("shading ") + name + "\nOK\n";
}

std::string cmdToggle(MyFrame* frame, const std::string& args)
{
    if (!frame) return "ERR frame unavailable\n";

    std::istringstream iss(args);
    std::string what, state;
    iss >> what >> state;

    if (what.empty())
        return "ERR usage: toggle fill|wireframe|points|warning|repere|grid|lighting [on|off]\n";

    int want = -1;                      // -1 : bascule
    if      (state == "on")  want = 1;
    else if (state == "off") want = 0;
    else if (!state.empty())
        return "ERR usage: toggle WHAT [on|off]\n";

    // -1 : vue absente ; -2 : nom inconnu.
    const int result = callOnMain([frame, what, want]() -> int {
        MyGLCanvas* c = frame->GetActiveCanvas();
        if (!c) return -1;

        // `Change*` bascule ; pour forcer un etat on ne bascule que si l'etat
        // courant differe de celui demande. C'est ce qui rend un script
        // idempotent, donc rejouable.
        auto apply = [want](bool current, auto change) {
            if (want < 0 || (want != 0) != current) change();
        };

        if      (what == "fill")      apply(c->GetFill(),      [c]{ c->ChangeFill(); });
        else if (what == "wireframe") apply(c->GetWireframe(), [c]{ c->ChangeWireframe(); });
        else if (what == "points")    apply(c->GetPoint(),     [c]{ c->ChangePoint(); });
        else if (what == "warning")   apply(c->GetWarning(),   [c]{ c->ChangeWarning(); });
        else if (what == "repere")    apply(c->GetRepere(),    [c]{ c->ChangeRepere(); });
        else if (what == "grid")      apply(c->GetGrid(),      [c]{ c->ChangeGrid(); });
        else if (what == "lighting")  c->SetLighting(want < 0 ? !c->GetLighting() : (want != 0));
        else return -2;

        if      (what == "fill")      return c->GetFill()      ? 1 : 0;
        else if (what == "wireframe") return c->GetWireframe() ? 1 : 0;
        else if (what == "points")    return c->GetPoint()     ? 1 : 0;
        else if (what == "warning")   return c->GetWarning()   ? 1 : 0;
        else if (what == "repere")    return c->GetRepere()    ? 1 : 0;
        else if (what == "grid")      return c->GetGrid()      ? 1 : 0;
        return c->GetLighting() ? 1 : 0;
    });

    if (result == -1) return "ERR no active view\n";
    if (result == -2)
        return "ERR unknown toggle: " + what
             + " (fill|wireframe|points|warning|repere|grid|lighting)\n";

    return "toggle " + what + " " + (result ? "on" : "off") + "\nOK\n";
}

// Controle d'erreur GL de cgre. Eteint par defaut : un glGetError par point de
// controle peut serialiser le pipeline. On l'allume le temps d'un diagnostic,
// les erreurs partent alors dans la fenetre « Logging Window ».
std::string cmdGlCheck(const std::string& arg)
{
    if      (arg == "on")  cgre::SetGlCheckEnabled(true);
    else if (arg == "off") cgre::SetGlCheckEnabled(false);
    else if (!arg.empty()) return "ERR usage: glcheck [on|off]\n";

    return std::string("glcheck ") + (cgre::IsGlCheckEnabled() ? "on" : "off") + "\nOK\n";
}

// Bascule du rendu de surface par shader. ACTIF au demarrage depuis la
// validation par captures de reference ; `off` restaure le pipeline fixe, ce qui
// reste le seul moyen de comparer A/B dans une meme session -- cgre n'ayant
// aucun test de rendu.
std::string cmdShader(MyFrame* frame, const std::string& arg)
{
    if (!frame) return "ERR frame unavailable\n";

    int want = -1;
    if      (arg == "on")  want = 1;
    else if (arg == "off") want = 0;
    else if (!arg.empty()) return "ERR usage: shader [on|off]\n";

    // Sur le fil principal : la bascule doit etre suivie d'un repaint, et la
    // construction du programme exige un contexte GL courant.
    const bool on = callOnMain([frame, want]() -> bool {
        if (want >= 0) cgre::SetSurfaceShaderEnabled(want != 0);
        if (MyGLCanvas* c = frame->GetActiveCanvas()) c->Refresh(false);
        return cgre::IsSurfaceShaderEnabled();
    });

    return std::string("shader ") + (on ? "on" : "off") + "\nOK\n";
}

std::string cmdHelp()
{
    return
        "Commands:\n"
        "  select [N|none]            choose the model the per-file commands act on\n"
        "                             (info/mesh/face/vertex/material/flip, and the\n"
        "                             Treatments menu). N is the index of the 'Models'\n"
        "                             panel, as in 'drop N'. No argument: report the\n"
        "                             current target. A view holding a SINGLE model\n"
        "                             needs no selection; with several models and none\n"
        "                             selected, those commands refuse rather than pick\n"
        "                             the first one. 'select none' clears it.\n"
        "  info                       global stats of the TARGET model\n"
        "  mesh N                     mesh N of the target model: name, counts, bbox\n"
        "  face N F                   face F of mesh N: vertex indices,\n"
        "                             positions, normal, material id\n"
        "  vertex N V                 vertex V of mesh N: pos, normal, uv\n"
        "  material N M               material M of mesh N: type, colors\n"
        "  flip N                     flip winding of every face of mesh N\n"
        "  open PATH                  load a model file into a new tab\n"
        "  append PATH                add a model file to the CURRENT view, world\n"
        "                             coordinates kept (no normalisation) -- the\n"
        "                             scripted form of the drag & drop\n"
        "  cuttingmat [on|off]        show / hide the cutting mat (no arg: toggle)\n"
        "  drop N                     move model N down onto the Z = 0 plane\n"
        "  normalize                  recentre + scale the active model to the target size\n"
        "  screenshot PATH            save current viewport to PATH as PNG\n"
        "  camera [show|reset|azel AZ EL|zoom Z|dist D|pivot X Y Z|pivot auto|\n"
        "          pivot model N|pivot mat|pivot pick PX PY|pan DX DY|\n"
        "          project X Y Z|unproject PX PY]\n"
        "                             deterministic camera, for reference renders.\n"
        "                             show: pivot, eye, distance, near/far, depth\n"
        "                             sphere (centre + radius), framing radius,\n"
        "                             fov and viewport -- one per line.\n"
        "                             zoom Z: signed (negative = further away).\n"
        "                             dist D: same, positive form (D > 0).\n"
        "                             pivot X Y Z: orbit centre, distance unchanged.\n"
        "                             pivot auto: reframe on the visible models.\n"
        "                             pivot model N: reframe on THAT model alone --\n"
        "                             pivot at its bbox centre, distance from ITS\n"
        "                             half-diagonal (key F does the same on the\n"
        "                             selected model). Out of range is an error.\n"
        "                             pivot mat: reframe on the cutting mat.\n"
        "                             pivot pick PX PY: put the pivot on the SURFACE\n"
        "                             point under that pixel, DISTANCE UNCHANGED --\n"
        "                             the scripted form of Alt + left click. Replies\n"
        "                             with the pivot, the ray parameter t and the\n"
        "                             model hit. A ray that hits nothing is an error\n"
        "                             and leaves the pivot where it was.\n"
        "                             pan DX DY: screen-pixel pan. It MOVES THE\n"
        "                             PIVOT, so the point under the cursor stays\n"
        "                             under it at any distance.\n"
        "                             project X Y Z: world point -> viewport pixels,\n"
        "                             ORIGIN TOP-LEFT, y downwards, as mouse events.\n"
        "                             Errors out when the point is behind the eye or\n"
        "                             in front of the near plane.\n"
        "                             unproject PX PY: the INVERSE -- viewport pixel\n"
        "                             -> world ray (origin + direction) and the Model\n"
        "                             it hits, same convention, same camera source.\n"
        "  shading [materials|neutral|vertexcolors]\n"
        "                             shading mode (no arg: report current)\n"
        "  toggle WHAT [on|off]       fill|wireframe|points|warning|repere|grid|lighting\n"
        "                             (no on/off: flip it)\n"
        "  glcheck [on|off]           cgre OpenGL error checking -> Logging Window\n"
        "  shader [on|off]            surface rendering through the GLSL program\n"
        "                             (on by default; 'off' restores the fixed pipeline)\n"
        "  help                       this help\n"
        "  quit                       close the connection\n"
        "OK\n";
}

// ----- dispatcher -----------------------------------------------------------

cgnet::Reply dispatch(const std::string& rawLine, MyFrame* frame)
{
    const std::string line = trim(rawLine);
    if (line.empty()) return { "OK\n", false };

    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "info")   return { cmdInfo(frame), false };
    if (cmd == "select")
    {
        std::string arg;
        std::getline(iss, arg);
        return { cmdSelect(frame, trim(arg)), false };
    }
    if (cmd == "help")   return { cmdHelp(), false };
    if (cmd == "quit")   return { "bye\nOK\n", true };

    if (cmd == "mesh")
    {
        int n = -1; iss >> n;
        if (iss.fail()) return { "ERR usage: mesh N\n", false };
        return { cmdMesh(frame, n), false };
    }
    if (cmd == "face")
    {
        int n = -1, f = -1; iss >> n >> f;
        if (iss.fail()) return { "ERR usage: face N F\n", false };
        return { cmdFace(frame, n, f), false };
    }
    if (cmd == "vertex")
    {
        int n = -1, v = -1; iss >> n >> v;
        if (iss.fail()) return { "ERR usage: vertex N V\n", false };
        return { cmdVertex(frame, n, v), false };
    }
    if (cmd == "material")
    {
        int n = -1, m = -1; iss >> n >> m;
        if (iss.fail()) return { "ERR usage: material N M\n", false };
        return { cmdMaterial(frame, n, m), false };
    }
    if (cmd == "flip")
    {
        int n = -1; iss >> n;
        if (iss.fail()) return { "ERR usage: flip N\n", false };
        return { cmdFlip(frame, n), false };
    }
    if (cmd == "screenshot")
    {
        std::string path;
        std::getline(iss, path);
        path = trim(path);
        if (path.empty()) return { "ERR usage: screenshot PATH\n", false };
        return { cmdScreenshot(frame, path), false };
    }
    if (cmd == "cuttingmat")
    {
        std::string arg;
        std::getline(iss, arg);
        return { cmdCuttingMat(frame, trim(arg)), false };
    }
    if (cmd == "camera")
    {
        std::string args;
        std::getline(iss, args);
        return { cmdCamera(frame, trim(args)), false };
    }
    if (cmd == "shading")
    {
        std::string arg;
        std::getline(iss, arg);
        return { cmdShading(frame, trim(arg)), false };
    }
    if (cmd == "toggle")
    {
        std::string args;
        std::getline(iss, args);
        return { cmdToggle(frame, trim(args)), false };
    }
    if (cmd == "shader")
    {
        std::string arg;
        std::getline(iss, arg);
        return { cmdShader(frame, trim(arg)), false };
    }
    if (cmd == "glcheck")
    {
        std::string arg;
        std::getline(iss, arg);
        return { cmdGlCheck(trim(arg)), false };
    }
    if (cmd == "normalize")
        return { cmdNormalize(frame), false };
    if (cmd == "drop")
    {
        int n = -1; iss >> n;
        if (iss.fail()) return { "ERR usage: drop N\n", false };
        return { cmdDrop(frame, n), false };
    }
    if (cmd == "open")
    {
        std::string path;
        std::getline(iss, path);
        path = trim(path);
        if (path.empty()) return { "ERR usage: open PATH\n", false };
        return { cmdOpen(frame, path), false };
    }
    if (cmd == "append")
    {
        std::string path;
        std::getline(iss, path);
        path = trim(path);
        if (path.empty()) return { "ERR usage: append PATH\n", false };
        return { cmdAppend(frame, path), false };
    }

    return { "ERR unknown command: " + cmd + " (try 'help')\n", false };
}

} // namespace

// ----- RemoteConsole impl ---------------------------------------------------

RemoteConsole& RemoteConsole::Get()
{
    static RemoteConsole inst;
    return inst;
}

void RemoteConsole::Start(unsigned short port, MyFrame* frame)
{
    if (m_console.IsRunning())
        return;
    m_frame = frame;
    MyFrame* capturedFrame = frame;
    m_console.Start(port,
                    "sinaia remote console 1.0 \xE2\x80\x94 type 'help'\n",
                    [capturedFrame](const std::string& line) -> cgnet::Reply {
                        return dispatch(line, capturedFrame);
                    });
}

void RemoteConsole::Stop()
{
    m_console.Stop();
}
