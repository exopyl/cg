#pragma once

#include "ui_dto.h"

#include <filesystem>
#include <string>
#include <vector>

namespace peles
{
namespace core
{

// PLAFOND DE FICHIERS AFFICHES.
//
// ⚠ IL BORNE DEJA LA BONNE GRANDEUR -- l'ensemble de la liste, pas une
// sous-partie. C'est sa VALEUR qui etait une estimation, et elle ne l'est plus.
//
// ⚠ STATUT EXACT DU CHIFFRE : MESURE SUR L'ARBRE, REPORTE ICI PAR INFERENCE.
// L'instrument est `peles --frames 120 --dump-timing --browse <dir>
// --browse-depth n`, qui rend le temps de la seule phase 2 sur la premiere
// image ; il a ete applique aux lignes d'ARBRE, pas aux lignes de LISTE.
// Mesure : ≈ 7,7 µs par ligne, donc ≈ 9 ms a 1000 lignes et 38 a 41 ms a 5002 --
// plus de DEUX periodes de 60 Hz. L'inference tient a ce que les deux zones
// passent par le MEME mecanisme, un `data-for` sur un vecteur lie ; ce qui la
// confirmerait est la meme mesure conduite sur --browse d'un repertoire a
// plusieurs milliers de fichiers importables.
inline constexpr int kMaxFiles = 1000;

// ---------------------------------------------------------------------------
//  LA LISTE DES FICHIERS IMPORTABLES D'UN REPERTOIRE
// ---------------------------------------------------------------------------
//
// Remplace PopulateFilesList de l'hote historique.
//
// ⚠ UN SEUL NIVEAU, ET C'EST UNE CORRECTION, PAS UN PORTAGE. L'hote historique
// appelle wxDir::GetAllFiles, qui est RECURSIF PAR DEFAUT dans wxWidgets : un
// clic sur C:\ y parcourt le disque entier avant de rendre la main. Le
// comportement attendu d'une liste de fichiers est le contenu du repertoire
// designe, pas sa descendance.
//
// ⚠ UNE ERREUR D'ACCES REND false ET SE LIT DANS LastError(). L'APPELANT
// L'AFFICHE : une liste vide et une liste inaccessible se ressemblent a
// l'ecran, et seule la premiere est une information.
class FileList
{
public:
	bool Populate (const std::filesystem::path& directory);
	void Clear ();

	const std::vector<FileRow>& Rows () const { return m_rows; }

	// Chemin du fichier de rang `rowId`, ou chemin VIDE si le rang ne designe
	// rien.
	//
	// ⚠ C'EST L'IDENTITE DU FICHIER, ET ELLE VIT ICI -- pas dans le DTO.
	// FileRow::path est une chaine d'AFFICHAGE en UTF-8 (cf. core/path_text.h) :
	// sous Windows, une conversion de chemin peut etre destructive, donc rouvrir
	// un fichier a partir d'elle peut viser autre chose. L'etape qui ouvre les
	// fichiers de la liste passe par ici.
	const std::filesystem::path& PathOf (int rowId) const;

	// Selection de la liste. -1 : aucune.
	//
	// ⚠ ELLE VIT DANS LA LISTE, PAS DANS LA COPIE DTO, exactement comme
	// DirectoryTree::m_selected. Portee par la copie, elle serait effacee par la
	// republication suivante -- et la republication est declenchee par des
	// operations qui n'ont rien a voir avec la selection.
	void Select (int rowId);

	// Selectionne le fichier de ce chemin s'il est dans la liste, et rend son
	// rang -- ou -1 si aucun ne correspond, ce qui DESELECTIONNE.
	int SelectByPath (const std::filesystem::path& path);

	int Selected () const { return m_selected; }

	// Nombre de fichiers importables au-dela du plafond. Zero en regime normal.
	int HiddenCount () const { return m_hidden; }

	// Repertoire courant : l'IDENTITE d'un cote, la chaine d'AFFICHAGE en UTF-8
	// de l'autre. Les deux ne sont pas interchangeables (cf. core/path_text.h).
	const std::filesystem::path& Directory () const { return m_directory; }
	const std::string&           DirectoryDisplay () const { return m_directoryDisplay; }
	const std::string&           LastError () const { return m_lastError; }

private:
	// ⚠ DEUX VECTEURS DE MEME RANG, ET C'EST LA CONTRAINTE DE RmlUi QUI
	// L'IMPOSE : le DTO lie ne peut porter que des types liables -- ni
	// std::filesystem::path, ni membre prive, ni membre `const`. Le chemin natif
	// vit donc a cote, indexe par le meme rang.
	std::vector<FileRow>               m_rows;
	std::vector<std::filesystem::path> m_paths;

	std::filesystem::path m_directory;
	std::string           m_directoryDisplay;
	std::string           m_lastError;
	int                   m_hidden   = 0;
	int                   m_selected = -1;
};

} // namespace core
} // namespace peles
