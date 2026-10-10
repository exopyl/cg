#include "asset_paths.h"

#include <cstdlib>
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

#ifndef PELES_ASSETS_DIR_DEFAULT
#error "PELES_ASSETS_DIR_DEFAULT doit etre definie par la configuration CMake."
#endif

namespace peles
{
namespace app
{

namespace
{

// Le marqueur qui fait d'un repertoire un repertoire d'assets. Tester la seule
// existence du repertoire laisserait passer un repertoire vide, et la panne
// reapparaitrait plus loin, sous la forme d'une fenetre vide.
const char* const kMarkerFile = "peles.rml";

bool LooksLikeAssetsDir (const std::string& directory)
{
	if (directory.empty ())
		return false;
	std::error_code ec;
	return std::filesystem::is_regular_file (
	    std::filesystem::path (directory) / kMarkerFile, ec);
}

std::string Describe (const std::string& directory)
{
	std::error_code ec;
	if (!std::filesystem::exists (std::filesystem::path (directory), ec))
		return "le repertoire n'existe pas";
	if (!std::filesystem::is_directory (std::filesystem::path (directory), ec))
		return "ce chemin n'est pas un repertoire";
	return std::string ("le repertoire existe mais ne contient pas ") + kMarkerFile;
}

std::string EnvironmentValue (const char* name)
{
#ifdef _MSC_VER
	// getenv declenche C4996 sous MSVC. _dupenv_s alloue ; la chaine est copiee
	// puis liberee immediatement.
	char*  value = nullptr;
	size_t size  = 0;
	if (_dupenv_s (&value, &size, name) != 0 || value == nullptr)
		return std::string ();
	const std::string result (value);
	std::free (value);
	return result;
#else
	const char* value = std::getenv (name);
	return value != nullptr ? std::string (value) : std::string ();
#endif
}

} // namespace

std::string ExecutableDirectory ()
{
	// argv[0] N'EST PAS UTILISE : il vaut ce que l'appelant a bien voulu passer,
	// et il est relatif au repertoire courant, pas a l'executable. Les deux
	// chemins ci-dessous interrogent le systeme.
#ifdef _WIN32
	char        buffer[MAX_PATH] = { 0 };
	const DWORD length           = GetModuleFileNameA (nullptr, buffer, (DWORD)sizeof (buffer));
	if (length == 0 || length >= sizeof (buffer))
		return std::string ();
	return std::filesystem::path (buffer).parent_path ().string ();
#else
	char          buffer[PATH_MAX] = { 0 };
	const ssize_t length = readlink ("/proc/self/exe", buffer, sizeof (buffer) - 1);
	if (length <= 0)
		return std::string ();
	buffer[length] = '\0';
	return std::filesystem::path (buffer).parent_path ().string ();
#endif
}

AssetsResolution ResolveAssetsDir (const std::string& explicitDir)
{
	AssetsResolution resolution;

	if (!explicitDir.empty ())
	{
		if (LooksLikeAssetsDir (explicitDir))
		{
			resolution.directory = explicitDir;
			resolution.origin    = "--assets";
			return resolution;
		}
		resolution.error = "--assets " + explicitDir + " : " + Describe (explicitDir);
		return resolution;
	}

	const std::string fromEnvironment = EnvironmentValue ("PELES_ASSETS_DIR");
	if (!fromEnvironment.empty ())
	{
		if (LooksLikeAssetsDir (fromEnvironment))
		{
			resolution.directory = fromEnvironment;
			resolution.origin    = "PELES_ASSETS_DIR";
			return resolution;
		}
		resolution.error =
		    "PELES_ASSETS_DIR = " + fromEnvironment + " : " + Describe (fromEnvironment);
		return resolution;
	}

	const std::string executableDirectory = ExecutableDirectory ();
	const std::string besideExecutable =
	    executableDirectory.empty ()
	        ? std::string ()
	        : (std::filesystem::path (executableDirectory) / "assets").string ();
	if (LooksLikeAssetsDir (besideExecutable))
	{
		resolution.directory = besideExecutable;
		resolution.origin    = "<repertoire de l'executable>/assets";
		return resolution;
	}

	const std::string compiled (PELES_ASSETS_DIR_DEFAULT);
	if (LooksLikeAssetsDir (compiled))
	{
		resolution.directory = compiled;
		resolution.origin    = "chemin compile";
		return resolution;
	}

	// Les deux candidats sont nommes : lequel des deux a ete cherche est
	// exactement la question que pose un deploiement incomplet.
	resolution.error = std::string ("aucun repertoire d'assets trouve. Candidats essayes : ") +
	                   "(3) " +
	                   (besideExecutable.empty () ? std::string ("<indeterminable>")
	                                              : besideExecutable) +
	                   " ; (4) " + compiled +
	                   ". Utiliser --assets <dir> ou PELES_ASSETS_DIR.";
	return resolution;
}

} // namespace app
} // namespace peles
