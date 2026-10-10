#include "path_text.h"

#include <exception>

namespace peles
{
namespace core
{

namespace fs = std::filesystem;

std::string PathToUtf8 (const fs::path& path)
{
	try
	{
		// C++17 : u8string() rend un std::string deja encode en UTF-8. Sous
		// Windows la conversion part du chemin natif WIDE, donc elle est
		// complete -- contrairement a string(), qui passe par la page de codes
		// ANSI.
		return path.u8string ();
	}
	catch (const std::exception&)
	{
		// Repli : une paire de substituts non appariee dans un nom de fichier
		// NTFS fait refuser la conversion. Ce qui compte alors est de rendre la
		// main, pas de rendre le nom exact -- l'identite du fichier est portee
		// par le path, pas par cette chaine.
	}

	std::string fallback;
	for (const fs::path::value_type unit : path.native ())
		fallback.push_back (unit >= 0x20 && unit < 0x7F ? (char)unit : '?');
	return fallback;
}

} // namespace core
} // namespace peles
