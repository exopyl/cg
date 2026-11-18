// Document du monument "monument/v14" : lecteur strict a exception, documents
// embarques, conversion vers les reglages du temple et mode partiel.
#include <cgarchi/classical_monument.h>

#include <cgarchi/classical_monument_data.h>
#include <cgarchi/json_reader.h>
#include "mesh.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace cgarchi
{
namespace classical
{

namespace
{
	using nlohmann::json;
	using json_reader::JsonFault;

	// Chiffres significatifs des nombres cites dans les messages.
	constexpr int kNumberPrecision = 6;

	constexpr int kCountMax = std::numeric_limits<int>::max();

	// Mots de statut du README, dans l'ordre de MonumentStatus, en octets UTF-8 : le texte
	// ne depend pas du jeu de caracteres d'execution du compilateur (deduit et mesure, accentues).
	constexpr const char *kStatusNames[] = {"lu", "d\303\251duit", "mesur\303\251", "convention", "manquant"};

	// Unites du document, dans l'ordre de MonumentUnit, en octets UTF-8 (degre).
	constexpr const char *kUnitNames[] = {"m", "in", "mm", "1", "ft", "ft+in", "\302\260", "ratio"};
	static_assert(std::size(kUnitNames) == static_cast<std::size_t>(MonumentUnit::Ratio) + 1,
	              "un nom par MonumentUnit, dans l'ordre");

	// Mots de la division du triglyphe, dans l'ordre de MonumentDivisionKind.
	constexpr const char *kDivisionKindNames[] = {"demi-canal", "bande", "canal"};

	// Mots de la suite des hauteurs de la corniche rampante, dans l'ordre de MonumentRakingMember.
	constexpr const char *kRakingMemberNames[] = {"listel", "filet", "larmier", "gorge de la sous-face (bec)"};

	// Suite R5 du script : mots admis et noms employes par ses refus.
	struct SeriesWords
	{
		const char *const *words;
		int                count;
		const char        *valueNoun;    // "width" : "not a finite width > 0"
		const char        *kindNoun;     // "division kind" : "unknown division kind \"x\""
		const char        *seriesNoun;   // "a division series" : "... is never \"manquant\""
	};
	static_assert(std::size(kDivisionKindNames) == static_cast<std::size_t>(MonumentDivisionKind::Channel) + 1,
	              "un mot par MonumentDivisionKind, dans l'ordre");
	static_assert(std::size(kRakingMemberNames) == static_cast<std::size_t>(MonumentRakingMember::Drip) + 1,
	              "un mot par MonumentRakingMember, dans l'ordre");
	constexpr SeriesWords kDivisionWords = {kDivisionKindNames, static_cast<int>(std::size(kDivisionKindNames)), "width",
	                                        "division kind", "a division series"};
	constexpr SeriesWords kRakingWords = {kRakingMemberNames, static_cast<int>(std::size(kRakingMemberNames)), "height",
	                                      "raking member", "a raking height series"};

	// Angle : en degres ; Ratio : rapport "1 : r".
	enum class Quantity { Length, Count, Angle, Ratio };

	// Lecteur strict commun ; chaque refus est garde, puis leve en une fois.
	class Reader final : public json_reader::StrictReader
	{
	public:
		Reader () : StrictReader(kNumberPrecision) {}

		void Error (const std::string &path, const std::string &message)
		{
			m_faults.push_back(std::string(kMonumentSchema) + " " + path + " : " + message);
		}

		bool Failed () const { return !m_faults.empty(); }

		[[noreturn]] void Throw () const
		{
			std::string text;
			for (const std::string &f : m_faults)
				text += (text.empty() ? "" : "\n") + f;
			throw std::runtime_error(text);
		}

	private:
		void OnFault (JsonFault, const std::string &path, const std::string &message) override
		{
			Error(path, message);
		}

		std::vector<std::string> m_faults;
	};

	// StrictReader::Known sur une liste construite a l'execution : chaque membre de `obj` absent de `names` est
	// refuse, « unknown field ».
	void KnownKeys (Reader &r, const json &obj, const std::string &parent, const std::vector<const char *> &names)
	{
		for (auto it = obj.begin(); it != obj.end(); ++it)
			if (std::none_of(names.begin(), names.end(), [&](const char *n) { return it.key() == n; }))
				r.Error(json_reader::Join(parent, it.key()), "unknown field");
	}

	// Membre objet requis : nullptr s'il manque ou n'est pas un objet (refus deja signale).
	const json *RequiredObject (Reader &r, const json &obj, const std::string &parent, const char *key)
	{
		if (!r.Require(obj, parent, {key}))
			return nullptr;
		return r.Object(obj, parent, key);
	}

	// Bloc facultatif a la racine : nullptr s'il est absent ou n'est pas un objet (refus deja signale) ;
	// `present` vrai s'il est un objet.
	const json *OptionalBlock (Reader &r, const json &doc, const char *key, bool &present)
	{
		if (!json_reader::Member(doc, key))
			return nullptr;
		const json *block = r.Object(doc, std::string(), key);
		present = block != nullptr;
		return block;
	}

	// `readme` : chaine non vide.
	void ReadPlace (Reader &r, const json &obj, const std::string &path, std::string &out)
	{
		if (r.String(obj, path, "readme", out) && out.empty())
			r.Error(json_reader::Join(path, "readme"), "empty place");
	}

	// `unit` : une des huit ; "1" pour un compte et lui seul, "°" pour un angle et lui seul, "ratio" pour un
	// rapport et lui seul ; "ft+in" pour une longueur dont `pairAllowed` admet la cote a deux nombres (une valeur,
	// et non une suite, un polygone, un controle ou un profil).
	bool ReadUnit (Reader &r, const json &obj, const std::string &path, Quantity quantity, MonumentUnit &out,
	               bool pairAllowed = false)
	{
		int unit = -1;
		if (!r.Enum(obj, path, "unit",
		            {kUnitNames[0], kUnitNames[1], kUnitNames[2], kUnitNames[3], kUnitNames[4], kUnitNames[5],
		             kUnitNames[6], kUnitNames[7]},
		            unit))
			return false;
		out = static_cast<MonumentUnit>(unit);
		if (quantity == Quantity::Count && out != MonumentUnit::One)
		{
			r.Error(json_reader::Join(path, "unit"), std::string("a count requires unit \"1\", not \"") +
			                                             kUnitNames[unit] + "\"");
			return false;
		}
		if (quantity == Quantity::Length && out == MonumentUnit::One)
		{
			r.Error(json_reader::Join(path, "unit"), "unit \"1\" on a length");
			return false;
		}
		if (quantity == Quantity::Length && out == MonumentUnit::FootInch && !pairAllowed)
		{
			r.Error(json_reader::Join(path, "unit"), "unit \"ft+in\" outside a length value");
			return false;
		}
		if (quantity == Quantity::Length && out == MonumentUnit::Degree)
		{
			r.Error(json_reader::Join(path, "unit"), std::string("unit \"") + kUnitNames[unit] + "\" on a length");
			return false;
		}
		if (quantity == Quantity::Angle && out != MonumentUnit::Degree)
		{
			r.Error(json_reader::Join(path, "unit"), std::string("an angle requires unit \"") +
			                                             kUnitNames[static_cast<int>(MonumentUnit::Degree)] +
			                                             "\", not \"" + kUnitNames[unit] + "\"");
			return false;
		}
		if (quantity == Quantity::Ratio && out != MonumentUnit::Ratio)
		{
			r.Error(json_reader::Join(path, "unit"), std::string("a ratio requires unit \"") +
			                                             kUnitNames[static_cast<int>(MonumentUnit::Ratio)] +
			                                             "\", not \"" + kUnitNames[unit] + "\"");
			return false;
		}
		if (quantity != Quantity::Ratio && out == MonumentUnit::Ratio)
		{
			r.Error(json_reader::Join(path, "unit"), "unit \"ratio\" outside a ratio");
			return false;
		}
		return true;
	}

	// Statut d'une valeur ou d'une suite ; false si absent ou hors des cinq.
	bool ReadStatus (Reader &r, const json &obj, const std::string &path, MonumentStatus &out)
	{
		int status = -1;
		if (!r.Enum(obj, path, "status",
		            {kStatusNames[0], kStatusNames[1], kStatusNames[2], kStatusNames[3], kStatusNames[4]}, status))
			return false;
		out = static_cast<MonumentStatus>(status);
		return true;
	}

	// Liste de nombres finis ; refus nomme a chaque element fautif.
	void ReadNumbers (Reader &r, const json &obj, const std::string &path, const char *key, std::vector<double> &out)
	{
		const json *list = json_reader::Member(obj, key);
		if (!list)
			return;
		const std::string listPath = json_reader::Join(path, key);
		if (!list->is_array())
		{
			r.TypeError(listPath, "an array", *list);
			return;
		}
		if (list->empty())
			r.Error(listPath, "empty array");
		for (std::size_t i = 0; i < list->size(); ++i)
		{
			const json &x = (*list)[i];
			if (!x.is_number())
			{
				r.TypeError(json_reader::Indexed(listPath, i), "a number", x);
				continue;
			}
			out.push_back(x.get<double>());
		}
	}

	class DocumentReader
	{
	public:
		// `table` : la table des sources, lue avant les valeurs.
		DocumentReader (Reader &r, const std::map<std::string, std::string> &table) : m_r(r), m_table(table) {}

		void Value (const json &section, const std::string &parent, const char *key, Quantity quantity,
		            MonumentValue &out)
		{
			const json *v = RequiredObject(m_r, section, parent, key);
			if (!v)
				return;
			const std::string path = json_reader::Join(parent, key);
			m_r.Known(*v, path, {"value", "unit", "status", "sources", "readme"});
			m_r.Require(*v, path, {"unit", "status", "sources", "readme"});
			const bool hasUnit = ReadUnit(m_r, *v, path, quantity, out.unit, true);

			const bool hasStatus = ReadStatus(m_r, *v, path, out.status);
			ReadPlace(m_r, *v, path, out.readme);

			const bool hasSources = ReadSources(*v, path, out.sources);
			const bool missing = hasStatus && out.status == MonumentStatus::Missing;
			if (hasStatus && hasSources)
				CheckSources(path, out.status, out.sources);

			const json *value = json_reader::Member(*v, "value");
			if (hasStatus && missing && value)
			{
				m_r.Error(json_reader::Join(path, "value"), "status \"manquant\" admits no value");
				return;
			}
			if (hasStatus && !missing && !value)
			{
				m_r.Error(json_reader::Join(path, "value"), "required field is missing");
				return;
			}
			if (!value)
				return;
			if (hasUnit && out.unit == MonumentUnit::FootInch)
				FeetAndInches(*value, json_reader::Join(path, "value"), out);
			else if (quantity == Quantity::Count)
			{
				int n = 0;
				if (m_r.Integer(*v, path, "value", 1, kCountMax, n))
					out.value = n;
			}
			else
			{
				double x = 0.0;
				if (m_r.Number(*v, path, "value", json_reader::Above(0.0), x))
					out.value = x;
			}
		}

		// Suite de valeurs (regle R5 du script) : values > 0, kinds parmi les mots de `w`, meme
		// taille, longueur avec son unite ; jamais manquante. Kind : enumeration dans l'ordre des mots.
		template <typename Kind>
		void Series (const json &section, const std::string &parent, const char *key, const SeriesWords &w,
		             std::vector<double> &values, std::vector<Kind> &kindsOut, MonumentUnit &unit,
		             MonumentStatus &status, std::vector<std::string> &sources, std::string &readme)
		{
			const json *v = RequiredObject(m_r, section, parent, key);
			if (!v)
				return;
			const std::string path = json_reader::Join(parent, key);
			m_r.Known(*v, path, {"values", "kinds", "unit", "status", "sources", "readme"});
			m_r.Require(*v, path, {"values", "kinds", "unit", "status", "sources", "readme"});
			ReadUnit(m_r, *v, path, Quantity::Length, unit);
			const bool hasStatus = ReadStatus(m_r, *v, path, status);
			if (hasStatus && status == MonumentStatus::Missing)
				m_r.Error(json_reader::Join(path, "status"), std::string(w.seriesNoun) + " is never \"manquant\"");
			ReadPlace(m_r, *v, path, readme);
			if (ReadSources(*v, path, sources) && hasStatus)
				CheckSources(path, status, sources);

			ReadNumbers(m_r, *v, path, "values", values);
			const std::string valuesPath = json_reader::Join(path, "values");
			for (std::size_t i = 0; i < values.size(); ++i)
				if (!(std::isfinite(values[i]) && values[i] > 0.0))
					m_r.Error(json_reader::Indexed(valuesPath, i), std::string("not a finite ") + w.valueNoun + " > 0");

			const json *kinds = json_reader::Member(*v, "kinds");
			if (!kinds)
				return;
			const std::string kindsPath = json_reader::Join(path, "kinds");
			if (!kinds->is_array())
			{
				m_r.TypeError(kindsPath, "an array", *kinds);
				return;
			}
			if (kinds->empty())
				m_r.Error(kindsPath, "empty array");
			for (std::size_t i = 0; i < kinds->size(); ++i)
			{
				const json &word = (*kinds)[i];
				const std::string itemPath = json_reader::Indexed(kindsPath, i);
				if (!word.is_string())
				{
					m_r.TypeError(itemPath, "a string", word);
					continue;
				}
				const std::string text = word.get<std::string>();
				bool known = false;
				for (int k = 0; k < w.count; ++k)
					if (text == w.words[k])
					{
						kindsOut.push_back(static_cast<Kind>(k));
						known = true;
					}
				if (!known)
					m_r.Error(itemPath, std::string("unknown ") + w.kindNoun + " \"" + text + "\"");
			}
			if (kinds->size() != values.size())
				m_r.Error(kindsPath, "not as many kinds as values");
		}

		void Divisions (const json &section, const std::string &parent, const char *key, MonumentDivisions &out)
		{
			Series(section, parent, key, kDivisionWords, out.values, out.kinds, out.unit, out.status, out.sources,
			       out.readme);
		}

		void RakingHeights (const json &section, const std::string &parent, const char *key, MonumentRakingHeights &out)
		{
			Series(section, parent, key, kRakingWords, out.values, out.kinds, out.unit, out.status, out.sources,
			       out.readme);
		}

		// Polygone (regle R11 du script) : listes u et h de meme taille, au moins 3 sommets, coordonnees finies et
		// >= 0, longueur avec son unite ; jamais manquant. Simplicite et sens sont ceux du consommateur.
		void Polygon (const json &section, const std::string &parent, const char *key, MonumentPolygon &out)
		{
			const json *v = RequiredObject(m_r, section, parent, key);
			if (!v)
				return;
			const std::string path = json_reader::Join(parent, key);
			m_r.Known(*v, path, {"u", "h", "unit", "status", "sources", "readme"});
			m_r.Require(*v, path, {"u", "h", "unit", "status", "sources", "readme"});
			ReadUnit(m_r, *v, path, Quantity::Length, out.unit);
			const bool hasStatus = ReadStatus(m_r, *v, path, out.status);
			if (hasStatus && out.status == MonumentStatus::Missing)
				m_r.Error(json_reader::Join(path, "status"), "a polygon is never \"manquant\"");
			ReadPlace(m_r, *v, path, out.readme);
			if (ReadSources(*v, path, out.sources) && hasStatus)
				CheckSources(path, out.status, out.sources);

			const bool hasU = json_reader::Member(*v, "u") != nullptr, hasH = json_reader::Member(*v, "h") != nullptr;
			ReadNumbers(m_r, *v, path, "u", out.u);
			ReadNumbers(m_r, *v, path, "h", out.h);
			for (const auto &[name, list] : {std::make_pair("u", &out.u), std::make_pair("h", &out.h)})
			{
				const std::string listPath = json_reader::Join(path, name);
				for (std::size_t i = 0; i < list->size(); ++i)
					if (!(std::isfinite((*list)[i]) && (*list)[i] >= 0.0))
						m_r.Error(json_reader::Indexed(listPath, i), "not a finite coordinate >= 0");
			}
			if (!hasU || !hasH)
				return;
			if (out.h.size() != out.u.size())
				m_r.Error(json_reader::Join(path, "h"), "not as many h as u");
			else if (!out.u.empty() && out.u.size() < 3)
				m_r.Error(json_reader::Join(path, "u"), "fewer than 3 vertices");
		}

		const std::set<std::string> &Cited () const { return m_cited; }

	private:
		// Cote "ft+in" (regle R6 du script) : [pieds, pouces], pieds entiers de [1, kCountMax], pouces finis >= 0.
		void FeetAndInches (const json &pair, const std::string &valuePath, MonumentValue &out)
		{
			if (!pair.is_array())
			{
				m_r.TypeError(valuePath, "an array [feet, inches]", pair);
				return;
			}
			if (pair.size() != 2)
			{
				m_r.Error(valuePath, "expected [feet, inches], not " + std::to_string(pair.size()) + " element(s)");
				return;
			}
			bool ok = true;
			const json &feet = pair[0], &inches = pair[1];
			const std::string feetPath = json_reader::Indexed(valuePath, 0);
			const std::string inchesPath = json_reader::Indexed(valuePath, 1);
			const double ft = feet.is_number() ? feet.get<double>() : 0.0;
			if (!feet.is_number() || !std::isfinite(ft) || std::floor(ft) != ft)
			{
				m_r.TypeError(feetPath, "an integer", feet);
				ok = false;
			}
			else if (ft < 1.0 || ft > kCountMax)
			{
				m_r.Error(feetPath, json_reader::NumberText(ft, kNumberPrecision) + " outside [1, " +
				                        std::to_string(kCountMax) + "]");
				ok = false;
			}
			const double in = inches.is_number() ? inches.get<double>() : 0.0;
			if (!inches.is_number())
			{
				m_r.TypeError(inchesPath, "a number", inches);
				ok = false;
			}
			else if (!(std::isfinite(in) && in >= 0.0))
			{
				m_r.Error(inchesPath, json_reader::NumberText(in, kNumberPrecision) + " outside " +
				                          json_reader::AtLeast(0.0).Text(kNumberPrecision));
				ok = false;
			}
			if (!ok)
				return;
			out.value = ft;
			out.inches = in;
		}

		// "lu" et "mesure" citent une source au moins ; "manquant" n'en cite aucune.
		void CheckSources (const std::string &path, MonumentStatus status, const std::vector<std::string> &sources)
		{
			const bool needsSource = status == MonumentStatus::Read || status == MonumentStatus::Measured;
			if (needsSource && sources.empty())
				m_r.Error(json_reader::Join(path, "sources"),
				          std::string("status \"") + kStatusNames[static_cast<int>(status)] + "\" requires a source");
			if (status == MonumentStatus::Missing && !sources.empty())
				m_r.Error(json_reader::Join(path, "sources"), "status \"manquant\" admits no source");
		}

		bool ReadSources (const json &v, const std::string &path, std::vector<std::string> &out)
		{
			const json *list = json_reader::Member(v, "sources");
			if (!list)
				return false;
			const std::string listPath = json_reader::Join(path, "sources");
			if (!list->is_array())
			{
				m_r.TypeError(listPath, "an array", *list);
				return false;
			}
			bool ok = true;
			for (std::size_t i = 0; i < list->size(); ++i)
			{
				const json &id = (*list)[i];
				if (!id.is_string())
				{
					m_r.TypeError(json_reader::Indexed(listPath, i), "a string", id);
					ok = false;
					continue;
				}
				out.push_back(id.get<std::string>());
				m_cited.insert(out.back());
				if (m_table.find(out.back()) == m_table.end())
				{
					m_r.Error(json_reader::Indexed(listPath, i), "unknown source \"" + out.back() + "\"");
					ok = false;
				}
			}
			return ok;
		}

		Reader                                   &m_r;
		const std::map<std::string, std::string> &m_table;
		std::set<std::string> m_cited;
	};

	void ReadSourceDocument (Reader &r, const json &doc, MonumentSourceDocument &out)
	{
		const json *s = RequiredObject(r, doc, std::string(), "source");
		if (!s)
			return;
		const std::string path = "source";
		r.Known(*s, path, {"document", "sha256", "bytes", "derivedBy"});
		r.Require(*s, path, {"document", "sha256", "bytes", "derivedBy"});
		r.String(*s, path, "document", out.document);
		r.String(*s, path, "derivedBy", out.derivedBy);
		if (r.String(*s, path, "sha256", out.sha256))
		{
			static const std::regex kSha256("[0-9a-f]{64}");
			if (!std::regex_match(out.sha256, kSha256))
				r.Error("source.sha256", "expected 64 lowercase hexadecimal digits");
		}
		int bytes = 0;
		if (r.Integer(*s, path, "bytes", 1, kCountMax, bytes))
			out.bytes = static_cast<std::uint64_t>(bytes);
	}

	void ReadSourceTable (Reader &r, const json &doc, std::map<std::string, std::string> &out)
	{
		const json *t = RequiredObject(r, doc, std::string(), "sources");
		if (!t)
			return;
		for (auto it = t->begin(); it != t->end(); ++it)
		{
			std::string label;
			if (r.String(*t, "sources", it.key().c_str(), label))
				out.emplace(it.key(), std::move(label));
		}
	}

	void ReadAxes (Reader &r, const json &controls, const char *key, std::vector<double> &out)
	{
		const json *a = RequiredObject(r, controls, "controls", key);
		if (!a)
			return;
		const std::string path = json_reader::Join("controls", key);
		r.Known(*a, path, {"values", "readme"});
		r.Require(*a, path, {"values", "readme"});
		std::string place;
		ReadPlace(r, *a, path, place);
		const json *values = json_reader::Member(*a, "values");
		if (!values)
			return;
		const std::string valuesPath = json_reader::Join(path, "values");
		if (!values->is_array())
		{
			r.TypeError(valuesPath, "an array", *values);
			return;
		}
		if (values->empty())
			r.Error(valuesPath, "empty array");
		for (std::size_t i = 0; i < values->size(); ++i)
		{
			const std::string itemPath = json_reader::Indexed(valuesPath, i);
			const json &x = (*values)[i];
			if (!x.is_number())
			{
				r.TypeError(itemPath, "a number", x);
				continue;
			}
			const double v = x.get<double>();
			// Message de StrictReader::Finite, dont l'acces par cle ne s'applique pas a un element.
			// Garde : nlohmann refuse deja a l'analyse un nombre qui deborde en double.
			if (!std::isfinite(v))
			{
				r.Error(itemPath, "non-finite value");
				continue;
			}
			if (!out.empty() && !(v > out.back()))
				r.Error(itemPath, "not strictly greater than the previous coordinate");
			out.push_back(v);
		}
	}

	void ReadResult (Reader &r, const json &controls, const char *parent, const char *key, double &out)
	{
		const json *s = RequiredObject(r, controls, parent, key);
		if (!s)
			return;
		const std::string path = json_reader::Join(parent, key);
		r.Known(*s, path, {"value", "readme"});
		r.Require(*s, path, {"value", "readme"});
		std::string place;
		ReadPlace(r, *s, path, place);
		r.Finite(*s, path, "value", out);
	}

	// Controle sans statut : longueur ou angle > 0 avec son unite, ou compte entier >= 1 en "1".
	void ReadControl (Reader &r, const json &controls, const char *parent, const char *key, Quantity quantity,
	                  MonumentControlValue &out)
	{
		const json *c = RequiredObject(r, controls, parent, key);
		if (!c)
			return;
		const std::string path = json_reader::Join(parent, key);
		r.Known(*c, path, {"value", "unit", "readme"});
		r.Require(*c, path, {"value", "unit", "readme"});
		std::string place;
		ReadPlace(r, *c, path, place);
		ReadUnit(r, *c, path, quantity, out.unit);
		if (quantity == Quantity::Count)
		{
			int n = 0;
			if (r.Integer(*c, path, "value", 1, kCountMax, n))
				out.value = n;
		}
		else
			r.Number(*c, path, "value", json_reader::Above(0.0), out.value);
	}

	void ReadProfileControl (Reader &r, const json &controls, const char *key, MonumentProfileControl &out)
	{
		const json *p = RequiredObject(r, controls, "columnControls", key);
		if (!p)
			return;
		const std::string path = json_reader::Join("columnControls", key);
		r.Known(*p, path, {"depths", "retreats", "unit", "readme"});
		r.Require(*p, path, {"depths", "retreats", "unit", "readme"});
		std::string place;
		ReadPlace(r, *p, path, place);
		ReadUnit(r, *p, path, Quantity::Length, out.unit);
		ReadNumbers(r, *p, path, "depths", out.depths);
		ReadNumbers(r, *p, path, "retreats", out.retreats);
		if (out.depths.size() != out.retreats.size())
			r.Error(json_reader::Join(path, "retreats"), "not as many retreats as depths");
		for (std::size_t i = 1; i < out.depths.size(); ++i)
			if (!(out.depths[i] > out.depths[i - 1]))
				r.Error(json_reader::Indexed(json_reader::Join(path, "depths"), i),
				        "not strictly greater than the previous depth");
	}

	// Dependances de blocs (MO-P2) : un bloc present exige un autre bloc. Table unique du lecteur
	// (`requires block`), du rapport des parties (ReportMonumentParts) et du retrait d'une partie
	// (MonumentWithoutPart).
	struct BlockDependency
	{
		bool        present;
		const char *block;
		bool        neededPresent;
		const char *needed;
	};

	std::array<BlockDependency, 16> BlockDependencies (const MonumentBlocks &b)
	{
		return {{{b.controls, "controls", b.peristyle, "peristyle"},
		         {b.krepisControls, "krepisControls", b.krepis, "krepis"},
		         {b.columnControls, "columnControls", b.columns, "columns"},
		         {b.entablatureControls, "entablatureControls", b.entablature, "entablature"},
		         {b.pedimentControls, "pedimentControls", b.pediment, "pediment"},
		         {b.cellaControls, "cellaControls", b.cella, "cella"},
		         {b.interior, "interior", b.cella, "cella"},
		         {b.interiorControls, "interiorControls", b.interior, "interior"},
		         {b.epicranitis, "epicranitis", b.cella, "cella"},
		         {b.epicranitisControls, "epicranitisControls", b.epicranitis, "epicranitis"},
		         {b.porchExtension, "porchExtension", b.epicranitis, "epicranitis"},
		         {b.porchExtensionControls, "porchExtensionControls", b.porchExtension, "porchExtension"},
		         {b.eastCeiling, "eastCeiling", b.porchExtension, "porchExtension"},
		         {b.eastCeilingControls, "eastCeilingControls", b.eastCeiling, "eastCeiling"},
		         {b.flankCeilings, "flankCeilings", b.eastCeiling, "eastCeiling"},
		         {b.flankCeilingsControls, "flankCeilingsControls", b.flankCeilings, "flankCeilings"}}};
	}

	// Retire le bloc `block` : drapeau faux, valeurs par defaut (manquantes).
	void ClearBlock (MonumentData &d, std::string_view block)
	{
		MonumentBlocks &p = d.present;
		if (block == "krepis")                      { p.krepis = false; d.krepis = {}; }
		else if (block == "krepisControls")         { p.krepisControls = false; d.krepisControls = {}; }
		else if (block == "peristyle")              { p.peristyle = false; d.peristyle = {}; }
		else if (block == "controls")               { p.controls = false; d.controls = {}; }
		else if (block == "columns")                { p.columns = false; d.columns = {}; }
		else if (block == "columnControls")         { p.columnControls = false; d.columnControls = {}; }
		else if (block == "entablature")            { p.entablature = false; d.entablature = {}; }
		else if (block == "entablatureControls")    { p.entablatureControls = false; d.entablatureControls = {}; }
		else if (block == "pediment")               { p.pediment = false; d.pediment = {}; }
		else if (block == "pedimentControls")       { p.pedimentControls = false; d.pedimentControls = {}; }
		else if (block == "cella")                  { p.cella = false; d.cella = {}; }
		else if (block == "cellaControls")          { p.cellaControls = false; d.cellaControls = {}; }
		else if (block == "interior")               { p.interior = false; d.interior = {}; }
		else if (block == "interiorControls")       { p.interiorControls = false; d.interiorControls = {}; }
		else if (block == "epicranitis")            { p.epicranitis = false; d.epicranitis = {}; }
		else if (block == "epicranitisControls")    { p.epicranitisControls = false; d.epicranitisControls = {}; }
		else if (block == "porchExtension")         { p.porchExtension = false; d.porchExtension = {}; }
		else if (block == "porchExtensionControls") { p.porchExtensionControls = false; d.porchExtensionControls = {}; }
		else if (block == "eastCeiling")            { p.eastCeiling = false; d.eastCeiling = {}; }
		else if (block == "eastCeilingControls")    { p.eastCeilingControls = false; d.eastCeilingControls = {}; }
		else if (block == "flankCeilings")          { p.flankCeilings = false; d.flankCeilings = {}; }
		else if (block == "flankCeilingsControls")  { p.flankCeilingsControls = false; d.flankCeilingsControls = {}; }
		else
			throw std::logic_error("Monument : bloc inconnu");
	}

	void ReadDocument (Reader &r, const json &doc, MonumentData &data)
	{
		if (!doc.is_object())
		{
			r.TypeError(std::string(), "an object", doc);
			return;
		}
		const json_reader::SchemaRead schema = json_reader::ReadSchema(r, doc, kMonumentSchema);
		if (schema.status == json_reader::SchemaStatus::Absent)
			r.Require(doc, std::string(), {"schema"});
		if (schema.status != json_reader::SchemaStatus::Expected)
			return;

		r.Known(doc, std::string(),
		        {"schema", "monument", "source", "sources", "krepis", "peristyle", "columns", "controls",
		         "krepisControls", "columnControls", "entablature", "entablatureControls", "pediment", "pedimentControls",
		         "cella", "cellaControls", "interior", "interiorControls", "epicranitis", "epicranitisControls",
		         "porchExtension", "porchExtensionControls", "eastCeiling", "eastCeilingControls", "flankCeilings",
		         "flankCeilingsControls"});
		r.Require(doc, std::string(), {"monument"});
		r.String(doc, std::string(), "monument", data.monument);
		ReadSourceDocument(r, doc, data.source);
		ReadSourceTable(r, doc, data.sources);

		DocumentReader values(r, data.sources);
		if (const json *k = OptionalBlock(r, doc, "krepis", data.present.krepis))
		{
			const std::string p = "krepis";
			r.Known(*k, p, {"stylobateWidth", "stylobateLength", "stepCount", "stylobateHeight", "secondStepHeight",
			                "thirdStepHeight", "secondStepTread", "thirdStepTread", "cornerMargin", "euthynteriaHeight",
			                "euthynteriaTread"});
			MonumentKrepis &m = data.krepis;
			values.Value(*k, p, "stylobateWidth", Quantity::Length, m.stylobateWidth);
			values.Value(*k, p, "stylobateLength", Quantity::Length, m.stylobateLength);
			values.Value(*k, p, "stepCount", Quantity::Count, m.stepCount);
			values.Value(*k, p, "stylobateHeight", Quantity::Length, m.stylobateHeight);
			values.Value(*k, p, "secondStepHeight", Quantity::Length, m.secondStepHeight);
			values.Value(*k, p, "thirdStepHeight", Quantity::Length, m.thirdStepHeight);
			values.Value(*k, p, "secondStepTread", Quantity::Length, m.secondStepTread);
			values.Value(*k, p, "thirdStepTread", Quantity::Length, m.thirdStepTread);
			values.Value(*k, p, "cornerMargin", Quantity::Length, m.cornerMargin);
			values.Value(*k, p, "euthynteriaHeight", Quantity::Length, m.euthynteriaHeight);
			values.Value(*k, p, "euthynteriaTread", Quantity::Length, m.euthynteriaTread);
		}
		if (const json *s = OptionalBlock(r, doc, "peristyle", data.present.peristyle))
		{
			const std::string p = "peristyle";
			r.Known(*s, p, {"frontColumns", "flankColumns", "frontSpacing", "flankSpacing", "frontCornerSpacing",
			                "flankCornerSpacing"});
			MonumentPeristyle &m = data.peristyle;
			values.Value(*s, p, "frontColumns", Quantity::Count, m.frontColumns);
			values.Value(*s, p, "flankColumns", Quantity::Count, m.flankColumns);
			values.Value(*s, p, "frontSpacing", Quantity::Length, m.frontSpacing);
			values.Value(*s, p, "flankSpacing", Quantity::Length, m.flankSpacing);
			values.Value(*s, p, "frontCornerSpacing", Quantity::Length, m.frontCornerSpacing);
			values.Value(*s, p, "flankCornerSpacing", Quantity::Length, m.flankCornerSpacing);
		}
		if (const json *c = OptionalBlock(r, doc, "columns", data.present.columns))
		{
			const std::string p = "columns";
			r.Known(*c, p, {"lowerDiameter", "cornerLowerDiameter", "upperDiameter", "height", "fluteCount",
			                "fluteDepth", "entasisSagitta", "entasisHeight", "abacusHeight", "abacusHalfWidth",
			                "echinusHeight", "echinusJunctionDepth", "echinusJunctionRetreat", "echinusTopRetreat",
			                "echinusLipRadius", "annuletCount", "annuletsHeight", "annuletsTopRetreat",
			                "annuletsBottomRetreat", "hypotrachelionHeight", "grooveHeight", "grooveDepth"});
			MonumentColumns &m = data.columns;
			const std::pair<const char *, MonumentValue *> lengths[] = {
				{"lowerDiameter", &m.lowerDiameter},
				{"cornerLowerDiameter", &m.cornerLowerDiameter},
				{"upperDiameter", &m.upperDiameter},
				{"height", &m.height},
				{"fluteDepth", &m.fluteDepth},
				{"entasisSagitta", &m.entasisSagitta},
				{"entasisHeight", &m.entasisHeight},
				{"abacusHeight", &m.abacusHeight},
				{"abacusHalfWidth", &m.abacusHalfWidth},
				{"echinusHeight", &m.echinusHeight},
				{"echinusJunctionDepth", &m.echinusJunctionDepth},
				{"echinusJunctionRetreat", &m.echinusJunctionRetreat},
				{"echinusTopRetreat", &m.echinusTopRetreat},
				{"echinusLipRadius", &m.echinusLipRadius},
				{"annuletsHeight", &m.annuletsHeight},
				{"annuletsTopRetreat", &m.annuletsTopRetreat},
				{"annuletsBottomRetreat", &m.annuletsBottomRetreat},
				{"hypotrachelionHeight", &m.hypotrachelionHeight},
				{"grooveHeight", &m.grooveHeight},
				{"grooveDepth", &m.grooveDepth},
			};
			for (const auto &[key, value] : lengths)
				values.Value(*c, p, key, Quantity::Length, *value);
			values.Value(*c, p, "fluteCount", Quantity::Count, m.fluteCount);
			values.Value(*c, p, "annuletCount", Quantity::Count, m.annuletCount);
		}
		if (const json *c = OptionalBlock(r, doc, "controls", data.present.controls))
		{
			r.Known(*c, "controls", {"frontAxesY", "flankAxesX", "frontSum", "flankSum"});
			ReadAxes(r, *c, "frontAxesY", data.controls.frontAxesY);
			ReadAxes(r, *c, "flankAxesX", data.controls.flankAxesX);
			for (const auto &[key, sum] : {std::make_pair("frontSum", &data.controls.frontSum),
			                               std::make_pair("flankSum", &data.controls.flankSum)})
				if (json_reader::Member(*c, key))
				{
					double value = 0.0;
					ReadResult(r, *c, "controls", key, value);
					*sum = value;
				}
		}
		if (const json *c = OptionalBlock(r, doc, "krepisControls", data.present.krepisControls))
		{
			r.Known(*c, "krepisControls", {"footprintWidth", "footprintLength"});
			ReadResult(r, *c, "krepisControls", "footprintWidth", data.krepisControls.footprintWidth);
			ReadResult(r, *c, "krepisControls", "footprintLength", data.krepisControls.footprintLength);
		}
		if (const json *c = OptionalBlock(r, doc, "columnControls", data.present.columnControls))
		{
			r.Known(*c, "columnControls",
			        {"shaftHeight", "footFluteChord", "fluteCentralRadius", "fluteLateralRadius", "topFluteDepth",
			         "cornerFluteChord", "cornerFluteDepth", "capitalHeight", "annuletsAndRelief", "upperRadius",
			         "echinusLipRetreat", "echinusProfile"});
			MonumentColumnControls &m = data.columnControls;
			const std::pair<const char *, MonumentControlValue *> controls[] = {
				{"shaftHeight", &m.shaftHeight},
				{"footFluteChord", &m.footFluteChord},
				{"fluteCentralRadius", &m.fluteCentralRadius},
				{"fluteLateralRadius", &m.fluteLateralRadius},
				{"topFluteDepth", &m.topFluteDepth},
				{"cornerFluteChord", &m.cornerFluteChord},
				{"cornerFluteDepth", &m.cornerFluteDepth},
				{"capitalHeight", &m.capitalHeight},
				{"annuletsAndRelief", &m.annuletsAndRelief},
				{"upperRadius", &m.upperRadius},
				{"echinusLipRetreat", &m.echinusLipRetreat},
			};
			for (const auto &[key, value] : controls)
				ReadControl(r, *c, "columnControls", key, Quantity::Length, *value);
			ReadProfileControl(r, *c, "echinusProfile", m.echinusProfile);
		}
		if (const json *e = OptionalBlock(r, doc, "entablature", data.present.entablature))
		{
			const std::string p = "entablature";
			r.Known(*e, p,
			        {"architraveHeight", "taeniaHeight", "regulaHeight", "guttaeHeight", "architraveFaceHeight",
			         "friezeHeight", "triglyphCapitalHeight", "corniceHeight", "mutulePlateHeight", "dripHeight",
			         "coronaHeight", "filletHeight", "crownHeight", "architraveTopProjection", "architraveFootProjection",
			         "taeniaProjection", "triglyphFace", "triglyphCapitalBandProjection",
			         "triglyphCapitalListelProjection", "triglyphCapitalListelHeight", "triglyphCapitalBandHeight",
			         "metopeFace", "mutulePlateProjection", "dripProjection", "coronaProjection", "filletProjection",
			         "crownProjection", "innerFace", "triglyphWidth", "metopeWidth", "triglyphDivisions", "channelDepth",
			         "regulaProjection", "regulaGuttaCount", "regulaGuttaDiameter", "regulaGuttaGap",
			         "regulaGuttaTopProjection", "regulaGuttaFootProjection", "mutuleWidth", "mutuleDepth", "soffitSlope",
			         "mutuleGuttaFrontDiameter", "mutuleGuttaMiddleDiameter", "mutuleGuttaRearDiameter",
			         "mutuleGuttaFrontGap", "mutuleGuttaRearGap", "mutuleGuttaHeight", "mutuleGuttaEdgeAxis",
			         "mutuleGuttaPerRow", "mutuleGuttaFootFlare"});
			MonumentEntablature &m = data.entablature;
			const std::pair<const char *, MonumentValue *> lengths[] = {
				{"architraveHeight", &m.architraveHeight},
				{"taeniaHeight", &m.taeniaHeight},
				{"regulaHeight", &m.regulaHeight},
				{"guttaeHeight", &m.guttaeHeight},
				{"architraveFaceHeight", &m.architraveFaceHeight},
				{"friezeHeight", &m.friezeHeight},
				{"triglyphCapitalHeight", &m.triglyphCapitalHeight},
				{"corniceHeight", &m.corniceHeight},
				{"mutulePlateHeight", &m.mutulePlateHeight},
				{"dripHeight", &m.dripHeight},
				{"coronaHeight", &m.coronaHeight},
				{"filletHeight", &m.filletHeight},
				{"crownHeight", &m.crownHeight},
				{"architraveTopProjection", &m.architraveTopProjection},
				{"architraveFootProjection", &m.architraveFootProjection},
				{"taeniaProjection", &m.taeniaProjection},
				{"triglyphFace", &m.triglyphFace},
				{"triglyphCapitalBandProjection", &m.triglyphCapitalBandProjection},
				{"triglyphCapitalListelProjection", &m.triglyphCapitalListelProjection},
				{"triglyphCapitalListelHeight", &m.triglyphCapitalListelHeight},
				{"triglyphCapitalBandHeight", &m.triglyphCapitalBandHeight},
				{"metopeFace", &m.metopeFace},
				{"mutulePlateProjection", &m.mutulePlateProjection},
				{"dripProjection", &m.dripProjection},
				{"coronaProjection", &m.coronaProjection},
				{"filletProjection", &m.filletProjection},
				{"crownProjection", &m.crownProjection},
				{"innerFace", &m.innerFace},
				{"triglyphWidth", &m.triglyphWidth},
				{"metopeWidth", &m.metopeWidth},
				{"channelDepth", &m.channelDepth},
				{"regulaProjection", &m.regulaProjection},
				{"regulaGuttaDiameter", &m.regulaGuttaDiameter},
				{"regulaGuttaGap", &m.regulaGuttaGap},
				{"regulaGuttaTopProjection", &m.regulaGuttaTopProjection},
				{"regulaGuttaFootProjection", &m.regulaGuttaFootProjection},
				{"mutuleWidth", &m.mutuleWidth},
				{"mutuleDepth", &m.mutuleDepth},
				{"mutuleGuttaFrontDiameter", &m.mutuleGuttaFrontDiameter},
				{"mutuleGuttaMiddleDiameter", &m.mutuleGuttaMiddleDiameter},
				{"mutuleGuttaRearDiameter", &m.mutuleGuttaRearDiameter},
				{"mutuleGuttaFrontGap", &m.mutuleGuttaFrontGap},
				{"mutuleGuttaRearGap", &m.mutuleGuttaRearGap},
				{"mutuleGuttaHeight", &m.mutuleGuttaHeight},
				{"mutuleGuttaEdgeAxis", &m.mutuleGuttaEdgeAxis},
				{"mutuleGuttaFootFlare", &m.mutuleGuttaFootFlare},
			};
			for (const auto &[key, value] : lengths)
				values.Value(*e, p, key, Quantity::Length, *value);
			values.Value(*e, p, "regulaGuttaCount", Quantity::Count, m.regulaGuttaCount);
			values.Value(*e, p, "mutuleGuttaPerRow", Quantity::Count, m.mutuleGuttaPerRow);
			values.Value(*e, p, "soffitSlope", Quantity::Angle, m.soffitSlope);
			values.Divisions(*e, p, "triglyphDivisions", m.triglyphDivisions);
		}
		if (const json *c = OptionalBlock(r, doc, "entablatureControls", data.present.entablatureControls))
		{
			const char *p = "entablatureControls";
			r.Known(*c, p,
			        {"total", "corniceSum", "triglyphDivisionSum", "frontMetope", "flankMetope", "metopeCount",
			         "frontTriglyphs", "flankTriglyphs", "channelBottom", "metopeRecess", "cornerTriglyphOffset",
			         "frontCornerDistance", "frontCornerMetope", "flankCornerMetope", "intermediateShift",
			         "frontMidBayInnerMetope", "frontMidBayOuterMetope", "flankMidBayInnerMetope",
			         "flankMidBayOuterMetope", "regulaGuttaMargin", "regulaGuttaShortfall", "regulaGuttaCountCheck",
			         "mutuleBack", "mutuleGuttaRows", "mutuleGuttaPitchCount", "mutuleGuttaMiddleAxis",
			         "mutuleGuttaRearAxis", "mutuleGuttaPitch", "mutuleRearMargin", "mutuleHorizontalDepth",
			         "mutuleInclinedBack", "soffitPivotZ", "mutuleFrontTopZ", "soffitBackZ", "soffitRise", "geisonBackStep",
			         "dripTopZ", "soffitPivotProjection", "mutuleGuttaRadialFlare", "mutuleGuttaRadialFlareExact",
			         "mutuleGuttaFrontFootProjection"});
			MonumentEntablatureControls &m = data.entablatureControls;
			const std::pair<const char *, MonumentControlValue *> lengths[] = {
				{"total", &m.total},
				{"corniceSum", &m.corniceSum},
				{"triglyphDivisionSum", &m.triglyphDivisionSum},
				{"frontMetope", &m.frontMetope},
				{"flankMetope", &m.flankMetope},
				{"channelBottom", &m.channelBottom},
				{"metopeRecess", &m.metopeRecess},
				{"cornerTriglyphOffset", &m.cornerTriglyphOffset},
				{"frontCornerDistance", &m.frontCornerDistance},
				{"frontCornerMetope", &m.frontCornerMetope},
				{"flankCornerMetope", &m.flankCornerMetope},
				{"intermediateShift", &m.intermediateShift},
				{"frontMidBayInnerMetope", &m.frontMidBayInnerMetope},
				{"frontMidBayOuterMetope", &m.frontMidBayOuterMetope},
				{"flankMidBayInnerMetope", &m.flankMidBayInnerMetope},
				{"flankMidBayOuterMetope", &m.flankMidBayOuterMetope},
				{"regulaGuttaMargin", &m.regulaGuttaMargin},
				{"regulaGuttaShortfall", &m.regulaGuttaShortfall},
				{"mutuleBack", &m.mutuleBack},
				{"mutuleGuttaMiddleAxis", &m.mutuleGuttaMiddleAxis},
				{"mutuleGuttaRearAxis", &m.mutuleGuttaRearAxis},
				{"mutuleGuttaPitch", &m.mutuleGuttaPitch},
				{"mutuleRearMargin", &m.mutuleRearMargin},
				{"mutuleHorizontalDepth", &m.mutuleHorizontalDepth},
				{"mutuleInclinedBack", &m.mutuleInclinedBack},
				{"soffitPivotZ", &m.soffitPivotZ},
				{"mutuleFrontTopZ", &m.mutuleFrontTopZ},
				{"soffitBackZ", &m.soffitBackZ},
				{"soffitRise", &m.soffitRise},
				{"geisonBackStep", &m.geisonBackStep},
				{"dripTopZ", &m.dripTopZ},
				{"soffitPivotProjection", &m.soffitPivotProjection},
				{"mutuleGuttaRadialFlare", &m.mutuleGuttaRadialFlare},
				{"mutuleGuttaRadialFlareExact", &m.mutuleGuttaRadialFlareExact},
				{"mutuleGuttaFrontFootProjection", &m.mutuleGuttaFrontFootProjection},
			};
			const std::pair<const char *, MonumentControlValue *> counts[] = {
				{"metopeCount", &m.metopeCount},
				{"frontTriglyphs", &m.frontTriglyphs},
				{"flankTriglyphs", &m.flankTriglyphs},
				{"regulaGuttaCountCheck", &m.regulaGuttaCountCheck},
				{"mutuleGuttaRows", &m.mutuleGuttaRows},
				{"mutuleGuttaPitchCount", &m.mutuleGuttaPitchCount},
			};
			for (const auto &[key, value] : lengths)
				ReadControl(r, *c, p, key, Quantity::Length, *value);
			for (const auto &[key, value] : counts)
				ReadControl(r, *c, p, key, Quantity::Count, *value);
		}
		if (const json *f = OptionalBlock(r, doc, "pediment", data.present.pediment))
		{
			const std::string p = "pediment";
			r.Known(*f, p, {"apexRise", "rakingRun", "rakingProjection", "tympanumFace", "rakingHeights", "rakingSection",
			                "sima"});
			MonumentPediment &m = data.pediment;
			values.Value(*f, p, "apexRise", Quantity::Length, m.apexRise);
			values.Value(*f, p, "rakingRun", Quantity::Ratio, m.rakingRun);
			values.Value(*f, p, "rakingProjection", Quantity::Length, m.rakingProjection);
			values.Value(*f, p, "tympanumFace", Quantity::Length, m.tympanumFace);
			values.RakingHeights(*f, p, "rakingHeights", m.rakingHeights);
			values.Polygon(*f, p, "rakingSection", m.rakingSection);
			values.Value(*f, p, "sima", Quantity::Length, m.sima);
		}
		if (const json *c = OptionalBlock(r, doc, "pedimentControls", data.present.pedimentControls))
		{
			const char *p = "pedimentControls";
			r.Known(*c, p,
			        {"apexRise", "rakingSlope", "eaveHalfSpan", "rakingHeight", "rakingHeightM", "rakingVerticalHeight",
			         "rakingVerticalHeightM", "rakingProjection", "tympanumFaceM", "rakingListelProjection",
			         "rakingCoronaProjection", "rakingBottomFilletProjection", "rakingCoronaGap"});
			MonumentPedimentControls &m = data.pedimentControls;
			const std::pair<const char *, MonumentControlValue *> lengths[] = {
				{"apexRise", &m.apexRise},
				{"eaveHalfSpan", &m.eaveHalfSpan},
				{"rakingHeight", &m.rakingHeight},
				{"rakingHeightM", &m.rakingHeightM},
				{"rakingVerticalHeight", &m.rakingVerticalHeight},
				{"rakingVerticalHeightM", &m.rakingVerticalHeightM},
				{"rakingProjection", &m.rakingProjection},
				{"tympanumFaceM", &m.tympanumFaceM},
				{"rakingListelProjection", &m.rakingListelProjection},
				{"rakingCoronaProjection", &m.rakingCoronaProjection},
				{"rakingBottomFilletProjection", &m.rakingBottomFilletProjection},
				{"rakingCoronaGap", &m.rakingCoronaGap},
			};
			for (const auto &[key, value] : lengths)
				ReadControl(r, *c, p, key, Quantity::Length, *value);
			ReadControl(r, *c, p, "rakingSlope", Quantity::Angle, m.rakingSlope);
		}
		if (const json *c = OptionalBlock(r, doc, "cella", data.present.cella))
		{
			const std::string p = "cella";
			MonumentCella &m = data.cella;
			const std::pair<const char *, MonumentValue *> lengths[] = {
				{"toichobateRise", &m.toichobateRise},
				{"pteronEast", &m.pteronEast},
				{"pteronWest", &m.pteronWest},
				{"toichobateOverhang", &m.toichobateOverhang},
				{"longWallThickness", &m.longWallThickness},
				{"longWallSpan", &m.longWallSpan},
				{"wallTop", &m.wallTop},
				{"eastCrossWallPosition", &m.eastCrossWallPosition},
				{"eastCrossWallThickness", &m.eastCrossWallThickness},
				{"westCrossWallPosition", &m.westCrossWallPosition},
				{"westCrossWallThickness", &m.westCrossWallThickness},
				{"doorWidth", &m.doorWidth},
				{"lintelHeight", &m.lintelHeight},
				{"doorJambOffset", &m.doorJambOffset},
				{"thresholdLength", &m.thresholdLength},
				{"thresholdProjection", &m.thresholdProjection},
				{"thresholdDepth", &m.thresholdDepth},
				{"naosFloorHeight", &m.naosFloorHeight},
				{"eastAntaRetreat", &m.eastAntaRetreat},
				{"westAntaRetreat", &m.westAntaRetreat},
				{"antaInnerProjection", &m.antaInnerProjection},
				{"antaOuterProjection", &m.antaOuterProjection},
				{"antaHeadLength", &m.antaHeadLength},
				{"porchColumnDiameter", &m.porchColumnDiameter},
				{"porchColumnHeight", &m.porchColumnHeight},
				{"porchColumnOffset", &m.porchColumnOffset},
				{"pronaosColumnAxis", &m.pronaosColumnAxis},
				{"opisthodomosColumnAxis", &m.opisthodomosColumnAxis},
				{"porchArchitraveHeight", &m.porchArchitraveHeight},
				{"porchFriezeHeight", &m.porchFriezeHeight},
				{"porchEntablatureDepth", &m.porchEntablatureDepth},
				{"porchEntablatureFront", &m.porchEntablatureFront},
			};
			std::vector<const char *> known;
			known.reserve(std::size(lengths));
			for (const auto &[key, value] : lengths)
				known.push_back(key);
			KnownKeys(r, *c, p, known);
			for (const auto &[key, value] : lengths)
				values.Value(*c, p, key, Quantity::Length, *value);
		}
		if (const json *c = OptionalBlock(r, doc, "cellaControls", data.present.cellaControls))
		{
			const char *p = "cellaControls";
			MonumentCellaControls &m = data.cellaControls;
			const std::pair<const char *, double *> results[] = {
				{"toichobateBottomZ", &m.toichobateBottomZ},
				{"toichobateEastX", &m.toichobateEastX},
				{"toichobateWestX", &m.toichobateWestX},
				{"toichobateWidth", &m.toichobateWidth},
				{"longWallInnerY", &m.longWallInnerY},
				{"longWallOuterY", &m.longWallOuterY},
				{"longWallLength", &m.longWallLength},
				{"eastCrossWallEastX", &m.eastCrossWallEastX},
				{"eastCrossWallWestX", &m.eastCrossWallWestX},
				{"westCrossWallWestX", &m.westCrossWallWestX},
				{"westCrossWallEastX", &m.westCrossWallEastX},
				{"naosLength", &m.naosLength},
				{"doorLintelZ", &m.doorLintelZ},
				{"eastAntaX", &m.eastAntaX},
				{"westAntaX", &m.westAntaX},
				{"antaInnerY", &m.antaInnerY},
				{"antaFaceWidth", &m.antaFaceWidth},
				{"porchArchitraveBottomZ", &m.porchArchitraveBottomZ},
				{"porchArchitraveTopZ", &m.porchArchitraveTopZ},
				{"porchFriezeTopZ", &m.porchFriezeTopZ},
			};
			std::vector<const char *> known;
			known.reserve(std::size(results));
			for (const auto &[key, value] : results)
				known.push_back(key);
			KnownKeys(r, *c, p, known);
			for (const auto &[key, value] : results)
				ReadResult(r, *c, p, key, *value);
		}
		if (const json *c = OptionalBlock(r, doc, "interior", data.present.interior))
		{
			const std::string p = "interior";
			MonumentInterior &m = data.interior;
			const std::pair<const char *, MonumentValue *> lengths[] = {
				{"spacing", &m.spacing},
				{"eastAxisOffset", &m.eastAxisOffset},
				{"westAxisOffset", &m.westAxisOffset},
				{"wallAxisOffset", &m.wallAxisOffset},
				{"naveLevel", &m.naveLevel},
				{"lowerColumnHeight", &m.lowerColumnHeight},
				{"lowerColumnLowerDiameter", &m.lowerColumnLowerDiameter},
				{"lowerColumnUpperDiameter", &m.lowerColumnUpperDiameter},
				{"lowerCapitalHeight", &m.lowerCapitalHeight},
				{"lowerAbacus", &m.lowerAbacus},
				{"lowerEpistyleHeight", &m.lowerEpistyleHeight},
				{"lowerEpistyleSoffit", &m.lowerEpistyleSoffit},
				{"upperColumnHeight", &m.upperColumnHeight},
				{"upperColumnLowerDiameter", &m.upperColumnLowerDiameter},
				{"upperColumnUpperDiameter", &m.upperColumnUpperDiameter},
				{"upperCapitalHeight", &m.upperCapitalHeight},
				{"upperAbacus", &m.upperAbacus},
				{"upperEpistyleHeight", &m.upperEpistyleHeight},
				{"upperEpistyleSoffit", &m.upperEpistyleSoffit},
				{"courseHeight", &m.courseHeight},
				{"courseThickness", &m.courseThickness},
				{"corniceHeight", &m.corniceHeight},
				{"corniceProjection", &m.corniceProjection},
				{"pocketCeiling", &m.pocketCeiling},
				{"pocketMargin", &m.pocketMargin},
			};
			std::vector<const char *> known;
			known.reserve(std::size(lengths) + 1);
			for (const auto &[key, value] : lengths)
				known.push_back(key);
			known.push_back("fluteCount");
			KnownKeys(r, *c, p, known);
			for (const auto &[key, value] : lengths)
				values.Value(*c, p, key, Quantity::Length, *value);
			values.Value(*c, p, "fluteCount", Quantity::Count, m.fluteCount);
		}
		if (const json *c = OptionalBlock(r, doc, "interiorControls", data.present.interiorControls))
		{
			const char *p = "interiorControls";
			MonumentInteriorControls &m = data.interiorControls;
			const std::pair<const char *, double *> results[] = {
				{"eastWallX", &m.eastWallX},
				{"flankAxis1X", &m.flankAxesX[0]},
				{"flankAxis2X", &m.flankAxesX[1]},
				{"flankAxis3X", &m.flankAxesX[2]},
				{"flankAxis4X", &m.flankAxesX[3]},
				{"flankAxis5X", &m.flankAxesX[4]},
				{"westAxisX", &m.westAxisX},
				{"westWallX", &m.westWallX},
				{"naosLength", &m.naosLength},
				{"lateralAxisY", &m.lateralAxisY},
				{"lowerAbacusTopZ", &m.lowerAbacusTopZ},
				{"lowerEpistyleTopZ", &m.lowerEpistyleTopZ},
				{"upperAbacusTopZ", &m.upperAbacusTopZ},
				{"upperEpistyleTopZ", &m.upperEpistyleTopZ},
				{"courseTopZ", &m.courseTopZ},
				{"corniceTopZ", &m.corniceTopZ},
				{"pocketBottomZ", &m.pocketBottomZ},
				{"pocketEastX", &m.pocketEastX},
				{"pocketWestX", &m.pocketWestX},
				{"pocketHalfWidthY", &m.pocketHalfWidthY},
			};
			std::vector<const char *> known;
			known.reserve(std::size(results));
			for (const auto &[key, value] : results)
				known.push_back(key);
			KnownKeys(r, *c, p, known);
			for (const auto &[key, value] : results)
				ReadResult(r, *c, p, key, *value);
		}
		if (const json *c = OptionalBlock(r, doc, "epicranitis", data.present.epicranitis))
		{
			const std::string p = "epicranitis";
			MonumentEpicranitis &m = data.epicranitis;
			const std::pair<const char *, MonumentValue *> lengths[] = {
				{"height", &m.height},
				{"innerOffset", &m.innerOffset},
				{"listelHeight", &m.listelHeight},
				{"filletHeight", &m.filletHeight},
				{"listelProjection", &m.listelProjection},
				{"bandProjection", &m.bandProjection},
			};
			std::vector<const char *> known;
			known.reserve(std::size(lengths));
			for (const auto &[key, value] : lengths)
				known.push_back(key);
			KnownKeys(r, *c, p, known);
			for (const auto &[key, value] : lengths)
				values.Value(*c, p, key, Quantity::Length, *value);
		}
		if (const json *c = OptionalBlock(r, doc, "epicranitisControls", data.present.epicranitisControls))
		{
			const char *p = "epicranitisControls";
			MonumentEpicranitisControls &m = data.epicranitisControls;
			const std::pair<const char *, double *> results[] = {
				{"topZ", &m.topZ},
				{"porchCrownBottomZ", &m.porchCrownBottomZ},
				{"porchCrownHeight", &m.porchCrownHeight},
				{"innerFaceY", &m.innerFaceY},
				{"wallBandY", &m.wallBandY},
				{"eastBandX", &m.eastBandX},
				{"westBandX", &m.westBandX},
			};
			std::vector<const char *> known;
			known.reserve(std::size(results));
			for (const auto &[key, value] : results)
				known.push_back(key);
			KnownKeys(r, *c, p, known);
			for (const auto &[key, value] : results)
				ReadResult(r, *c, p, key, *value);
		}
		if (const json *c = OptionalBlock(r, doc, "porchExtension", data.present.porchExtension))
		{
			const std::string p = "porchExtension";
			KnownKeys(r, *c, p, {"eastSpan"});
			values.Value(*c, p, "eastSpan", Quantity::Length, data.porchExtension.eastSpan);
		}
		if (const json *c = OptionalBlock(r, doc, "porchExtensionControls", data.present.porchExtensionControls))
		{
			const char *p = "porchExtensionControls";
			MonumentPorchExtensionControls &m = data.porchExtensionControls;
			KnownKeys(r, *c, p, {"ringInnerY", "architraveFootZ"});
			ReadResult(r, *c, p, "ringInnerY", m.ringInnerY);
			ReadResult(r, *c, p, "architraveFootZ", m.architraveFootZ);
		}
		if (const json *c = OptionalBlock(r, doc, "eastCeiling", data.present.eastCeiling))
		{
			const std::string p = "eastCeiling";
			MonumentEastCeiling &m = data.eastCeiling;
			const std::pair<const char *, MonumentValue *> fields[] = {
				{"beamCount", &m.beamCount},
				{"beamSpacing", &m.beamSpacing},
				{"beamWidth", &m.beamWidth},
				{"beamHeight", &m.beamHeight},
				{"beamWestBearing", &m.beamWestBearing},
				{"endMassifWidth", &m.endMassifWidth},
				{"corniceHeight", &m.corniceHeight},
				{"corniceProjection", &m.corniceProjection},
				{"bandHeight", &m.bandHeight},
				{"corniceDepth", &m.corniceDepth},
				{"cofferPitch", &m.cofferPitch},
				{"cofferOpening", &m.cofferOpening},
				{"cofferFrame", &m.cofferFrame},
				{"cofferDepth", &m.cofferDepth},
				{"cofferRows", &m.cofferRows},
			};
			std::vector<const char *> known;
			known.reserve(std::size(fields));
			for (const auto &[key, value] : fields)
				known.push_back(key);
			KnownKeys(r, *c, p, known);
			for (const auto &[key, value] : fields)
				values.Value(*c, p, key, std::string_view(key) == "beamCount" || std::string_view(key) == "cofferRows" ? Quantity::Count : Quantity::Length, *value);
		}
		if (const json *c = OptionalBlock(r, doc, "eastCeilingControls", data.present.eastCeilingControls))
		{
			const char *p = "eastCeilingControls";
			MonumentEastCeilingControls &m = data.eastCeilingControls;
			const std::pair<const char *, double *> results[] = {
				{"beamOuterAxisY", &m.beamOuterAxisY},
				{"beamBottomZ", &m.beamBottomZ},
				{"beamTopZ", &m.beamTopZ},
				{"beamWestEndX", &m.beamWestEndX},
				{"beamEastEndX", &m.beamEastEndX},
				{"beamLength", &m.beamLength},
				{"clearBay", &m.clearBay},
				{"massifFaceY", &m.massifFaceY},
				{"edgeBay", &m.edgeBay},
				{"corniceTopZ", &m.corniceTopZ},
				{"bandTopZ", &m.bandTopZ},
				{"slabThickness", &m.slabThickness},
				{"cofferFloor", &m.cofferFloor},
				{"cofferRun", &m.cofferRun},
				{"cofferMargin", &m.cofferMargin},
			};
			std::vector<const char *> known;
			known.reserve(std::size(results));
			for (const auto &[key, value] : results)
				known.push_back(key);
			KnownKeys(r, *c, p, known);
			for (const auto &[key, value] : results)
				ReadResult(r, *c, p, key, *value);
		}
		if (const json *c = OptionalBlock(r, doc, "flankCeilings", data.present.flankCeilings))
		{
			const std::string p = "flankCeilings";
			MonumentFlankCeilings &m = data.flankCeilings;
			const std::pair<const char *, MonumentValue *> fields[] = {
				{"beamSpacing", &m.beamSpacing},
				{"firstBeamOffset", &m.firstBeamOffset},
				{"eastBeamCount", &m.eastBeamCount},
				{"westBeamCount", &m.westBeamCount},
				{"beamWidth", &m.beamWidth},
				{"beamHeight", &m.beamHeight},
				{"peristyleGap", &m.peristyleGap},
				{"cofferRows", &m.cofferRows},
			};
			std::vector<const char *> known;
			known.reserve(std::size(fields));
			for (const auto &[key, value] : fields)
				known.push_back(key);
			KnownKeys(r, *c, p, known);
			for (const auto &[key, value] : fields)
				values.Value(*c, p, key, std::string_view(key) == "eastBeamCount" || std::string_view(key) == "westBeamCount" ||
				                                std::string_view(key) == "cofferRows" ? Quantity::Count : Quantity::Length, *value);
		}
		if (const json *c = OptionalBlock(r, doc, "flankCeilingsControls", data.present.flankCeilingsControls))
		{
			const char *p = "flankCeilingsControls";
			MonumentFlankCeilingsControls &m = data.flankCeilingsControls;
			const std::pair<const char *, double *> results[] = {
				{"lastEastAxisX", &m.lastEastAxisX},
				{"lastWestAxisX", &m.lastWestAxisX},
				{"wallEndY", &m.wallEndY},
				{"peristyleEndY", &m.peristyleEndY},
				{"beamLength", &m.beamLength},
				{"cofferRun", &m.cofferRun},
				{"cofferMargin", &m.cofferMargin},
			};
			std::vector<const char *> known;
			known.reserve(std::size(results));
			for (const auto &[key, value] : results)
				known.push_back(key);
			KnownKeys(r, *c, p, known);
			for (const auto &[key, value] : results)
				ReadResult(r, *c, p, key, *value);
		}

		// Blocs de controles et leur partie (MO-P2) : un controle sans sa partie est refuse ; les axes sont
		// exiges par le peristyle.
		const MonumentBlocks &b = data.present;
		for (const BlockDependency &dep : BlockDependencies(b))
			if (dep.present && !dep.neededPresent)
				r.Error(dep.block, std::string("requires block \"") + dep.needed + "\"");
		if (b.peristyle && json_reader::Member(doc, "controls") == nullptr)
			r.Require(doc, std::string(), {"controls"});

		for (const auto &entry : data.sources)
			if (values.Cited().count(entry.first) == 0)
				r.Error(json_reader::Join("sources", entry.first), "listed but never cited");
	}

	constexpr double kPi = 3.14159265358979323846;

	// Valeur employee par une conversion : presente, sinon refus nomme.
	double Required (const MonumentValue &v, std::string_view key)
	{
		if (!v.value)
			throw std::invalid_argument("Monument : valeur manquante '" + std::string(key) + "'");
		return *v.value;
	}

	// Compte employe par une conversion : entier de [1, kCountMax]. ParseMonument
	// le garantit ; un MonumentData construit a la main peut ne pas le faire.
	int RequiredCount (const MonumentValue &v, const char *key)
	{
		const double x = Required(v, key);
		if (!(x >= 1.0 && x <= kCountMax && std::floor(x) == x))
			throw std::invalid_argument(std::string("Monument : '") + key + "' n'est pas un compte");
		return static_cast<int>(x);
	}

	// Longueur employee par une conversion, dans son unite d'ecriture : `unit` recoit l'unite scalaire du nombre
	// rendu. Une cote "ft+in" est rendue en pouces, pieds x 12 + pouces : le produit par 12 est exact pour des
	// pieds entiers, si bien que la somme ne depend pas d'une contraction FMA. Refus de MonumentLength.
	double WrittenLength (const MonumentValue &v, std::string_view key, MonumentUnit &unit)
	{
		const double x = Required(v, key);
		if (v.unit != MonumentUnit::FootInch)
		{
			MonumentMetres(1.0, v.unit, key);   // refus d'une unite qui n'est pas une longueur
			unit = v.unit;
			return x;
		}
		if (!(x >= 1.0 && x <= kCountMax && std::floor(x) == x))
			throw std::invalid_argument("Monument : '" + std::string(key) +
			                            "' n'est pas une cote en pieds et pouces : pieds non entiers ou < 1");
		if (!(std::isfinite(v.inches) && v.inches >= 0.0))
			throw std::invalid_argument("Monument : '" + std::string(key) +
			                            "' n'est pas une cote en pieds et pouces : pouces non finis ou < 0");
		unit = MonumentUnit::Inch;
		return x * 12.0 + v.inches;
	}

	// Deux operandes d'un calcul de longueur (registre section 10.14) : de meme unite, chacun dans cette unite
	// (pouces pour deux cotes "ft+in"), et le resultat du calcul se convertit en metres en dernier, une fois, par
	// MonumentMetres (unit) ; d'unites differentes, chacun d'abord en metres, et unit vaut Metre.
	struct CommonUnitOperands
	{
		double       a = 0.0, b = 0.0;
		MonumentUnit unit = MonumentUnit::Metre;
	};

	CommonUnitOperands InCommonUnit (const MonumentValue &a, const char *keyA, const MonumentValue &b, const char *keyB)
	{
		CommonUnitOperands o;
		MonumentUnit unitA = MonumentUnit::Metre, unitB = MonumentUnit::Metre;
		o.a = WrittenLength(a, keyA, unitA);
		o.b = WrittenLength(b, keyB, unitB);
		o.unit = unitA;
		if (a.unit == b.unit)
			return o;
		o.a = MonumentMetres(o.a, unitA, keyA);
		o.b = MonumentMetres(o.b, unitB, keyB);
		o.unit = MonumentUnit::Metre;
		return o;
	}

	// Angle employe par une conversion : present et en degres, sinon refus nomme.
	double RequiredDegrees (const MonumentValue &v, const char *key)
	{
		const double x = Required(v, key);
		if (v.unit != MonumentUnit::Degree)
			throw std::invalid_argument(std::string("Monument : '") + key + "' n'est pas en degres (unite \"" +
			                            kUnitNames[static_cast<int>(MonumentUnit::Degree)] + "\" attendue)");
		return x;
	}
}

double MonumentMetres (double value, MonumentUnit unit, std::string_view key)
{
	switch (unit)
	{
	case MonumentUnit::Metre:
		return value;
	case MonumentUnit::Inch:
		return value * kMonumentInch;
	case MonumentUnit::Millimetre:
		return value / 1000.0;
	case MonumentUnit::One:
		throw std::invalid_argument("Monument : '" + std::string(key) + "' n'est pas une longueur (unite sans dimension)");
	case MonumentUnit::Foot:
		return value * kMonumentFoot;
	case MonumentUnit::FootInch:
		throw std::invalid_argument("Monument : '" + std::string(key) +
		                            "' est une cote en pieds et pouces, deux nombres (MonumentLength)");
	case MonumentUnit::Degree:
		throw std::invalid_argument("Monument : '" + std::string(key) + "' n'est pas une longueur (angle en degres)");
	case MonumentUnit::Ratio:
		throw std::invalid_argument("Monument : '" + std::string(key) + "' n'est pas une longueur (rapport 1 : r)");
	}
	throw std::invalid_argument("Monument : '" + std::string(key) + "' a une unite hors de l'enumeration");
}

double MonumentLength (const MonumentValue &v, std::string_view key)
{
	MonumentUnit unit = MonumentUnit::Metre;
	const double written = WrittenLength(v, key, unit);
	return MonumentMetres(written, unit, key);
}

double MonumentRatio (const MonumentValue &v, std::string_view key)
{
	const double r = Required(v, key);
	if (v.unit != MonumentUnit::Ratio)
		throw std::invalid_argument("Monument : '" + std::string(key) + "' n'est pas un rapport (unite \"" +
		                            kUnitNames[static_cast<int>(MonumentUnit::Ratio)] + "\" attendue)");
	return r;
}

MonumentData ParseMonument (std::string_view jsonText)
{
	Reader r;
	MonumentData data;
	json doc;
	try
	{
		doc = r.ParseRejectingDuplicateKeys(std::string(jsonText));
	}
	catch (const json::exception &e)
	{
		r.Error(std::string(), std::string("unreadable text (") + e.what() + ")");
		r.Throw();
	}
	ReadDocument(r, doc, data);
	if (r.Failed())
		r.Throw();
	return data;
}

const MonumentData &EmbeddedMonument (std::string_view name)
{
	if (name == "temple-hephaistos")
	{
		static const MonumentData data = ParseMonument(TempleHephaistosJson());
		return data;
	}
	if (name == "temple-ares")
	{
		static const MonumentData data = ParseMonument(TempleAresJson());
		return data;
	}
	if (name == "temple-parthenon")
	{
		static const MonumentData data = ParseMonument(TempleParthenonJson());
		return data;
	}
	throw std::invalid_argument("Monument : '" + std::string(name) + "' n'est pas un monument embarque");
}

const MonumentData &HephaistosMonument ()
{
	return EmbeddedMonument("temple-hephaistos");
}

MonumentFramePoint TempleFrameFromMonument (double x, double y, double z, double stylobateTop)
{
	return MonumentFramePoint{y, -x, z + stylobateTop};
}

KrepisSettings MonumentKrepisSettings (const MonumentData &data)
{
	const MonumentKrepis &k = data.krepis;
	if (RequiredCount(k.stepCount, "krepis.stepCount") != 3)
		throw std::invalid_argument("Monument : 'krepis.stepCount' vaut " +
		                            std::to_string(RequiredCount(k.stepCount, "krepis.stepCount")) +
		                            ", le schema porte trois hauteurs de degre");
	const double secondTread = MonumentLength(k.secondStepTread, "krepis.secondStepTread");
	const double thirdTread = MonumentLength(k.thirdStepTread, "krepis.thirdStepTread");
	const double euthynteriaTread = MonumentLength(k.euthynteriaTread, "krepis.euthynteriaTread");
	KrepisSettings s;
	s.stylobateHalfWidth = 0.5 * MonumentLength(k.stylobateWidth, "krepis.stylobateWidth");
	s.stylobateHalfDepth = 0.5 * MonumentLength(k.stylobateLength, "krepis.stylobateLength");
	s.stylobateHeight = MonumentLength(k.stylobateHeight, "krepis.stylobateHeight");
	s.lowerSteps = {
		{MonumentLength(k.secondStepHeight, "krepis.secondStepHeight"), secondTread, secondTread},
		{MonumentLength(k.thirdStepHeight, "krepis.thirdStepHeight"), thirdTread, thirdTread},
		// Euthynteria (KR3, convention Q6) : assise pleine, ressaut sur toute sa hauteur.
		{MonumentLength(k.euthynteriaHeight, "krepis.euthynteriaHeight"), euthynteriaTread, euthynteriaTread},
	};
	return s;
}

PeristyleSettings MonumentPeristyleSettings (const MonumentData &data)
{
	const MonumentPeristyle &p = data.peristyle;
	PeristyleSettings s;
	s.front = RequiredCount(p.frontColumns, "peristyle.frontColumns");
	s.flank = RequiredCount(p.flankColumns, "peristyle.flankColumns");
	s.frontSpacing = MonumentLength(p.frontSpacing, "peristyle.frontSpacing");
	s.flankSpacing = MonumentLength(p.flankSpacing, "peristyle.flankSpacing");
	s.frontCornerSpacing = MonumentLength(p.frontCornerSpacing, "peristyle.frontCornerSpacing");
	s.flankCornerSpacing = MonumentLength(p.flankCornerSpacing, "peristyle.flankCornerSpacing");
	return s;
}

GreekDoricColumnSettings MonumentColumnSettings (const MonumentData &data, bool corner)
{
	const MonumentColumns &c = data.columns;
	// GD3d s'applique a une profondeur de rainure manquante ; une profondeur donnee
	// demanderait sa propre lecture.
	if (c.grooveDepth.status != MonumentStatus::Missing)
		throw std::invalid_argument("Monument : 'columns.grooveDepth' donnee, la convention GD3d ne vaut que manquante");

	const int flutes = RequiredCount(c.fluteCount, "columns.fluteCount");
	const int steps = RequiredCount(c.annuletCount, "columns.annuletCount");
	// Borne d'interface de la colonne, tenue avant toute allocation des listels.
	if (steps > kGreekDoricMaxAnnulets)
		throw std::invalid_argument("Monument : 'columns.annuletCount' vaut " + std::to_string(steps) + ", au-dela de " +
		                            std::to_string(kGreekDoricMaxAnnulets) + " listels");
	const double upperRadius = 0.5 * MonumentLength(c.upperDiameter, "columns.upperDiameter");
	// Demi-cote de l'abaque depuis l'axe ; un retrait depuis son nu se lit en rayon A - retrait (GD5a).
	const double abacus = MonumentLength(c.abacusHalfWidth, "columns.abacusHalfWidth");
	const double lowerDiameter = MonumentLength(c.lowerDiameter, "columns.lowerDiameter");

	GreekDoricColumnSettings s;
	s.height = MonumentLength(c.height, "columns.height");
	s.lowerDiameter = corner ? MonumentLength(c.cornerLowerDiameter, "columns.cornerLowerDiameter") : lowerDiameter;
	s.upperDiameter = MonumentLength(c.upperDiameter, "columns.upperDiameter");
	s.entasisSagitta = MonumentLength(c.entasisSagitta, "columns.entasisSagitta");
	s.entasisHeight = MonumentLength(c.entasisHeight, "columns.entasisHeight");

	// Rapport profondeur / corde au pied de la colonne courante, pour les deux jeux : la
	// section d'angle est homothetique (README section 4.4, ligne "Cannelures au pied", GD2a).
	s.fluteCount = flutes;
	s.fluteSection = FluteSection::ThreeCentred;
	s.fluteDepthRatio = MonumentLength(c.fluteDepth, "columns.fluteDepth") / (lowerDiameter * std::sin(kPi / flutes));

	// Rainure sous le gorgerin, comptee dans le chapiteau du modele (GD3c) ; fond sous
	// celui des cannelures au sommet du fut, d'une hauteur de rainure (GD3d).
	s.grooveHeight = MonumentLength(c.grooveHeight, "columns.grooveHeight");
	s.hypotrachelionHeight = MonumentLength(c.hypotrachelionHeight, "columns.hypotrachelionHeight") + s.grooveHeight;
	s.grooveDepth =
		upperRadius * (1.0 - GreekDoricFluteBottomRatio(flutes, s.fluteSection, s.fluteDepthRatio)) + s.grooveHeight;

	// Annelets : gradins egaux, face puis lit (GD3a). Le listel du haut porte la saillie du
	// pied de l'echine elle-meme : les deux anneaux fusionnent (Ring::operator==, a 1e-12
	// pres) ; un ecart au-dela de cette tolerance laisserait une couronne quasi nulle, que
	// ValidateMesh refuse.
	const double top = MonumentLength(c.annuletsTopRetreat, "columns.annuletsTopRetreat");
	const double bottom = MonumentLength(c.annuletsBottomRetreat, "columns.annuletsBottomRetreat");
	const double foot = (abacus - top) - upperRadius;
	const double stepRetreat = (bottom - top) / steps;
	const double stepHeight = MonumentLength(c.annuletsHeight, "columns.annuletsHeight") / steps;
	s.annulets.reserve(static_cast<std::size_t>(steps));
	for (int k = 0; k < steps; ++k)
		s.annulets.push_back({stepHeight, k == steps - 1 ? foot : foot - (steps - 1 - k) * stepRetreat});

	// Echine : profondeurs sous l'abaque, retraits depuis son nu (GD4a).
	s.echinusHeight = MonumentLength(c.echinusHeight, "columns.echinusHeight");
	s.echinusFootProjection = foot;
	s.echinusJunctionHeight = s.echinusHeight - MonumentLength(c.echinusJunctionDepth, "columns.echinusJunctionDepth");
	s.echinusJunctionProjection =
		(abacus - MonumentLength(c.echinusJunctionRetreat, "columns.echinusJunctionRetreat")) - upperRadius;
	s.echinusTopProjection = (abacus - MonumentLength(c.echinusTopRetreat, "columns.echinusTopRetreat")) - upperRadius;
	s.echinusLipRadius = MonumentLength(c.echinusLipRadius, "columns.echinusLipRadius");

	s.abacusHeight = MonumentLength(c.abacusHeight, "columns.abacusHeight");
	s.abacusProjection = abacus - upperRadius;
	return s;
}

GreekEntablatureSettings MonumentEntablatureSettings (const MonumentData &data)
{
	const MonumentEntablature &e = data.entablature;

	// Le chapiteau du triglyphe est lu deux fois : sa hauteur (corps de la frise) et son partage
	// (deux bandes). Un desaccord deplacerait le dessus de la frise. Tolerance : une longueur de
	// reference, 1e-9 in en metres, quelle que soit l'unite des trois valeurs ; au-dessus du bruit
	// d'une somme en metres et en deca du plus petit ecart ecrit par la source (1e-3 in).
	constexpr double kCapitalPartitionTolerance = 1e-9 * kMonumentInch;
	const double capital = MonumentLength(e.triglyphCapitalHeight, "entablature.triglyphCapitalHeight");
	const double capitalBand = MonumentLength(e.triglyphCapitalBandHeight, "entablature.triglyphCapitalBandHeight");
	const double capitalListel =
		MonumentLength(e.triglyphCapitalListelHeight, "entablature.triglyphCapitalListelHeight");
	if (!(std::fabs(capitalBand + capitalListel - capital) <= kCapitalPartitionTolerance))
		throw std::invalid_argument("Monument : 'entablature.triglyphCapitalBandHeight' plus le listel ne fait pas la "
		                            "hauteur du chapiteau du triglyphe (EN8)");

	const MonumentDivisions &dv = e.triglyphDivisions;
	const std::size_t n = dv.values.size();
	if (n == 0 || dv.kinds.size() != n)
		throw std::invalid_argument("Monument : 'entablature.triglyphDivisions' vide, ou de listes de tailles differentes");
	if (n < 2 || dv.kinds.front() != MonumentDivisionKind::HalfChannel ||
	    dv.kinds.back() != MonumentDivisionKind::HalfChannel)
		throw std::invalid_argument("Monument : 'entablature.triglyphDivisions' sans demi-canal a chaque bout");
	std::size_t channel = n;
	for (std::size_t i = 1; i + 1 < n; ++i)
	{
		if (dv.kinds[i] == MonumentDivisionKind::HalfChannel)
			throw std::invalid_argument("Monument : 'entablature.triglyphDivisions' a un demi-canal a l'interieur");
		if (dv.kinds[i] != MonumentDivisionKind::Channel)
			continue;
		if (channel == n)
			channel = i;
		else if (dv.values[i] != dv.values[channel])
			throw std::invalid_argument("Monument : 'entablature.triglyphDivisions' a des canaux de largeurs differentes");
	}
	if (channel == n)
		throw std::invalid_argument("Monument : 'entablature.triglyphDivisions' sans canal");
	if (dv.values.front() != dv.values.back())
		throw std::invalid_argument("Monument : 'entablature.triglyphDivisions' a des demi-canaux de largeurs differentes");

	// Saillies depuis la ligne des axes ; valeurs en pouces telles qu'ecrites (EN9).
	const double architraveFoot =
		MonumentLength(e.architraveFootProjection, "entablature.architraveFootProjection");
	const double architraveTop = MonumentLength(e.architraveTopProjection, "entablature.architraveTopProjection");
	const double taenia = MonumentLength(e.taeniaProjection, "entablature.taeniaProjection");
	const double metopes = MonumentLength(e.metopeFace, "entablature.metopeFace");
	const double bandProjection =
		MonumentLength(e.triglyphCapitalBandProjection, "entablature.triglyphCapitalBandProjection");
	const double listelProjection =
		MonumentLength(e.triglyphCapitalListelProjection, "entablature.triglyphCapitalListelProjection");
	const double mutules = MonumentLength(e.mutulePlateProjection, "entablature.mutulePlateProjection");
	const double drip = MonumentLength(e.dripProjection, "entablature.dripProjection");
	const double corona = MonumentLength(e.coronaProjection, "entablature.coronaProjection");
	const double fillet = MonumentLength(e.filletProjection, "entablature.filletProjection");
	const double crown = MonumentLength(e.crownProjection, "entablature.crownProjection");

	// Zone des regulae et des gouttes : une bande verticale au nu du haut de l'architrave (EN9) ;
	// corps de la frise : la frise sans le chapiteau du triglyphe, porte par l'anneau (EN8).
	const double regulaZone = MonumentLength(e.regulaHeight, "entablature.regulaHeight") +
	                          MonumentLength(e.guttaeHeight, "entablature.guttaeHeight");
	const double friezeBody = MonumentLength(e.friezeHeight, "entablature.friezeHeight") - capital;
	// Sous-face inclinee (EN21, EN22, D3-67) : pivot sur la face de la plaque, profondeur L le long de la pente ;
	// dos P - L cos theta et montee L sin theta dans l'unite commune de P et L, convertis en dernier (registre
	// section 10.14) : un seul produit par le pouce pour deux valeurs en pouces.
	// Pente dans ]0, 90[ degres : au-dela, cos et sin rendraient la geometrie d'une autre pente (374 celle de 14).
	const double slopeDegrees = RequiredDegrees(e.soffitSlope, "entablature.soffitSlope");
	if (!(slopeDegrees > 0.0 && slopeDegrees < 90.0))
		throw std::invalid_argument("Monument : 'entablature.soffitSlope' vaut " + std::to_string(slopeDegrees) +
		                            " degres, hors de ]0, 90[");
	const double slope = slopeDegrees * (kPi / 180.0);
	const CommonUnitOperands mutule = InCommonUnit(e.mutulePlateProjection, "entablature.mutulePlateProjection",
	                                               e.mutuleDepth, "entablature.mutuleDepth");
	const double mutuleBack =
		MonumentMetres(mutule.a - mutule.b * std::cos(slope), mutule.unit, "entablature.mutuleDepth");
	const double mutuleRise = MonumentMetres(mutule.b * std::sin(slope), mutule.unit, "entablature.mutuleDepth");

	GreekEntablatureSettings s;
	s.bands = {
		{MonumentLength(e.architraveFaceHeight, "entablature.architraveFaceHeight"), architraveFoot, architraveTop},
		{regulaZone, architraveTop, architraveTop},
		{MonumentLength(e.taeniaHeight, "entablature.taeniaHeight"), taenia, taenia},
		{friezeBody, metopes, metopes},
		{capitalBand, bandProjection, bandProjection},
		{capitalListel, listelProjection, listelProjection},
		{MonumentLength(e.mutulePlateHeight, "entablature.mutulePlateHeight"), mutuleBack, mutuleBack},   // EN21
		{MonumentLength(e.dripHeight, "entablature.dripHeight"), drip, drip},
		{MonumentLength(e.coronaHeight, "entablature.coronaHeight"), corona, corona},
		{MonumentLength(e.filletHeight, "entablature.filletHeight"), fillet, fillet},
		{MonumentLength(e.crownHeight, "entablature.crownHeight"), crown, crown},
	};
	s.friezeBand = 3;
	// EN1 en repli (CE-32) : face interieure lue si donnee, au nu du pied de l'architrave sinon.
	s.innerFace = e.innerFace.status == MonumentStatus::Missing ? architraveFoot
	                                                           : MonumentLength(e.innerFace, "entablature.innerFace");
	// EN15 : triglyphe d'angle et deux metopes egales dans la travee d'angle, premiere solution du
	// README (section 6.3, ligne "Metopes d'angle" : « la regle grecque (convention) ») ; la
	// contraction relevee du peristyle reste celle
	// de MonumentPeristyleSettings.
	s.corner = DoricCornerStrategy::CornerTriglyphEqualMetopes;

	GreekTriglyphSettings &t = s.triglyph;
	t.width = MonumentLength(e.triglyphWidth, "entablature.triglyphWidth");
	t.face = MonumentLength(e.triglyphFace, "entablature.triglyphFace");
	t.divisionWidths.reserve(n);
	t.divisionIsChannel.reserve(n);
	for (std::size_t i = 0; i < n; ++i)
	{
		t.divisionWidths.push_back(MonumentMetres(dv.values[i], dv.unit, "entablature.triglyphDivisions"));
		t.divisionIsChannel.push_back(dv.kinds[i] != MonumentDivisionKind::Band);
	}
	// Canaux en V a angle droit (EN11) : la profondeur d'un canal est sa demi-largeur ; celle d'un
	// demi-canal, de meme pente, est sa largeur.
	t.channelDepth = t.divisionWidths[channel] / 2.0;
	t.halfChannelDepth = t.divisionWidths.front();

	// Regulae sous la tenia, une par placement de triglyphe (EN17, EN18, EN20) ; gouttes en troncs de
	// cone, diametre lu au pied (EN19), hauteur deduite de la zone des regulae ; 8 cotes (format).
	GreekRegulaSettings regulae;
	regulae.height = MonumentLength(e.regulaHeight, "entablature.regulaHeight");
	regulae.face = MonumentLength(e.regulaProjection, "entablature.regulaProjection");
	regulae.guttaCount = RequiredCount(e.regulaGuttaCount, "entablature.regulaGuttaCount");
	regulae.guttaGap = MonumentLength(e.regulaGuttaGap, "entablature.regulaGuttaGap");
	regulae.gutta.footDiameter = MonumentLength(e.regulaGuttaDiameter, "entablature.regulaGuttaDiameter");
	regulae.gutta.footProjection = MonumentLength(e.regulaGuttaFootProjection, "entablature.regulaGuttaFootProjection");
	regulae.gutta.topProjection = MonumentLength(e.regulaGuttaTopProjection, "entablature.regulaGuttaTopProjection");
	regulae.gutta.sides = 8;
	s.regulae = regulae;

	// Gouttes des mutules (EN23, EN24) : diametres de tete et vides entre tetes, du devant vers le dos (D3-76) ;
	// nombre par rang lu (D3-77) ; evasement lu en diametre, conicity radiale : la moitie, exacte en binaire
	// (D3-78) ; 8 cotes (format).
	GreekMutuleGuttaSettings guttae;
	guttae.rowDiameters = {MonumentLength(e.mutuleGuttaFrontDiameter, "entablature.mutuleGuttaFrontDiameter"),
	                       MonumentLength(e.mutuleGuttaMiddleDiameter, "entablature.mutuleGuttaMiddleDiameter"),
	                       MonumentLength(e.mutuleGuttaRearDiameter, "entablature.mutuleGuttaRearDiameter")};
	guttae.rowGaps = {MonumentLength(e.mutuleGuttaFrontGap, "entablature.mutuleGuttaFrontGap"),
	                  MonumentLength(e.mutuleGuttaRearGap, "entablature.mutuleGuttaRearGap")};
	guttae.perRow = RequiredCount(e.mutuleGuttaPerRow, "entablature.mutuleGuttaPerRow");
	guttae.edgeAxis = MonumentLength(e.mutuleGuttaEdgeAxis, "entablature.mutuleGuttaEdgeAxis");
	guttae.height = MonumentLength(e.mutuleGuttaHeight, "entablature.mutuleGuttaHeight");
	guttae.conicity = 0.5 * MonumentLength(e.mutuleGuttaFootFlare, "entablature.mutuleGuttaFootFlare");
	guttae.sides = 8;

	// Mutules en plaques dans la bande 6, de son dos a la face de la plaque (EN21), sous la sous-face inclinee
	// (EN22), avec leurs gouttes.
	GreekMutuleSettings plates;
	plates.band = 6;
	plates.width = MonumentLength(e.mutuleWidth, "entablature.mutuleWidth");
	plates.face = mutules;
	plates.rise = mutuleRise;
	plates.guttae = std::move(guttae);
	s.mutules = std::move(plates);
	return s;
}

GreekPedimentSettings MonumentPedimentSettings (const MonumentData &data)
{
	const MonumentPediment &f = data.pediment;
	// PE4 (comble plein, ni sima, ni acroteres, ni tuiles) ne vaut que pour une sima manquante ; une sima donnee
	// demanderait sa propre construction.
	if (f.sima.status != MonumentStatus::Missing)
		throw std::invalid_argument("Monument : 'pediment.sima' donnee, la convention PE4 ne vaut que manquante");
	const MonumentPolygon &p = f.rakingSection;
	if (p.status == MonumentStatus::Missing || p.u.empty() || p.u.size() != p.h.size())
		throw std::invalid_argument("Monument : 'pediment.rakingSection' manquante, vide, ou de listes u et h de tailles "
		                            "differentes");

	GreekPedimentSettings s;
	s.apexRise = MonumentLength(f.apexRise, "pediment.apexRise");
	s.rakingRun = MonumentRatio(f.rakingRun, "pediment.rakingRun");
	s.tympanumFace = MonumentLength(f.tympanumFace, "pediment.tympanumFace");
	// Un seul produit par coordonnee (MonumentMetres) : u depuis le nu du tympan, h normale a la pente (PE1).
	s.rakingSection.reserve(p.u.size());
	for (std::size_t k = 0; k < p.u.size(); ++k)
		s.rakingSection.push_back({MonumentMetres(p.u[k], p.unit, "pediment.rakingSection"),
		                           MonumentMetres(p.h[k], p.unit, "pediment.rakingSection")});
	return s;
}

namespace
{
	// Temple du README : krepis, peristyle et colonnes, puis l'entablement et le fronton demandes. Un fronton sans
	// entablement est refuse par LayoutTemple, non ici.
	TempleSettings TempleSettingsOf (const MonumentData &data, bool withEntablature, bool withPediment)
	{
		TempleSettings t;
		t.krepis = MonumentKrepisSettings(data);
		t.peristyle = MonumentPeristyleSettings(data);
		t.column = MonumentColumnSettings(data, false);
		t.cornerColumn = MonumentColumnSettings(data, true);
		if (withEntablature)
			t.entablature = MonumentEntablatureSettings(data);
		if (withPediment)
			t.pediment = MonumentPedimentSettings(data);
		return t;
	}
}

TempleSettings MonumentTempleSettings (const MonumentData &data)
{
	return TempleSettingsOf(data, true, true);
}

CellaSettings MonumentCellaSettings (const MonumentData &data)
{
	const MonumentCella &c = data.cella;
	CellaSettings s;
	s.toichobateRise = MonumentLength(c.toichobateRise, "cella.toichobateRise");
	s.frontPteron = MonumentLength(c.pteronEast, "cella.pteronEast");
	s.backPteron = MonumentLength(c.pteronWest, "cella.pteronWest");
	s.toichobateOverhang = MonumentLength(c.toichobateOverhang, "cella.toichobateOverhang");
	s.longWallThickness = MonumentLength(c.longWallThickness, "cella.longWallThickness");
	s.longWallSpan = MonumentLength(c.longWallSpan, "cella.longWallSpan");
	s.wallTop = MonumentLength(c.wallTop, "cella.wallTop");
	s.frontCrossWallPosition = MonumentLength(c.eastCrossWallPosition, "cella.eastCrossWallPosition");
	s.frontCrossWallThickness = MonumentLength(c.eastCrossWallThickness, "cella.eastCrossWallThickness");
	s.backCrossWallPosition = MonumentLength(c.westCrossWallPosition, "cella.westCrossWallPosition");
	s.backCrossWallThickness = MonumentLength(c.westCrossWallThickness, "cella.westCrossWallThickness");
	s.doorWidth = MonumentLength(c.doorWidth, "cella.doorWidth");
	s.lintelHeight = MonumentLength(c.lintelHeight, "cella.lintelHeight");
	s.doorJambOffset = MonumentLength(c.doorJambOffset, "cella.doorJambOffset");
	s.thresholdLength = MonumentLength(c.thresholdLength, "cella.thresholdLength");
	s.thresholdProjection = MonumentLength(c.thresholdProjection, "cella.thresholdProjection");
	s.thresholdDepth = MonumentLength(c.thresholdDepth, "cella.thresholdDepth");
	s.naosFloorHeight = MonumentLength(c.naosFloorHeight, "cella.naosFloorHeight");
	s.frontAntaRetreat = MonumentLength(c.eastAntaRetreat, "cella.eastAntaRetreat");
	s.backAntaRetreat = MonumentLength(c.westAntaRetreat, "cella.westAntaRetreat");
	s.antaInnerProjection = MonumentLength(c.antaInnerProjection, "cella.antaInnerProjection");
	s.antaOuterProjection = MonumentLength(c.antaOuterProjection, "cella.antaOuterProjection");
	s.antaHeadLength = MonumentLength(c.antaHeadLength, "cella.antaHeadLength");
	s.porchColumnHeight = MonumentLength(c.porchColumnHeight, "cella.porchColumnHeight");
	s.frontPorchAxis = MonumentLength(c.pronaosColumnAxis, "cella.pronaosColumnAxis");
	s.backPorchAxis = MonumentLength(c.opisthodomosColumnAxis, "cella.opisthodomosColumnAxis");
	s.porchArchitraveHeight = MonumentLength(c.porchArchitraveHeight, "cella.porchArchitraveHeight");
	s.porchFriezeHeight = MonumentLength(c.porchFriezeHeight, "cella.porchFriezeHeight");
	s.porchEntablatureDepth = MonumentLength(c.porchEntablatureDepth, "cella.porchEntablatureDepth");
	s.porchEntablatureFront = MonumentLength(c.porchEntablatureFront, "cella.porchEntablatureFront");
	return s;
}

GreekPorchColumnsSettings MonumentPorchSettings (const MonumentData &data)
{
	const MonumentCella &c = data.cella;
	const GreekDoricColumnSettings peristyle = MonumentColumnSettings(data, false);
	// Chapiteau du peristyle, inchange (README section 7.3) : seul le fut change de hauteur.
	double capital = peristyle.hypotrachelionHeight + peristyle.echinusHeight + peristyle.abacusHeight;
	for (const GreekDoricAnnulet &a : peristyle.annulets)
		capital += a.height;
	GreekPorchColumnsSettings s;
	s.column = peristyle;
	s.column.lowerDiameter = MonumentLength(c.porchColumnDiameter, "cella.porchColumnDiameter");
	s.column.height = MonumentLength(c.porchColumnHeight, "cella.porchColumnHeight");
	const double scale = (s.column.height - capital) / (peristyle.height - capital);
	s.column.entasisSagitta = peristyle.entasisSagitta * scale;
	s.column.entasisHeight = peristyle.entasisHeight * scale;
	s.offset = MonumentLength(c.porchColumnOffset, "cella.porchColumnOffset");
	return s;
}

namespace
{
	// Colonne d'un etage de l'interieur (registre CE-16 a CE-18) : celle du peristyle, fut et chapiteau reduits.
	GreekDoricColumnSettings InteriorColumn (const GreekDoricColumnSettings &peristyle, double lowerDiameter,
	                                         double upperDiameter, double height, double capitalHeight, double abacus,
	                                         int flutes)
	{
		double capital = peristyle.hypotrachelionHeight + peristyle.echinusHeight + peristyle.abacusHeight;
		for (const GreekDoricAnnulet &a : peristyle.annulets)
			capital += a.height;
		const double kh = capitalHeight / capital;
		const double upperRadius = 0.5 * upperDiameter;
		const double kp = (0.5 * abacus - upperRadius) / peristyle.abacusProjection;
		const double shaftScale = (height - capitalHeight) / (peristyle.height - capital);

		GreekDoricColumnSettings s = peristyle;
		s.height = height;
		s.lowerDiameter = lowerDiameter;
		s.upperDiameter = upperDiameter;
		s.entasisSagitta = peristyle.entasisSagitta * (lowerDiameter / peristyle.lowerDiameter);
		s.entasisHeight = peristyle.entasisHeight * shaftScale;
		s.fluteCount = flutes;

		s.hypotrachelionHeight = peristyle.hypotrachelionHeight * kh;
		s.grooveHeight = peristyle.grooveHeight * kh;
		for (GreekDoricAnnulet &a : s.annulets)
		{
			a.height *= kh;
			a.projection *= kp;
		}
		s.echinusHeight = peristyle.echinusHeight * kh;
		s.echinusJunctionHeight = peristyle.echinusJunctionHeight * kh;
		s.echinusFootProjection = peristyle.echinusFootProjection * kp;
		s.echinusJunctionProjection = peristyle.echinusJunctionProjection * kp;
		s.echinusTopProjection = peristyle.echinusTopProjection * kp;
		// Levre : arc semblable entre la jonction et la tete reduites, rayon au rapport des cordes.
		const double chord = std::hypot(peristyle.echinusTopProjection - peristyle.echinusJunctionProjection,
		                                peristyle.echinusHeight - peristyle.echinusJunctionHeight);
		const double scaledChord = std::hypot(s.echinusTopProjection - s.echinusJunctionProjection,
		                                      s.echinusHeight - s.echinusJunctionHeight);
		s.echinusLipRadius = peristyle.echinusLipRadius * (scaledChord / chord);
		s.abacusHeight = peristyle.abacusHeight * kh;
		s.abacusProjection = 0.5 * abacus - upperRadius;

		// Rainure : GD3d sur le nouveau fut et le nouveau nombre de cannelures.
		s.grooveDepth =
			upperRadius * (1.0 - GreekDoricFluteBottomRatio(flutes, s.fluteSection, s.fluteDepthRatio)) + s.grooveHeight;
		return s;
	}
}

NaosInteriorSettings MonumentInteriorSettings (const MonumentData &data)
{
	const MonumentInterior &v = data.interior;
	const GreekDoricColumnSettings peristyle = MonumentColumnSettings(data, false);
	const int flutes = RequiredCount(v.fluteCount, "interior.fluteCount");
	const auto L = [](const MonumentValue &x, const char *key) { return MonumentLength(x, key); };
	NaosInteriorSettings s;
	s.spacing = L(v.spacing, "interior.spacing");
	s.frontAxisOffset = L(v.eastAxisOffset, "interior.eastAxisOffset");
	s.backAxisOffset = L(v.westAxisOffset, "interior.westAxisOffset");
	s.wallAxisOffset = L(v.wallAxisOffset, "interior.wallAxisOffset");
	s.naveLevel = L(v.naveLevel, "interior.naveLevel");
	s.lowerColumn = InteriorColumn(peristyle, L(v.lowerColumnLowerDiameter, "interior.lowerColumnLowerDiameter"),
	                               L(v.lowerColumnUpperDiameter, "interior.lowerColumnUpperDiameter"),
	                               L(v.lowerColumnHeight, "interior.lowerColumnHeight"),
	                               L(v.lowerCapitalHeight, "interior.lowerCapitalHeight"),
	                               L(v.lowerAbacus, "interior.lowerAbacus"), flutes);
	s.upperColumn = InteriorColumn(peristyle, L(v.upperColumnLowerDiameter, "interior.upperColumnLowerDiameter"),
	                               L(v.upperColumnUpperDiameter, "interior.upperColumnUpperDiameter"),
	                               L(v.upperColumnHeight, "interior.upperColumnHeight"),
	                               L(v.upperCapitalHeight, "interior.upperCapitalHeight"),
	                               L(v.upperAbacus, "interior.upperAbacus"), flutes);
	s.lowerEpistyleHeight = L(v.lowerEpistyleHeight, "interior.lowerEpistyleHeight");
	s.lowerEpistyleSoffit = L(v.lowerEpistyleSoffit, "interior.lowerEpistyleSoffit");
	s.upperEpistyleHeight = L(v.upperEpistyleHeight, "interior.upperEpistyleHeight");
	s.upperEpistyleSoffit = L(v.upperEpistyleSoffit, "interior.upperEpistyleSoffit");
	s.courseHeight = L(v.courseHeight, "interior.courseHeight");
	s.courseThickness = L(v.courseThickness, "interior.courseThickness");
	s.corniceHeight = L(v.corniceHeight, "interior.corniceHeight");
	s.corniceProjection = L(v.corniceProjection, "interior.corniceProjection");
	s.pocketCeiling = L(v.pocketCeiling, "interior.pocketCeiling");
	s.pocketMargin = L(v.pocketMargin, "interior.pocketMargin");
	return s;
}

EpicranitisSettings MonumentEpicranitisSettings (const MonumentData &data)
{
	const MonumentEpicranitis &v = data.epicranitis;
	EpicranitisSettings s;
	s.height = MonumentLength(v.height, "epicranitis.height");
	s.innerOffset = MonumentLength(v.innerOffset, "epicranitis.innerOffset");
	s.listelHeight = MonumentLength(v.listelHeight, "epicranitis.listelHeight");
	s.filletHeight = MonumentLength(v.filletHeight, "epicranitis.filletHeight");
	s.listelProjection = MonumentLength(v.listelProjection, "epicranitis.listelProjection");
	s.bandProjection = MonumentLength(v.bandProjection, "epicranitis.bandProjection");
	return s;
}

PorchExtensionSettings MonumentPorchExtensionSettings (const MonumentData &data)
{
	PorchExtensionSettings s;
	s.eastSpan = MonumentLength(data.porchExtension.eastSpan, "porchExtension.eastSpan");
	return s;
}

EastCeilingSettings MonumentEastCeilingSettings (const MonumentData &data)
{
	const MonumentEastCeiling &v = data.eastCeiling;
	EastCeilingSettings s;
	s.beamCount = RequiredCount(v.beamCount, "eastCeiling.beamCount");
	s.beamSpacing = MonumentLength(v.beamSpacing, "eastCeiling.beamSpacing");
	s.beamWidth = MonumentLength(v.beamWidth, "eastCeiling.beamWidth");
	s.beamHeight = MonumentLength(v.beamHeight, "eastCeiling.beamHeight");
	s.beamWestBearing = MonumentLength(v.beamWestBearing, "eastCeiling.beamWestBearing");
	s.endMassifWidth = MonumentLength(v.endMassifWidth, "eastCeiling.endMassifWidth");
	s.cornice.height = MonumentLength(v.corniceHeight, "eastCeiling.corniceHeight");
	s.cornice.projection = MonumentLength(v.corniceProjection, "eastCeiling.corniceProjection");
	s.cornice.bandHeight = MonumentLength(v.bandHeight, "eastCeiling.bandHeight");
	s.cornice.depth = MonumentLength(v.corniceDepth, "eastCeiling.corniceDepth");
	s.coffers.pitch = MonumentLength(v.cofferPitch, "eastCeiling.cofferPitch");
	s.coffers.opening = MonumentLength(v.cofferOpening, "eastCeiling.cofferOpening");
	s.coffers.frame = MonumentLength(v.cofferFrame, "eastCeiling.cofferFrame");
	s.coffers.depth = MonumentLength(v.cofferDepth, "eastCeiling.cofferDepth");
	s.cofferRows = RequiredCount(v.cofferRows, "eastCeiling.cofferRows");
	return s;
}

FlankCeilingSettings MonumentFlankCeilingSettings (const MonumentData &data)
{
	const MonumentFlankCeilings &v = data.flankCeilings;
	FlankCeilingSettings s;
	s.beamSpacing = MonumentLength(v.beamSpacing, "flankCeilings.beamSpacing");
	s.firstBeamOffset = MonumentLength(v.firstBeamOffset, "flankCeilings.firstBeamOffset");
	s.frontBeamCount = RequiredCount(v.eastBeamCount, "flankCeilings.eastBeamCount");
	s.backBeamCount = RequiredCount(v.westBeamCount, "flankCeilings.westBeamCount");
	s.beamWidth = MonumentLength(v.beamWidth, "flankCeilings.beamWidth");
	s.beamHeight = MonumentLength(v.beamHeight, "flankCeilings.beamHeight");
	s.peristyleGap = MonumentLength(v.peristyleGap, "flankCeilings.peristyleGap");
	s.cofferRows = RequiredCount(v.cofferRows, "flankCeilings.cofferRows");
	return s;
}

namespace
{
	using ConsumedValues = std::vector<std::pair<const char *, const MonumentValue *>>;

	// Cles consommees par la conversion de chaque partie (MO-P1), dans l'ordre du document. Hors table : les cles
	// que la conversion exige manquantes (gardes GD3d, PE4), les cles facultatives a repli (entablature.innerFace,
	// EN1) et celles qu'elle ne lit pas.
	ConsumedValues ConsumedValuesOf (const MonumentData &d, MonumentPart part)
	{
		switch (part)
		{
		case MonumentPart::Krepis:
		{
			const MonumentKrepis &k = d.krepis;
			return {{"krepis.stylobateWidth", &k.stylobateWidth},     {"krepis.stylobateLength", &k.stylobateLength},
			        {"krepis.stepCount", &k.stepCount},               {"krepis.stylobateHeight", &k.stylobateHeight},
			        {"krepis.secondStepHeight", &k.secondStepHeight}, {"krepis.thirdStepHeight", &k.thirdStepHeight},
			        {"krepis.secondStepTread", &k.secondStepTread},   {"krepis.thirdStepTread", &k.thirdStepTread},
			        {"krepis.euthynteriaHeight", &k.euthynteriaHeight},
			        {"krepis.euthynteriaTread", &k.euthynteriaTread}};
		}
		case MonumentPart::Peristyle:
		{
			const MonumentPeristyle &p = d.peristyle;
			return {{"peristyle.frontColumns", &p.frontColumns},
			        {"peristyle.flankColumns", &p.flankColumns},
			        {"peristyle.frontSpacing", &p.frontSpacing},
			        {"peristyle.flankSpacing", &p.flankSpacing},
			        {"peristyle.frontCornerSpacing", &p.frontCornerSpacing},
			        {"peristyle.flankCornerSpacing", &p.flankCornerSpacing}};
		}
		case MonumentPart::Columns:
		{
			const MonumentColumns &c = d.columns;
			return {{"columns.lowerDiameter", &c.lowerDiameter},
			        {"columns.cornerLowerDiameter", &c.cornerLowerDiameter},
			        {"columns.upperDiameter", &c.upperDiameter},
			        {"columns.height", &c.height},
			        {"columns.fluteCount", &c.fluteCount},
			        {"columns.fluteDepth", &c.fluteDepth},
			        {"columns.entasisSagitta", &c.entasisSagitta},
			        {"columns.entasisHeight", &c.entasisHeight},
			        {"columns.abacusHeight", &c.abacusHeight},
			        {"columns.abacusHalfWidth", &c.abacusHalfWidth},
			        {"columns.echinusHeight", &c.echinusHeight},
			        {"columns.echinusJunctionDepth", &c.echinusJunctionDepth},
			        {"columns.echinusJunctionRetreat", &c.echinusJunctionRetreat},
			        {"columns.echinusTopRetreat", &c.echinusTopRetreat},
			        {"columns.echinusLipRadius", &c.echinusLipRadius},
			        {"columns.annuletCount", &c.annuletCount},
			        {"columns.annuletsHeight", &c.annuletsHeight},
			        {"columns.annuletsTopRetreat", &c.annuletsTopRetreat},
			        {"columns.annuletsBottomRetreat", &c.annuletsBottomRetreat},
			        {"columns.hypotrachelionHeight", &c.hypotrachelionHeight},
			        {"columns.grooveHeight", &c.grooveHeight}};
		}
		case MonumentPart::Entablature:
		{
			// La suite triglyphDivisions est traitee a part (MissingMonumentKeys).
			const MonumentEntablature &e = d.entablature;
			return {{"entablature.taeniaHeight", &e.taeniaHeight},
			        {"entablature.regulaHeight", &e.regulaHeight},
			        {"entablature.guttaeHeight", &e.guttaeHeight},
			        {"entablature.architraveFaceHeight", &e.architraveFaceHeight},
			        {"entablature.friezeHeight", &e.friezeHeight},
			        {"entablature.triglyphCapitalHeight", &e.triglyphCapitalHeight},
			        {"entablature.mutulePlateHeight", &e.mutulePlateHeight},
			        {"entablature.dripHeight", &e.dripHeight},
			        {"entablature.coronaHeight", &e.coronaHeight},
			        {"entablature.filletHeight", &e.filletHeight},
			        {"entablature.crownHeight", &e.crownHeight},
			        {"entablature.architraveTopProjection", &e.architraveTopProjection},
			        {"entablature.architraveFootProjection", &e.architraveFootProjection},
			        {"entablature.taeniaProjection", &e.taeniaProjection},
			        {"entablature.triglyphFace", &e.triglyphFace},
			        {"entablature.triglyphCapitalBandProjection", &e.triglyphCapitalBandProjection},
			        {"entablature.triglyphCapitalListelProjection", &e.triglyphCapitalListelProjection},
			        {"entablature.triglyphCapitalListelHeight", &e.triglyphCapitalListelHeight},
			        {"entablature.triglyphCapitalBandHeight", &e.triglyphCapitalBandHeight},
			        {"entablature.metopeFace", &e.metopeFace},
			        {"entablature.mutulePlateProjection", &e.mutulePlateProjection},
			        {"entablature.dripProjection", &e.dripProjection},
			        {"entablature.coronaProjection", &e.coronaProjection},
			        {"entablature.filletProjection", &e.filletProjection},
			        {"entablature.crownProjection", &e.crownProjection},
			        {"entablature.triglyphWidth", &e.triglyphWidth},
			        {"entablature.regulaProjection", &e.regulaProjection},
			        {"entablature.regulaGuttaCount", &e.regulaGuttaCount},
			        {"entablature.regulaGuttaDiameter", &e.regulaGuttaDiameter},
			        {"entablature.regulaGuttaGap", &e.regulaGuttaGap},
			        {"entablature.regulaGuttaTopProjection", &e.regulaGuttaTopProjection},
			        {"entablature.regulaGuttaFootProjection", &e.regulaGuttaFootProjection},
			        {"entablature.mutuleWidth", &e.mutuleWidth},
			        {"entablature.mutuleDepth", &e.mutuleDepth},
			        {"entablature.soffitSlope", &e.soffitSlope},
			        {"entablature.mutuleGuttaFrontDiameter", &e.mutuleGuttaFrontDiameter},
			        {"entablature.mutuleGuttaMiddleDiameter", &e.mutuleGuttaMiddleDiameter},
			        {"entablature.mutuleGuttaRearDiameter", &e.mutuleGuttaRearDiameter},
			        {"entablature.mutuleGuttaFrontGap", &e.mutuleGuttaFrontGap},
			        {"entablature.mutuleGuttaRearGap", &e.mutuleGuttaRearGap},
			        {"entablature.mutuleGuttaHeight", &e.mutuleGuttaHeight},
			        {"entablature.mutuleGuttaEdgeAxis", &e.mutuleGuttaEdgeAxis},
			        {"entablature.mutuleGuttaPerRow", &e.mutuleGuttaPerRow},
			        {"entablature.mutuleGuttaFootFlare", &e.mutuleGuttaFootFlare}};
		}
		case MonumentPart::Pediment:
		{
			const MonumentPediment &f = d.pediment;
			return {{"pediment.apexRise", &f.apexRise},
			        {"pediment.rakingRun", &f.rakingRun},
			        {"pediment.tympanumFace", &f.tympanumFace}};
		}
		case MonumentPart::Cella:
		{
			// Tout le bloc sauf le diametre et l'ecart des colonnes des porches : la hauteur des colonnes donne le pied
			// de l'architrave des porches, leurs axes la ligne sur laquelle elle est centree.
			const MonumentCella &c = d.cella;
			return {{"cella.toichobateRise", &c.toichobateRise},
			        {"cella.pteronEast", &c.pteronEast},
			        {"cella.pteronWest", &c.pteronWest},
			        {"cella.toichobateOverhang", &c.toichobateOverhang},
			        {"cella.longWallThickness", &c.longWallThickness},
			        {"cella.longWallSpan", &c.longWallSpan},
			        {"cella.wallTop", &c.wallTop},
			        {"cella.eastCrossWallPosition", &c.eastCrossWallPosition},
			        {"cella.eastCrossWallThickness", &c.eastCrossWallThickness},
			        {"cella.westCrossWallPosition", &c.westCrossWallPosition},
			        {"cella.westCrossWallThickness", &c.westCrossWallThickness},
			        {"cella.doorWidth", &c.doorWidth},
			        {"cella.lintelHeight", &c.lintelHeight},
			        {"cella.doorJambOffset", &c.doorJambOffset},
			        {"cella.thresholdLength", &c.thresholdLength},
			        {"cella.thresholdProjection", &c.thresholdProjection},
			        {"cella.thresholdDepth", &c.thresholdDepth},
			        {"cella.naosFloorHeight", &c.naosFloorHeight},
			        {"cella.eastAntaRetreat", &c.eastAntaRetreat},
			        {"cella.westAntaRetreat", &c.westAntaRetreat},
			        {"cella.antaInnerProjection", &c.antaInnerProjection},
			        {"cella.antaOuterProjection", &c.antaOuterProjection},
			        {"cella.antaHeadLength", &c.antaHeadLength},
			        {"cella.porchColumnHeight", &c.porchColumnHeight},
			        {"cella.pronaosColumnAxis", &c.pronaosColumnAxis},
			        {"cella.opisthodomosColumnAxis", &c.opisthodomosColumnAxis},
			        {"cella.porchArchitraveHeight", &c.porchArchitraveHeight},
			        {"cella.porchFriezeHeight", &c.porchFriezeHeight},
			        {"cella.porchEntablatureDepth", &c.porchEntablatureDepth},
			        {"cella.porchEntablatureFront", &c.porchEntablatureFront}};
		}
		case MonumentPart::Porches:
		{
			// Les axes des porches sont lus par la cella (ligne des axes de l'architrave), dont les porches dependent.
			const MonumentCella &c = d.cella;
			return {{"cella.porchColumnDiameter", &c.porchColumnDiameter},
			        {"cella.porchColumnHeight", &c.porchColumnHeight},
			        {"cella.porchColumnOffset", &c.porchColumnOffset}};
		}
		case MonumentPart::Interior:
		{
			const MonumentInterior &i = d.interior;
			return {{"interior.spacing", &i.spacing},
			        {"interior.eastAxisOffset", &i.eastAxisOffset},
			        {"interior.westAxisOffset", &i.westAxisOffset},
			        {"interior.wallAxisOffset", &i.wallAxisOffset},
			        {"interior.naveLevel", &i.naveLevel},
			        {"interior.lowerColumnHeight", &i.lowerColumnHeight},
			        {"interior.lowerColumnLowerDiameter", &i.lowerColumnLowerDiameter},
			        {"interior.lowerColumnUpperDiameter", &i.lowerColumnUpperDiameter},
			        {"interior.lowerCapitalHeight", &i.lowerCapitalHeight},
			        {"interior.lowerAbacus", &i.lowerAbacus},
			        {"interior.lowerEpistyleHeight", &i.lowerEpistyleHeight},
			        {"interior.lowerEpistyleSoffit", &i.lowerEpistyleSoffit},
			        {"interior.upperColumnHeight", &i.upperColumnHeight},
			        {"interior.upperColumnLowerDiameter", &i.upperColumnLowerDiameter},
			        {"interior.upperColumnUpperDiameter", &i.upperColumnUpperDiameter},
			        {"interior.upperCapitalHeight", &i.upperCapitalHeight},
			        {"interior.upperAbacus", &i.upperAbacus},
			        {"interior.fluteCount", &i.fluteCount},
			        {"interior.upperEpistyleHeight", &i.upperEpistyleHeight},
			        {"interior.upperEpistyleSoffit", &i.upperEpistyleSoffit},
			        {"interior.courseHeight", &i.courseHeight},
			        {"interior.courseThickness", &i.courseThickness},
			        {"interior.corniceHeight", &i.corniceHeight},
			        {"interior.corniceProjection", &i.corniceProjection},
			        {"interior.pocketCeiling", &i.pocketCeiling},
			        {"interior.pocketMargin", &i.pocketMargin}};
		}
		case MonumentPart::Epicranitis:
		{
			const MonumentEpicranitis &e = d.epicranitis;
			return {{"epicranitis.height", &e.height},
			        {"epicranitis.innerOffset", &e.innerOffset},
			        {"epicranitis.listelHeight", &e.listelHeight},
			        {"epicranitis.filletHeight", &e.filletHeight},
			        {"epicranitis.listelProjection", &e.listelProjection},
			        {"epicranitis.bandProjection", &e.bandProjection}};
		}
		case MonumentPart::PorchExtension:
			return {{"porchExtension.eastSpan", &d.porchExtension.eastSpan}};
		case MonumentPart::EastCeiling:
		{
			const MonumentEastCeiling &e = d.eastCeiling;
			return {{"eastCeiling.beamCount", &e.beamCount},
			        {"eastCeiling.beamSpacing", &e.beamSpacing},
			        {"eastCeiling.beamWidth", &e.beamWidth},
			        {"eastCeiling.beamHeight", &e.beamHeight},
			        {"eastCeiling.beamWestBearing", &e.beamWestBearing},
			        {"eastCeiling.endMassifWidth", &e.endMassifWidth},
			        {"eastCeiling.corniceHeight", &e.corniceHeight},
			        {"eastCeiling.corniceProjection", &e.corniceProjection},
			        {"eastCeiling.bandHeight", &e.bandHeight},
			        {"eastCeiling.corniceDepth", &e.corniceDepth},
			        {"eastCeiling.cofferPitch", &e.cofferPitch},
			        {"eastCeiling.cofferOpening", &e.cofferOpening},
			        {"eastCeiling.cofferFrame", &e.cofferFrame},
			        {"eastCeiling.cofferDepth", &e.cofferDepth},
			        {"eastCeiling.cofferRows", &e.cofferRows}};
		}
		case MonumentPart::FlankCeilings:
		{
			const MonumentFlankCeilings &f = d.flankCeilings;
			return {{"flankCeilings.beamSpacing", &f.beamSpacing},
			        {"flankCeilings.firstBeamOffset", &f.firstBeamOffset},
			        {"flankCeilings.eastBeamCount", &f.eastBeamCount},
			        {"flankCeilings.westBeamCount", &f.westBeamCount},
			        {"flankCeilings.beamWidth", &f.beamWidth},
			        {"flankCeilings.beamHeight", &f.beamHeight},
			        {"flankCeilings.peristyleGap", &f.peristyleGap},
			        {"flankCeilings.cofferRows", &f.cofferRows}};
		}
		}
		throw std::invalid_argument("Monument : partie hors de l'enumeration");
	}

	bool PartPresent (const MonumentData &d, MonumentPart part)
	{
		switch (part)
		{
		case MonumentPart::Krepis:      return d.present.krepis;
		case MonumentPart::Peristyle:   return d.present.peristyle;
		case MonumentPart::Columns:     return d.present.columns;
		case MonumentPart::Entablature: return d.present.entablature;
		case MonumentPart::Pediment:    return d.present.pediment;
		case MonumentPart::Cella:       return d.present.cella;
		case MonumentPart::Porches:     return d.present.cella;
		case MonumentPart::Interior:    return d.present.interior;
		case MonumentPart::Epicranitis: return d.present.epicranitis;
		case MonumentPart::PorchExtension: return d.present.porchExtension;
		case MonumentPart::EastCeiling: return d.present.eastCeiling;
		case MonumentPart::FlankCeilings: return d.present.flankCeilings;
		}
		throw std::invalid_argument("Monument : partie hors de l'enumeration");
	}

	// Plages des degres de la krepis de BuildKrepisMesh sans stations : kKrepisPlainStepSize par degre, concatenes.
	std::vector<TempleMeshPart> KrepisParts (const Mesh &mesh, std::size_t steps)
	{
		constexpr unsigned kStepVertices = kKrepisPlainStepSize.vertices, kStepFaces = kKrepisPlainStepSize.faces;
		if (static_cast<std::size_t>(mesh.GetNVertices()) != steps * kStepVertices ||
		    static_cast<std::size_t>(mesh.GetNFaces()) != steps * kStepFaces)
			throw std::logic_error("Monument : krepis de taille inattendue");
		std::vector<TempleMeshPart> parts;
		parts.reserve(steps);
		for (std::size_t k = 0; k < steps; ++k)
		{
			TempleMeshPart part;
			part.address = {TempleElement::KrepisStep, PeristyleSide::Front, static_cast<int>(k)};
			part.firstVertex = static_cast<unsigned>(k) * kStepVertices;
			part.vertexCount = kStepVertices;
			part.firstFace = static_cast<unsigned>(k) * kStepFaces;
			part.faceCount = kStepFaces;
			parts.push_back(part);
		}
		return parts;
	}
}

std::vector<std::string> MissingMonumentKeys (const MonumentData &data, MonumentPart part)
{
	std::vector<std::string> missing;
	const ConsumedValues values = ConsumedValuesOf(data, part);
	for (const auto &[key, value] : values)
	{
		if (!value->value)
			missing.emplace_back(key);
		// Suite du triglyphe a sa place au document, entre metopeWidth (non consommee) et regulaProjection.
		if (part == MonumentPart::Entablature && std::string_view(key) == "entablature.triglyphWidth")
		{
			const MonumentDivisions &dv = data.entablature.triglyphDivisions;
			if (dv.status == MonumentStatus::Missing || dv.values.empty())
				missing.emplace_back("entablature.triglyphDivisions");
		}
	}
	if (part == MonumentPart::Pediment)
	{
		const MonumentPolygon &p = data.pediment.rakingSection;
		if (p.status == MonumentStatus::Missing || p.u.empty())
			missing.emplace_back("pediment.rakingSection");
	}
	return missing;
}

std::vector<MonumentPart> MonumentPartDependencies (MonumentPart part)
{
	using P = MonumentPart;
	switch (part)
	{
	case P::Krepis:      return {};
	case P::Peristyle:   return {P::Krepis};
	case P::Columns:     return {P::Krepis, P::Peristyle};
	case P::Entablature: return {P::Krepis, P::Peristyle, P::Columns};
	case P::Pediment:    return {P::Krepis, P::Peristyle, P::Columns, P::Entablature};
	case P::Cella:       return {P::Krepis};
	case P::Porches:     return {P::Krepis, P::Peristyle, P::Columns, P::Cella};
	case P::Interior:    return {P::Krepis, P::Peristyle, P::Columns, P::Cella};
	case P::Epicranitis: return {P::Krepis, P::Cella};
	case P::PorchExtension:
		return {P::Krepis, P::Peristyle, P::Columns, P::Entablature, P::Cella, P::Epicranitis};
	case P::EastCeiling:
		return {P::Krepis, P::Peristyle, P::Columns, P::Entablature, P::Cella, P::Epicranitis, P::PorchExtension};
	case P::FlankCeilings:
		return {P::Krepis, P::Peristyle, P::Columns, P::Entablature, P::Cella, P::Epicranitis, P::PorchExtension,
		        P::EastCeiling};
	}
	throw std::invalid_argument("Monument : partie hors de l'enumeration");
}

namespace
{
	// Piece deja dans le repere du temple et son adresse.
	struct AddressedPiece
	{
		const Mesh   *mesh = nullptr;
		TempleAddress address;
	};

	// Krepis de BuildKrepisMesh, puis, s'il y en a, les pieces de la cella et du couronnement, deja dans le repere du
	// temple : sommets recopies, faces decalees, une partie par piece.
	std::unique_ptr<Mesh> AppendPieces (const Mesh &krepis, const std::vector<AddressedPiece> &pieces,
	                                    std::vector<TempleMeshPart> &parts)
	{
		std::vector<float> vertices = krepis.GetVertices();
		std::vector<unsigned> faces;
		std::size_t faceCount = krepis.GetNFaces();
		for (const AddressedPiece &piece : pieces)
			faceCount += piece.mesh->GetNFaces();
		faces.reserve(3 * faceCount);
		for (unsigned f = 0; f < krepis.GetNFaces(); ++f)
			for (unsigned k = 0; k < 3; ++k)
				faces.push_back(static_cast<unsigned>(krepis.GetFaceVertex(f, k)));
		for (const AddressedPiece &piece : pieces)
		{
			TempleMeshPart part;
			part.address = piece.address;
			part.firstVertex = static_cast<unsigned>(vertices.size() / 3);
			part.firstFace = static_cast<unsigned>(faces.size() / 3);
			const std::vector<float> &v = piece.mesh->GetVertices();
			vertices.insert(vertices.end(), v.begin(), v.end());
			for (unsigned f = 0; f < piece.mesh->GetNFaces(); ++f)
				for (unsigned k = 0; k < 3; ++k)
					faces.push_back(part.firstVertex + static_cast<unsigned>(piece.mesh->GetFaceVertex(f, k)));
			part.vertexCount = static_cast<unsigned>(vertices.size() / 3) - part.firstVertex;
			part.faceCount = static_cast<unsigned>(faces.size() / 3) - part.firstFace;
			parts.push_back(part);
		}
		auto mesh = std::make_unique<Mesh>();
		mesh->SetVertices(static_cast<unsigned>(vertices.size() / 3), vertices.data());
		mesh->SetFaces(static_cast<unsigned>(faces.size() / 3), 3, faces.data());
		return mesh;
	}
}

MonumentData MonumentWithoutPart (const MonumentData &data, MonumentPart part)
{
	MonumentData out = data;
	switch (part)
	{
	case MonumentPart::Krepis:         ClearBlock(out, "krepis"); break;
	case MonumentPart::Peristyle:      ClearBlock(out, "peristyle"); break;
	case MonumentPart::Columns:        ClearBlock(out, "columns"); break;
	case MonumentPart::Entablature:    ClearBlock(out, "entablature"); break;
	case MonumentPart::Pediment:       ClearBlock(out, "pediment"); break;
	case MonumentPart::Cella:
	case MonumentPart::Porches:        ClearBlock(out, "cella"); break;
	case MonumentPart::Interior:       ClearBlock(out, "interior"); break;
	case MonumentPart::Epicranitis:    ClearBlock(out, "epicranitis"); break;
	case MonumentPart::PorchExtension: ClearBlock(out, "porchExtension"); break;
	case MonumentPart::EastCeiling:    ClearBlock(out, "eastCeiling"); break;
	case MonumentPart::FlankCeilings:  ClearBlock(out, "flankCeilings"); break;
	}
	// Blocs qui exigeaient un bloc retire, de proche en proche (MO-P2).
	for (bool changed = true; changed;)
	{
		changed = false;
		for (const BlockDependency &dep : BlockDependencies(out.present))
			if (dep.present && !dep.neededPresent)
			{
				ClearBlock(out, dep.block);
				changed = true;
			}
	}
	return out;
}

MonumentData MonumentExterior (const MonumentData &data)
{
	return MonumentWithoutPart(data, MonumentPart::Cella);
}

std::array<MonumentPartReport, kMonumentPartCount> ReportMonumentParts (const MonumentData &data)
{
	for (const BlockDependency &dep : BlockDependencies(data.present))
		if (dep.present && !dep.neededPresent)
			throw std::invalid_argument(std::string("Monument : '") + dep.block + "' demande le bloc '" + dep.needed + "'");
	constexpr std::size_t kPartCount = kMonumentPartCount;
	std::array<MonumentPartReport, kPartCount> report;
	std::array<bool, kPartCount> complete{};
	for (std::size_t i = 0; i < kPartCount; ++i)
	{
		const MonumentPart part = static_cast<MonumentPart>(i);
		MonumentPartReport &r = report[i];
		r.part = part;
		if (!PartPresent(data, part))
			r.state = MonumentPartState::Absent;
		else
		{
			r.missingKeys = MissingMonumentKeys(data, part);
			r.state = r.missingKeys.empty() ? MonumentPartState::Built : MonumentPartState::Incomplete;
		}
		complete[i] = r.state == MonumentPartState::Built;
	}
	// Table des dependances (MO-P3) : une partie complete est bloquee par sa premiere dependance non complete.
	std::array<bool, kPartCount> buildable{};
	for (std::size_t i = 0; i < kPartCount; ++i)
	{
		if (!complete[i])
			continue;
		std::optional<MonumentPart> blocker;
		for (const MonumentPart dependency : MonumentPartDependencies(static_cast<MonumentPart>(i)))
			if (!blocker && !complete[static_cast<std::size_t>(dependency)])
				blocker = dependency;
		buildable[i] = !blocker;
		if (blocker)
		{
			report[i].state = MonumentPartState::Blocked;
			report[i].blockedBy = blocker;
		}
	}
	// MO-P4 : sans colonnes, le peristyle n'a pas de geometrie propre.
	if (buildable[static_cast<std::size_t>(MonumentPart::Peristyle)] && !buildable[static_cast<std::size_t>(MonumentPart::Columns)])
		report[static_cast<std::size_t>(MonumentPart::Peristyle)].state = MonumentPartState::CompleteNotBuilt;
	return report;
}

MonumentBuild BuildMonumentMesh (const MonumentData &data)
{
	MonumentBuild build;
	build.report = ReportMonumentParts(data);
	const auto can = [&](MonumentPart part) {
		const MonumentPartState state = build.report[static_cast<std::size_t>(part)].state;
		return state == MonumentPartState::Built || state == MonumentPartState::CompleteNotBuilt;
	};

	if (can(MonumentPart::Columns))
	{
		TempleSettings settings = TempleSettingsOf(data, can(MonumentPart::Entablature), can(MonumentPart::Pediment));
		if (can(MonumentPart::Cella))
		{
			settings.cella = MonumentCellaSettings(data);
			if (can(MonumentPart::Porches))
				settings.cella->porches = MonumentPorchSettings(data);
			if (can(MonumentPart::Interior))
				settings.interior = MonumentInteriorSettings(data);
			if (can(MonumentPart::Epicranitis))
				settings.epicranitis = MonumentEpicranitisSettings(data);
			if (can(MonumentPart::PorchExtension))
				settings.porchExtension = MonumentPorchExtensionSettings(data);
			if (can(MonumentPart::EastCeiling))
				settings.eastCeiling = MonumentEastCeilingSettings(data);
			if (can(MonumentPart::FlankCeilings))
				settings.flankCeilings = MonumentFlankCeilingSettings(data);
		}
		build.mesh = BuildTempleMesh(settings, nullptr, &build.parts);
		return build;
	}
	if (can(MonumentPart::Peristyle))
	{
		LayoutPeristyle(MonumentPeristyleSettings(data));   // MO-P5 : validation seule, aucune geometrie
	}
	if (can(MonumentPart::Krepis))
	{
		KrepisLayout layout;
		build.mesh = BuildKrepisMesh(MonumentKrepisSettings(data), &layout);
		build.parts = KrepisParts(*build.mesh, layout.steps.size());
		if (can(MonumentPart::Cella))
		{
			const CellaSettings cella = MonumentCellaSettings(data);
			const CellaLayout cellaLayout = LayoutGreekCella(cella, layout);
			const std::vector<CellaPiece> cellaPieces = BuildGreekCellaPieces(cella, cellaLayout);
			std::vector<EpicranitisPiece> crownPieces;
			if (can(MonumentPart::Epicranitis))
			{
				const EpicranitisSettings crown = MonumentEpicranitisSettings(data);
				crownPieces = BuildGreekEpicranitisPieces(crown, LayoutGreekEpicranitis(crown, cellaLayout));
			}
			std::vector<AddressedPiece> pieces;
			pieces.reserve(cellaPieces.size() + crownPieces.size());
			for (const CellaPiece &piece : cellaPieces)
				pieces.push_back({piece.mesh.get(), TempleAddressOf(piece)});
			for (const EpicranitisPiece &piece : crownPieces)
				pieces.push_back({piece.mesh.get(), TempleAddressOf(piece)});
			build.mesh = AppendPieces(*build.mesh, pieces, build.parts);
		}
		return build;
	}
	build.mesh = std::make_unique<Mesh>();
	return build;
}

} // namespace classical
} // namespace cgarchi
