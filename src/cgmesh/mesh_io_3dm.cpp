// ============================================================================
//  3dm import (Rhino / openNURBS)
// ============================================================================
//
// Entry point: VMeshes::import_3dm(char* filename), dispatched by
// VMeshes::load on the ".3dm" extension. Produces one cgmesh::Mesh per
// renderable geometry component encountered in the openNURBS model.
//
// ---------- Supported features --------------------------------------------
//
// Geometry:
//   * ON_Mesh                         (one Mesh per object)
//   * ON_Brep                         (per-face render meshes merged into
//                                      one Mesh per Brep)
//   * ON_Extrusion                    (cached render mesh)
//   Quads are triangulated on the 0-2 diagonal.
//
// Orientation:
//   * ON_BrepFace::m_bRev respected: when true the face's winding & vertex
//     normals are flipped so the resulting Mesh is consistently outward-
//     facing (Rhino stores meshes aligned with the surface parameterization
//     du x dv, which is opposite to BRep outward when m_bRev is set).
//
// Normals:
//   * ComputeNormals() is always called after writing faces (populates
//     m_pFaceNormals which the renderer requires for flat shading).
//   * When every source face mesh carries an m_N array, those vertex
//     normals are overlaid on top of the per-face-averaged normals so
//     smooth shading reflects Rhino's surface curvature.
//
// Units:
//   * Vertex positions scaled to meters via
//     ON::UnitScale(model.m_settings.m_ModelUnitsAndTolerances.m_unit_system,
//                   ON::LengthUnitSystem::Meters).
//
// Materials:
//   * Legacy ON_Material: Ambient/Diffuse/Specular/Emission/Transparency/
//     Shine read with per-component "has content" filtering — zeros are
//     treated as "not configured" and fall back to visible defaults so
//     Rhino's default-material objects never render pitch black.
//   * PBR materials (Rhino v7+): resolved via RdkMaterialInstanceId ->
//     ComponentFromId(RenderContent) -> ON_RenderMaterial::ToOnMaterial().
//     PBR base color, metallic and roughness are projected to Phong:
//         diffuse  = baseColor * (1 - metallic)
//         specular = baseColor * metallic + 0.04 * (1 - metallic)  (F0)
//         shininess = (1 - roughness)^2
//         ambient  = 0.2 * baseColor                              (tinted)
//   * Object color override (color_from_object) and layer color
//     (color_from_layer / color_from_parent) override diffuse; pure black
//     is filtered out as Rhino's default unconfigured wireframe color.
//
// UVs & textures:
//   * ON_Mesh::m_T copied into Mesh::m_pTextureCoordinates when every
//     contributing source mesh has a matching m_T.
//   * First ON_Texture of type diffuse_texture / bitmap_texture (or
//     pbr_base_color, same enum value) is loaded via stb_image. Path
//     resolution tries: m_image_file_reference.FullPath(), then
//     RelativePath() joined to the .3dm directory, then the basename in
//     the .3dm directory. On success a MaterialTexture replaces the
//     MaterialColorExt and per-face UV indices are wired (the renderer
//     only enters the texture branch when m_bUseTextureCoordinates is
//     set; doing it unconditionally would force glColor3f(1,1,1) and
//     wash everything out).
//
// ---------- Known limitations ---------------------------------------------
//
//   * ON_SubD ignored.
//   * ON_InstanceRef / ON_InstanceDefinition unresolved (Rhino blocks are
//     invisible in the import).
//   * ON_NurbsSurface standalone (not wrapped in a Brep) ignored.
//   * Quad triangulation forced on diagonal 0-2 — for concave or
//     near-degenerate quads this can produce flipped or self-intersecting
//     triangles. Picking the shorter diagonal would improve robustness.
//   * BRep face render meshes that ship without an attached ON_Mesh
//     (uncommon: requires a Rhino save with "render meshes" disabled)
//     silently yield no geometry — no tessellator is invoked.
//   * Transparency is read from material (alpha component) but sinaia's
//     renderer doesn't draw transparent surfaces.
//   * No per-face materials: ApplyMaterial paints every face with the
//     same material id even if Rhino multi-channel material data is
//     present.
//   * PBR features beyond base color / metallic / roughness ignored:
//     subsurface, sheen, anisotropic, clearcoat, normal/roughness/metallic
//     textures.
//   * Unicode object/material names go through ON_String narrow conversion
//     (locale-dependent) — non-ASCII names may be mangled.
//   * MyGLCanvas::SetVMeshes calls Mesh::ComputeNormals on every mesh
//     AFTER import, overwriting the Rhino-provided m_N overlay we took
//     pains to copy. Cosmetic: smooth shading still works, just from
//     winding-derived averages rather than Rhino's curvature normals.
//
// ---------- Possible improvements -----------------------------------------
//
//   * Tessellate ON_NurbsSurface / ON_SubD / Brep-without-cached-meshes by
//     calling into openNURBS' meshing engine (ON_MeshParameters).
//   * Resolve ON_InstanceRef: look up the InstanceDefinition, instantiate
//     each referenced geometry and apply the m_xform transform.
//   * Choose the shorter diagonal for quad triangulation (or call out to
//     an ear-clipping triangulator for the rare general n-gon).
//   * Honor per-face materials via ON_Mesh::m_FaceMaterialChannel — add
//     one Material per channel and SetMaterialId on each Face.
//   * Load extra PBR textures (metallic map, roughness map, normal map)
//     once the renderer has a shader pipeline that can consume them.
//   * Surface a logger for skipped geometry so users see "12 SubDs
//     ignored" instead of silent omissions.
//
// ============================================================================

