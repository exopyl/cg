#include "RemoteConsole.h"

#include "SinaiaFrame.h"
#include "wxOpenGLCanvas.h"

#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/material.h"
#include "../src/cgmesh/vmeshes.h"

#include <wx/app.h>
#include <wx/string.h>

#include <cmath>
#include <cstring>
#include <future>
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
    os << "name \"" << m->m_name << "\"\n";
    os << "vertices " << m->GetNVertices() << "\n";
    os << "faces " << m->GetNFaces() << "\n";
    os << "materials " << m->m_nMaterials << "\n";
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
    if (!m || f < 0 || (unsigned)f >= m->m_nFaces)
        return "ERR face index out of range\n";

    Face* face = m->m_pFaces[f];
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
        if (vi >= m->m_nVertices) { os << "  invalid\n"; continue; }
        char buf[128];
        std::snprintf(buf, sizeof(buf), "  %g %g %g\n",
                      m->m_pVertices[3*vi], m->m_pVertices[3*vi+1], m->m_pVertices[3*vi+2]);
        os << buf;
    }
    if (!m->m_pFaceNormals.empty() && (unsigned)f * 3 + 2 < m->m_pFaceNormals.size())
    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "face_normal %g %g %g\n",
                      m->m_pFaceNormals[3*f], m->m_pFaceNormals[3*f+1], m->m_pFaceNormals[3*f+2]);
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
    if (!m || v < 0 || (unsigned)v >= m->m_nVertices)
        return "ERR vertex index out of range\n";

    std::ostringstream os;
    char buf[160];
    std::snprintf(buf, sizeof(buf), "position %g %g %g\n",
                  m->m_pVertices[3*v], m->m_pVertices[3*v+1], m->m_pVertices[3*v+2]);
    os << buf;
    if (!m->m_pVertexNormals.empty() && (unsigned)v * 3 + 2 < m->m_pVertexNormals.size())
    {
        std::snprintf(buf, sizeof(buf), "normal %g %g %g\n",
                      m->m_pVertexNormals[3*v], m->m_pVertexNormals[3*v+1], m->m_pVertexNormals[3*v+2]);
        os << buf;
    }
    if (!m->m_pTextureCoordinates.empty() && (unsigned)v * 2 + 1 < m->m_pTextureCoordinates.size())
    {
        std::snprintf(buf, sizeof(buf), "uv %g %g\n",
                      m->m_pTextureCoordinates[2*v], m->m_pTextureCoordinates[2*v+1]);
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
    if (!m || matId < 0 || (unsigned)matId >= m->m_nMaterials)
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
    os << "type " << tname << "\n";

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

        for (unsigned int i = 0; i < m->m_nFaces; ++i)
        {
            Face* face = m->m_pFaces[i];
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

        return static_cast<int>(m->m_nFaces);
    });

    if (result == -1) return "ERR no model loaded\n";
    if (result == -2) return "ERR mesh index out of range\n";

    std::ostringstream os;
    os << "flipped " << result << " faces of mesh " << n << "\n";
    os << "OK\n";
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
        "  screenshot PATH            save current viewport to PATH as PNG\n"
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
