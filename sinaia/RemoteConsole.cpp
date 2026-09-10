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
#include <cstring>
#include <future>
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

VMeshes* getActiveVMeshes(MyFrame* frame)
{
    if (!frame) return nullptr;
    MyGLCanvas* canvas = frame->GetActiveCanvas();
    return canvas ? canvas->GetVMeshes() : nullptr;
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
    VMeshes* vm = getActiveVMeshes(frame);
    if (!vm) return "ERR no model loaded\n";

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
    VMeshes* vm = getActiveVMeshes(frame);
    if (!vm) return "ERR no model loaded\n";
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
    VMeshes* vm = getActiveVMeshes(frame);
    if (!vm) return "ERR no model loaded\n";
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
    VMeshes* vm = getActiveVMeshes(frame);
    if (!vm) return "ERR no model loaded\n";
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
    VMeshes* vm = getActiveVMeshes(frame);
    if (!vm) return "ERR no model loaded\n";
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

    int result = callOnMain([frame, n]() -> int {
        VMeshes* vm = getActiveVMeshes(frame);
        if (!vm) return -1;
        auto& meshes = vm->GetMeshes();
        if (n < 0 || (size_t)n >= meshes.size()) return -2;
        Mesh* m = meshes[n];
        if (!m) return -2;

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

        return static_cast<int>(m->GetNFaces ());
    });

    if (result == -1) return "ERR no model loaded\n";
    if (result == -2) return "ERR mesh index out of range\n";

    std::ostringstream os;
    os << "flipped " << result << " faces of mesh " << n << "\n";
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

    const bool ok = callOnMain([frame]() -> bool {
        MyGLCanvas* c = frame->GetActiveCanvas();
        if (!c || !c->GetVMeshes()) return false;
        frame->NormalizeActiveModel();
        return true;
    });

    if (!ok) return "ERR no model loaded\n";
    return "normalized\nOK\n";
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
        const float zoom = callOnMain([frame]() -> float {
            MyGLCanvas* c = frame->GetActiveCanvas();
            return c ? c->GetCameraZoom() : 0.f;
        });
        std::ostringstream os;
        os << "camera zoom = " << zoom << "\nOK\n";
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

    return "ERR usage: camera [show|reset|azel AZ EL|zoom Z]\n";
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
        "  info                       global stats of the active VMeshes\n"
        "  mesh N                     mesh N: name, counts, bbox\n"
        "  face N F                   face F of mesh N: vertex indices,\n"
        "                             positions, normal, material id\n"
        "  vertex N V                 vertex V of mesh N: pos, normal, uv\n"
        "  material N M               material M of mesh N: type, colors\n"
        "  flip N                     flip winding of every face of mesh N\n"
        "  open PATH                  load a model file into a new tab\n"
        "  cuttingmat [on|off]        show / hide the cutting mat (no arg: toggle)\n"
        "  drop N                     move model N down onto the Z = 0 plane\n"
        "  normalize                  recentre + scale the active model to the target size\n"
        "  screenshot PATH            save current viewport to PATH as PNG\n"
        "  camera [show|reset|azel AZ EL|zoom Z]\n"
        "                             deterministic camera, for reference renders\n"
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