#include "mesh.h"
#include "vmeshes.h"

#ifdef CG_HAS_OPENNURBS

#include "opennurbs.h"

#include <stb/stb_image.h>

#include <string>
#include <vector>

namespace {

struct OpenNurbsInitializer {
    OpenNurbsInitializer()  { ON::Begin(); }
    ~OpenNurbsInitializer() { ON::End();   }
};
static OpenNurbsInitializer s_opennurbs_init;

std::string utf8FromOnString(const ON_wString& w)
{
    if (w.Length() <= 0)
        return {};
    ON_String narrow(w);
    return narrow.Array() ? std::string(narrow.Array()) : std::string{};
}

std::string dirOf(const std::string& path)
{
    const size_t i = path.find_last_of("/\\");
    return i == std::string::npos ? std::string{} : path.substr(0, i);
}

std::string baseOf(const std::string& path)
{
    const size_t i = path.find_last_of("/\\");
    return i == std::string::npos ? path : path.substr(i + 1);
}

unsigned int triCountForMesh(const ON_Mesh* m)
{
    unsigned int n = 0;
    for (int i = 0; i < m->FaceCount(); ++i)
        n += m->m_F[i].IsTriangle() ? 1u : 2u;
    return n;
}

void writeVertices(std::vector<float>& dst, unsigned int off, const ON_Mesh* src, double scale)
{
    const int n = src->VertexCount();
    const bool dbl = src->HasDoublePrecisionVertices() && src->m_dV.Count() == n;
    const float s = (float) scale;
    for (int i = 0; i < n; ++i)
    {
        if (dbl)
        {
            const ON_3dPoint& p = src->m_dV[i];
            dst[3*(off+i)  ] = (float)(p.x * scale);
            dst[3*(off+i)+1] = (float)(p.y * scale);
            dst[3*(off+i)+2] = (float)(p.z * scale);
        }
        else
        {
            const ON_3fPoint& p = src->m_V[i];
            dst[3*(off+i)  ] = p.x * s;
            dst[3*(off+i)+1] = p.y * s;
            dst[3*(off+i)+2] = p.z * s;
        }
    }
}

bool writeNormals(std::vector<float>& dst, unsigned int off, const ON_Mesh* src, bool reversed)
{
    const int n = src->VertexCount();
    if (src->m_N.Count() != n)
        return false;
    const float sign = reversed ? -1.0f : 1.0f;
    for (int i = 0; i < n; ++i)
    {
        const ON_3fVector& nrm = src->m_N[i];
        dst[3*(off+i)  ] = nrm.x * sign;
        dst[3*(off+i)+1] = nrm.y * sign;
        dst[3*(off+i)+2] = nrm.z * sign;
    }
    return true;
}

bool writeUVs(std::vector<float>& dst, unsigned int off, const ON_Mesh* src)
{
    const int n = src->VertexCount();
    if (src->m_T.Count() != n)
        return false;
    for (int i = 0; i < n; ++i)
    {
        const ON_2fPoint& uv = src->m_T[i];
        dst[2*(off+i)  ] = uv.x;
        dst[2*(off+i)+1] = uv.y;
    }
    return true;
}

unsigned int writeFaces(Face** dstFaces, unsigned int faceOff,
                        const ON_Mesh* src, unsigned int vertexOff, bool reversed)
{
    unsigned int dst = faceOff;
    for (int i = 0; i < src->FaceCount(); ++i)
    {
        const ON_MeshFace& f = src->m_F[i];

        Face* face = dstFaces[dst++];
        face->SetNVertices(3);
        face->SetVertex(0, vertexOff + f.vi[0]);
        face->SetVertex(1, vertexOff + (reversed ? f.vi[2] : f.vi[1]));
        face->SetVertex(2, vertexOff + (reversed ? f.vi[1] : f.vi[2]));

        if (!f.IsTriangle())
        {
            face = dstFaces[dst++];
            face->SetNVertices(3);
            face->SetVertex(0, vertexOff + f.vi[0]);
            face->SetVertex(1, vertexOff + (reversed ? f.vi[3] : f.vi[2]));
            face->SetVertex(2, vertexOff + (reversed ? f.vi[2] : f.vi[3]));
        }
    }
    return dst - faceOff;
}

// Wire per-face UV indices using the face's own vertex indices (per-vertex UV).
// Only call this once we've committed to a textured material — flipping
// m_bUseTextureCoordinates without a bound texture triggers the renderer's
// glColor3f(1,1,1) override and washes the model out.
void wireFaceUVsFromVertexIndices(Mesh& dst)
{
    if (dst.m_pTextureCoordinates.empty())
        return;
    for (unsigned int i = 0; i < dst.m_nFaces; ++i)
    {
        Face* f = dst.m_pFaces[i];
        const int nv = f->GetNVertices();
        if (nv <= 0) continue;
        f->m_bUseTextureCoordinates = true;
        f->ActivateTextureCoordinatesIndices();
        for (int j = 0; j < nv; ++j)
            f->SetTexCoord((unsigned int) j, (unsigned int) f->GetVertex((unsigned int) j));
    }
}

struct MeshSource
{
    const ON_Mesh* mesh;
    bool reversed; // flip winding & m_N when ingesting (used for ON_BrepFace::m_bRev)
};

bool fillFromMeshes(Mesh& dst, const std::vector<MeshSource>& sources,
                    const std::string& name, double scale)
{
    if (sources.empty())
        return false;

    unsigned int totalV = 0, totalF = 0;
    bool allHaveUVs = true;
    for (const auto& s : sources)
    {
        const int nv = s.mesh->VertexCount();
        totalV += (unsigned int) nv;
        totalF += triCountForMesh(s.mesh);
        if (s.mesh->m_T.Count() != nv)
            allHaveUVs = false;
    }

    dst.Init(totalV, totalF);

    if (allHaveUVs)
    {
        dst.m_nTextureCoordinates = totalV;
        dst.m_pTextureCoordinates.assign(2 * totalV, 0.0f);
    }

    unsigned int vOff = 0, fOff = 0;
    bool allHaveN = true;
    for (const auto& s : sources)
    {
        writeVertices(dst.m_pVertices, vOff, s.mesh, scale);
        if (allHaveUVs)
            writeUVs(dst.m_pTextureCoordinates, vOff, s.mesh);
        fOff += writeFaces(dst.m_pFaces, fOff, s.mesh, vOff, s.reversed);
        if (s.mesh->m_N.Count() != s.mesh->VertexCount())
            allHaveN = false;
        vOff += (unsigned int) s.mesh->VertexCount();
    }

    // Face normals are derived from the (now correctly oriented) winding.
    // ComputeNormals populates both m_pFaceNormals (required by mesh_renderer
    // for flat shading) and m_pVertexNormals.
    dst.ComputeNormals();

    // Overlay Rhino-provided vertex normals on top when available — they reflect
    // the smooth surface curvature better than the per-face average.
    if (allHaveN)
    {
        vOff = 0;
        for (const auto& s : sources)
        {
            writeNormals(dst.m_pVertexNormals, vOff, s.mesh, s.reversed);
            vOff += (unsigned int) s.mesh->VertexCount();
        }
    }

    dst.m_name = name;
    return true;
}

bool fillFromONMesh(Mesh& dst, const ON_Mesh* src, const std::string& name, double scale)
{
    if (!src || src->VertexCount() <= 0 || src->FaceCount() <= 0)
        return false;
    return fillFromMeshes(dst, { { src, false } }, name, scale);
}

std::vector<MeshSource> collectBrepFaceMeshes(const ON_Brep& brep)
{
    std::vector<MeshSource> out;
    for (int i = 0; i < brep.m_F.Count(); ++i)
    {
        const ON_BrepFace& bf = brep.m_F[i];
        const ON_Mesh* m = bf.Mesh(ON::mesh_type::render_mesh);
        if (m && m->VertexCount() > 0 && m->FaceCount() > 0)
        {
            // Rhino's BRep render meshes are wound consistent with the underlying
            // surface's parameterization (du x dv). When m_bRev is true, the BRep's
            // outward direction is opposite, so we flip winding & normals.
            out.push_back({ m, bf.m_bRev });
        }
    }
    return out;
}

bool fillFromBrep(Mesh& dst, const ON_Brep* brep, const std::string& name, double scale)
{
    if (!brep)
        return false;
    return fillFromMeshes(dst, collectBrepFaceMeshes(*brep), name, scale);
}

bool fillFromExtrusion(Mesh& dst, const ON_Extrusion* ext, const std::string& name, double scale)
{
    if (!ext)
        return false;
    const ON_Mesh* m = ext->Mesh(ON::mesh_type::render_mesh);
    if (m && m->VertexCount() > 0 && m->FaceCount() > 0)
        return fillFromONMesh(dst, m, name, scale);
    return false;
}

bool fillFromGeometry(Mesh& dst, const ON_Geometry* geom, const std::string& name, double scale)
{
    if (!geom)
        return false;
    if (const ON_Mesh* m = ON_Mesh::Cast(geom))
        return fillFromONMesh(dst, m, name, scale);
    if (const ON_Brep* b = ON_Brep::Cast(geom))
        return fillFromBrep(dst, b, name, scale);
    if (const ON_Extrusion* e = ON_Extrusion::Cast(geom))
        return fillFromExtrusion(dst, e, name, scale);
    return false;
}

double computeUnitScale(const ONX_Model& model)
{
    const ON_UnitSystem meters(ON::LengthUnitSystem::Meters);
    return ON::UnitScale(model.m_settings.m_ModelUnitsAndTolerances.m_unit_system, meters);
}

const ON_Texture* findDiffuseTexture(const ON_Material& mat)
{
    for (int i = 0; i < mat.m_textures.Count(); ++i)
    {
        const ON_Texture& t = mat.m_textures[i];
        // bitmap_texture and diffuse_texture share the same enum value (1)
        if (t.m_type == ON_Texture::TYPE::diffuse_texture)
            return &t;
    }
    return nullptr;
}

struct LoadedImage
{
    std::vector<unsigned char> pixels; // RGBA
    int width = 0;
    int height = 0;
};

bool loadImageRGBA(const std::string& path, LoadedImage& out)
{
    if (path.empty())
        return false;
    int w = 0, h = 0, n = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &n, STBI_rgb_alpha);
    if (!data)
        return false;
    out.pixels.assign(data, data + static_cast<size_t>(w) * h * 4);
    out.width  = w;
    out.height = h;
    stbi_image_free(data);
    return true;
}

