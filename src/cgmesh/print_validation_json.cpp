#include "print_validation_json.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <locale>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
	using nlohmann::json;

	// Messages : locale classique (point decimal), quelle que soit la locale
	// globale.
	std::string numberText (double v)
	{
		std::ostringstream os;
		os.imbue(std::locale::classic());
		os << v;
		return os.str();
	}

	// Domaine d'un nombre : [lo, hi], chaque borne ouverte ou fermee.
	struct Bounds
	{
		double lo = 0.0;
		bool   loOpen = false;
		double hi = std::numeric_limits<double>::infinity();
		bool   hiOpen = true;

		bool Contains (double v) const
		{
			return (loOpen ? v > lo : v >= lo) && (hiOpen ? v < hi : v <= hi);
		}
		std::string Text () const
		{
			return (loOpen ? "(" : "[") + numberText(lo) + ", " + numberText(hi) + (hiOpen ? ")" : "]");
		}
	};

	Bounds positive () { Bounds b; b.loOpen = true; return b; }
	Bounds printLength () { Bounds b; b.lo = kMinPrintLengthMm; b.hi = kMaxPrintLengthMm; b.hiOpen = false; return b; }
	Bounds ratio () { Bounds b; b.hi = 1.0; return b; }

	class Reader
	{
	public:
		explicit Reader (std::vector<PrintJsonError> &errors) : m_errors(errors) {}

		void Error (const std::string &path, const std::string &message) { m_errors.push_back({path, message}); }

		// Nombre fini dans `bounds` ; false (avec une erreur) sinon.
		bool Number (const json &v, const std::string &path, const Bounds &bounds, double &out)
		{
			if (!v.is_number())
			{
				Error(path, std::string("expected a number, not ") + v.type_name());
				return false;
			}
			const double x = v.get<double>();
			if (!std::isfinite(x) || !bounds.Contains(x))
			{
				Error(path, numberText(x) + " outside " + bounds.Text());
				return false;
			}
			out = x;
			return true;
		}

	private:
		std::vector<PrintJsonError> &m_errors;
	};

	// Champs d'un document print-constraints/v1, schema exclu, dans l'ordre
	// d'ecriture.
	constexpr const char *kFields[] = {"mmPerUnit", "minWallMm", "minHoleMm", "maxTipLengthMm", "noiseDepthRatio",
	                                   "minDetailMm", "envelopeMm", "maxRecordsPerKind"};

	struct Threshold
	{
		const char *name;
		double PrintConstraints::*field;
		Bounds (*bounds) ();
	};
	constexpr Threshold kThresholds[] = {
		{"minWallMm",       &PrintConstraints::minWallMm,       printLength},
		{"minHoleMm",       &PrintConstraints::minHoleMm,       printLength},
		{"maxTipLengthMm",  &PrintConstraints::maxTipLengthMm,  printLength},
		{"noiseDepthRatio", &PrintConstraints::noiseDepthRatio, ratio},
		{"minDetailMm",     &PrintConstraints::minDetailMm,     positive},
	};

	json constraintsObject (const PrintConstraints &c)
	{
		json j = json::object();
		if (c.mmPerUnit)
			j["mmPerUnit"] = *c.mmPerUnit;
		for (const Threshold &t : kThresholds)
			j[t.name] = c.*(t.field);
		if (c.envelopeMm)
			j["envelopeMm"] = json::array({(*c.envelopeMm)[0], (*c.envelopeMm)[1], (*c.envelopeMm)[2]});
		j["maxRecordsPerKind"] = c.maxRecordsPerKind;
		return j;
	}

	json countsObject (const std::array<std::size_t, kPrintDefectCount> &counts)
	{
		json j = json::object();
		for (std::size_t k = 0; k < kPrintDefectCount; ++k)
			j[PrintDefectName(static_cast<PrintDefect>(k))] = counts[k];
		return j;
	}

	json recordObject (const PrintDefectRecord &d)
	{
		return json{
			{"kind",         PrintDefectName(d.kind)},
			{"boxMm",        json::array({d.boxMm[0], d.boxMm[1], d.boxMm[2], d.boxMm[3]})},
			{"depthMm",      d.depthMm},
			{"areaMm2",      d.areaMm2},
			{"contacts",     d.contacts},
			{"widthBelowMm", d.widthBelowMm},
			{"slice",        d.slice},
			{"zLowMm",       d.zLowMm},
			{"zHighMm",      d.zHighMm}};
	}

	json sliceObject (const PrintSliceSummary &s)
	{
		return json{
			{"zLowMm",     s.zLowMm},
			{"zHighMm",    s.zHighMm},
			{"zCutMm",     s.zCutMm},
			{"areaMm2",    s.areaMm2},
			{"silhouette", s.silhouette},
			{"counts",     countsObject(s.counts)}};
	}
}

