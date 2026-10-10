#include "directory_tree.h"

#include "file_catalog.h"
#include "path_text.h"

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#endif

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <utility>

namespace peles
{
namespace core
{

namespace
{

namespace fs = std::filesystem;

std::string DepthClass (int depth)
{
	const int clamped = depth < 0 ? 0 : (depth > kMaxDepthClass ? kMaxDepthClass : depth);
	return "d" + std::to_string (clamped);
}

#ifdef _WIN32

// ⚠ LE MODE D'ERREUR EST BASCULE LE TEMPS DE LA QUESTION, ET C'EST INDISPENSABLE.
// Interroger un lecteur amovible vide sans SEM_FAILCRITICALERRORS fait ouvrir
// par le systeme la boite « Inserez un disque dans le lecteur » -- dans un
// processus qui n'a pas encore de fenetre, et qui parait donc fige.
// SetThreadErrorMode et non SetErrorMode : le second est a l'echelle du
// processus et se retrouverait modifie pour du code qui ne l'a pas demande.
bool DriveIsReady (const std::string& root)
{
	const UINT type = GetDriveTypeA (root.c_str ());
	if (type == DRIVE_UNKNOWN || type == DRIVE_NO_ROOT_DIR)
		return false;

	DWORD previous = 0;
	SetThreadErrorMode (SEM_FAILCRITICALERRORS, &previous);

	ULARGE_INTEGER available {};
	ULARGE_INTEGER total {};
	ULARGE_INTEGER free {};
	const bool     ready = GetDiskFreeSpaceExA (root.c_str (), &available, &total, &free) != FALSE;

	SetThreadErrorMode (previous, nullptr);
	return ready;
}

#endif

} // namespace

std::string DirectoryLabel (const fs::path& path)
{
	// ⚠ UN SEPARATEUR FINAL NE DOIT PAS CHANGER LE LIBELLE.
	// `path("a/b/").filename()` est VIDE : sans ce traitement, "fixture/"
	// afficherait le chemin complet la ou "fixture" affiche le nom seul, et deux
	// lignes de commande equivalentes rendraient deux vidages differents.
	fs::path normal = path.lexically_normal ();
	if (!normal.has_filename () && normal != normal.parent_path ())
		normal = normal.parent_path ();

	const std::string label = PathToUtf8 (normal.filename ());
	if (!label.empty ())
		return label;
	const std::string whole = PathToUtf8 (normal);
	return whole.empty () ? PathToUtf8 (path) : whole;
}

bool DirectoryTree::ValidIndex (int index) const
{
	return index >= 0 && (std::size_t)index < m_nodes.size ();
}

int DirectoryTree::AddNode (const fs::path& path, const std::string& label, int depth)
{
	Node node;
	node.path  = path;
	node.label = label;
	node.depth = depth;
	m_nodes.push_back (std::move (node));
	return (int)m_nodes.size () - 1;
}

void DirectoryTree::SetSystemRoots ()
{
	m_nodes.clear ();
	m_roots.clear ();
	m_rows.clear ();
	m_selected  = -1;
	m_truncated = false;
	m_lastError.clear ();

#ifdef _WIN32
	const DWORD mask = GetLogicalDrives ();
	for (int letter = 0; letter < 26; ++letter)
	{
		if ((mask & (1u << letter)) == 0)
			continue;

		const std::string root = std::string (1, (char)('A' + letter)) + ":\\";
		if (!DriveIsReady (root))
			continue;

		m_roots.push_back (AddNode (fs::path (root), root.substr (0, 2), 0));
	}
#else
	m_roots.push_back (AddNode ("/", "/", 0));
#endif

	for (const int root : m_roots)
		m_nodes[(std::size_t)root].drive = true;

	RebuildRows ();
}

bool DirectoryTree::SetRoot (const fs::path& directory)
{
	m_nodes.clear ();
	m_roots.clear ();
	m_rows.clear ();
	m_selected  = -1;
	m_truncated = false;
	m_lastError.clear ();

	std::error_code ec;
	if (!fs::is_directory (directory, ec) || ec)
	{
		// ⚠ LA CAUSE EST NOMMEE, PAS SEULEMENT LE SYMPTOME. Sans `ec.message()`,
		// un chemin inexistant et un chemin dont l'acces est refuse produisent le
		// MEME message -- alors que les distinguer est la seule chose que ce
		// message ait a faire.
		m_lastError = "\"" + PathToUtf8 (directory) + "\" n'est pas un repertoire accessible";
		if (ec)
			m_lastError += " (" + ec.message () + ")";
		RebuildRows ();
		return false;
	}

	m_roots.push_back (AddNode (directory, DirectoryLabel (directory), 0));
	RebuildRows ();
	return true;
}

void DirectoryTree::SetMaxChildrenPerNode (int maxChildren)
{
	// ⚠ UNE VALEUR NULLE OU NEGATIVE EST IGNOREE, PAS APPLIQUEE. Un plafond de
	// zero rendrait tout noeud vide et tout compte de masques egal a son nombre
	// d'enfants : l'arbre deviendrait une suite de lignes de debordement, ce
	// qu'aucun appelant ne peut vouloir.
	if (maxChildren <= 0)
		return;
	m_maxChildrenPerNode = maxChildren;
}

void DirectoryTree::Enumerate (int index)
{
	if (!ValidIndex (index))
		return;

	// Le drapeau est pose AVANT la lecture : un noeud dont l'enumeration a
	// echoue est enumere -- avec zero enfant et une erreur -- pas « pas encore
	// enumere ». Sans cela, chaque ouverture retenterait l'acces refuse.
	{
		Node& node = m_nodes[(std::size_t)index];
		if (node.enumerated)
			return;
		node.enumerated = true;
	}

	const fs::path path  = m_nodes[(std::size_t)index].path;
	const int      depth = m_nodes[(std::size_t)index].depth;

	// ⚠ LA SURCHARGE A std::error_code, JAMAIS CELLE QUI LEVE. Sur C:\,
	// System Volume Information refuse l'acces : une version qui leverait ferait
	// tomber l'ouverture de la racine ENTIERE pour un seul enfant fautif.
	//
	// ⚠ ET SANS `skip_permission_denied` : cette option fait rendre un
	// iterateur VIDE et AUCUNE erreur quand l'acces est refuse. Le noeud
	// s'afficherait alors comme un repertoire VIDE, c'est-a-dire exactement le
	// mode de panne que la ligne en erreur existe pour empecher.
	std::error_code        ec;
	fs::directory_iterator it (path, ec);
	if (ec)
	{
		m_nodes[(std::size_t)index].error = true;
		return;
	}

	std::vector<std::pair<std::string, fs::path>> found; // libelle, chemin
	const fs::directory_iterator                  end;

	// ⚠ LE TEST DE `ec` EST ENTRE L'INCREMENT ET LA PROCHAINE LECTURE, JAMAIS EN
	// TETE DE CORPS NI APRES LA BOUCLE, et ce n'est pas un gout de forme : apres
	// un increment en echec l'etat de l'iterateur n'est PAS SPECIFIE, et les
	// deux bibliotheques standard que ce depot cible en font l'inverse l'une de
	// l'autre.
	//   -- libstdc++ remet l'iterateur a la fin (`_M_dir.reset()` des que
	//      `advance` rend faux) : un test en tete de corps n'est JAMAIS atteint,
	//      donc l'erreur est perdue et un repertoire partiellement lu se presente
	//      comme un repertoire entierement lu ;
	//   -- MSVC, lui, ne relache l'implementation que sur `_No_more_files` et
	//      laisse l'iterateur DIFFERENT DE LA FIN sur toute autre erreur
	//      (<filesystem>, `_Advance_and_reset_if_no_more_files`) : un test place
	//      APRES la boucle ne serait jamais atteint non plus -- la boucle
	//      tournerait indefiniment.
	// Cette forme ne depend d'aucun des deux comportements : elle ne dereference
	// jamais apres un echec et ne boucle jamais apres un echec.
	int unreadable = 0;
	while (it != end)
	{
		std::error_code entryEc;
		const bool      isDirectory = it->is_directory (entryEc);
		if (entryEc)
		{
			// UNE ENTREE DONT LE STATUT NE SE LIT PAS N'EST PAS UNE ENTREE
			// ABSENTE. La retirer en silence est la meme faute que le repertoire
			// vide qui cache un refus d'acces.
			//
			// ⚠ ELLE NE MET PAS LE NOEUD EN ERREUR POUR AUTANT : le noeud a bien
			// ete enumere, et le marquer le rendrait NON OUVRABLE -- donc
			// masquerait les enfants qui, eux, se sont lus. Le compte devient une
			// ligne, au meme titre que le debordement.
			++unreadable;
		}
		else if (isDirectory)
		{
			found.emplace_back (PathToUtf8 (it->path ().filename ()), it->path ());
		}

		it.increment (ec);
		if (ec)
		{
			// Ce qui a ete lu est conserve, la suite est abandonnee, et le noeud
			// porte l'erreur.
			m_nodes[(std::size_t)index].error = true;
			break;
		}
	}

	std::sort (found.begin (), found.end (),
	           [] (const std::pair<std::string, fs::path>& left,
	               const std::pair<std::string, fs::path>& right)
	           { return NameLess (left.first, right.first); });

	const int shown =
	    (int)std::min<std::size_t> (found.size (), (std::size_t)m_maxChildrenPerNode);
	std::vector<int> children;
	children.reserve ((std::size_t)shown);
	for (int i = 0; i < shown; ++i)
		children.push_back (AddNode (found[(std::size_t)i].second, found[(std::size_t)i].first,
		                             depth + 1));

	Node& node      = m_nodes[(std::size_t)index];
	node.children   = std::move (children);
	node.hidden     = (int)found.size () - shown;
	node.unreadable = unreadable;
}

bool DirectoryTree::Toggle (int rowId)
{
	if (!ValidIndex (rowId))
		return false;
	if (m_nodes[(std::size_t)rowId].error)
		return false;

	if (!m_nodes[(std::size_t)rowId].expanded)
	{
		Enumerate (rowId);
		// Un noeud dont l'enumeration vient d'echouer devient une ligne en
		// erreur : il ne s'ouvre pas, et l'echec est VISIBLE plutot que rendu
		// comme un repertoire vide.
		if (m_nodes[(std::size_t)rowId].error)
		{
			RebuildRows ();
			return false;
		}
	}

	m_nodes[(std::size_t)rowId].expanded = !m_nodes[(std::size_t)rowId].expanded;
	RebuildRows ();
	return true;
}

void DirectoryTree::ExpandToDepth (int maxDepth)
{
	for (int depth = 0; depth <= maxDepth; ++depth)
	{
		// Les identifiants sont collectes AVANT d'ouvrir : chaque Toggle
		// reconstruit le vecteur de lignes, donc l'iterer pendant qu'on ouvre
		// invaliderait l'iterateur.
		std::vector<int> pending;
		for (const TreeRow& row : m_rows)
		{
			if (row.openable && !row.expanded && row.depth == depth)
				pending.push_back (row.id);
		}
		for (const int id : pending)
			Toggle (id);
	}
}

bool DirectoryTree::IsExpanded (int rowId) const
{
	return ValidIndex (rowId) && m_nodes[(std::size_t)rowId].expanded;
}

void DirectoryTree::Select (int rowId)
{
	m_selected = ValidIndex (rowId) ? rowId : -1;
	RebuildRows ();
}

const std::filesystem::path& DirectoryTree::PathOf (int rowId) const
{
	static const fs::path empty;
	if (!ValidIndex (rowId))
		return empty;
	return m_nodes[(std::size_t)rowId].path;
}

void DirectoryTree::RebuildRows ()
{
	m_rows.clear ();
	m_truncated = false;
	for (const int root : m_roots)
	{
		if (!AppendRows (root))
			break;
	}
}

void DirectoryTree::AppendTruncationRow (int depth)
{
	m_truncated = true;

	TreeRow truncation;
	truncation.depth      = depth;
	truncation.label      = "... plafond de " + std::to_string (kMaxVisibleRows) +
	                   " ligne(s) atteint, la suite n'est pas affichee";
	truncation.styleClass = DepthClass (depth) + " overflow";
	m_rows.push_back (std::move (truncation));
}

bool DirectoryTree::AppendRows (int index)
{
	if (!ValidIndex (index))
		return true;

	const Node& node = m_nodes[(std::size_t)index];

	// ⚠ LE PLAFOND EST GLOBAL ET IL EST TESTE ICI, avant chaque ligne : c'est le
	// TOTAL des lignes liees qui porte le cout, pas le nombre d'enfants d'un
	// noeud. La ligne de debordement porte la profondeur du niveau atteint.
	if ((int)m_rows.size () >= kMaxVisibleRows)
	{
		AppendTruncationRow (node.depth);
		return false;
	}

	TreeRow row;
	row.id       = index;
	row.depth    = node.depth;
	row.label    = node.label;
	row.path     = PathToUtf8 (node.path);
	row.expanded = node.expanded;
	row.error    = node.error;
	row.openable = !node.error;
	// `hidden` n'y figure pas : le plafond par noeud vaut au moins 1, donc un
	// noeud qui masque des enfants en affiche au moins un.
	row.expandable =
	    !node.error && (!node.enumerated || !node.children.empty () || node.unreadable > 0);
	row.drive    = node.drive;
	row.selected = index == m_selected;

	row.styleClass = DepthClass (node.depth);
	row.styleClass += node.error ? " error" : (node.expanded ? " open" : " closed");
	if (row.expandable)
		row.styleClass += " expandable";
	if (row.drive)
		row.styleClass += " drive";
	if (row.selected)
		row.styleClass += " selected";
	m_rows.push_back (std::move (row));

	if (!node.expanded)
		return true;

	for (const int child : node.children)
	{
		if (!AppendRows (child))
			return false;
	}

	// LA LIGNE DE DEBORDEMENT EST UNE LIGNE COMME UNE AUTRE, et c'est voulu : ce
	// qui est tronque doit etre VU. Elle ne designe aucun noeud, donc elle n'est
	// pas ouvrable.
	if (node.hidden > 0)
	{
		if ((int)m_rows.size () >= kMaxVisibleRows)
		{
			AppendTruncationRow (node.depth + 1);
			return false;
		}

		TreeRow overflow;
		overflow.depth      = node.depth + 1;
		overflow.label      = "... " + std::to_string (node.hidden) + " entree(s) non affichee(s)";
		overflow.styleClass = DepthClass (overflow.depth) + " overflow";
		m_rows.push_back (std::move (overflow));
	}

	// MEME TRAITEMENT POUR LES ENTREES ILLISIBLES : elles sont comptees, pas
	// retirees en silence.
	if (node.unreadable > 0)
	{
		if ((int)m_rows.size () >= kMaxVisibleRows)
		{
			AppendTruncationRow (node.depth + 1);
			return false;
		}

		TreeRow unreadable;
		unreadable.depth = node.depth + 1;
		unreadable.label = "... " + std::to_string (node.unreadable) +
		                   " entree(s) dont le statut n'a pas pu etre lu";
		unreadable.error      = true;
		unreadable.styleClass = DepthClass (unreadable.depth) + " error";
		m_rows.push_back (std::move (unreadable));
	}

	return true;
}

} // namespace core
} // namespace peles