bool loadTexture(const ON_Texture& tex, const std::string& threeDmDir, LoadedImage& out)
{
    const ON_FileReference& ref = tex.m_image_file_reference;

    const std::string full = utf8FromOnString(ref.FullPath());
    const std::string rel  = utf8FromOnString(ref.RelativePath());

    if (loadImageRGBA(full, out))
        return true;
    if (!rel.empty() && !threeDmDir.empty() && loadImageRGBA(threeDmDir + "/" + rel, out))
        return true;
    if (!full.empty() && !threeDmDir.empty() && loadImageRGBA(threeDmDir + "/" + baseOf(full), out))
        return true;
    return false;
}

struct Appearance
{
    bool        hasMaterial = false;       // true if a non-default material is attached
    ON_Material material;                  // valid if hasMaterial — copied so RDK-upgraded
                                           // versions outlive the model lookup
    bool     hasColor = false;             // explicit display color (object or layer)
    ON_Color color    = ON_Color::White;
};

bool colorHasContent(ON_Color c)
{
    // ON_UNSET_COLOR is sentinel; pure black is Rhino's default layer/object color
    // which signals "user didn't configure anything" — treat as no content so we
    // fall back to sensible visible defaults rather than rendering pitch black.
    if (c == ON_UNSET_COLOR)
        return false;
    return c.FractionRed() > 0.0 || c.FractionGreen() > 0.0 || c.FractionBlue() > 0.0;
}

