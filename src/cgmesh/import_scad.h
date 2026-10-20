#pragma once

// ============================================================================
//  OpenSCAD (.scad) -> VMeshes, par l'executable OpenSCAD
// ============================================================================
//
// Le langage OpenSCAD n'est pas interprete ici : le fichier est confie a
// l'executable OpenSCAD, qui l'evalue et ecrit un maillage OFF dans un
// repertoire temporaire, relu ensuite par l'import OFF de cgmesh.
//
//   openscad -o <temp>/model.off [-D nom=valeur]... <fichier.scad>
//
// Le repertoire de travail du processus est celui du .scad, de sorte que les
// `include`, `use` et `import` relatifs se resolvent comme dans l'editeur
// d'OpenSCAD. Le repertoire temporaire est supprime dans tous les cas.
//
// OFF plutot que STL, 3MF ou AMF : sommets indexes (donc maillage ferme sans
// soudure a la lecture), lecteur sans dependance, sept fois plus compact que
// le STL ASCII. Cf. scad_import.md a la racine du depot pour les mesures.
//
// Hors du build WebAssembly (absent de la liste EMSCRIPTEN de
// src/cgmesh/CMakeLists.txt) : il lance un processus.
//
// ============================================================================

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

class VMeshes;

namespace cgmesh {
namespace scad {

// Variable d'environnement designant l'executable (ou son repertoire).
constexpr const char* kEnvironmentVariable = "CG_OPENSCAD";

// Parametre `-D nom=valeur`. OpenSCAD concatene `nom=valeur;` au source :
// `value` doit donc etre un LITTERAL (IsLiteralValue), sans quoi l'import est
// refuse (InvalidInput). Une chaine s'ecrit avec ses guillemets
// ("\"texte\""), un nombre tel quel ("10"), un vecteur entre crochets
// ("[1, [2, 3]]").
struct Define
{
	std::string name;    // identifiant OpenSCAD, `$` initial admis ($fn)
	std::string value;   // litteral OpenSCAD
};

// Ordre de recherche de l'executable :
//   1. `configured`, s'il est renseigne -- et lui seul : un reglage explicite
//      qui ne mene a rien est une erreur a signaler, pas a contourner ;
//   2. la variable d'environnement kEnvironmentVariable, avec la meme regle ;
//   3. le PATH (Windows : openscad.exe puis openscad.com ; ailleurs : openscad) ;
//   4. Windows : %ProgramFiles%\OpenSCAD, emplacement de l'installeur officiel ;
//      sous Windows, seuls les programmes .exe et .com sont admis (un .bat ou
//      un .cmd passerait par cmd.exe) ;
//   5. `fallbacks`, dans l'ordre.
// Chaque entree peut designer l'executable ou le repertoire qui le contient.
struct ExecutableSearch
{
	std::filesystem::path              configured;
	std::vector<std::filesystem::path> fallbacks;
};

enum class ExecutableSource { None, Configured, Environment, SystemPath, ProgramFiles, Fallback };

struct ExecutableLookup
{
	std::filesystem::path    executable;   // vide si introuvable
	ExecutableSource         source = ExecutableSource::None;
	std::vector<std::string> searched;     // emplacements essayes, lisibles, dans l'ordre
};

ExecutableLookup FindOpenScad (const ExecutableSearch& search);

// --- Repertoire d'OpenSCAD designe par un reglage ---------------------------
//
// Regle de validite d'un repertoire choisi par l'utilisateur : chemin ABSOLU
// (un chemin relatif dependrait du repertoire courant du programme), existant,
// qui contient kDirectoryExecutableName, fichier regulier -- sous POSIX,
// executable par l'utilisateur courant (access X_OK). Rien n'est lance pour en
// decider.

#ifdef _WIN32
constexpr const char* kDirectoryExecutableName = "openscad.exe";
#else
constexpr const char* kDirectoryExecutableName = "openscad";
#endif

enum class DirectoryState
{
	Empty,          // aucun reglage
	Relative,       // chemin relatif : refuse
	Missing,        // le chemin n'existe pas
	NotADirectory,  // le chemin designe autre chose qu'un repertoire
	NoExecutable,   // repertoire sans kDirectoryExecutableName utilisable
	Valid,
};

const char* DirectoryStateName (DirectoryState state);

DirectoryState CheckOpenScadDirectory (const std::filesystem::path& directory);

// Chemin de l'executable si le repertoire est valide, vide sinon.
std::filesystem::path OpenScadInDirectory (const std::filesystem::path& directory);

// Conversion d'un reglage enregistre par une version qui designait
// l'EXECUTABLE (ou, toleré par FindOpenScad, son repertoire) :
//   - repertoire absolu existant : retenu tel quel (valide ou non, son etat
//     s'affiche) ;
//   - tout autre chemin absolu (fichier, chemin disparu) : son repertoire
//     parent s'il est valide ;
//   - sinon : rien n'est retenu, et `note` dit pourquoi -- l'ancien reglage
//     ne disparait jamais sans trace.
struct SettingMigration
{
	std::filesystem::path directory;          // vide si non convertible
	bool                  converted = false;  // repertoire valide obtenu
	std::string           note;               // raison d'un echec de conversion
};

SettingMigration MigrateOpenScadSetting (const std::filesystem::path& stored);

struct ImportOptions
{
	ExecutableSearch    search;
	std::vector<Define> defines;

	// 0 : pas de limite. Le rendu CGAL d'OpenSCAD 2021 peut durer des minutes.
	std::chrono::milliseconds timeout {0};

	// Interroge periodiquement sur le thread appelant ; vrai => abandon.
	std::function<bool ()> shouldCancel;

	// Repertoire ou creer les repertoires temporaires ; vide : celui du systeme.
	std::filesystem::path temporaryRoot;
};

enum class ImportStatus
{
	Ok,
	InvalidInput,         // fichier .scad absent, nom de parametre invalide
	ExecutableNotFound,
	LaunchFailed,
	OpenScadError,        // OpenSCAD a echoue : syntaxe, objet 2D, ... (voir log)
	EmptyResult,          // evaluation reussie mais aucune geometrie 3D
	ReadFailed,           // le fichier produit n'a pas pu etre relu
	Cancelled,
	TimedOut,
	OutputLimitExceeded,  // sortie d'OpenSCAD au-dela du plafond (processus tue)
};

const char* ImportStatusName (ImportStatus status);

// Vrai si `value` est un litteral OpenSCAD : nombre, true, false, undef,
// chaine entre guillemets (echappements \x admis, aucun caractere de
// controle), ou vecteur de ceux-ci, de profondeur 32 au plus.
bool IsLiteralValue (const std::string& value);

struct ImportResult
{
	ImportStatus          status = ImportStatus::InvalidInput;
	std::filesystem::path executable;    // executable employe, s'il a ete trouve
	int                   exitCode = -1;
	std::string           log;           // sortie complete d'OpenSCAD (stdout + stderr)
	std::string           message;       // resume pour l'utilisateur

	bool ok () const { return status == ImportStatus::Ok; }
};

// Ajoute a `out` le maillage produit par OpenSCAD. En cas d'echec, `out` est
// laisse intact. Ne leve pas.
ImportResult ImportScad (VMeshes& out, const std::filesystem::path& scadFile,
                         const ImportOptions& options = {});

} // namespace scad
} // namespace cgmesh
