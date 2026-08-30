#pragma once
//
//  Description des types qui transitent sur les liens du graphe.
//
// Le moteur ne connait AUCUN type applicatif : il n'en manipule que le
// descripteur. C'est ce qui lui permet de se compiler et de se tester seul,
// sans tirer la moindre bibliotheque de domaine.
//
// Un TypeDesc est enregistre une fois par un module de domaine, et sa duree de
// vie est celle du registre. Les ports et les valeurs n'en retiennent que
// l'adresse : l'egalite de type est donc une comparaison de pointeurs.
//
#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>

namespace cggraph
{

struct TypeDesc
{
	// Forkable : un noeud aval a une raison legitime de modifier la valeur en
	// place apres l'avoir recue, donc le type doit savoir se cloner.
	// Immutable : la valeur est produite une fois et lue ensuite ; `clone` reste
	// nul, et ce n'est pas un trou a combler -- certains types ne sont pas
	// copiables par declaration.
	enum Mutability { Forkable, Immutable };

	// Identifiant stable, prefixe par son domaine ("domaine.Type"). Il est
	// serialise : le renommer casse les documents deja ecrits.
	std::string name;

	// Nul pour un type Immutable, obligatoire pour un type Forkable.
	std::shared_ptr<void> (*clone) (const void *value) = nullptr;

	// Nul si le type ne sait pas se mesurer ; le cache lit alors 0.
	std::size_t (*sizeHint) (const void *value) = nullptr;

	Mutability mutability = Immutable;
};

class TypeRegistry
{
public:
	// Rend le descripteur enregistre, ou nullptr en cas de refus : nom vide,
	// nom deja pris, ou type Forkable sans `clone`.
	const TypeDesc *Register (const TypeDesc &desc);

	const TypeDesc *Find (const std::string &name) const;

	std::size_t GetCount () const { return m_byName.size (); }

private:
	// deque, et non vector : les descripteurs sont distribues par adresse, donc
	// aucune croissance du registre ne doit les deplacer.
	std::deque<TypeDesc> m_storage;
	std::unordered_map<std::string, const TypeDesc *> m_byName;
};

} // namespace cggraph
