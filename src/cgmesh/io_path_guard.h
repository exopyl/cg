#pragma once
#include <filesystem>
#include <string>

// ===========================================================================
//  Gardes sur les chaines lues DANS un fichier de donnees
// ===========================================================================
//
// Un fichier de modele peut nommer d'autres fichiers : `mtllib` dans un OBJ,
// `map_Kd` dans un MTL, le nom de la carte dans un 3DS. Ces noms sont des DONNEES
// NON FIABLES -- ils viennent du fichier, pas de l'appelant -- et ils finissent
// dans deux sinks distincts, souvent confondus :
//
//   - une ouverture de fichier, d'ou la traversee de repertoire ;
//   - un diagnostic, d'ou la forge de lignes de log.
//
// Les deux gardes ci-dessous couvrent chacun un de ces sinks.

namespace io_guard {

// Vrai si `name` ne peut designer qu'un fichier CONTENU dans le repertoire de
// reference : chemin relatif, sans composant de remontee.
//
// On autorise les sous-repertoires -- `map_Kd textures/wood.png` est courant et
// legitime -- et on refuse ce qui sort : tout `..`, toute racine, et les formes
// absolues propres a Windows (lettre de lecteur `C:`, chemin UNC `\\serveur`),
// que is_absolute() seul ne rejette pas de facon portable.
inline bool isContainedRelativePath (const std::string& name)
{
	if (name.empty())
		return false;

	const std::filesystem::path p (name);
	if (p.is_absolute() || p.has_root_name() || p.has_root_directory())
		return false;

	for (const auto& part : p)
		if (part == "..")
			return false;

	return true;
}

// Neutralise les caracteres de controle avant journalisation, et borne la
// longueur. Sans cela, un nom contenant un retour a la ligne fait passer la suite
// pour un message distinct emis par le programme.
//
// Volontairement sans <cctype> : isprint() depend de la locale et son argument
// doit etre promu en unsigned char, deux pieges pour un test qui se dit en une
// comparaison.
inline std::string forLog (const char *s)
{
	if (s == nullptr)
		return std::string("(null)");

	const size_t kMax = 260;   // ordre de grandeur de MAX_PATH
	std::string out;
	for (const char *p = s; *p != '\0' && out.size() < kMax; ++p)
	{
		const unsigned char c = (unsigned char)*p;
		out += (c < 0x20 || c == 0x7f) ? '?' : (char)c;
	}
	if (out.size() == kMax)
		out += "...";
	return out;
}

inline std::string forLog (const std::string& s)
{
	return forLog (s.c_str());
}

} // namespace io_guard
