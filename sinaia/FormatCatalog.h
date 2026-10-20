#pragma once

// Catalogue des formats 3D importables par Sinaia, et règle d'accès à
// l'import OpenSCAD — sans wxWidgets, pour être testé dans TU.
// SupportedFormats.h en dérive les chaînes wx.
//
// Deux usages, deux listes :
//   - les dialogues File > Open et File > Add n'offrent que les formats
//     OUVRABLES : OpenSCAD n'y figure que si le répertoire réglé est valide ;
//   - le panneau "Files" montre le catalogue ENTIER : un .scad y reste visible,
//     et son ouverture sans OpenSCAD est refusée avec la marche à suivre.

#include <string>
#include <vector>

#include "../src/cgmesh/import_scad.h"

namespace sinaia {

// Emplacement d'icône dans l'image-list du panneau "Files". L'ordre DOIT
// correspondre à celui dans lequel les bitmaps sont ajoutés dans le
// constructeur de MyFrame (0=obj, 1=stl, 2=3ds/générique).
enum class FormatIcon { Obj = 0, Stl = 1, Generic = 2 };

// Une famille de format importable (une ou plusieurs extensions partageant un
// libellé et une icône).
struct FormatSpec
{
    const char*              label;                    // libellé lisible, ex. "STEP"
    std::vector<const char*> exts;                     // minuscules, sans point
    FormatIcon               icon;                     // icône du panneau "Files"
    bool                     requiresOpenScad = false; // ouvrable seulement avec OpenSCAD réglé
};

// L'unique endroit à éditer pour ajouter ou retirer un format.
//
// COHÉRENCE AVEC LE BUILD : quatre importeurs de cgmesh sont conditionnels ;
// compilés sans leur dépendance, ils retombent sur un stub qui renvoie false.
// Annoncer un format pour ensuite échouer à l'ouvrir serait trompeur, donc
// chaque famille concernée porte la garde de son importeur. Les macros
// CG_HAS_* sont PUBLIC sur la cible cgmesh (cf. src/cgmesh/CMakeLists.txt).
//
//   3DM              CG_HAS_OPENNURBS   mesh_io_3dm.cpp
//   STEP / IGES      CG_HAS_OCCT        vmeshes_io_occt.cpp
//   3MF              CG_HAS_LIB3MF      vmeshes_io_3mf.cpp
//   NBT              CG_HAS_ZLIB        nbt.cpp (l'analyseur décompresse)
//
// OpenSCAD dépend d'un EXÉCUTABLE dont le répertoire se règle dans
// File > Settings > Import : la décision se prend à l'exécution
// (requiresOpenScad), pas à la compilation.
inline const std::vector<FormatSpec>& FormatCatalog()
{
    static const std::vector<FormatSpec> formats = {
        { "Wavefront OBJ", { "obj" },                                FormatIcon::Obj     },
        { "STL",           { "stl" },                                FormatIcon::Stl     },
        { "3D Studio",     { "3ds" },                                FormatIcon::Generic },
#ifdef CG_HAS_OPENNURBS
        { "Rhino 3DM",     { "3dm" },                                FormatIcon::Generic },
#endif
        { "glTF",          { "gltf", "glb" },                        FormatIcon::Generic },
#ifdef CG_HAS_OCCT
        { "STEP",          { "step", "stp" },                        FormatIcon::Generic },
        { "IGES",          { "iges", "igs" },                        FormatIcon::Generic },
#endif
        { "Point clouds",  { "ply", "pset", "npts", "pts", "asc" },  FormatIcon::Obj     },
        { "OFF",           { "off" },                                FormatIcon::Generic },
        { "KVX voxel",     { "kvx" },                                FormatIcon::Generic },
        { "OpenSCAD",      { "scad" },                               FormatIcon::Generic, true },
#ifdef CG_HAS_ZLIB
        { "Minecraft NBT", { "nbt" },                                FormatIcon::Generic },
#endif
#ifdef CG_HAS_LIB3MF
        { "3MF",           { "3mf" },                                FormatIcon::Generic },
#endif
    };
    return formats;
}

// Formats ouvrables dans l'état courant du réglage OpenSCAD.
inline std::vector<FormatSpec> OfferedFormats(bool openScadAvailable)
{
    std::vector<FormatSpec> offered;
    for (const FormatSpec& f : FormatCatalog())
        if (!f.requiresOpenScad || openScadAvailable)
            offered.push_back(f);
    return offered;
}

inline std::string ToLowerAscii(std::string s)
{
    for (char& c : s)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    return s;
}

// Chaîne wildcard du wxFileDialog : "All supported formats" agrégé, puis une
// entrée par famille, puis "All files (*.*)".
inline std::string BuildOpenWildcardText(bool openScadAvailable)
{
    auto joinMasks = [](const std::vector<const char*>& exts) {
        std::string mask;
        for (const char* e : exts)
        {
            if (!mask.empty()) mask += ';';
            mask += std::string("*.") + e;
        }
        return mask;
    };

    const std::vector<FormatSpec> formats = OfferedFormats(openScadAvailable);

    std::string all;
    for (const FormatSpec& f : formats)
    {
        if (!all.empty()) all += ';';
        all += joinMasks(f.exts);
    }

    std::string wildcard = "All supported formats|" + all + "|";
    for (const FormatSpec& f : formats)
    {
        const std::string mask = joinMasks(f.exts);
        wildcard += std::string(f.label) + " (" + mask + ")|" + mask + "|";
    }
    wildcard += "All files (*.*)|*.*";
    return wildcard;
}

// Famille d'une extension dans le catalogue ENTIER (insensible à la casse), ou
// nullptr si l'extension n'est pas importable.
inline const FormatSpec* FindFormat(const std::string& ext)
{
    const std::string lower = ToLowerAscii(ext);
    for (const FormatSpec& f : FormatCatalog())
        for (const char* e : f.exts)
            if (lower == e)
                return &f;
    return nullptr;
}

inline bool IsScadExtension(const std::string& ext)
{
    const FormatSpec* f = FindFormat(ext);
    return f && f->requiresOpenScad;
}

// État du réglage, tel que l'affiche File > Settings > Import.
inline std::string OpenScadDirectoryStatusText(cgmesh::scad::DirectoryState state)
{
    using cgmesh::scad::DirectoryState;
    const std::string exe = cgmesh::scad::kDirectoryExecutableName;
    switch (state)
    {
        case DirectoryState::Empty:         return "Not set: .scad files cannot be opened.";
        case DirectoryState::Relative:      return "Invalid: the path must be absolute.";
        case DirectoryState::Missing:       return "Invalid: this directory does not exist.";
        case DirectoryState::NotADirectory: return "Invalid: this path is not a directory.";
        case DirectoryState::NoExecutable:  return "Invalid: no " + exe + " in this directory.";
        case DirectoryState::Valid:         return "Valid: " + exe + " found; .scad files can be opened.";
    }
    return std::string();
}

// À l'enregistrement des réglages, l'ancienne clé (chemin de l'exécutable)
// n'est retirée que si la nouvelle valeur la remplace pour de bon : répertoire
// valide, ou champ modifié par l'utilisateur. Sinon un ancien réglage
// momentanément inaccessible (disque réseau, clé USB) serait perdu sur un
// simple OK.
inline bool ShouldDeleteLegacyOpenScadSetting(bool newValueValid, bool fieldModified)
{
    return newValueValid || fieldModified;
}

// Refus d'ouvrir un .scad quand le réglage n'est pas valide. Vide si valide.
inline std::string ScadUnavailableText(cgmesh::scad::DirectoryState state, const std::string& directory)
{
    using cgmesh::scad::DirectoryState;
    if (state == DirectoryState::Valid)
        return std::string();
    std::string why = OpenScadDirectoryStatusText(state);
    if (state != DirectoryState::Empty)
        why = "OpenSCAD directory \"" + directory + "\": " + why;
    return "Opening .scad files requires OpenSCAD.\n" + why +
           "\nSet the directory that contains " + cgmesh::scad::kDirectoryExecutableName +
           " in File > Settings > Import.";
}

} // namespace sinaia