Appearance resolveAppearance(const ONX_Model& model, const ON_3dmObjectAttributes* attrs)
{
    Appearance r;
    if (!attrs)
        return r;

    // Material (MaterialFromAttributes already resolves from_object / from_layer / from_parent).
    // Skip the persistent ON_Material::Default fallback Rhino returns when nothing is
    // assigned — its ambient/diffuse are zero and would wash everything to black.
    const ON_ModelComponentReference matRef = model.MaterialFromAttributes(*attrs);
    if (const ON_Material* mat = ON_Material::Cast(matRef.ModelComponent()))
    {
        if (ON_UuidCompare(mat->Id(), ON_Material::Default.Id()) != 0)
        {
            r.material    = *mat;
            r.hasMaterial = true;
        }
    }

    // Upgrade to the RDK render-content material when available (Rhino v6+). The legacy
    // ON_Material returned above is often a placeholder whose Diffuse()/Ambient() are not
    // populated for modern materials — the actual values live in an ON_RenderMaterial
    // referenced by RdkMaterialInstanceId.
    if (r.hasMaterial && r.material.RdkMaterialInstanceIdIsNotNil())
    {
        const ON_UUID rdkId = r.material.RdkMaterialInstanceId();
        const ON_ModelComponentReference contentRef =
            model.ComponentFromId(ON_ModelComponent::Type::RenderContent, rdkId);
        if (const ON_RenderContent* content = ON_RenderContent::Cast(contentRef.ModelComponent()))
        {
            if (const ON_RenderMaterial* renderMat = ON_RenderMaterial::Cast(content))
                r.material = renderMat->ToOnMaterial();
        }
    }

    // Display color: object override > layer color > none.
    // Layer::Color() is Rhino's wireframe line color, not a surface color, and defaults to
    // black on uncustomized layers — only adopt it when the user clearly set something.
    switch (attrs->ColorSource())
    {
    case ON::object_color_source::color_from_object:
        if (colorHasContent(attrs->m_color))
        {
            r.color    = attrs->m_color;
            r.hasColor = true;
        }
        break;
    case ON::object_color_source::color_from_layer:
    case ON::object_color_source::color_from_parent:
    {
        const ON_ModelComponentReference layerRef = model.LayerFromIndex(attrs->m_layer_index);
        if (const ON_Layer* layer = ON_Layer::Cast(layerRef.ModelComponent()))
        {
            const ON_Color c = layer->Color();
            if (colorHasContent(c))
            {
                r.color    = c;
                r.hasColor = true;
            }
        }
        break;
    }
    case ON::object_color_source::color_from_material:
        // diffuse comes from material — nothing to override
        break;
    }

    return r;
}

