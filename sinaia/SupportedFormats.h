#pragma once

// Source unique de vérité des formats 3D importables par Sinaia.
//
// Historiquement la liste des extensions était dupliquée à deux endroits qui
// divergeaient : le wildcard du wxFileDialog (File > Open) et le filtre/icône du
// panneau latéral "Files". Ce fichier centralise la description de chaque format
// (libellé, extensions, icône) pour que les deux se dérivent du même tableau.

#include <wx/string.h>
#include <wx/filename.h>
#include <vector>

namespace sinaia {

// Emplacement d'icône dans l'image-list du panneau "Files". L'ordre DOIT
// correspondre à celui dans lequel les bitmaps sont ajoutés dans le
// constructeur de MyFrame (0=obj, 1=stl, 2=3ds/générique).
enum class FormatIcon { Obj = 0, Stl = 1, Generic = 2 };

// Une famille de format importable (un ou plusieurs extensions partageant un
// libellé et une icône).
struct Format3D
{
    const wxChar*             label;   // libellé lisible pour le dialog, ex. "STEP"
    std::vector<const wxChar*> exts;   // extensions en minuscules, sans point, ex. {"step","stp"}
    FormatIcon                icon;    // icône utilisée dans le panneau "Files"
};

// Le catalogue des formats supportés — l'unique endroit à éditer pour en
// ajouter/retirer un.
//
// COHÉRENCE AVEC LE BUILD : quatre importeurs de cgmesh sont conditionnels ;
// compilés sans leur dépendance, ils retombent sur un stub qui renvoie false.
// Annoncer un format dans le dialogue d'ouverture pour ensuite échouer à
// l'ouvrir serait trompeur, donc chaque famille concernée porte la garde de
// son importeur. Les macros CG_HAS_* sont PUBLIC sur la cible cgmesh, donc
// visibles ici (cf. src/cgmesh/CMakeLists.txt).
//
//   3DM              CG_HAS_OPENNURBS   mesh_io_3dm.cpp
//   STEP / IGES      CG_HAS_OCCT        vmeshes_io_occt.cpp
//   3MF              CG_HAS_LIB3MF      vmeshes_io_3mf.cpp
//   NBT              CG_HAS_ZLIB        nbt.cpp (l'analyseur décompresse)
//
// Les autres familles sont inconditionnelles : OBJ / 3DS / glTF / KVX sont
// traités par VMeshesIO::load, STL / OFF / nuages de points par le repli
// Mesh::load. Aucune dépendance externe.
inline const std::vector<Format3D>& SupportedFormats()
{
    static const std::vector<Format3D> formats = {
        { wxT("Wavefront OBJ"), { wxT("obj") },                                             FormatIcon::Obj     },
        { wxT("STL"),           { wxT("stl") },                                             FormatIcon::Stl     },
        { wxT("3D Studio"),     { wxT("3ds") },                                             FormatIcon::Generic },
#ifdef CG_HAS_OPENNURBS
        { wxT("Rhino 3DM"),     { wxT("3dm") },                                             FormatIcon::Generic },
#endif
        { wxT("glTF"),          { wxT("gltf"), wxT("glb") },                                FormatIcon::Generic },
#ifdef CG_HAS_OCCT
        { wxT("STEP"),          { wxT("step"), wxT("stp") },                                FormatIcon::Generic },
        { wxT("IGES"),          { wxT("iges"), wxT("igs") },                                FormatIcon::Generic },
#endif
        { wxT("Point clouds"),  { wxT("ply"), wxT("pset"), wxT("npts"), wxT("pts"), wxT("asc") }, FormatIcon::Obj },
        { wxT("OFF"),           { wxT("off") },                                             FormatIcon::Generic },
        { wxT("KVX voxel"),     { wxT("kvx") },                                             FormatIcon::Generic },
#ifdef CG_HAS_ZLIB
        { wxT("Minecraft NBT"), { wxT("nbt") },                                             FormatIcon::Generic },
#endif
#ifdef CG_HAS_LIB3MF
        { wxT("3MF"),           { wxT("3mf") },                                             FormatIcon::Generic },
#endif
    };
    return formats;
}

// Construit la chaîne wildcard du wxFileDialog : "All supported formats" agrégé,
// puis une entrée par famille, puis "All files (*.*)".
inline wxString BuildOpenWildcard()
{
    auto joinMasks = [](const std::vector<const wxChar*>& exts) {
        wxString mask;
        for (const wxChar* e : exts)
        {
            if (!mask.empty()) mask += wxT(';');
            mask << wxT("*.") << e;
        }
        return mask;
    };

    const std::vector<Format3D>& formats = SupportedFormats();

    wxString all;
    for (const Format3D& f : formats)
    {
        if (!all.empty()) all += wxT(';');
        all += joinMasks(f.exts);
    }

    wxString wildcard;
    wildcard << wxT("All supported formats|") << all << wxT("|");
    for (const Format3D& f : formats)
    {
        const wxString mask = joinMasks(f.exts);
        wildcard << f.label << wxT(" (") << mask << wxT(")|") << mask << wxT("|");
    }
    wildcard << wxT("All files (*.*)|*.*");
    return wildcard;
}

// Renvoie l'emplacement d'icône (>= 0) d'une extension supportée, insensible à
// la casse, ou -1 si l'extension n'est pas importable (à masquer du panneau).
inline int IconForExtension(const wxString& ext)
{
    const wxString lower = ext.Lower();
    for (const Format3D& f : SupportedFormats())
        for (const wxChar* e : f.exts)
            if (lower == e)
                return static_cast<int>(f.icon);
    return -1;
}

} // namespace sinaia
