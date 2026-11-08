// Description JSON de ClassicalColumnSettings : lecteur strict et export du
// schema "classical-column/v1" (src/cgarchi/schemas/classical-column-v1.schema).
#include <cgarchi/parameterized_classical_json.h>

#include <cgarchi/architecture_gothic.h>   // GothicBuildReport
#include <cgarchi/classical_orders.h>      // OrderKey, kOrderCount
#include <cgarchi/json_reader.h>

#include <nlohmann/json.hpp>

#include <charconv>
#include <iterator>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace cgarchi
{

namespace
{
	using nlohmann::json;
	using json_reader::Interval;
	using json_reader::JsonFault;

	// Chiffres significatifs des nombres cites dans les messages : ceux de la
	// fenetre gothique.
	constexpr int kNumberPrecision = 6;

	static_assert(kGothicBuildReportMaxEntries <= json_reader::kDuplicateKeyPathLimit,
	              "chaque entree du rapport porte le chemin de son doublon");

	// Valeurs de doricCornice et de doricFlutes, dans l'ordre de leurs indices.
	constexpr const char *kDoricCorniceNames[] = {"dentils", "mutules"};
	constexpr const char *kDoricFluteNames[]   = {"segment60", "semicircle"};

	static_assert(classical::kOrderCount == 5, "LoadClassicalColumnFromJson enumere une cle par ordre");

	const char *OrderKeyAt (int order)
	{
		return classical::OrderKey(static_cast<classical::Order>(order));
	}

	GothicBuildError toError (JsonFault kind)
	{
		switch (kind)
		{
		case JsonFault::Type:    return GothicBuildError::JsonType;
		case JsonFault::Value:   return GothicBuildError::JsonValue;
		case JsonFault::Missing: return GothicBuildError::JsonMissing;
		case JsonFault::Schema:  break;
		}
		return GothicBuildError::JsonSchema;
	}

	// Lecteur strict commun (json_reader.h), refus portes au GothicBuildReport.
	class Reader final : public json_reader::StrictReader
	{
	public:
		explicit Reader (GothicBuildReport &report) : StrictReader(kNumberPrecision), m_report(report) {}

		void Error (GothicBuildError kind, const std::string &path, const std::string &message)
		{
			m_report.Record(kind, path, message);
		}

	private:
		void OnFault (JsonFault kind, const std::string &path, const std::string &message) override
		{
			Error(toError(kind), path, message);
		}

		GothicBuildReport &m_report;
	};

	void readDocument (const json &doc, Reader &r, ClassicalColumnSettings &s)
	{
		const json_reader::SchemaRead schema = json_reader::ReadSchema(r, doc, kClassicalColumnSchema);
		if (schema.status == json_reader::SchemaStatus::Absent)
			r.Require(doc, std::string(), {"schema"});
		if (schema.status != json_reader::SchemaStatus::Expected)
			return;

		const std::string root;
		r.Known(doc, root, {"schema", "order", "module", "pedestal", "entablature", "doricCornice", "envelope",
		                    "volutes", "flutes", "doricFlutes", "radialSegments", "curveSegments"});
		r.Require(doc, root, {"order", "module", "pedestal", "entablature", "doricCornice", "envelope", "volutes",
		                      "flutes", "doricFlutes", "radialSegments", "curveSegments"});

		r.Enum(doc, root, "order", {OrderKeyAt(0), OrderKeyAt(1), OrderKeyAt(2), OrderKeyAt(3), OrderKeyAt(4)}, s.order);
		r.Float(doc, root, "module", Interval(kClassicalModuleMin, false, kClassicalModuleMax, false), s.module);
		r.Boolean(doc, root, "pedestal", s.pedestal);
		r.Boolean(doc, root, "entablature", s.entablature);
		r.Enum(doc, root, "doricCornice", {kDoricCorniceNames[0], kDoricCorniceNames[1]}, s.doricCornice);
		r.Boolean(doc, root, "envelope", s.envelope);
		r.Boolean(doc, root, "volutes", s.volutes);
		r.Boolean(doc, root, "flutes", s.flutes);
		r.Enum(doc, root, "doricFlutes", {kDoricFluteNames[0], kDoricFluteNames[1]}, s.doricFlutes);
		r.Integer(doc, root, "radialSegments", kClassicalRadialSegmentsMin, kClassicalRadialSegmentsMax, s.radialSegments);
		r.Integer(doc, root, "curveSegments", kClassicalCurveSegmentsMin, kClassicalCurveSegmentsMax, s.curveSegments);
	}

	// Double le plus proche de la plus courte ecriture decimale de `f`, celle de
	// std::to_chars (la plus courte qui redonne `f`, la plus proche de `f` a
	// longueur egale), relue en locale classique. Leve std::logic_error si
	// l'ecriture echoue ou si sa relecture, arrondie en float, n'est pas `f`.
	double shortestDecimal (float f)
	{
		char text[64];
		const std::to_chars_result written = std::to_chars(text, text + sizeof text, f);
		if (written.ec != std::errc())
			throw std::logic_error("ExportClassicalColumnJson : module non ecrit par std::to_chars");
		std::istringstream is(std::string(text, written.ptr));
		is.imbue(std::locale::classic());
		double d = 0.0;
		if (!(is >> d) || static_cast<float>(d) != f)
			throw std::logic_error("ExportClassicalColumnJson : module " + std::string(text, written.ptr) +
			                       " ne se relit pas a l'identique");
		return d;
	}

	void checkIndex (const char *field, int value, int count)
	{
		if (value < 0 || value >= count)
			throw std::invalid_argument(std::string("ExportClassicalColumnJson : ") + field + " " + std::to_string(value) +
			                            " hors de [0, " + std::to_string(count) + ")");
	}

	void checkRange (const char *field, int value, int lo, int hi)
	{
		if (value < lo || value > hi)
			throw std::invalid_argument(std::string("ExportClassicalColumnJson : ") + field + " " + std::to_string(value) +
			                            " hors de [" + std::to_string(lo) + ", " + std::to_string(hi) + "]");
	}
}

std::optional<ClassicalColumnSettings> LoadClassicalColumnFromJson (const std::string &jsonText, GothicBuildReport *report)
{
	ClassicalColumnSettings settings;
	GothicBuildReport load;
	{
		Reader r(load);
		try
		{
			const json doc = r.ParseRejectingDuplicateKeys(jsonText);
			if (!doc.is_object())
				r.Error(GothicBuildError::JsonType, std::string(),
				        std::string("the document root must be an object, not ") + doc.type_name());
			else
				readDocument(doc, r, settings);
		}
		catch (const json::parse_error &e)
		{
			r.Error(GothicBuildError::JsonParse, std::string(), e.what());
		}
		catch (const json::exception &e)
		{
			r.Error(GothicBuildError::JsonValue, std::string(), e.what());
		}
	}

	const bool accepted = !load.HasErrors();
	if (report)
		report->Merge(load);
	if (!accepted)
		return std::nullopt;
	return settings;
}

std::string ExportClassicalColumnJson (const ClassicalColumnSettings &s)
{
	checkIndex("order", s.order, classical::kOrderCount);
	checkIndex("doricCornice", s.doricCornice, static_cast<int>(std::size(kDoricCorniceNames)));
	checkIndex("doricFlutes", s.doricFlutes, static_cast<int>(std::size(kDoricFluteNames)));
	checkRange("radialSegments", s.radialSegments, kClassicalRadialSegmentsMin, kClassicalRadialSegmentsMax);
	checkRange("curveSegments", s.curveSegments, kClassicalCurveSegmentsMin, kClassicalCurveSegmentsMax);
	if (!(s.module >= kClassicalModuleMin && s.module <= kClassicalModuleMax))
		throw std::invalid_argument("ExportClassicalColumnJson : module " + json_reader::NumberText(s.module, kNumberPrecision) +
		                            " hors de [" + json_reader::NumberText(kClassicalModuleMin, kNumberPrecision) + ", " +
		                            json_reader::NumberText(kClassicalModuleMax, kNumberPrecision) + "]");

	json j;
	j["schema"]         = kClassicalColumnSchema;
	j["order"]          = OrderKeyAt(s.order);
	j["module"]         = shortestDecimal(s.module);
	j["pedestal"]       = s.pedestal;
	j["entablature"]    = s.entablature;
	j["doricCornice"]   = kDoricCorniceNames[s.doricCornice];
	j["envelope"]       = s.envelope;
	j["volutes"]        = s.volutes;
	j["flutes"]         = s.flutes;
	j["doricFlutes"]    = kDoricFluteNames[s.doricFlutes];
	j["radialSegments"] = s.radialSegments;
	j["curveSegments"]  = s.curveSegments;
	return j.dump(2);
}

} // namespace cgarchi
