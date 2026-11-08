// Lecteur et ecrivain de la specification v1 de la microarchitecture gothique.
// Schema : src/cgarchi/schemas/gothic-microarchitecture-v1.schema. Les bornes
// appliquees ici sont celles du schema (verifie par tu_cgarchi_gothic_micro_json).
#include <cgarchi/gothic_microarchitecture_json.h>
#include <cgarchi/gothic_microarchitecture_detail.h>
#include <cgarchi/json_reader.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <climits>
#include <cmath>
#include <exception>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace cgarchi::microarch
{

namespace
{
	using nlohmann::json;

	// Valeur illisible, deja signalee : la validation structurelle la saute.
	const double kUnread = std::numeric_limits<double>::quiet_NaN();

	using json_reader::Bounds;
	using json_reader::Indexed;
	using json_reader::Interval;
	using json_reader::JsonFault;
	using json_reader::Join;
	using json_reader::Member;

	// Chiffres significatifs des nombres cites dans les messages : un double
	// s'y relit a l'identique.
	constexpr int kNumberPrecision = 17;

	static_assert(kMicroarchReportMaxEntries <= json_reader::kDuplicateKeyPathLimit,
	              "chaque entree du rapport porte le chemin de son doublon");

	MicroarchError toError (JsonFault kind)
	{
		switch (kind)
		{
		case JsonFault::Type:    return MicroarchError::JsonType;
		case JsonFault::Value:   return MicroarchError::JsonValue;
		case JsonFault::Missing: return MicroarchError::JsonMissing;
		case JsonFault::Schema:  break;
		}
		return MicroarchError::JsonSchema;
	}

	// Lecteur strict commun (json_reader.h), refus portes au MicroarchReport ;
	// s'y ajoutent les tableaux bornes, les identifiants et les points du plan.
	class Reader final : public json_reader::StrictReader
	{
	public:
		explicit Reader (MicroarchReport &report) : StrictReader(kNumberPrecision), m_report(report) {}

		void Error (MicroarchError kind, const std::string &path, const std::string &message)
		{
			m_report.Record(kind, path, message);
		}
		void Warning (MicroarchWarning kind, const std::string &path, const std::string &message)
		{
			m_report.Record(kind, path, message);
		}

		// Tableau d'au plus `maxItems` elements ; `nonEmpty` : un tableau vide est
		// une erreur json_value. Un tableau refuse n'est pas lu.
		const json *Array (const json &obj, const std::string &parent, const char *key, bool nonEmpty,
		                   std::size_t maxItems = std::numeric_limits<std::size_t>::max())
		{
			const json *v = Member(obj, key);
			if (!v)
				return nullptr;
			const std::string path = Join(parent, key);
			if (!v->is_array())
			{
				TypeError(path, "an array", *v);
				return nullptr;
			}
			if (nonEmpty && v->empty())
			{
				Error(MicroarchError::JsonValue, path, "the array must not be empty");
				return nullptr;
			}
			if (v->size() > maxItems)
			{
				Error(MicroarchError::JsonValue, path,
				      std::to_string(v->size()) + " items ; at most " + std::to_string(maxItems));
				return nullptr;
			}
			return v;
		}

		// Chaine non vide.
		bool Id (const json &obj, const std::string &parent, const char *key, std::string &out)
		{
			std::string s;
			if (!String(obj, parent, key, s))
				return false;
			if (s.empty())
			{
				Error(MicroarchError::JsonValue, Join(parent, key), "empty string");
				return false;
			}
			out = std::move(s);
			return true;
		}

		// Tableau de deux nombres finis.
		bool Point (const json &obj, const std::string &parent, const char *key, Vector2d &out)
		{
			const json *v = Member(obj, key);
			if (!v)
				return false;
			const std::string path = Join(parent, key);
			if (!v->is_array() || v->size() != 2 || !(*v)[0].is_number() || !(*v)[1].is_number())
			{
				Error(MicroarchError::JsonType, path, "expected an array of 2 numbers");
				return false;
			}
			const double x = (*v)[0].get<double>();
			const double y = (*v)[1].get<double>();
			if (!std::isfinite(x) || !std::isfinite(y))
			{
				Error(MicroarchError::JsonValue, path, "non-finite coordinates");
				return false;
			}
			out = Vector2d(x, y);
			return true;
		}

	private:
		void OnFault (JsonFault kind, const std::string &path, const std::string &message) override
		{
			Error(toError(kind), path, message);
		}

		MicroarchReport &m_report;
	};

	// Bornes du schema v1.
	const Bounds kAxisDeg       = Interval(0.0, false, 180.0, true);
	const Bounds kPrcU          = Interval(0.0, true, 1.0, true);
	constexpr int kMaxLevelId   = INT_MAX;

	// =======================================================================
	//  Lecture, par sujet
	// =======================================================================

	void readSymmetry (const json &s, Reader &r, SymmetrySpec &out)
	{
		const std::string path = "symmetry";
		r.Known(s, path, {"center", "rotation_order", "reflection_axis_deg"});
		r.Require(s, path, {"center", "rotation_order"});
		if (s.contains("center") && !r.Point(s, path, "center", out.center))
			out.center = Vector2d(kUnread, kUnread);
		r.Integer(s, path, "rotation_order", 1, SpecLimits::max_rotation_order, out.rotation_order);
		double deg = 0.0;
		if (r.Number(s, path, "reflection_axis_deg", kAxisDeg, deg))
			out.reflection_axis_deg = deg;
	}

	// Un niveau par element du tableau, meme mal forme : le nombre de niveaux
	// declares reste connu (unsupported_multi_level). Un id illisible vaut -1, une
	// base illisible NaN (voir detail::ValidateStructure).
	void readLevels (const json &doc, Reader &r, std::vector<LevelSpec> &out)
	{
		const json *levels = r.Array(doc, "", "levels", true);
		if (!levels)
			return;
		std::vector<LevelSpec> read;
		for (std::size_t i = 0; i < levels->size(); ++i)
		{
			const json &l = (*levels)[i];
			const std::string path = Indexed("levels", i);
			LevelSpec level;
			level.id = -1;
			if (!l.is_object())
			{
				r.TypeError(path, "an object", l);
				read.push_back(level);
				continue;
			}
			r.Known(l, path, {"id", "base"});
			r.Require(l, path, {"id"});
			if (!r.Integer(l, path, "id", 0, kMaxLevelId, level.id))
				level.id = -1;
			if (l.contains("base") && !r.Number(l, path, "base", Bounds(), level.base))
				level.base = kUnread;
			read.push_back(level);
		}
		out = std::move(read);
	}

	void readPoint (const json &p, const std::string &path, Reader &r, PointSpec &point)
	{
		r.Known(p, path, {"id", "xy", "ground", "height"});
		r.Require(p, path, {"id", "xy"});
		r.Id(p, path, "id", point.id);
		r.Point(p, path, "xy", point.xy);
		bool groundRead = true;
		if (p.contains("ground"))
			groundRead = r.Boolean(p, path, "ground", point.ground);
		double h = 0.0;
		if (p.contains("height"))
			point.height = r.Number(p, path, "height", Bounds(), h) ? h : kUnread;
		else if (!groundRead)
			point.height = kUnread;   // ground illisible : deja signale, pas de json_missing en cascade
	}

	void readPoints (const json &points, Reader &r, std::vector<PointSpec> &out)
	{
		for (std::size_t i = 0; i < points.size(); ++i)
		{
			const json &p = points[i];
			const std::string path = Indexed("points", i);
			PointSpec point;
			point.xy = Vector2d(kUnread, kUnread);
			if (!p.is_object())
			{
				r.TypeError(path, "an object", p);
				point.height = kUnread;   // ni ground ni height : deja signale
			}
			else
				readPoint(p, path, r, point);
			out.push_back(point);
		}
	}

	enum class CurveType { Unknown, Segment, Arc };

	CurveType readCurveType (const json &c, const std::string &path, Reader &r)
	{
		int index = 0;
		if (!r.Enum(c, path, "type", {"segment", "arc"}, index))
			return CurveType::Unknown;
		return index == 0 ? CurveType::Segment : CurveType::Arc;
	}

	void readPrc (const json &c, const std::string &path, Reader &r, PrcSelection &out)
	{
		const json *prc = r.Object(c, path, "prc");
		if (!prc)
			return;
		const std::string prcPath = Join(path, "prc");
		r.Known(*prc, prcPath, {"u"});
		r.Require(*prc, prcPath, {"u"});
		double u = 0.0;
		if (r.Number(*prc, prcPath, "u", kPrcU, u))
			out.u = u;
	}

	// References des courbes, resolues par id. Les id cites sont notes, resolus ou non.
	struct PointResolver
	{
		std::map<std::string, int> byId;          // premier gagnant ; doublons : duplicate_id
		bool                       usable = false;   // tableau `points` lu
		std::set<std::string>      referenced;

		int Resolve (const json &c, const std::string &path, const char *field, Reader &r)
		{
			std::string id;
			if (!r.Id(c, path, field, id))
				return -1;
			referenced.insert(id);
			const auto it = byId.find(id);
			if (it != byId.end())
				return it->second;
			if (usable)
				r.Error(MicroarchError::UnknownReference, Join(path, field), "unknown point \"" + id + "\"");
			return -1;
		}
	};

	// Arc : centre et sens ; segment : ni l'un ni l'autre. Rend false si une
	// donnee d'arc manque ou est illisible.
	bool readArcData (const json &c, const std::string &path, CurveType type, Reader &r, Vector2d &center, bool &ccw)
	{
		if (type == CurveType::Arc)
		{
			r.Require(c, path, {"center", "ccw"});
			const bool centerRead = r.Point(c, path, "center", center);
			return r.Boolean(c, path, "ccw", ccw) && centerRead;
		}
		if (type == CurveType::Segment)
			for (const char *f : {"center", "ccw"})
				if (c.contains(f))
					r.Error(MicroarchError::JsonSchema, Join(path, f), "field only allowed for an arc");
		return true;
	}

	// Une courbe qui ne peut pas etre construite garde start_point = end_point = -1.
	BasicCurveSpec readCurve (const json &c, const std::string &path, Reader &r,
	                          const std::vector<PointSpec> &points, PointResolver &resolver)
	{
		BasicCurveSpec curve;
		r.Known(c, path, {"id", "type", "start", "end", "level", "prc", "center", "ccw"});
		r.Require(c, path, {"id", "type", "start", "end"});
		r.Id(c, path, "id", curve.id);
		if (c.contains("level") && !r.Integer(c, path, "level", 0, kMaxLevelId, curve.level))
			curve.level = -1;   // illisible : deja signale, pas de reference controlee
		readPrc(c, path, r, curve.prc);
		const CurveType type = readCurveType(c, path, r);
		const int start = resolver.Resolve(c, path, "start", r);
		const int end   = resolver.Resolve(c, path, "end", r);
		Vector2d center;
		bool ccw = false;
		const bool arcData = readArcData(c, path, type, r, center, ccw);

		if (type == CurveType::Unknown || start < 0 || end < 0 || !arcData)
			return curve;
		const Vector2d &a = points[static_cast<std::size_t>(start)].xy;
		const Vector2d &b = points[static_cast<std::size_t>(end)].xy;
		if (!std::isfinite(a.x) || !std::isfinite(b.x))
			return curve;
		curve.start_point = start;
		curve.end_point   = end;
		if (type == CurveType::Segment)
			curve.plan = PlanSegment{a, b};
		else
			curve.plan = MakeArc(a, b, center, ccw);
		return curve;
	}

	void readCurves (const json &curves, Reader &r, MicroarchSpec &spec, PointResolver &resolver)
	{
		for (std::size_t i = 0; i < spec.points.size(); ++i)
			if (!spec.points[i].id.empty())
				resolver.byId.emplace(spec.points[i].id, static_cast<int>(i));
		for (std::size_t i = 0; i < curves.size(); ++i)
		{
			const json &c = curves[i];
			const std::string path = Indexed("curves", i);
			if (!c.is_object())
			{
				r.TypeError(path, "an object", c);
				spec.curves.emplace_back();
				continue;
			}
			spec.curves.push_back(readCurve(c, path, r, spec.points, resolver));
		}
	}

	void readTolerances (const json &t, Reader &r, Tolerances &out)
	{
		const std::string path = "tolerances";
		r.Known(t, path, {"point", "parameter", "height", "prc_snap"});
		r.Number(t, path, "point",     Interval(0.0, true, ToleranceBounds::point, false),     out.point);
		r.Number(t, path, "parameter", Interval(0.0, true, ToleranceBounds::parameter, false), out.parameter);
		r.Number(t, path, "height",    Interval(0.0, true, ToleranceBounds::height, false),    out.height);
		r.Number(t, path, "prc_snap",  Interval(0.0, true, ToleranceBounds::prc_snap, false),  out.prc_snap);
	}

	// Rend false si le document n'a pas pu etre lu du tout (controles structurels inutiles).
	// Un schema absent ou qui n'est pas une chaine est deja signale : la lecture continue.
	bool readSchema (const json &doc, Reader &r)
	{
		return json_reader::ReadSchema(r, doc, kMicroarchSchemaV1).status != json_reader::SchemaStatus::Unsupported;
	}

	bool readSpec (const json &doc, Reader &r, MicroarchSpec &spec)
	{
		r.Known(doc, "", {"schema", "meta", "symmetry", "levels", "points", "curves", "tolerances"});
		r.Require(doc, "", {"schema", "symmetry", "points", "curves"});
		if (!readSchema(doc, r))
			return false;

		r.Object(doc, "", "meta");   // contenu libre
		if (const json *s = r.Object(doc, "", "symmetry"))
			readSymmetry(*s, r, spec.symmetry);
		readLevels(doc, r, spec.levels);

		const json *points = r.Array(doc, "", "points", true, SpecLimits::max_points);
		if (points)
			readPoints(*points, r, spec.points);

		PointResolver resolver;
		resolver.usable = points != nullptr;
		const json *curves = r.Array(doc, "", "curves", true, SpecLimits::max_curves);
		if (curves)
			readCurves(*curves, r, spec, resolver);

		if (const json *t = r.Object(doc, "", "tolerances"))
			readTolerances(*t, r, spec.tolerances);

		// Un point que ne cite aucune courbe n'a pas d'effet.
		if (curves)
			for (std::size_t i = 0; i < spec.points.size(); ++i)
				if (!spec.points[i].id.empty() && resolver.referenced.count(spec.points[i].id) == 0)
					r.Warning(MicroarchWarning::FieldIgnored, Indexed("points", i),
					          "point \"" + spec.points[i].id + "\" is not referenced by any curve");
		return true;
	}

	// =======================================================================
	//  Ecriture, par sujet
	// =======================================================================

	json writeSymmetry (const SymmetrySpec &sym)
	{
		json j;
		j["center"] = json::array({sym.center.x, sym.center.y});
		j["rotation_order"] = sym.rotation_order;
		if (sym.reflection_axis_deg)
			j["reflection_axis_deg"] = *sym.reflection_axis_deg;
		return j;
	}

	json writeLevels (const std::vector<LevelSpec> &levels)
	{
		json j = json::array();
		for (const LevelSpec &l : levels)
			j.push_back(json{{"id", l.id}, {"base", l.base}});
		return j;
	}

	json writePoints (const std::vector<PointSpec> &points)
	{
		json j = json::array();
		for (const PointSpec &p : points)
		{
			json o;
			o["id"] = p.id;
			o["xy"] = json::array({p.xy.x, p.xy.y});
			if (p.ground)
				o["ground"] = true;
			if (p.height)
				o["height"] = *p.height;
			j.push_back(std::move(o));
		}
		return j;
	}

	json writeCurves (const MicroarchSpec &spec)
	{
		auto pointId = [&](int index) -> std::string {
			return (index >= 0 && static_cast<std::size_t>(index) < spec.points.size())
			       ? spec.points[static_cast<std::size_t>(index)].id : std::string();
		};
		json j = json::array();
		for (const BasicCurveSpec &c : spec.curves)
		{
			json o;
			o["id"] = c.id;
			o["start"] = pointId(c.start_point);
			o["end"] = pointId(c.end_point);
			o["level"] = c.level;
			if (const auto *arc = std::get_if<PlanArc>(&c.plan))
			{
				o["type"] = "arc";
				o["center"] = json::array({arc->center.x, arc->center.y});
				o["ccw"] = arc->sweep > 0.0;
			}
			else
				o["type"] = "segment";
			if (c.prc.u)
				o["prc"] = json{{"u", *c.prc.u}};
			j.push_back(std::move(o));
		}
		return j;
	}

	json writeTolerances (const Tolerances &t)
	{
		return json{
			{"point",     t.point},
			{"parameter", t.parameter},
			{"height",    t.height},
			{"prc_snap",  t.prc_snap}};
	}
}

bool LoadSpecFromJson (const std::string &text, MicroarchSpec &out, MicroarchReport &report)
{
	// Avant le parsing : le DOM nlohmann serait construit avant que les bornes des
	// tableaux ne refusent le document.
	if (text.size() > SpecLimits::max_text_bytes)
	{
		report.Record(MicroarchError::JsonTooLarge, std::string(),
		              "the specification text has " + std::to_string(text.size()) + " bytes ; at most "
		              + std::to_string(SpecLimits::max_text_bytes));
		return false;
	}
	MicroarchSpec spec;
	MicroarchReport load;
	try
	{
		Reader r(load);
		bool readable = false;
		json doc;
		bool parsed = false;
		try
		{
			// Toute exception du parseur est une erreur de syntaxe, y compris un
			// nombre hors du domaine des doubles (out_of_range.406).
			// Une cle en double est refusee (json_schema, chemin du doublon) sans
			// arreter la lecture : comme les autres refus d'un document bien forme,
			// elle n'empeche pas de rapporter les erreurs independantes, lues sur la
			// derniere valeur. Un texte illisible apres un doublon donne les deux
			// erreurs, json_schema puis json_parse.
			doc = r.ParseRejectingDuplicateKeys(text);
			parsed = true;
		}
		catch (const json::exception &e)
		{
			r.Error(MicroarchError::JsonParse, std::string(), e.what());
		}
		if (parsed)
		{
			try
			{
				if (!doc.is_object())
					r.Error(MicroarchError::JsonType, std::string(),
					        std::string("the document root must be an object, not ") + doc.type_name());
				else
					readable = readSpec(doc, r, spec);
			}
			catch (const json::exception &e)
			{
				r.Error(MicroarchError::JsonValue, std::string(), e.what());
				readable = false;
			}
		}
		if (readable)
			detail::ValidateStructure(spec, load, true);
	}
	catch (const std::exception &)
	{
		// Epuisement memoire ou autre echec de la bibliotheque : le document est
		// refuse. Compte sans entree, pour ne rien allouer ici.
		++load.errorCounts[static_cast<std::size_t>(MicroarchError::JsonParse)];
	}

	const bool accepted = !load.HasErrors();
	report.Merge(load);
	if (accepted)
		out = std::move(spec);
	return accepted;
}

bool LoadSpecFromFile (const std::string &path, MicroarchSpec &out, MicroarchReport &report)
{
	std::string text;
	try
	{
		std::ifstream in(path, std::ios::binary);
		if (!in)
		{
			report.Record(MicroarchError::JsonParse, std::string(), "cannot open \"" + path + "\"");
			return false;
		}
		std::ostringstream os;
		os << in.rdbuf();
		text = os.str();
	}
	catch (const std::exception &)
	{
		// Lecture interrompue (memoire) : comptee sans entree, pour ne rien allouer ici.
		++report.errorCounts[static_cast<std::size_t>(MicroarchError::JsonParse)];
		return false;
	}
	return LoadSpecFromJson(text, out, report);
}

std::string SpecToJson (const MicroarchSpec &spec)
{
	json j;
	j["schema"]     = kMicroarchSchemaV1;
	j["symmetry"]   = writeSymmetry(spec.symmetry);
	j["levels"]     = writeLevels(spec.levels);
	j["points"]     = writePoints(spec.points);
	j["curves"]     = writeCurves(spec);
	j["tolerances"] = writeTolerances(spec.tolerances);
	// Un id en UTF-8 invalide (refuse par ValidateStructure) est ecrit avec U+FFFD
	// au lieu de lever.
	return j.dump(2, ' ', false, json::error_handler_t::replace);
}

namespace
{
	json reportJson (const MicroarchReport &report)
	{
		json j;
		json &errors = j["errors"];
		for (std::size_t k = 0; k < kMicroarchErrorCount; ++k)
			errors[MicroarchErrorName(static_cast<MicroarchError>(k))] = report.errorCounts[k];
		json &warnings = j["warnings"];
		for (std::size_t k = 0; k < kMicroarchWarningCount; ++k)
			warnings[MicroarchWarningName(static_cast<MicroarchWarning>(k))] = report.warningCounts[k];
		json entries = json::array();
		for (const MicroarchDiagnostic &d : report.entries)
			entries.push_back(json{
				{"severity",  d.error ? "error" : "warning"},
				{"code",      d.error ? MicroarchErrorName(static_cast<MicroarchError>(d.code))
				                      : MicroarchWarningName(static_cast<MicroarchWarning>(d.code))},
				{"parameter", d.path},
				{"message",   d.message}});
		j["entries"] = std::move(entries);
		return j;
	}

	json xyJson (const Vector2d &p) { return json::array({p.x, p.y}); }
	json xyzJson (const Vector3d &p) { return json::array({p.x, p.y, p.z}); }

	const char *ruleName (PrcRule rule)
	{
		switch (rule)
		{
		case PrcRule::ReplicaFixedPoint: return "replica_fixed_point";
		case PrcRule::OtherBasicCurve:   return "other_basic_curve";
		case PrcRule::Midpoint:          return "midpoint";
		case PrcRule::Explicit:          return "explicit";
		case PrcRule::ReplicaCrossing:   return "replica_crossing";
		}
		return "unknown";
	}

	json incidencesJson (const std::vector<Incidence> &incidences)
	{
		json j = json::array();
		for (const Incidence &i : incidences)
			j.push_back(json{{"curve", i.curve}, {"s", i.s}});
		return j;
	}

	json planJson (const PlanCurve &curve)
	{
		if (const auto *seg = std::get_if<PlanSegment>(&curve))
			return json{{"type", "segment"}, {"start", xyJson(seg->start)}, {"end", xyJson(seg->end)}};
		const PlanArc &arc = std::get<PlanArc>(curve);
		return json{{"type", "arc"}, {"center", xyJson(arc.center)}, {"radius", arc.radius},
		            {"start_angle_rad", arc.start_angle}, {"sweep_rad", arc.sweep}};
	}

	json profileJson (const OgeeProfile &p)
	{
		return json{
			{"length",       p.length},
			{"z_start",      p.z_start},
			{"z_end",        p.z_end},
			{"s_prc",        p.s_prc},
			{"z_prc",        p.z_prc},
			{"r_start",      p.r_start},
			{"r_end",        p.r_end},
			{"center_start", xyJson(StartCenter(p))},
			{"center_end",   xyJson(EndCenter(p))},
			{"monotonic",    p.monotonic}};
	}

	json basicCurveJson (const BasicCurveResult &b, const BasicCurveSpec &spec)
	{
		const double length = Length(spec.plan);
		json candidates = json::array();
		for (const PrcCandidate &c : b.prc.candidates)
			candidates.push_back(json{{"s", c.s}, {"u", c.s / length}, {"tier", ruleName(c.tier)},
			                          {"xy", xyJson(c.xy)}, {"tangential", c.tangential}});
		json prc{{"s", b.prc.s}, {"u", b.prc.s / length}, {"rule", ruleName(b.prc.rule)},
		         {"xy", xyJson(PointAt(spec.plan, b.prc.s))}, {"candidates", std::move(candidates)}};
		return json{{"id", spec.id}, {"level", spec.level}, {"length", length},
		            {"prc", std::move(prc)}, {"profile", profileJson(b.profile)}};
	}
}

std::string ReportToJson (const MicroarchReport &report)
{
	// Les textes d'exception ne sont pas garantis UTF-8 : les octets invalides sont remplaces.
	return reportJson(report).dump(-1, ' ', false, json::error_handler_t::replace);
}

std::string NetworkToJson (const Network &network, const MicroarchSpec &spec, const MicroarchReport &report)
{
	json j;
	j["schema"] = kMicroarchNetworkSchemaV1;
	j["scale"]  = network.tol.scale;
	j["tolerances"] = json{{"point", network.tol.point}, {"parameter", network.tol.parameter},
	                       {"height", network.tol.height}, {"prc_snap", network.tol.prc_snap}};
	j["levels"] = writeLevels(network.levels);

	json basics = json::array();
	for (std::size_t i = 0; i < network.basics.size() && i < spec.curves.size(); ++i)
		basics.push_back(basicCurveJson(network.basics[i], spec.curves[i]));
	j["basic_curves"] = std::move(basics);

	json curves = json::array();
	for (const SpaceCurve &c : network.curves)
	{
		const bool known = c.basic >= 0 && static_cast<std::size_t>(c.basic) < spec.curves.size();
		curves.push_back(json{
			{"basic",    c.basic},
			{"basic_id", known ? spec.curves[static_cast<std::size_t>(c.basic)].id : std::string()},
			{"element",  json{{"k", c.element.k}, {"reflected", c.element.reflected}}},
			{"level",    c.level},
			{"length",   c.profile.length},
			{"plan",     planJson(c.plan)}});
	}
	j["curves"] = std::move(curves);

	json junctions = json::array();
	for (const Junction &jn : network.junctions)
		junctions.push_back(json{{"position", xyzJson(jn.position)}, {"incidences", incidencesJson(jn.incidences)},
		                         {"max_gap", jn.max_gap}, {"prc", jn.prc}, {"ground", jn.ground}});
	j["junctions"] = std::move(junctions);

	json crossings = json::array();
	for (const VisualCrossing &x : network.crossings)
	{
		json groups = json::array();
		for (const HeightGroup &g : x.groups)
			groups.push_back(json{{"z", g.z}, {"incidences", incidencesJson(g.incidences)}});
		crossings.push_back(json{{"xy", xyJson(x.xy)}, {"groups", std::move(groups)}, {"min_dz", x.min_dz}});
	}
	j["crossings"] = std::move(crossings);
	j["free_ends"] = incidencesJson(network.free_ends);
	j["report"]    = reportJson(report);
	return j.dump(2, ' ', false, json::error_handler_t::replace);
}

} // namespace cgarchi::microarch
