#pragma once

// Lecture d'un fichier de modele dans un VMeshes : point de passage unique des
// chemins d'ouverture de sinaia (File > Open, File > Add, glisser-deposer,
// panneau Files, rafraichissement, console distante).
//
// Les .scad sont confies a l'executable OpenSCAD (cgmesh/import_scad.h). Son
// REPERTOIRE, regle dans File > Settings > Import, est la seule source : ni
// CG_OPENSCAD, ni le PATH, ni un chemin compile ne sont consultes. Tant que ce
// reglage n'est pas valide, tout .scad est refuse avec la marche a suivre. Le
// reglage est relu a chaque ouverture : un changement prend effet sans
// redemarrer. Les autres formats passent par VMeshesIO::load.

#include <wx/string.h>

#include "../src/cgmesh/import_scad.h"

class VMeshes;
class wxWindow;

namespace sinaia {

struct ModelLoadResult
{
	bool     ok = false;
	bool     cancelled = false;   // abandon demande par l'utilisateur : pas une erreur a afficher
	bool     unavailable = false; // .scad refuse : repertoire d'OpenSCAD non regle ou invalide
	wxString error;   // pourquoi, pour l'utilisateur ; vide si ok
	wxString log;     // sortie d'un outil externe (OpenSCAD), meme en cas de succes
};

// `parent` porte le dialogue de progression d'un rendu OpenSCAD long, qui
// offre l'annulation. Peut etre nul.
ModelLoadResult LoadModelFile (VMeshes& out, const wxString& path, wxWindow* parent);

bool IsScadFile (const wxString& path);

// Repertoire d'OpenSCAD (wxConfig /External/OpenSCADDirectory). A defaut,
// l'ancien reglage /External/OpenSCAD, qui designait l'executable, est converti
// (MigrateOpenScadSetting) ; rien n'est ecrit a la lecture.
struct OpenScadSetting
{
	wxString directory;
	bool     legacyPresent = false;   // l'ancienne cle existe encore
	wxString note;                    // ancienne valeur non convertie : pourquoi
};
OpenScadSetting LoadOpenScadSetting ();
wxString LoadOpenScadDirectory ();
// Ecrit le repertoire ; retire l'ancienne cle seulement si `deleteLegacy`
// (ShouldDeleteLegacyOpenScadSetting).
void     SaveOpenScadDirectory (const wxString& directory, bool deleteLegacy);

cgmesh::scad::DirectoryState OpenScadDirectoryState ();
bool     IsScadImportAvailable ();
// Message de refus d'un .scad ; vide si l'import est disponible.
wxString ScadUnavailableMessage ();

} // namespace sinaia
