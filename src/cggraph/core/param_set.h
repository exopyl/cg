#pragma once
//
//  Parametres d'un noeud.
//
// Deux proprietes structurelles dictent la forme de ce fichier :
//
//  - une valeur est Literal OU Driven. La branche Driven est stockee et refusee
//    a l'evaluation. Elle existe des maintenant parce qu'ajouter la variante
//    plus tard toucherait le stockage, la serialisation, le hachage de
//    signature et la projection vers les panneaux -- quatre mecanismes deja
//    ecrits ; ajouter l'evaluateur d'expression plus tard ne touche que le code
//    qui lit une valeur ;
//  - le stockage est un deque. La projection vers les panneaux distribue des
//    pointeurs bruts sur les valeurs, en lecture ET en ecriture : une
//    reallocation ne rendrait pas une valeur perimee, elle ecrirait dans de la
//    memoire liberee. deque ne deplace pas ses elements existants, donc la
//    propriete ne depend d'aucune capacite a calculer ni d'aucune assertion a
//    maintenir.
//
#include <cstddef>
#include <deque>
#include <string>

namespace cggraph
{

enum class ParamKind
{
	Literal,
	Driven
};

enum class ParamType
{
	Int,
	Float,
	Bool,
	String
};

// Drapeau `invalidate`, un par parametre : un parametre non semantique --
// verbosite, chemin de log, nombre de fils -- ne doit pas invalider un calcul
// long. Hacher le jeu entier est faux des qu'un noeud en expose un.
enum class ParamRole
{
	Semantic,
	NonSemantic
};

// Axe ORTHOGONAL au role, et les deux sont necessaires. Le role decide si un
// parametre entre dans la signature, donc s'il invalide un calcul ; la
// visibilite decide s'il se regle a la main. `source.identity` porte les deux a
// la fois -- Semantic parce que c'est lui qui invalide le cache quand le fichier
// change, Internal parce que personne ne le tape au clavier : c'est de la tenue
// de livre que le noeud ecrit lui-meme. Un axe unique forcerait a sacrifier
// l'une des deux proprietes.
//
// Defaut Public : un parametre qui ne dit rien se montre.
enum class ParamVisibility
{
	Public,
	Internal
};

struct ParamValue
{
	ParamKind kind = ParamKind::Literal;
	ParamType type = ParamType::Int;

	// Literal : `type` designe celui de ces quatre champs qui porte la valeur.
	// Ce sont des membres et non une variante afin que leur adresse soit
	// distribuable a un panneau.
	int intValue = 0;
	float floatValue = 0.0f;
	bool boolValue = false;
	std::string stringValue;

	// Driven : la source de la valeur. Elle se stocke, se serialise et se hache
	// sans que personne sache l'evaluer.
	std::string expression;
};

struct ParamEntry
{
	std::string name;
	ParamRole role = ParamRole::Semantic;
	ParamVisibility visibility = ParamVisibility::Public;
	ParamValue value;
};

class ParamSet
{
public:
	// Les cinq mutateurs creent l'entree ou la remplacent en place, et rendent
	// une adresse qui ne bougera plus tant que le jeu vit. Nul si le nom est
	// vide. Remplacer une entree change son type : l'adresse survit, la valeur
	// qu'elle designe non.
	//
	// Le role ET la visibilite sont repris a chaque appel : un noeud qui reecrit
	// un parametre interne depuis son propre code doit le redire interne, sans
	// quoi la reecriture le rendrait editable.
	ParamValue *SetInt (const std::string &name, int value, ParamRole role = ParamRole::Semantic,
	                    ParamVisibility visibility = ParamVisibility::Public);
	ParamValue *SetFloat (const std::string &name, float value, ParamRole role = ParamRole::Semantic,
	                      ParamVisibility visibility = ParamVisibility::Public);
	ParamValue *SetBool (const std::string &name, bool value, ParamRole role = ParamRole::Semantic,
	                     ParamVisibility visibility = ParamVisibility::Public);
	ParamValue *SetString (const std::string &name, const std::string &value,
	                       ParamRole role = ParamRole::Semantic,
	                       ParamVisibility visibility = ParamVisibility::Public);
	ParamValue *SetDriven (const std::string &name, ParamType type, const std::string &expression,
	                       ParamRole role = ParamRole::Semantic,
	                       ParamVisibility visibility = ParamVisibility::Public);

	// Reecrit la CHAINE d'une entree qui existe deja et qui porte deja une valeur
	// litterale de type String -- rien d'autre : ni le nom, ni le role, ni la
	// visibilite, ni le genre, ni le type. Faux si l'entree manque ou n'a pas
	// cette forme, auquel cas l'appelant pose l'entree par SetString.
	//
	// Elle existe pour la relevee d'etat exterieur (Node::RefreshExternalState),
	// qui est la SEULE ecriture de parametre pouvant survenir pendant une
	// evaluation. SetString y passerait par Touch, qui remet la valeur entiere a
	// zero -- donc ecrit `kind` et `type`, que l'evaluateur lit hors de la
	// pre-passe pour refuser un parametre Driven. Restreindre l'ecriture au seul
	// champ chaine fait que les deux ne designent plus le meme emplacement
	// memoire, et la lecture concurrente cesse d'etre une course.
	bool UpdateString (const std::string &name, const std::string &value);

	// Vide le jeu. La relecture d'un document s'en sert : c'est le DOCUMENT qui
	// fait foi, jamais ce que le constructeur du noeud avait pose, sans quoi un
	// parametre absent du fichier reapparaitrait a la re-sauvegarde et
	// l'aller-retour ne serait pas fidele. Invalide toutes les adresses
	// distribuees -- une relecture precede par construction toute projection.
	void Clear ();

	const ParamValue *Find (const std::string &name) const;
	ParamValue *Find (const std::string &name);

	// Premiere entree Driven dans l'ordre de declaration, nulle s'il n'y en a
	// pas. L'evaluateur s'en sert pour nommer le parametre qu'il refuse.
	const ParamEntry *FindDriven () const;

	// Entree complete, role et visibilite compris. Find() ne rend que la valeur,
	// qui ne les porte pas.
	const ParamEntry *FindEntry (const std::string &name) const;

	std::size_t GetCount () const { return m_entries.size (); }

	const std::deque<ParamEntry> &GetEntries () const { return m_entries; }

private:
	ParamEntry *Touch (const std::string &name, ParamRole role, ParamVisibility visibility);

	std::deque<ParamEntry> m_entries;
};

} // namespace cggraph
