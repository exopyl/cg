// Document du monument "monument/v6" : lecteur strict a exception, document
// embarque et conversion vers les reglages du temple.
#include <cgarchi/classical_monument.h>

#include <cgarchi/classical_monument_data.h>
#include <cgarchi/json_reader.h>

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
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
	constexpr const char *kUnitNames[] = {"m", "in", "mm", "1", "ft", "\302\260"};

	// Mots de la division du triglyphe, dans l'ordre de MonumentDivisionKind.
	constexpr const char *kDivisionKindNames[] = {"demi-canal", "bande", "canal"};

	// Feet : pieds d'une cote en pieds et pouces, cle "<nom>Ft" ; Angle : en degres.
	enum class Quantity { Length, Count, Feet, Angle };

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

	// Membre objet requis : nullptr s'il manque ou n'est pas un objet (refus deja signale).
	const json *RequiredObject (Reader &r, const json &obj, const std::string &parent, const char *key)
	{
		if (!r.Require(obj, parent, {key}))
			return nullptr;
		return r.Object(obj, parent, key);
	}

	// `readme` : chaine non vide.
	void ReadPlace (Reader &r, const json &obj, const std::string &path, std::string &out)
	{
		if (r.String(obj, path, "readme", out) && out.empty())
			r.Error(json_reader::Join(path, "readme"), "empty place");
	}

	// `unit` : une des six ; "1" pour un compte et lui seul, "ft" pour des pieds et eux seuls, "°" pour un
	// angle et lui seul.
	bool ReadUnit (Reader &r, const json &obj, const std::string &path, Quantity quantity, MonumentUnit &out)
	{
		int unit = -1;
		if (!r.Enum(obj, path, "unit",
		            {kUnitNames[0], kUnitNames[1], kUnitNames[2], kUnitNames[3], kUnitNames[4], kUnitNames[5]}, unit))
			return false;
		out = static_cast<MonumentUnit>(unit);
		if (quantity == Quantity::Count && out != MonumentUnit::One)
		{
			r.Error(json_reader::Join(path, "unit"), std::string("a count requires unit \"1\", not \"") +
			                                             kUnitNames[unit] + "\"");
			return false;
		}
		if (quantity == Quantity::Feet && out != MonumentUnit::Foot)
		{
			r.Error(json_reader::Join(path, "unit"), std::string("a feet value requires unit \"ft\", not \"") +
			                                             kUnitNames[unit] + "\"");
			return false;
		}
		if (quantity == Quantity::Length && out == MonumentUnit::One)
		{
			r.Error(json_reader::Join(path, "unit"), "unit \"1\" on a length");
			return false;
		}
		if (quantity == Quantity::Length && out == MonumentUnit::Foot)
		{
			r.Error(json_reader::Join(path, "unit"), "unit \"ft\" outside a feet key");
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
			ReadUnit(m_r, *v, path, quantity, out.unit);

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
			if (quantity == Quantity::Count || quantity == Quantity::Feet)
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

		// Suite de largeurs (regle R5 du script) : values > 0, kinds parmi les trois mots, meme
		// taille, longueur avec son unite ; jamais manquante.
		void Divisions (const json &section, const std::string &parent, const char *key, MonumentDivisions &out)
		{
			const json *v = RequiredObject(m_r, section, parent, key);
			if (!v)
				return;
			const std::string path = json_reader::Join(parent, key);
			m_r.Known(*v, path, {"values", "kinds", "unit", "status", "sources", "readme"});
			m_r.Require(*v, path, {"values", "kinds", "unit", "status", "sources", "readme"});
			ReadUnit(m_r, *v, path, Quantity::Length, out.unit);
			const bool hasStatus = ReadStatus(m_r, *v, path, out.status);
			if (hasStatus && out.status == MonumentStatus::Missing)
				m_r.Error(json_reader::Join(path, "status"), "a division series is never \"manquant\"");
			ReadPlace(m_r, *v, path, out.readme);
			if (ReadSources(*v, path, out.sources) && hasStatus)
				CheckSources(path, out.status, out.sources);

			ReadNumbers(m_r, *v, path, "values", out.values);
			const std::string valuesPath = json_reader::Join(path, "values");
			for (std::size_t i = 0; i < out.values.size(); ++i)
				if (!(std::isfinite(out.values[i]) && out.values[i] > 0.0))
					m_r.Error(json_reader::Indexed(valuesPath, i), "not a finite width > 0");

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
				for (int k = 0; k < 3; ++k)
					if (text == kDivisionKindNames[k])
					{
						out.kinds.push_back(static_cast<MonumentDivisionKind>(k));
						known = true;
					}
				if (!known)
					m_r.Error(itemPath, "unknown division kind \"" + text + "\"");
			}
			if (kinds->size() != out.values.size())
				m_r.Error(kindsPath, "not as many kinds as values");
		}

		const std::set<std::string> &Cited () const { return m_cited; }

	private:
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

	void ReadResult (Reader &r, const json &controls, const char *key, double &out)
	{
		const json *s = RequiredObject(r, controls, "controls", key);
		if (!s)
			return;
		const std::string path = json_reader::Join("controls", key);
		r.Known(*s, path, {"value", "readme"});
		r.Require(*s, path, {"value", "readme"});
		std::string place;
		ReadPlace(r, *s, path, place);
		r.Finite(*s, path, "value", out);
	}

	// Controle sans statut : longueur > 0 avec son unite, ou compte entier >= 1 en "1".
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
		         "columnControls", "entablature", "entablatureControls"});
		r.Require(doc, std::string(), {"monument"});
		r.String(doc, std::string(), "monument", data.monument);
		ReadSourceDocument(r, doc, data.source);
		ReadSourceTable(r, doc, data.sources);

		DocumentReader values(r, data.sources);
		if (const json *k = RequiredObject(r, doc, std::string(), "krepis"))
		{
			const std::string p = "krepis";
			r.Known(*k, p, {"stylobateWidth", "stylobateLength", "stepCount", "stylobateHeight", "secondStepHeight",
			                "thirdStepHeight", "secondStepTread", "thirdStepTread", "cornerMargin", "euthynteria"});
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
			values.Value(*k, p, "euthynteria", Quantity::Length, m.euthynteria);
		}
		if (const json *s = RequiredObject(r, doc, std::string(), "peristyle"))
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
		if (const json *c = RequiredObject(r, doc, std::string(), "columns"))
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
		if (const json *c = RequiredObject(r, doc, std::string(), "controls"))
		{
			r.Known(*c, "controls", {"frontAxesY", "flankAxesX", "frontSum", "flankSum"});
			ReadAxes(r, *c, "frontAxesY", data.controls.frontAxesY);
			ReadAxes(r, *c, "flankAxesX", data.controls.flankAxesX);
			ReadResult(r, *c, "frontSum", data.controls.frontSum);
			ReadResult(r, *c, "flankSum", data.controls.flankSum);
		}
		if (const json *c = RequiredObject(r, doc, std::string(), "columnControls"))
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
		if (const json *e = RequiredObject(r, doc, std::string(), "entablature"))
		{
			const std::string p = "entablature";
			r.Known(*e, p,
			        {"architraveHeightFt", "architraveHeightIn", "taeniaHeight", "regulaHeight", "guttaeHeight",
			         "architraveFaceHeight", "friezeHeightFt", "friezeHeightIn", "triglyphCapitalHeight",
			         "corniceHeightFt", "corniceHeightIn", "mutulePlateHeight", "dripHeight", "coronaHeight",
			         "filletHeight", "crownHeight", "architraveTopProjection", "architraveFootProjection",
			         "taeniaProjection", "triglyphFace", "triglyphCapitalBandProjection",
			         "triglyphCapitalListelProjection", "triglyphCapitalListelHeight", "triglyphCapitalBandHeight",
			         "metopeFace", "mutulePlateProjection", "dripProjection", "coronaProjection", "filletProjection",
			         "crownProjection", "innerFace", "triglyphWidthFt", "triglyphWidthIn", "metopeWidthFt",
			         "metopeWidthIn", "triglyphDivisions", "channelDepth", "regulaProjection", "regulaGuttaCount",
			         "regulaGuttaDiameter", "regulaGuttaGap", "regulaGuttaTopProjection", "regulaGuttaFootProjection",
			         "mutuleWidthFt", "mutuleWidthIn", "mutuleDepth", "soffitSlope", "mutuleGuttaFrontDiameter",
			         "mutuleGuttaMiddleDiameter", "mutuleGuttaRearDiameter", "mutuleGuttaFrontGap", "mutuleGuttaRearGap",
			         "mutuleGuttaHeight", "mutuleGuttaEdgeAxis", "mutuleGuttaPerRow", "mutuleGuttaFootFlare"});
			MonumentEntablature &m = data.entablature;
			const std::pair<const char *, MonumentValue *> feet[] = {
				{"architraveHeightFt", &m.architraveHeightFt},
				{"friezeHeightFt", &m.friezeHeightFt},
				{"corniceHeightFt", &m.corniceHeightFt},
				{"triglyphWidthFt", &m.triglyphWidthFt},
				{"metopeWidthFt", &m.metopeWidthFt},
				{"mutuleWidthFt", &m.mutuleWidthFt},
			};
			const std::pair<const char *, MonumentValue *> lengths[] = {
				{"architraveHeightIn", &m.architraveHeightIn},
				{"taeniaHeight", &m.taeniaHeight},
				{"regulaHeight", &m.regulaHeight},
				{"guttaeHeight", &m.guttaeHeight},
				{"architraveFaceHeight", &m.architraveFaceHeight},
				{"friezeHeightIn", &m.friezeHeightIn},
				{"triglyphCapitalHeight", &m.triglyphCapitalHeight},
				{"corniceHeightIn", &m.corniceHeightIn},
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
				{"triglyphWidthIn", &m.triglyphWidthIn},
				{"metopeWidthIn", &m.metopeWidthIn},
				{"channelDepth", &m.channelDepth},
				{"regulaProjection", &m.regulaProjection},
				{"regulaGuttaDiameter", &m.regulaGuttaDiameter},
				{"regulaGuttaGap", &m.regulaGuttaGap},
				{"regulaGuttaTopProjection", &m.regulaGuttaTopProjection},
				{"regulaGuttaFootProjection", &m.regulaGuttaFootProjection},
				{"mutuleWidthIn", &m.mutuleWidthIn},
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
			for (const auto &[key, value] : feet)
				values.Value(*e, p, key, Quantity::Feet, *value);
			for (const auto &[key, value] : lengths)
				values.Value(*e, p, key, Quantity::Length, *value);
			values.Value(*e, p, "regulaGuttaCount", Quantity::Count, m.regulaGuttaCount);
			values.Value(*e, p, "mutuleGuttaPerRow", Quantity::Count, m.mutuleGuttaPerRow);
			values.Value(*e, p, "soffitSlope", Quantity::Angle, m.soffitSlope);
			values.Divisions(*e, p, "triglyphDivisions", m.triglyphDivisions);
		}
		if (const json *c = RequiredObject(r, doc, std::string(), "entablatureControls"))
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

	// Valeur en pouces employee par une difference calculee en pouces : presente et
	// d'unite "in", sinon refus nomme.
	double RequiredInches (const MonumentValue &v, const char *key)
	{
		const double x = Required(v, key);
		if (v.unit != MonumentUnit::Inch)
			throw std::invalid_argument(std::string("Monument : '") + key + "' n'est pas en pouces (unite \"in\" attendue)");
		return x;
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
	case MonumentUnit::Degree:
		throw std::invalid_argument("Monument : '" + std::string(key) + "' n'est pas une longueur (angle en degres)");
	}
	throw std::invalid_argument("Monument : '" + std::string(key) + "' a une unite hors de l'enumeration");
}

