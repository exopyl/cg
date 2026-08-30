#pragma once
//
//  Palette -- la liste des types de noeuds, telle qu'un editeur la presente.
//
// ELLE EST DERIVEE DU CATALOGUE, jamais ecrite. Aucun nom de type de noeud
// n'apparait dans ce fichier ni dans son corps : ajouter un noeud au catalogue
// le fait apparaitre ici sans qu'une ligne d'interface soit touchee, et c'est
// la seule propriete que ce fichier existe pour tenir. Une liste recopiee
// divergerait au premier noeud ajoute -- c'est le defaut que la fabrique de
// relecture a deja evite du meme cote.
//
// Le regroupement suit l'ordre de DECLARATION du catalogue, categorie par
// categorie. Un tri alphabetique retirerait a l'auteur du catalogue le controle
// de ce que l'utilisateur voit en premier, sans rien garantir de plus.
//
#include <cstddef>
#include <string>
#include <vector>

namespace cggraph_ui
{

struct PaletteItem
{
	// Chaines detenues par le catalogue, dont la duree de vie est celle du
	// programme. La palette n'en recopie aucune.
	const char *typeName = nullptr;
	const char *label = nullptr;

	// Ce que le corps emballe fait et que sa declaration ne dit pas. Nul vaut
	// affirmation d'absence, et un editeur qui le tait perdrait la seule trace
	// que la couche B en porte.
	const char *caveat = nullptr;
};

struct PaletteCategory
{
	std::string name;
	std::vector<PaletteItem> items;
};

// Categorie des entrees dont le catalogue n'en nomme aucune. Elle existe pour
// qu'un noeud sans categorie reste VISIBLE : le faire disparaitre de la palette
// serait le pire des deux comportements.
extern const char *const kUncategorized;

class Palette
{
public:
	Palette ();

	const std::vector<PaletteCategory> &GetCategories () const { return m_categories; }

	std::size_t GetItemCount () const;

	// Nul si le type ne figure pas au catalogue.
	const PaletteItem *Find (const std::string &typeName) const;

private:
	std::vector<PaletteCategory> m_categories;
};

} // namespace cggraph_ui