void applyAppearance(Mesh& dst, const Appearance& app, const std::string& threeDmDir)
{
    Material* applied = nullptr;

    const ON_Texture* texture = app.hasMaterial ? findDiffuseTexture(app.material) : nullptr;

    // Try texture path first (requires both a material entry and a resolvable bitmap)
    if (texture)
    {
        LoadedImage img;
        if (loadTexture(*texture, threeDmDir, img))
        {
            std::string name = utf8FromOnString(app.material.Name());
            if (name.empty())
                name = "3dm_texture";
            applied = new MaterialTexture(name,
                                          (unsigned int) img.width,
                                          (unsigned int) img.height,
                                          img.pixels.data());
            wireFaceUVsFromVertexIndices(dst);
        }
    }

    if (!applied)
    {
        auto* m = new MaterialColorExt();

        // Always seed with usable defaults so we never render fully black even when
        // the 3dm file leaves a component at zero (very common for Rhino's defaults).
        m->SetAmbient (0.2f, 0.2f, 0.2f, 1.0f);
        m->SetDiffuse (0.8f, 0.8f, 0.8f, 1.0f);
        m->SetSpecular(0.5f, 0.5f, 0.5f, 1.0f);
        m->SetEmission(0.0f, 0.0f, 0.0f, 1.0f);
        m->SetShininess(0.3f);

        if (app.hasMaterial)
        {
            if (app.material.IsPhysicallyBased())
            {
                // PBR path (Rhino v7+): legacy Diffuse()/Ambient()/etc. are not populated;
                // real colors live in PhysicallyBased(). We project to Phong:
                //   diffuse  = baseColor * (1 - metallic)
                //   specular = baseColor * metallic + 0.04 * (1 - metallic)    [Schlick F0]
                //   shininess = (1 - roughness)^2
                //   ambient  = 0.2 * baseColor                                  [tint our default]
                const auto pbr = app.material.PhysicallyBased();
                if (pbr)
                {
                    const ON_4fColor base = pbr->BaseColor();
                    const float br = base.Red();
                    const float bg = base.Green();
                    const float bb = base.Blue();
                    const float ba = base.Alpha();
                    const float metallic  = (float) pbr->Metallic();
                    const float roughness = (float) pbr->Roughness();

                    const float oneMinusMet = 1.0f - metallic;
                    const float dielectric  = 0.04f;

                    m->SetAmbient (0.2f * br, 0.2f * bg, 0.2f * bb, 1.0f);
                    m->SetDiffuse (br * oneMinusMet, bg * oneMinusMet, bb * oneMinusMet, ba);
                    m->SetSpecular(br * metallic + dielectric * oneMinusMet,
                                   bg * metallic + dielectric * oneMinusMet,
                                   bb * metallic + dielectric * oneMinusMet,
                                   1.0f);
                    m->SetShininess((1.0f - roughness) * (1.0f - roughness));
                }
            }
            else
            {
                const ON_Color a = app.material.Ambient();
                const ON_Color d = app.material.Diffuse();
                const ON_Color s = app.material.Specular();
                const ON_Color e = app.material.Emission();
                const float alpha = (float)(1.0 - app.material.Transparency());

                // Per-component override only when the material defines content.
                if (colorHasContent(a))
                    m->SetAmbient((float)a.FractionRed(), (float)a.FractionGreen(), (float)a.FractionBlue(), 1.0f);
                if (colorHasContent(d))
                    m->SetDiffuse((float)d.FractionRed(), (float)d.FractionGreen(), (float)d.FractionBlue(), alpha);
                if (colorHasContent(s))
                    m->SetSpecular((float)s.FractionRed(), (float)s.FractionGreen(), (float)s.FractionBlue(), 1.0f);
                if (colorHasContent(e))
                    m->SetEmission((float)e.FractionRed(), (float)e.FractionGreen(), (float)e.FractionBlue(), 1.0f);
                if (app.material.Shine() > 0.0)
                    m->SetShininess((float)(app.material.Shine() / 255.0));
            }

            const std::string name = utf8FromOnString(app.material.Name());
            if (!name.empty())
                m->SetName(name);
        }

        // Display color override (object override, or layer color when user customized it).
        // resolveAppearance already filtered out the unset/black "unconfigured" case.
        if (app.hasColor)
        {
            const float alpha = app.hasMaterial ? (float)(1.0 - app.material.Transparency()) : 1.0f;
            m->SetDiffuse((float)app.color.FractionRed(),
                          (float)app.color.FractionGreen(),
                          (float)app.color.FractionBlue(),
                          alpha);
        }

        applied = m;
    }

    const unsigned int matId = dst.Material_Add(applied);
    dst.ApplyMaterial(matId);
}

} // namespace

bool VMeshes::import_3dm(const char* filename)
{
    if (!filename)
        return false;

    ONX_Model model;
    if (!model.Read(filename))
        return false;

    const double scale = computeUnitScale(model);
    const std::string threeDmDir = dirOf(filename);

    ONX_ModelComponentIterator it(model, ON_ModelComponent::Type::ModelGeometry);
    for (const ON_ModelComponent* mc = it.FirstComponent(); mc != nullptr; mc = it.NextComponent())
    {
        const ON_ModelGeometryComponent* gc = ON_ModelGeometryComponent::Cast(mc);
        if (!gc) continue;

        const std::string name = utf8FromOnString(mc->Name());
        Mesh* pMesh = new Mesh();
        if (fillFromGeometry(*pMesh, gc->Geometry(nullptr), name, scale))
        {
            applyAppearance(*pMesh, resolveAppearance(model, gc->Attributes(nullptr)), threeDmDir);
            m_Meshes.push_back(pMesh);
        }
        else
        {
            delete pMesh;
        }
    }

    return !m_Meshes.empty();
}

#else // !CG_HAS_OPENNURBS

bool VMeshes::import_3dm(const char * /*filename*/)     { return false; }

#endif
