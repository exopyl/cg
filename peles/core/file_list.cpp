#include "file_list.h"

#include "file_catalog.h"
#include "path_text.h"

#include <algorithm>
#include <system_error>
#include <utility>

namespace peles
{
namespace core
{

namespace fs = std::filesystem;

void FileList::Clear ()
{
	m_rows.clear ();
	m_paths.clear ();
	m_directory.clear ();
	m_directoryDisplay.clear ();
	m_lastError.clear ();
	m_hidden   = 0;
	m_selected = -1;
}

const fs::path& FileList::PathOf (int rowId) const
{
	static const fs::path empty;
	if (rowId < 0 || (std::size_t)rowId >= m_paths.size ())
		return empty;
	return m_paths[(std::size_t)rowId];
}

void FileList::Select (int rowId)
{
	const bool valid = rowId >= 0 && (std::size_t)rowId < m_rows.size ();
	m_selected       = valid ? rowId : -1;
	// ⚠ LA BORNE EST CELLE DES DEUX VECTEURS. Ils sont construits ensemble et de
	// meme taille, mais cette boucle est la seule qui les indexe TOUS LES DEUX :
	// la garde coute une comparaison et supprime la classe entiere des fautes
	// d'indice si l'un des deux vient a etre rempli ailleurs.
	const std::size_t count = std::min (m_rows.size (), m_paths.size ());
	for (std::size_t i = 0; i < count; ++i)
	{
		const bool selected = (int)i == m_selected;
		m_rows[i].selected  = selected;

		// ⚠ LA CLASSE EST RECONSTRUITE ENTIEREMENT, ET LA FAMILLE EST RELUE A SA
		// SOURCE. `data-attr-class` ECRASE l'attribut `class` : une classe de
		// selection posee ailleurs serait perdue, et la famille ajoutee a la
		// suite de la precedente s'accumulerait a chaque selection.
		m_rows[i].kind = KindClassForExtension (PathToUtf8 (m_paths[i].extension ()));
		if (selected)
			m_rows[i].kind += " selected";
	}
}

int FileList::SelectByPath (const fs::path& path)
{
	if (path.empty ())
	{
		Select (-1);
		return -1;
	}

	// ⚠ COMPARAISON SUR DES CHEMINS NORMALISES, PAS SUR DES CHAINES. Le chemin
	// demande vient de la ligne de commande -- donc relatif, avec des
	// separateurs quelconques -- tandis que celui de la liste vient de
	// l'enumeration. `lexically_normal` est LEXICAL : il ne touche pas le
	// disque, donc il ne suit pas les liens. Deux ecritures d'un meme fichier
	// passant par un lien ne sont pas reconnues comme egales -- limite assumee,
	// l'alternative etant un acces disque par ligne.
	std::error_code ec;
	const fs::path  wanted = fs::absolute (path, ec).lexically_normal ();
	if (ec)
	{
		Select (-1);
		return -1;
	}

	for (std::size_t i = 0; i < m_paths.size (); ++i)
	{
		std::error_code candidateEc;
		if (fs::absolute (m_paths[i], candidateEc).lexically_normal () != wanted || candidateEc)
			continue;

		Select ((int)i);
		return (int)i;
	}

	Select (-1);
	return -1;
}

bool FileList::Populate (const fs::path& directory)
{
	Clear ();
	m_directory = directory;

	// ⚠ MEME REGLE QUE POUR L'ARBRE : la surcharge a std::error_code, jamais
	// celle qui leve. Un repertoire devenu inaccessible entre son affichage
	// dans l'arbre et le clic qui l'ouvre est un cas courant -- disque retire,
	// partage reseau tombe -- pas un defaut du programme.
	//
	// ⚠ ET SANS `skip_permission_denied`, POUR LA MEME RAISON QUE L'ARBRE :
	// cette option rend un iterateur vide SANS erreur sur un acces refuse, donc
	// une liste vide au lieu d'un message. L'appelant doit pouvoir afficher la
	// cause.
	m_directoryDisplay = PathToUtf8 (directory);
	const std::string& shownDirectory = m_directoryDisplay;

	std::error_code        ec;
	fs::directory_iterator it (directory, ec);
	if (ec)
	{
		// LA CAUSE EST NOMMEE. Sans `ec.message()`, un repertoire inexistant et
		// un repertoire dont l'acces est refuse rendent le MEME message, alors
		// que les distinguer est la raison d'etre de cette ligne.
		m_lastError =
		    "\"" + shownDirectory + "\" : le contenu n'a pas pu etre lu (" + ec.message () + ")";
		return false;
	}

	// Le libelle est calcule UNE FOIS par entree, pas a chaque comparaison du
	// tri : la conversion d'un chemin n'est pas gratuite, et un comparateur qui
	// la refait rendrait le tri O(n log n) conversions.
	struct Found
	{
		std::string label;
		std::string kind;
		fs::path    path;
	};
	std::vector<Found>           found;
	const fs::directory_iterator end;

	// ⚠ LE TEST DE `ec` EST ENTRE L'INCREMENT ET LA PROCHAINE LECTURE, jamais en
	// tete de corps ni apres la boucle -- meme raison, et meme divergence entre
	// les deux bibliotheques standard, que dans DirectoryTree::Enumerate.
	int unreadable = 0;
	while (it != end)
	{
		std::error_code entryEc;
		const bool      isRegular = it->is_regular_file (entryEc);
		if (entryEc)
		{
			// UNE ENTREE DONT LE STATUT NE SE LIT PAS N'EST PAS UNE ENTREE
			// ABSENTE : elle est comptee et le compte est dit.
			++unreadable;
		}
		else if (isRegular)
		{
			const fs::path    path = it->path ();
			const std::string kind = KindClassForExtension (PathToUtf8 (path.extension ()));
			// LE FILTRE EST LE CATALOGUE, ET RIEN D'AUTRE. Une extension absente
			// ne s'affiche pas : la liste annonce ce que l'application sait
			// ouvrir.
			if (!kind.empty ())
				found.push_back (Found { PathToUtf8 (path.filename ()), kind, path });
		}

		it.increment (ec);
		if (ec)
		{
			m_lastError = "\"" + shownDirectory + "\" : lecture interrompue, la liste est " +
			              "partielle (" + ec.message () + ")";
			break;
		}
	}

	std::sort (found.begin (), found.end (), [] (const Found& left, const Found& right)
	           { return NameLess (left.label, right.label); });

	const std::size_t shown = std::min (found.size (), (std::size_t)kMaxFiles);
	m_hidden                = (int)(found.size () - shown);

	m_rows.reserve (shown);
	m_paths.reserve (shown);
	for (std::size_t i = 0; i < shown; ++i)
	{
		FileRow row;
		row.id    = (int)i;
		row.label = found[i].label;
		row.path  = PathToUtf8 (found[i].path);
		row.kind  = found[i].kind;
		m_rows.push_back (std::move (row));
		m_paths.push_back (found[i].path);
	}

	if (unreadable > 0)
	{
		// ⚠ AJOUTE AU MESSAGE, PAS SUBSTITUE : une lecture interrompue et des
		// entrees illisibles sont deux faits distincts, et ils peuvent coexister.
		if (!m_lastError.empty ())
			m_lastError += " ; ";
		m_lastError += std::to_string (unreadable) +
		               " entree(s) dont le statut n'a pas pu etre lu, non affichee(s)";
	}

	return m_lastError.empty ();
}

} // namespace core
} // namespace peles