double MonumentLength (const MonumentValue &v, std::string_view key)
{
	// Des pieds seuls ne font pas une longueur du README : ils sont la moitie d'une cote en pieds et pouces.
	if (v.unit == MonumentUnit::Foot)
		throw std::invalid_argument("Monument : '" + std::string(key) +
		                            "' est en pieds, partie d'une cote en pieds et pouces (MonumentFeetAndInches)");
	return MonumentMetres(Required(v, key), v.unit, key);
}

double MonumentFeetAndInches (const MonumentValue &feet, const MonumentValue &inches, std::string_view key)
{
	const std::string feetKey = std::string(key) + "Ft";
	const std::string inchesKey = std::string(key) + "In";
	const double ft = Required(feet, feetKey);
	const double in = Required(inches, inchesKey);
	if (feet.unit != MonumentUnit::Foot)
		throw std::invalid_argument("Monument : '" + feetKey + "' n'est pas en pieds (unite \"ft\" attendue)");
	if (!(ft >= 1.0 && ft <= kCountMax && std::floor(ft) == ft))
		throw std::invalid_argument("Monument : '" + feetKey + "' n'est pas un nombre entier de pieds >= 1");
	if (inches.unit != MonumentUnit::Inch)
		throw std::invalid_argument("Monument : '" + inchesKey + "' n'est pas en pouces (unite \"in\" attendue)");
	if (!(std::isfinite(in) && in >= 0.0))
		throw std::invalid_argument("Monument : '" + inchesKey + "' n'est pas un nombre fini de pouces >= 0");
	// ft x 12 est exact pour ft entier ; le seul produit inexact vient en dernier, rien a contracter.
	return (ft * 12.0 + in) * kMonumentInch;
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

const MonumentData &HephaistosMonument ()
{
	static const MonumentData data = ParseMonument(TempleHephaistosJson());
	return data;
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
	if (k.euthynteria.status != MonumentStatus::Missing)
		throw std::invalid_argument("Monument : 'krepis.euthynteria' donnee, sans debord pour en faire un degre (KR3)");

	const double secondTread = MonumentLength(k.secondStepTread, "krepis.secondStepTread");
	const double thirdTread = MonumentLength(k.thirdStepTread, "krepis.thirdStepTread");
	KrepisSettings s;
	s.stylobateHalfWidth = 0.5 * MonumentLength(k.stylobateWidth, "krepis.stylobateWidth");
	s.stylobateHalfDepth = 0.5 * MonumentLength(k.stylobateLength, "krepis.stylobateLength");
	s.stylobateHeight = MonumentLength(k.stylobateHeight, "krepis.stylobateHeight");
	s.lowerSteps = {
		{MonumentLength(k.secondStepHeight, "krepis.secondStepHeight"), secondTread, secondTread},
		{MonumentLength(k.thirdStepHeight, "krepis.thirdStepHeight"), thirdTread, thirdTread},
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
	// section d'angle est homothetique (README l. 130, GD2a).
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
	// EN1 s'applique a une face interieure manquante ; une face donnee demanderait sa propre lecture.
	if (e.innerFace.status != MonumentStatus::Missing)
		throw std::invalid_argument("Monument : 'entablature.innerFace' donnee, la convention EN1 ne vaut que manquante");

	// Le chapiteau du triglyphe est lu deux fois : sa hauteur (corps de la frise) et son partage
	// (deux bandes). Un desaccord deplacerait le dessus de la frise ; 1e-9 in est au-dessus du bruit
	// d'une somme et en deca du plus petit ecart ecrivable (1e-3 in).
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
	const double friezeBody = MonumentFeetAndInches(e.friezeHeightFt, e.friezeHeightIn, "entablature.friezeHeight") -
	                          capital;
	// Sous-face inclinee (EN21, EN22, D3-67) : pivot sur la face de la plaque, profondeur L le long de la pente ;
	// dos P - L cos theta et montee L sin theta en pouces, puis un seul produit par le pouce.
	// Pente dans ]0, 90[ degres : au-dela, cos et sin rendraient la geometrie d'une autre pente (374 celle de 14).
	const double slopeDegrees = RequiredDegrees(e.soffitSlope, "entablature.soffitSlope");
	if (!(slopeDegrees > 0.0 && slopeDegrees < 90.0))
		throw std::invalid_argument("Monument : 'entablature.soffitSlope' vaut " + std::to_string(slopeDegrees) +
		                            " degres, hors de ]0, 90[");
	const double slope = slopeDegrees * (kPi / 180.0);
	const double plate = RequiredInches(e.mutulePlateProjection, "entablature.mutulePlateProjection");
	const double depth = RequiredInches(e.mutuleDepth, "entablature.mutuleDepth");
	const double mutuleBack = (plate - depth * std::cos(slope)) * kMonumentInch;
	const double mutuleRise = (depth * std::sin(slope)) * kMonumentInch;

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
	s.innerFace = architraveFoot;   // EN1
	// EN15 : triglyphe d'angle et deux metopes egales dans la travee d'angle, premiere solution de la
	// l. 256 (« la regle grecque (convention) ») ; la contraction relevee du peristyle reste celle
	// de MonumentPeristyleSettings.
	s.corner = DoricCornerStrategy::CornerTriglyphEqualMetopes;

	GreekTriglyphSettings &t = s.triglyph;
	t.width = MonumentFeetAndInches(e.triglyphWidthFt, e.triglyphWidthIn, "entablature.triglyphWidth");
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
	plates.width = MonumentFeetAndInches(e.mutuleWidthFt, e.mutuleWidthIn, "entablature.mutuleWidth");
	plates.face = mutules;
	plates.rise = mutuleRise;
	plates.guttae = std::move(guttae);
	s.mutules = std::move(plates);
	return s;
}

TempleSettings MonumentTempleSettings (const MonumentData &data)
{
	TempleSettings t;
	t.krepis = MonumentKrepisSettings(data);
	t.peristyle = MonumentPeristyleSettings(data);
	t.column = MonumentColumnSettings(data, false);
	t.cornerColumn = MonumentColumnSettings(data, true);
	t.entablature = MonumentEntablatureSettings(data);
	return t;
}

} // namespace classical
} // namespace cgarchi
