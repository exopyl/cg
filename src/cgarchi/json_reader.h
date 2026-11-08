#pragma once

// Lecture stricte des documents JSON de cgarchi, commune a la fenetre gothique
// (parameterized_gothic_json.cpp), a la microarchitecture
// (gothic_microarchitecture_json.cpp) et a la colonne classique
// (parameterized_classical_json.cpp).
//
//   - champ hors schema refuse (additionalProperties false) ;
//   - type et domaine controles ; un nombre non fini est refuse ;
//   - chaque refus est nomme par le chemin JSON du champ : « geometry.width »,
//     « points[0].id », chemin vide pour le document entier ;
//   - version : "schema": "nom/vN" (D2-8).
//
// Le lecteur commun ne connait pas le rapport : il classe chacun de SES refus
// (JsonFault) et le passe a OnFault, que le lecteur concret traduit dans son
// propre code d'erreur. Les refus propres a chaque lecteur concret restent
// locaux et vont directement a son rapport.

#include <cstddef>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace cgarchi::json_reader
{

enum class JsonFault
{
	Schema,    // champ inconnu, schema non pris en charge
	Type,      // type faux
	Value,     // hors domaine, non fini, valeur d'enumeration inconnue
	Missing,   // champ requis absent
};

// Doublons nommes par leur chemin dans ParseRejectingDuplicateKeys. Un lecteur
// dont le rapport garde au plus ce nombre d'entrees n'en perd aucune : les
// doublons suivants n'y sont que comptes.
inline constexpr std::size_t kDuplicateKeyPathLimit = 64;

// Chemin d'un membre : « parent.key », ou « key » a la racine.
std::string Join (const std::string &parent, const std::string &key);

// Chemin d'un element : « array[i] ».
std::string Indexed (const std::string &array, std::size_t i);

// Nombre d'un message : notation d'ostream, `precision` chiffres significatifs,
// locale classique (point decimal) quelle que soit la locale globale.
std::string NumberText (double v, int precision);

// Membre `key` de l'objet `obj`, nullptr s'il est absent.
const nlohmann::json *Member (const nlohmann::json &obj, const char *key);

// Domaine d'un nombre au sens de JSON Schema (minimum, exclusiveMinimum...).
struct Bounds
{
	double lo = -std::numeric_limits<double>::infinity();
	double hi =  std::numeric_limits<double>::infinity();
	bool   loOpen = false;
	bool   hiOpen = false;

	bool Contains (double v) const
	{
		return (loOpen ? v > lo : v >= lo) && (hiOpen ? v < hi : v <= hi);
	}

	// « [lo, hi] », crochet ouvert « ( » ou « ) » pour une borne exclue.
	std::string Text (int precision) const;
};

Bounds AnyNumber ();
Bounds AtLeast (double lo);
Bounds Above (double lo);
Bounds Interval (double lo, bool loOpen, double hi, bool hiOpen);

struct SchemaRead;

// Lecture typee d'un champ. Chaque accesseur rend true et ecrit `out`
// seulement si le champ est present et valide ; absent, il rend false sans
// rien signaler. Present et invalide, il signale un refus et rend false.
class StrictReader
{
public:
	StrictReader (const StrictReader &) = delete;
	StrictReader &operator= (const StrictReader &) = delete;

	// json::parse, et chaque cle repetee dans un objet est un refus Schema
	// « duplicate key », nomme par son chemin ; la derniere valeur est gardee.
	// Au-dela de kDuplicateKeyPathLimit doublons, le refus est transmis avec un
	// chemin vide : il reste compte, et le chemin n'est plus copie.
	// Deux passes : une lecture SAX signale les doublons, puis json::parse
	// construit le document. Cout lineaire en la taille du texte, plus la copie
	// d'au plus kDuplicateKeyPathLimit chemins. Leve les json::exception de
	// json::parse, apres les refus des doublons qui precedent l'erreur ; une
	// exception levee par OnFault (std::bad_alloc) traverse aussi l'appel.
	nlohmann::json ParseRejectingDuplicateKeys (const std::string &text);

	// « expected <expected>, not <type> ».
	void TypeError (const std::string &path, const char *expected, const nlohmann::json &value);

	// Chaque membre de `obj` absent de `names` : refus Schema « unknown field ».
	// Precondition : `obj` est un objet.
	void Known (const nlohmann::json &obj, const std::string &parent, std::initializer_list<const char *> names);

	// Chaque nom absent de `obj` : refus Missing. Rend false si l'un manque.
	bool Require (const nlohmann::json &obj, const std::string &parent, std::initializer_list<const char *> names);

	// Membre objet, nullptr s'il est absent ou d'un autre type.
	const nlohmann::json *Object (const nlohmann::json &obj, const std::string &parent, const char *key);

	// Nombre fini, sans controle de domaine.
	bool Finite (const nlohmann::json &obj, const std::string &parent, const char *key, double &out);

	bool Number (const nlohmann::json &obj, const std::string &parent, const char *key, const Bounds &bounds,
	             double &out);

	// Reglage float : `x` doit rester fini une fois arrondi en float (refus
	// Value « is not finite as a float »).
	bool ToFloat (const std::string &path, double x, float &out);

	// Nombre fini lu dans un reglage float. Le domaine est controle sur la
	// valeur arrondie en float, celle qui est posee, contre les bornes arrondies
	// en float : une borne incluse non representable (0.3) reste acceptee.
	bool Float (const nlohmann::json &obj, const std::string &parent, const char *key, const Bounds &bounds,
	            float &out);

	// Entier au sens de JSON Schema : 2 et 2.0 le sont, 2.5 non.
	bool Integer (const nlohmann::json &obj, const std::string &parent, const char *key, int lo, int hi, int &out);

	bool Boolean (const nlohmann::json &obj, const std::string &parent, const char *key, bool &out);

	bool String (const nlohmann::json &obj, const std::string &parent, const char *key, std::string &out);

	// `index` recoit la position de la valeur dans `names`.
	bool Enum (const nlohmann::json &obj, const std::string &parent, const char *key,
	           std::initializer_list<const char *> names, int &index);

protected:
	// `precision` : chiffres significatifs des nombres cites dans les messages.
	explicit StrictReader (int precision) : m_precision(precision) {}
	~StrictReader () = default;

	void Fault (JsonFault kind, const std::string &path, const std::string &message);

private:
	friend SchemaRead ReadSchema (StrictReader &r, const nlohmann::json &doc, const char *expected);

	virtual void OnFault (JsonFault kind, const std::string &path, const std::string &message) = 0;

	int m_precision;
};

// Identifiant de schema « nom/vN » : nom non vide et sans « / » (espaces et
// caracteres non ASCII compris), N entier decimal >= 1 sans zero de tete.
struct SchemaId
{
	std::string name;
	int         version = 0;
};

// nullopt si `text` n'est pas de la forme « nom/vN ».
std::optional<SchemaId> ParseSchemaId (std::string_view text);

enum class SchemaStatus
{
	Absent,        // pas de champ `schema`
	NotAString,    // refus Type, deja signale
	Expected,      // la valeur attendue
	Unsupported,   // une autre valeur ; refus Schema, deja signale
};

struct SchemaRead
{
	SchemaStatus            status = SchemaStatus::Absent;
	std::optional<SchemaId> id;   // valeur lue, si elle est de la forme « nom/vN »
};

// Champ `schema` de la racine `doc`, compare a `expected` (« nom/vN »).
SchemaRead ReadSchema (StrictReader &r, const nlohmann::json &doc, const char *expected);

} // namespace cgarchi::json_reader