std::vector<PrintJsonError> PrintConstraintsFromJson (const std::string &jsonText, PrintConstraints &out)
{
	std::vector<PrintJsonError> errors;
	Reader reader(errors);
	// Cle deja vue dans l'objet courant : refusee (nlohmann garde la derniere).
	// Une entree par objet ou tableau ouvert ; chemin de l'objet, et derniere
	// cle lue, pour nommer l'erreur.
	struct Scope
	{
		bool                  object = true;
		std::string           path;
		std::string           lastKey;
		std::set<std::string> keys;
	};
	std::vector<Scope> scopes;
	const auto childPath = [&scopes] () {
		if (scopes.empty())
			return std::string();
		const Scope &top = scopes.back();
		const std::string here = top.object ? top.lastKey : "[]";
		return top.path.empty() ? here : top.path + (top.object ? "." : "") + here;
	};
	const json::parser_callback_t duplicates = [&] (int, json::parse_event_t event, json &parsed) {
		switch (event)
		{
		case json::parse_event_t::object_start:
		case json::parse_event_t::array_start:
		{
			Scope sc;
			sc.object = (event == json::parse_event_t::object_start);
			sc.path = childPath();
			scopes.push_back(std::move(sc));
			break;
		}
		case json::parse_event_t::object_end:
		case json::parse_event_t::array_end:
			if (!scopes.empty())
				scopes.pop_back();
			break;
		case json::parse_event_t::key:
			if (!scopes.empty() && parsed.is_string())
			{
				Scope &top = scopes.back();
				top.lastKey = parsed.get<std::string>();
				if (!top.keys.insert(top.lastKey).second)
					reader.Error(childPath(), "duplicate key");
			}
			break;
		case json::parse_event_t::value:
			break;
		}
		return true;
	};
	json doc;
	try
	{
		doc = json::parse(jsonText, duplicates);
	}
	catch (const json::exception &e)
	{
		reader.Error("", e.what());
		return errors;
	}
	if (!doc.is_object())
	{
		reader.Error("", std::string("expected an object, not ") + doc.type_name());
		return errors;
	}

	const auto schema = doc.find("schema");
	if (schema == doc.end())
		reader.Error("schema", "required field is missing");
	else if (!schema->is_string() || schema->get<std::string>() != kPrintConstraintsSchema)
		reader.Error("schema", "expected \"" + std::string(kPrintConstraintsSchema) + "\", not " + schema->dump());
	for (auto it = doc.begin(); it != doc.end(); ++it)
	{
		bool known = (it.key() == "schema");
		for (const char *f : kFields)
			known = known || (it.key() == f);
		if (!known)
			reader.Error(it.key(), "unknown field");
	}

	PrintConstraints read;
	if (const auto v = doc.find("mmPerUnit"); v != doc.end())
	{
		double s = 0.0;
		if (reader.Number(*v, "mmPerUnit", positive(), s))
			read.mmPerUnit = s;
	}
	for (const Threshold &t : kThresholds)
		if (const auto v = doc.find(t.name); v != doc.end())
			reader.Number(*v, t.name, t.bounds(), read.*(t.field));
	if (const auto v = doc.find("envelopeMm"); v != doc.end())
	{
		if (!v->is_array() || v->size() != 3)
			reader.Error("envelopeMm", std::string("expected an array of 3 numbers, not ")
			             + (v->is_array() ? "an array of " + std::to_string(v->size()) : std::string(v->type_name())));
		else
		{
			std::array<double, 3> e {};
			bool ok = true;
			for (std::size_t k = 0; k < 3; ++k)
				ok = reader.Number((*v)[k], "envelopeMm[" + std::to_string(k) + "]", positive(), e[k]) && ok;
			if (ok)
				read.envelopeMm = e;
		}
	}
	if (const auto v = doc.find("maxRecordsPerKind"); v != doc.end())
	{
		if (!v->is_number_unsigned())
			reader.Error("maxRecordsPerKind", "expected an integer >= 0, not " + v->dump());
		else
		{
			const auto n = v->get<std::uint64_t>();
			if (n > std::numeric_limits<std::size_t>::max())
				reader.Error("maxRecordsPerKind", v->dump() + " exceeds the record capacity");
			else
				read.maxRecordsPerKind = static_cast<std::size_t>(n);
		}
	}

	if (errors.empty())
		out = read;
	return errors;
}

std::string PrintConstraintsToJson (const PrintConstraints &constraints)
{
	json j = constraintsObject(constraints);
	j["schema"] = kPrintConstraintsSchema;
	return j.dump(2);
}

std::string PrintReportToJson (const PrintReportEnvelope &envelope, const PrintabilityReport &print)
{
	json j;
	j["schema"] = kPrintReportSchema;
	j["scope"] = envelope.scope;
	j["reason"] = envelope.reason;
	j["printable"] = envelope.printable;
	j["exportAllowed"] = envelope.exportAllowed;
	j["geometry"] = json{
		{"allowed", envelope.geometry.allowed},
		{"strict",  envelope.geometry.strict},
		{"message", envelope.geometry.message}};
	j["evaluated"] = print.evaluated;
	j["mmPerUnit"] = print.mmPerUnit > 0.0 ? json(print.mmPerUnit) : json(nullptr);
	j["printedSizeMm"] = envelope.printedSizeMm
		? json::array({(*envelope.printedSizeMm)[0], (*envelope.printedSizeMm)[1], (*envelope.printedSizeMm)[2]})
		: json(nullptr);
	j["counts"] = countsObject(print.counts);
	j["displayCounts"] = countsObject(PrintDisplayCounts(print));
	json records = json::array();
	for (const PrintDefectRecord &d : print.records)
		records.push_back(recordObject(d));
	j["records"] = std::move(records);
	json slices = json::array();
	for (const PrintSliceSummary &s : print.slices)
		slices.push_back(sliceObject(s));
	j["slices"] = std::move(slices);
	j["error"] = envelope.error.empty() ? json(nullptr) : json(envelope.error);
	j["constraints"] = envelope.constraints ? constraintsObject(*envelope.constraints) : json(nullptr);
	// Les messages d'exception ne sont pas garantis UTF-8 : octets invalides remplaces.
	return j.dump(-1, ' ', false, json::error_handler_t::replace);
}
