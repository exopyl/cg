// Description JSON de ParameterizedGothicWindow : lecteur v2 strict, conversion
// v1 -> v2, export v2. Schemas : src/cgmath/gothic-window-v2.schema (v2) et
// src/cgmath/gothic-tracery.schema (v1).
#include "parameterized_gothic.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
	using nlohmann::json;

	constexpr double kPi = 3.14159265358979323846;
	const char *const kV2Schema = "gothic-window/v2";

	// Objet `thresholds` : un champ par seuil d'element, en unites monde.
	constexpr std::pair<const char *, double GothicFeatureThresholds::*> kThresholdFields[] = {
		{"minRosetteRadius",     &GothicFeatureThresholds::minRosetteRadius},
		{"minRingWidth",         &GothicFeatureThresholds::minRingWidth},
		{"minEyeletInset",       &GothicFeatureThresholds::minEyeletInset},
		{"minSubRosetteRadius",  &GothicFeatureThresholds::minSubRosetteRadius},
		{"minMouchetteRadius",   &GothicFeatureThresholds::minMouchetteRadius},
		{"minFilletInnerOffset", &GothicFeatureThresholds::minFilletInnerOffset},
		{"maxMouchetteInset",    &GothicFeatureThresholds::maxMouchetteInset},
		{"chamferWidthAdd",      &GothicFeatureThresholds::chamferWidthAdd},
		{"minChamferWidth",      &GothicFeatureThresholds::minChamferWidth},
		{"barRadiusAdd",         &GothicFeatureThresholds::barRadiusAdd},
		{"minBarRadius",         &GothicFeatureThresholds::minBarRadius},
		{"minBand",              &GothicFeatureThresholds::minBand},
	};

	// Seuil du schema sans effet : lu et borne comme les autres, signale
	// field_ignored, jamais exporte.
	const char *const kIgnoredThreshold = "fallbackFilletInset";

	std::string join (const std::string &parent, const std::string &key)
	{
		return parent.empty() ? key : parent + "." + key;
	}

	std::string valueText (double v)
	{
		std::ostringstream os;
		os << v;
		return os.str();
	}

	// Domaine d'un nombre au sens de JSON Schema (minimum / exclusiveMinimum...).
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

		// Reglage float : la valeur arrondie est comparee aux bornes arrondies,
		// pour qu'une borne incluse non representable (0.3) reste acceptee.
		bool ContainsFloat (float v) const
		{
			const float flo = static_cast<float>(lo);
			const float fhi = static_cast<float>(hi);
			return (loOpen ? v > flo : v >= flo) && (hiOpen ? v < fhi : v <= fhi);
		}

		std::string Text () const
		{
			return (loOpen ? "(" : "[") + valueText(lo) + ", " + valueText(hi) + (hiOpen ? ")" : "]");
		}
	};

	Bounds anyNumber () { return Bounds(); }
	Bounds atLeast (double lo) { Bounds b; b.lo = lo; return b; }
	Bounds above (double lo) { Bounds b; b.lo = lo; b.loOpen = true; return b; }
	Bounds interval (double lo, bool loOpen, double hi, bool hiOpen)
	{
		Bounds b;
		b.lo = lo; b.loOpen = loOpen;
		b.hi = hi; b.hiOpen = hiOpen;
		return b;
	}

	const json *member (const json &obj, const char *key)
	{
		const auto it = obj.find(key);
		return it == obj.end() ? nullptr : &*it;
	}

	// Lecture typee d'un champ, avec diagnostic nomme par son chemin. Chaque
	// accesseur rend true et ecrit `out` seulement si le champ est present et
	// valide ; absent, il rend false sans rien enregistrer.
	class Reader
	{
	public:
		explicit Reader (GothicBuildReport &report) : m_report(report) {}

		void Error (GothicBuildError kind, const std::string &path, const std::string &message)
		{
			m_report.Record(kind, path, message);
		}
		void TypeError (const std::string &path, const char *expected, const json &value)
		{
			Error(GothicBuildError::JsonType, path, std::string("expected ") + expected + ", not " + value.type_name());
		}
		void Ignored (const std::string &path, const std::string &message)
		{
			m_report.Record(GothicBuildWarning::FieldIgnored, path, message);
		}
		void Converted (const std::string &path, const std::string &message)
		{
			m_report.Record(GothicBuildWarning::FieldConverted, path, message);
		}
		void Clamped (const std::string &path, const std::string &message)
		{
			m_report.Record(GothicBuildWarning::ParameterClamped, path, message);
		}
		bool Failed () const { return m_report.HasErrors(); }

		// Champ hors schema (additionalProperties false).
		void Known (const json &obj, const std::string &parent, std::initializer_list<const char *> names)
		{
			for (auto it = obj.begin(); it != obj.end(); ++it)
				if (std::none_of(names.begin(), names.end(), [&](const char *n) { return it.key() == n; }))
					Error(GothicBuildError::JsonSchema, join(parent, it.key()), "unknown field");
		}

		bool Require (const json &obj, const std::string &parent, std::initializer_list<const char *> names)
		{
			bool ok = true;
			for (const char *n : names)
				if (!obj.contains(n))
				{
					Error(GothicBuildError::JsonMissing, join(parent, n), "required field is missing");
					ok = false;
				}
			return ok;
		}

		const json *Object (const json &obj, const std::string &parent, const char *key)
		{
			const json *v = member(obj, key);
			if (v && !v->is_object())
			{
				TypeError(join(parent, key), "an object", *v);
				return nullptr;
			}
			return v;
		}

		// Nombre fini, sans controle de domaine.
		bool Finite (const json &obj, const std::string &parent, const char *key, double &out)
		{
			const json *v = member(obj, key);
			if (!v)
				return false;
			const std::string path = join(parent, key);
			if (!v->is_number())
			{
				TypeError(path, "a number", *v);
				return false;
			}
			const double x = v->get<double>();
			if (!std::isfinite(x))
			{
				Error(GothicBuildError::JsonValue, path, "non-finite value");
				return false;
			}
			out = x;
			return true;
		}

		bool Number (const json &obj, const std::string &parent, const char *key, const Bounds &bounds, double &out)
		{
			double x = 0.0;
			if (!Finite(obj, parent, key, x))
				return false;
			const std::string path = join(parent, key);
			if (!bounds.Contains(x))
			{
				Error(GothicBuildError::JsonValue, path, valueText(x) + " outside " + bounds.Text());
				return false;
			}
			out = x;
			return true;
		}

		// Reglage float : le nombre doit aussi rester fini une fois arrondi en float.
		bool ToFloat (const std::string &path, double x, float &out)
		{
			if (std::fabs(x) > std::numeric_limits<float>::max())
			{
				Error(GothicBuildError::JsonValue, path, valueText(x) + " is not finite as a float");
				return false;
			}
			out = static_cast<float>(x);
			return true;
		}

		// Le domaine est controle sur la valeur arrondie en float, celle qui est posee.
		bool Float (const json &obj, const std::string &parent, const char *key, const Bounds &bounds, float &out)
		{
			double x = 0.0;
			float f = 0.f;
			const std::string path = join(parent, key);
			if (!Finite(obj, parent, key, x) || !ToFloat(path, x, f))
				return false;
			if (!bounds.ContainsFloat(f))
			{
				std::ostringstream read;
				read << std::setprecision(12) << x;
				const std::string rounded = (static_cast<double>(f) != x) ? " (" + valueText(f) + " as a float)" : "";
				Error(GothicBuildError::JsonValue, path, read.str() + rounded + " outside " + bounds.Text());
				return false;
			}
			out = f;
			return true;
		}

		// Entier au sens de JSON Schema : 2 et 2.0 le sont, 2.5 non.
		bool Integer (const json &obj, const std::string &parent, const char *key, int lo, int hi, int &out)
		{
			const json *v = member(obj, key);
			if (!v)
				return false;
			const std::string path = join(parent, key);
			const double x = v->is_number() ? v->get<double>() : 0.0;
			if (!v->is_number() || !std::isfinite(x) || std::floor(x) != x)
			{
				TypeError(path, "an integer", *v);
				return false;
			}
			if (x < lo || x > hi)
			{
				Error(GothicBuildError::JsonValue, path,
				      valueText(x) + " outside [" + std::to_string(lo) + ", " + std::to_string(hi) + "]");
				return false;
			}
			out = static_cast<int>(x);
			return true;
		}

		bool Boolean (const json &obj, const std::string &parent, const char *key, bool &out)
		{
			const json *v = member(obj, key);
			if (!v)
				return false;
			if (!v->is_boolean())
			{
				TypeError(join(parent, key), "a boolean", *v);
				return false;
			}
			out = v->get<bool>();
			return true;
		}

		bool String (const json &obj, const std::string &parent, const char *key, std::string &out)
		{
			const json *v = member(obj, key);
			if (!v)
				return false;
			if (!v->is_string())
			{
				TypeError(join(parent, key), "a string", *v);
				return false;
			}
			out = v->get<std::string>();
			return true;
		}

		// `index` recoit la position de la valeur dans `names`.
		bool Enum (const json &obj, const std::string &parent, const char *key,
		           std::initializer_list<const char *> names, int &index)
		{
			std::string s;
			if (!String(obj, parent, key, s))
				return false;
			int i = 0;
			for (const char *n : names)
			{
				if (s == n)
				{
					index = i;
					return true;
				}
				++i;
			}
			std::string expected;
			for (const char *n : names)
				expected += (expected.empty() ? "" : ", ") + std::string(n);
			Error(GothicBuildError::JsonValue, join(parent, key),
			      "unknown value \"" + s + "\" (expected one of: " + expected + ")");
			return false;
		}

	private:
		GothicBuildReport &m_report;
	};

	// =======================================================================
	//  Lecteur v2
	// =======================================================================

	// Profil d'un champ ; rend son indice (menu Profile), -1 si invalide ou absent.
	int readProfile (const json &p, const std::string &path, Reader &r)
	{
		r.Known(p, path, {"type", "depth"});
		r.Require(p, path, {"type"});
		int type = -1;
		r.Enum(p, path, "type", {"flat", "chamfer", "roll", "keel", "ogee"}, type);
		double depth = 0.0;
		if (r.Number(p, path, "depth", above(0.0), depth))
			r.Ignored(join(path, "depth"), "the profile depth derives from the offsets");
		return type;
	}

	void readGeometry (const json &g, Reader &r, GothicWindowSettings &s)
	{
		const std::string p = "geometry";
		r.Known(g, p, {"width", "excess", "bdOuter", "bdInner", "heightBott", "wallSetback", "kseg", "subwindows"});
		r.Require(g, p, {"width", "excess", "bdOuter", "bdInner"});
		r.Float(g, p, "width",   above(0.0),   s.width);
		r.Float(g, p, "excess",  above(0.5),   s.excess);
		r.Float(g, p, "bdOuter", atLeast(0.0), s.offsetOuter);
		r.Float(g, p, "bdInner", atLeast(0.0), s.offsetInner);
		// heightBott : niveau du bas de la baie, springline a y = 0. Au-dessus
		// de la naissance, le corps est vide.
		double heightBott = 0.0;
		float body = 0.f;
		if (r.Number(g, p, "heightBott", anyNumber(), heightBott) && r.ToFloat(p + ".heightBott", -heightBott, body))
		{
			if (body < 0.f)
			{
				r.Clamped(p + ".heightBott", valueText(heightBott) + " is above the springline: body height 0");
				body = 0.f;
			}
			s.bodyHeight = body;
		}
		double setback = 0.0;
		if (r.Number(g, p, "wallSetback", atLeast(0.0), setback) && setback != 0.0)
			r.Ignored(p + ".wallSetback", "the window is not set in a wall");
		// kseg : segments par cercle complet.
		int kseg = 0;
		if (r.Integer(g, p, "kseg", 8, 720, kseg))
			s.maxAngleRad = 2.0 * kPi / kseg;

		const json *sw = r.Object(g, p, "subwindows");
		if (!sw)
			return;
		const std::string ps = p + ".subwindows";
		r.Known(*sw, ps, {"count", "arcDown", "excess", "layout", "gap"});
		r.Require(*sw, ps, {"count"});
		if (r.Integer(*sw, ps, "count", 1, 6, s.subCount) && s.subCount != 2)
			r.Ignored(ps + ".count", "the lancet count is fixed at 2");
		r.Float(*sw, ps, "arcDown", atLeast(0.0), s.subDrop);
		r.Float(*sw, ps, "excess",  above(0.5),   s.subExcess);
		r.Enum(*sw, ps, "layout", {"uniform", "prototype"}, s.lancetLayout);
		if (const json *gap = r.Object(*sw, ps, "gap"))
		{
			const std::string pg = ps + ".gap";
			r.Known(*gap, pg, {"mode", "gapFraction", "absoluteWidth"});
			int mode = 0;
			if (r.Enum(*gap, pg, "mode", {"fraction", "absolute"}, mode) && mode == 1)
				r.Ignored(pg + ".mode", "absolute gaps are not supported: gapFraction is used");
			// Lu dans les deux dispositions : il borne aussi l'offset interieur.
			r.Float(*gap, pg, "gapFraction", interval(0.0, true, 0.3, false), s.gapFraction);
			double absolute = 0.0;
			if (r.Number(*gap, pg, "absoluteWidth", above(0.0), absolute))
				r.Ignored(pg + ".absoluteWidth", "absolute gaps are not supported");
		}
	}

	void readStyle (const json &st, Reader &r, double mouchetteSize, GothicWindowSettings &s)
	{
		const std::string p = "style";
		r.Known(st, p, {"mainArch", "subArch", "rosette", "fillet"});

		// Le profil est global : celui de l'arc principal. Les profils des autres
		// champs qui en different sont sans effet.
		std::vector<std::pair<std::string, int>> fieldProfiles;
		auto readFieldProfile = [&](const json &field, const std::string &path) {
			if (const json *pr = r.Object(field, path, "profile"))
			{
				const int type = readProfile(*pr, path + ".profile", r);
				if (type >= 0)
					fieldProfiles.emplace_back(path + ".profile.type", type);
			}
		};

		if (const json *ma = r.Object(st, p, "mainArch"))
		{
			const std::string pm = p + ".mainArch";
			r.Known(*ma, pm, {"profile", "trefoil"});
			if (const json *pr = r.Object(*ma, pm, "profile"))
			{
				const int type = readProfile(*pr, pm + ".profile", r);
				if (type >= 0)
					s.profile = type;
			}
			if (const json *tf = r.Object(*ma, pm, "trefoil"))
			{
				const std::string pt = pm + ".trefoil";
				r.Known(*tf, pt, {"enabled", "splitParameter", "foilRadiusFactor"});
				if (r.Boolean(*tf, pt, "enabled", s.archTrefoil) && s.archTrefoil)
					r.Ignored(pt + ".enabled", "the main arch trefoil is not built");
				r.Float(*tf, pt, "splitParameter",   interval(0.0, true, 1.0, true),  s.trefoilSplit);
				r.Float(*tf, pt, "foilRadiusFactor", interval(0.0, true, 0.5, false), s.trefoilFoilRadius);
			}
		}

		if (const json *sa = r.Object(st, p, "subArch"))
		{
			const std::string ps = p + ".subArch";
			r.Known(*sa, ps, {"profile", "head"});
			readFieldProfile(*sa, ps);
			if (const json *h = r.Object(*sa, ps, "head"))
			{
				const std::string ph = ps + ".head";
				r.Known(*h, ph, {"type", "foils"});
				r.Enum(*h, ph, "type", {"plain", "foiled"}, s.lancetHead);
				r.Integer(*h, ph, "foils", 3, 8, s.lancetHeadFoils);
			}
		}

		if (const json *ro = r.Object(st, p, "rosette"))
		{
			const std::string pr = p + ".rosette";
			r.Known(*ro, pr, {"present", "profile", "foils"});
			r.Boolean(*ro, pr, "present", s.rosette);
			readFieldProfile(*ro, pr);
			if (const json *f = r.Object(*ro, pr, "foils"))
			{
				const std::string pf = pr + ".foils";
				r.Known(*f, pf, {"count", "type", "pointedness", "orientation"});
				r.Require(*f, pf, {"count"});
				s.rosetteFoils = true;
				r.Integer(*f, pf, "count", 3, 24, s.rosetteFoilCount);
				r.Enum(*f, pf, "type", {"round", "pointed"}, s.rosetteFoilType);
				r.Float(*f, pf, "pointedness", interval(0.0, true, 2.0, false), s.rosetteFoilPointed);
				int orientation = 0;
				if (r.Enum(*f, pf, "orientation", {"standing", "lying"}, orientation) && orientation == 1)
					r.Ignored(pf + ".orientation", "the rosette always stands, a foil at the top");
			}
			else if (!ro->contains("foils"))
				s.rosetteFoils = false;   // rosace decrite sans foils : sans lobes
		}

		if (const json *fi = r.Object(st, p, "fillet"))
		{
			const std::string pf = p + ".fillet";
			r.Known(*fi, pf, {"fill", "mouchette"});
			int fill = 0;
			if (r.Enum(*fi, pf, "fill", {"plain", "mouchette", "none"}, fill))
			{
				s.fillets    = (fill != 2);
				s.mouchettes = (fill == 1);
			}
			if (const json *mo = r.Object(*fi, pf, "mouchette"))
			{
				const std::string pm = pf + ".mouchette";
				r.Known(*mo, pm, {"type", "size"});
				r.Require(*mo, pm, {"type"});
				r.Enum(*mo, pm, "type", {"vesica", "teardrop", "soufflet"}, s.mouchetteType);
				double size = 0.0;
				if (r.Number(*mo, pm, "size", interval(0.0, true, 1.0, false), size) && size != mouchetteSize)
					r.Ignored(pm + ".size", "the mouchette size is fixed at " + valueText(mouchetteSize));
			}
		}

		for (const auto &[path, type] : fieldProfiles)
			if (type != s.profile)
				r.Ignored(path, "per-field profiles are not supported: the main arch profile applies");
	}

	void readV2 (const json &doc, Reader &r, double mouchetteSize,
	             GothicWindowSettings &s, GothicFeatureThresholds &t)
	{
		r.Known(doc, std::string(), {"schema", "meta", "geometry", "style", "recursion", "extrusion", "thresholds"});
		if (const json *meta = r.Object(doc, std::string(), "meta"))
		{
			std::string text;
			for (const char *key : {"id", "label", "period", "cathedral_ref"})
				r.String(*meta, "meta", key, text);
		}
		r.Require(doc, std::string(), {"geometry"});
		if (const json *g = r.Object(doc, std::string(), "geometry"))
			readGeometry(*g, r, s);
		if (const json *st = r.Object(doc, std::string(), "style"))
			readStyle(*st, r, mouchetteSize, s);
		if (const json *rec = r.Object(doc, std::string(), "recursion"))
		{
			r.Known(*rec, "recursion", {"depth", "harmonic"});
			r.Integer(*rec, "recursion", "depth", 0, 2, s.recursion);
			bool harmonic = false;
			if (r.Boolean(*rec, "recursion", "harmonic", harmonic) && harmonic)
				r.Ignored("recursion.harmonic", "harmonic rules are not implemented");
		}
		if (const json *ex = r.Object(doc, std::string(), "extrusion"))
		{
			r.Known(*ex, "extrusion", {"zHeight"});
			r.Float(*ex, "extrusion", "zHeight", atLeast(0.0), s.zHeight);
		}
		if (const json *th = r.Object(doc, std::string(), "thresholds"))
		{
			for (auto it = th->begin(); it != th->end(); ++it)
				if (it.key() != kIgnoredThreshold
				    && std::none_of(std::begin(kThresholdFields), std::end(kThresholdFields),
				                    [&](const auto &f) { return it.key() == f.first; }))
					r.Error(GothicBuildError::JsonSchema, join("thresholds", it.key()), "unknown field");
			for (const auto &[name, field] : kThresholdFields)
				r.Number(*th, "thresholds", name, atLeast(0.0), t.*field);
			double ignored = 0.0;
			if (r.Number(*th, "thresholds", kIgnoredThreshold, atLeast(0.0), ignored))
				r.Ignored(join("thresholds", kIgnoredThreshold),
				          "the spandrel inset is max(bdInner, minFilletInnerOffset)");
		}
	}

	// =======================================================================
	//  Conversion v1 -> v2
	// =======================================================================
	//
	// Le document v1 est traduit en un document v2, lu ensuite par le lecteur v2.
	// Les diagnostics de la traduction sont nommes par le chemin v1 ; tout champ
	// v1 sans effet sur la baie donne field_ignored, tout champ traduit par son
	// plus proche equivalent field_converted. Les champs v1 facultatifs que la
	// traduction porte prennent leur defaut v1 quand ils sont absents (drop 0,
	// fillets actifs, pas de mouchettes ni de recursion, 72 segments) : un
	// document v1 decrit une fenetre entiere. Les sous-arbres perdus en entier
	// (wall, style.glass...) ne sont controles qu'a leur racine.

	bool readPoint (const json &obj, const std::string &parent, const char *key, Reader &r, double &x, double &y)
	{
		const json *pt = r.Object(obj, parent, key);
		if (!pt)
			return false;
		const std::string path = join(parent, key);
		r.Known(*pt, path, {"x", "y"});
		const bool complete = r.Require(*pt, path, {"x", "y"});
		const bool hasX = r.Number(*pt, path, "x", anyNumber(), x);
		const bool hasY = r.Number(*pt, path, "y", anyNumber(), y);
		return complete && hasX && hasY;
	}

	struct FoilsV1
	{
		bool   valid = false;
		int    count = 0;
		int    type = 0;          // 0 round, 1 pointed
		double pointedness = 0.0;
		double phi0 = 0.0;
		int    orientation = 0;   // 0 standing, 1 lying
	};

	FoilsV1 readFoilsV1 (const json &f, const std::string &path, Reader &r)
	{
		FoilsV1 out;
		r.Known(f, path, {"count", "type", "pointedness", "phi0", "orientation"});
		bool ok = r.Require(f, path, {"count", "type"});
		ok = r.Integer(f, path, "count", 3, 24, out.count) && ok;
		ok = r.Enum(f, path, "type", {"round", "pointed"}, out.type) && ok;
		r.Number(f, path, "pointedness", interval(0.0, false, 2.0, false), out.pointedness);
		r.Number(f, path, "phi0", anyNumber(), out.phi0);
		r.Enum(f, path, "orientation", {"standing", "lying"}, out.orientation);
		if (out.type == 0 && out.pointedness != 0.0)
			r.Error(GothicBuildError::JsonValue, path + ".pointedness", "round foils have pointedness 0");
		out.valid = ok;
		return out;
	}

	json convertTrefoilV1 (const json &tf, const std::string &path, Reader &r, bool &enabled)
	{
		json out = json::object();
		r.Known(tf, path, {"enabled", "splitParameter", "foilRadiusFactor", "continuity"});
		r.Require(tf, path, {"enabled"});
		enabled = false;
		if (r.Boolean(tf, path, "enabled", enabled))
			out["enabled"] = enabled;
		double v = 0.0;
		if (r.Number(tf, path, "splitParameter", interval(0.0, true, 1.0, true), v))
			out["splitParameter"] = v;
		if (r.Number(tf, path, "foilRadiusFactor", interval(0.0, true, 0.5, false), v))
			out["foilRadiusFactor"] = v;
		int continuity = 0;
		r.Enum(tf, path, "continuity", {"tangent"}, continuity);
		return out;
	}

	// v1 admet 0,5 (plein cintre), que v2 exclut : porte au minimum du generateur.
	double convertExcessV1 (double excess, const std::string &path, Reader &r)
	{
		if (excess > 0.5)
			return excess;
		r.Converted(path, valueText(excess) + " (semicircular) -> 0.7, the smallest excess of the generator");
		return 0.7;
	}

	json convertV1 (const json &doc, Reader &r)
	{
		json v2 = json::object();
		v2["schema"] = kV2Schema;

		r.Known(doc, std::string(), {"$schema", "wall", "window", "variants"});
		std::string text;
		r.String(doc, std::string(), "$schema", text);
		if (r.Object(doc, std::string(), "wall"))
			r.Ignored("wall", "the wall is not modelled");
		if (const json *variants = member(doc, "variants"))
		{
			if (!variants->is_array())
				r.TypeError("variants", "an array", *variants);
			else
				r.Ignored("variants", std::to_string(variants->size()) + " variant(s) not applied");
		}
		r.Require(doc, std::string(), {"window"});
		const json *w = r.Object(doc, std::string(), "window");
		if (!w)
			return v2;

		const std::string pw = "window";
		r.Known(*w, pw, {"id", "label", "period", "cathedral_ref", "basis", "arch", "subwindows",
		                 "rosette", "fillets", "mouchettes", "style", "tessellation"});
		r.Require(*w, pw, {"basis", "arch", "subwindows", "rosette", "style"});
		for (const char *key : {"id", "label", "period", "cathedral_ref"})
			if (r.String(*w, pw, key, text))
				v2["meta"][key] = text;

		json &g     = v2["geometry"];
		json &style = v2["style"];

		// --- basis : centres de l'arc et hauteur des jambages ---
		const std::string pb = pw + ".basis";
		double archRise = 0.0, pLx = 0.0, pLy = 0.0, pRx = 0.0, pRy = 0.0;
		bool havePoints = false;
		if (const json *b = r.Object(*w, pw, "basis"))
		{
			r.Known(*b, pb, {"origin", "archRise", "pL", "pR"});
			r.Require(*b, pb, {"archRise", "pL", "pR"});
			double ox = 0.0, oy = 0.0;
			if (readPoint(*b, pb, "origin", r, ox, oy) && (ox != 0.0 || oy != 0.0))
				r.Ignored(pb + ".origin", "the bay is placed with its springline centre at the origin");
			const bool haveRise = r.Number(*b, pb, "archRise", atLeast(0.0), archRise);
			const bool left  = readPoint(*b, pb, "pL", r, pLx, pLy);
			const bool right = readPoint(*b, pb, "pR", r, pRx, pRy);
			havePoints = left && right;
			if (havePoints && pLx != -pRx)
				r.Ignored(pb + ".pL", "pL and pR are not symmetric about x = 0: the bay is centred");
			if (haveRise && left && pLy != archRise)
				r.Ignored(pb + ".pL", "pL.y differs from archRise: archRise is used");
			if (haveRise && right && pRy != archRise)
				r.Ignored(pb + ".pR", "pR.y differs from archRise: archRise is used");
			if (haveRise)
				g["heightBott"] = -archRise;
		}

		// --- arch ---
		const std::string pa = pw + ".arch";
		std::string profileName;   // type v1, pour le conflit avec archStyle.molding
		if (const json *a = r.Object(*w, pw, "arch"))
		{
			r.Known(*a, pa, {"width", "excess", "offset", "profile", "trefoil"});
			r.Require(*a, pa, {"excess", "offset", "profile"});
			double width = 0.0;
			if (r.Number(*a, pa, "width", above(0.0), width))
			{
				g["width"] = width;
				if (havePoints && std::fabs(pRx - pLx) != width)
					r.Ignored(pb, "|pR.x - pL.x| = " + valueText(std::fabs(pRx - pLx))
					              + " differs from window.arch.width: arch.width is used");
			}
			else if (!a->contains("width"))
			{
				if (havePoints)
					g["width"] = std::fabs(pRx - pLx);
				else
					r.Error(GothicBuildError::JsonMissing, pa + ".width", "required field is missing (no basis pL / pR)");
			}
			double excess = 0.0;
			if (r.Number(*a, pa, "excess", interval(0.5, false, 2.0, false), excess))
				g["excess"] = convertExcessV1(excess, pa + ".excess", r);
			if (const json *o = r.Object(*a, pa, "offset"))
			{
				const std::string po = pa + ".offset";
				r.Known(*o, po, {"outer", "inner", "method"});
				r.Require(*o, po, {"outer", "inner"});
				double v = 0.0;
				if (r.Number(*o, po, "outer", atLeast(0.0), v))
					g["bdOuter"] = v;
				if (r.Number(*o, po, "inner", atLeast(0.0), v))
					g["bdInner"] = v;
				int method = 0;
				r.Enum(*o, po, "method", {"fixed-centers"}, method);
			}
			if (const json *pr = r.Object(*a, pa, "profile"))
			{
				const std::string pp = pa + ".profile";
				r.Known(*pr, pp, {"type", "depth", "segments"});
				r.Require(*pr, pp, {"type", "depth", "segments"});
				static const char *const kNames[] = {"chamfer", "ogee", "cavetto", "bolection", "bead", "custom"};
				int type = -1;
				if (r.Enum(*pr, pp, "type", {"chamfer", "ogee", "cavetto", "bolection", "bead", "custom"}, type))
				{
					profileName = kNames[type];
					if (profileName == "chamfer" || profileName == "ogee")
						style["mainArch"]["profile"]["type"] = profileName;
					else if (profileName == "bead")
					{
						style["mainArch"]["profile"]["type"] = "roll";
						r.Converted(pp + ".type", "bead -> roll");
					}
					else
						r.Ignored(pp + ".type", "\"" + profileName + "\" has no equivalent: the profile is unchanged");
				}
				double depth = 0.0;
				if (r.Number(*pr, pp, "depth", above(0.0), depth))
					r.Ignored(pp + ".depth", "the profile depth derives from the offsets");
				if (const json *segments = member(*pr, "segments"))
				{
					if (!segments->is_array())
						r.TypeError(pp + ".segments", "an array", *segments);
					else
						r.Ignored(pp + ".segments", "explicit profile segments are not read");
				}
			}
			if (const json *tf = r.Object(*a, pa, "trefoil"))
			{
				bool enabled = false;
				style["mainArch"]["trefoil"] = convertTrefoilV1(*tf, pa + ".trefoil", r, enabled);
			}
		}

		// --- subwindows : lancettes ---
		const std::string ps = pw + ".subwindows";
		if (const json *sw = r.Object(*w, pw, "subwindows"))
		{
			r.Known(*sw, ps, {"count", "drop", "excess", "gap", "trefoil", "foils"});
			r.Require(*sw, ps, {"count", "excess", "gap", "foils"});
			json &sub = g["subwindows"];
			int count = 0;
			if (r.Integer(*sw, ps, "count", 1, 6, count))
				sub["count"] = count;
			double drop = 0.0;
			r.Number(*sw, ps, "drop", atLeast(0.0), drop);
			sub["arcDown"] = drop;
			double excess = 0.0;
			if (r.Number(*sw, ps, "excess", interval(0.5, false, 2.0, false), excess))
				sub["excess"] = convertExcessV1(excess, ps + ".excess", r);
			if (const json *gap = r.Object(*sw, ps, "gap"))
			{
				const std::string pg = ps + ".gap";
				r.Known(*gap, pg, {"mode", "gapFraction", "absoluteWidth"});
				r.Require(*gap, pg, {"mode"});
				sub["layout"] = "uniform";   // meneaux egaux, regles par gap
				int mode = -1;
				r.Enum(*gap, pg, "mode", {"fraction", "absolute"}, mode);
				double fraction = 0.0, absolute = 0.0;
				const bool haveFraction = r.Number(*gap, pg, "gapFraction", interval(0.0, true, 0.3, false), fraction);
				const json *absoluteValue = member(*gap, "absoluteWidth");
				const bool haveAbsolute = absoluteValue && !absoluteValue->is_null()
				                          && r.Number(*gap, pg, "absoluteWidth", above(0.0), absolute);
				if (mode == 0)
				{
					if (!gap->contains("gapFraction"))
						r.Error(GothicBuildError::JsonMissing, pg + ".gapFraction", "required in fraction mode");
					else if (haveFraction)
						sub["gap"] = json{ {"mode", "fraction"}, {"gapFraction", fraction} };
				}
				else if (mode == 1)
				{
					if (!absoluteValue || absoluteValue->is_null())
						r.Error(GothicBuildError::JsonMissing, pg + ".absoluteWidth", "required in absolute mode");
					else if (haveAbsolute && g.contains("width"))
					{
						const double f = absolute / g["width"].get<double>();
						sub["gap"] = json{ {"mode", "fraction"}, {"gapFraction", f} };
						r.Converted(pg + ".absoluteWidth", "absolute width " + valueText(absolute)
						                                   + " -> gap fraction " + valueText(f) + " of the bay width");
					}
				}
			}
			if (const json *tf = r.Object(*sw, ps, "trefoil"))
			{
				bool enabled = false;
				convertTrefoilV1(*tf, ps + ".trefoil", r, enabled);
				if (enabled)
					r.Ignored(ps + ".trefoil", "lancet trefoils are not built");
			}
			if (const json *f = r.Object(*sw, ps, "foils"))
			{
				const std::string pf = ps + ".foils";
				const FoilsV1 foils = readFoilsV1(*f, pf, r);
				if (foils.valid)
				{
					const int head = std::min(std::max(foils.count, 3), 8);
					style["subArch"]["head"] = json{ {"type", "foiled"}, {"foils", head} };
					r.Converted(pf, std::to_string(foils.count) + " lancet foils -> foiled lancet head of "
					                + std::to_string(head) + " foils");
				}
				if (foils.type == 1)
					r.Ignored(pf + ".type", "lancet head foils are round");
				if (foils.type == 1 && foils.pointedness != 0.0)
					r.Ignored(pf + ".pointedness", "lancet head foils are round");
				if (foils.phi0 != 0.0)
					r.Ignored(pf + ".phi0", "lancet head foils are not rotated");
				if (foils.orientation == 1)
					r.Ignored(pf + ".orientation", "lancet head foils are not rotated");
			}
		}

		// --- rosette ---
		const std::string pr = pw + ".rosette";
		if (const json *ro = r.Object(*w, pw, "rosette"))
		{
			r.Known(*ro, pr, {"construction", "foils", "hub"});
			r.Require(*ro, pr, {"construction", "foils"});
			int construction = 0;
			r.Enum(*ro, pr, "construction", {"ellipse-intersection"}, construction);
			json &rosette = style["rosette"];
			rosette["present"] = true;
			if (const json *f = r.Object(*ro, pr, "foils"))
			{
				const std::string pf = pr + ".foils";
				const FoilsV1 foils = readFoilsV1(*f, pf, r);
				if (foils.valid)
				{
					json out = json{ {"count", foils.count}, {"type", foils.type == 1 ? "pointed" : "round"} };
					if (foils.type == 1 && foils.pointedness > 0.0)
						out["pointedness"] = foils.pointedness;
					else if (foils.type == 1)
						r.Ignored(pf + ".pointedness", "pointedness 0 is outside the v2 domain: the current value is kept");
					rosette["foils"] = out;
				}
				if (foils.phi0 != 0.0)
					r.Ignored(pf + ".phi0", "the rosette always has a foil at the top");
				if (foils.orientation == 1)
					r.Ignored(pf + ".orientation", "the rosette always stands, a foil at the top");
			}
			if (const json *hub = r.Object(*ro, pr, "hub"))
			{
				const std::string ph = pr + ".hub";
				r.Known(*hub, ph, {"radiusFactor"});
				double v = 0.0;
				if (r.Number(*hub, ph, "radiusFactor", interval(0.0, true, 0.3, false), v))
					r.Ignored(ph + ".radiusFactor", "the rosette hub is not built");
			}
		}

		// --- fillets et mouchettes : remplissage des ecoincons ---
		bool fillets = true, mouchettes = false;
		const std::string pfi = pw + ".fillets";
		if (const json *fi = r.Object(*w, pw, "fillets"))
		{
			r.Known(*fi, pfi, {"enabled", "subdivide", "subdivisionDepth", "stoneBandWidth"});
			r.Boolean(*fi, pfi, "enabled", fillets);
			bool subdivide = false;
			if (r.Boolean(*fi, pfi, "subdivide", subdivide))
				r.Ignored(pfi + ".subdivide", "fillets are not subdivided");
			int depth = 0;
			if (r.Integer(*fi, pfi, "subdivisionDepth", 1, 3, depth))
				r.Ignored(pfi + ".subdivisionDepth", "fillets are not subdivided");
			double band = 0.0;
			if (r.Number(*fi, pfi, "stoneBandWidth", above(0.0), band)
			    && !(g.contains("bdInner") && g["bdInner"].get<double>() == band))
				r.Ignored(pfi + ".stoneBandWidth", "the fillet stone band is window.arch.offset.inner");
		}
		const std::string pm = pw + ".mouchettes";
		if (const json *mo = r.Object(*w, pw, "mouchettes"))
		{
			r.Known(*mo, pm, {"enabled", "type", "radiusFactor", "rotation"});
			r.Require(*mo, pm, {"enabled"});
			r.Boolean(*mo, pm, "enabled", mouchettes);
			static const char *const kTypes[] = {"vesica", "teardrop", "soufflet"};
			int type = 0;
			if (r.Enum(*mo, pm, "type", {"vesica", "teardrop", "soufflet"}, type))
				style["fillet"]["mouchette"]["type"] = kTypes[type];
			double v = 0.0;
			if (r.Number(*mo, pm, "radiusFactor", interval(0.0, true, 0.5, false), v))
				r.Ignored(pm + ".radiusFactor", "the mouchette fills its spandrel field");
			if (r.Number(*mo, pm, "rotation", anyNumber(), v))
				r.Ignored(pm + ".rotation", "mouchettes are not rotated");
		}
		if (mouchettes && !fillets)
			r.Ignored(pm + ".enabled", "mouchettes need the fillets, and window.fillets.enabled is false");
		style["fillet"]["fill"] = !fillets ? "none" : mouchettes ? "mouchette" : "plain";

		// --- style : apparence et recursion ---
		const std::string pst = pw + ".style";
		int recursion = 0;
		if (const json *st = r.Object(*w, pw, "style"))
		{
			r.Known(*st, pst, {"windowStyle", "archStyle", "glass", "stone", "subdivisionSurface", "recursive"});
			r.Require(*st, pst, {"glass", "stone"});
			for (const char *key : {"windowStyle", "glass", "stone", "subdivisionSurface"})
				if (r.Object(*st, pst, key))
					r.Ignored(pst + "." + key, "not modelled by the generator");
			if (const json *as = r.Object(*st, pst, "archStyle"))
			{
				const std::string pas = pst + ".archStyle";
				r.Known(*as, pas, {"name", "molding"});
				r.String(*as, pas, "name", text);
				static const char *const kMoldings[] = {"chamfer", "ogee", "cavetto", "bolection", "bead"};
				int molding = 0;
				if (r.Enum(*as, pas, "molding", {"chamfer", "ogee", "cavetto", "bolection", "bead"}, molding)
				    && !profileName.empty() && profileName != kMoldings[molding])
					r.Ignored(pas + ".molding", "conflicts with window.arch.profile.type, which is used");
			}
			if (const json *rec = r.Object(*st, pst, "recursive"))
			{
				const std::string prc = pst + ".recursive";
				r.Known(*rec, prc, {"enabled", "scaleFactor", "maxDepth"});
				r.Require(*rec, prc, {"enabled"});
				bool enabled = false;
				r.Boolean(*rec, prc, "enabled", enabled);
				int maxDepth = 2;
				r.Integer(*rec, prc, "maxDepth", 1, 4, maxDepth);
				double v = 0.0;
				if (r.Number(*rec, prc, "scaleFactor", interval(0.0, true, 1.0, true), v))
					r.Ignored(prc + ".scaleFactor", "the recursion scale derives from the lancets");
				if (enabled)
				{
					recursion = std::min(maxDepth, 2);
					if (maxDepth > 2)
						r.Converted(prc + ".maxDepth", std::to_string(maxDepth) + " -> 2, the deepest recursion of the generator");
				}
			}
		}
		v2["recursion"]["depth"] = recursion;

		// --- tessellation ---
		const std::string pt = pw + ".tessellation";
		int kseg = 72;
		if (const json *te = r.Object(*w, pw, "tessellation"))
		{
			r.Known(*te, pt, {"targetEdgeLength", "angularResolution", "adaptiveSubdivision", "manifold"});
			r.Integer(*te, pt, "angularResolution", 12, 360, kseg);
			double length = 0.0;
			if (r.Number(*te, pt, "targetEdgeLength", above(0.0), length))
				r.Ignored(pt + ".targetEdgeLength", "arcs are tessellated by angle only");
			bool flag = false;
			if (r.Boolean(*te, pt, "adaptiveSubdivision", flag))
				r.Ignored(pt + ".adaptiveSubdivision", "arcs are tessellated by angle only");
			if (r.Boolean(*te, pt, "manifold", flag))
				r.Ignored(pt + ".manifold", "not an input of the generator");
		}
		g["kseg"] = kseg;
		return v2;
	}

	// =======================================================================
	//  Detection de version
	// =======================================================================

	enum class JsonVersion { V1, V2, Unsupported };

	JsonVersion detectVersion (const json &doc, Reader &r)
	{
		const json *dollar = member(doc, "$schema");
		if (dollar && dollar->is_string() && dollar->get<std::string>().find("/v1/") != std::string::npos)
			return JsonVersion::V1;
		const json *window = member(doc, "window");
		if (window && window->is_object() && !doc.contains("geometry"))
			return JsonVersion::V1;
		if (const json *schema = member(doc, "schema"))
		{
			if (!schema->is_string())
			{
				r.TypeError("schema", "a string", *schema);
				return JsonVersion::Unsupported;
			}
			if (schema->get<std::string>() != kV2Schema)
			{
				r.Error(GothicBuildError::JsonSchema, "schema",
				        "unsupported schema \"" + schema->get<std::string>() + "\" (expected " + kV2Schema + ")");
				return JsonVersion::Unsupported;
			}
		}
		return JsonVersion::V2;
	}
}

bool ParameterizedGothicWindow::LoadFromJson (const std::string &jsonText, GothicBuildReport *report)
{
	// Reglages et seuils sont lus sur des copies, puis poses ensemble par deux
	// affectations qui ne levent pas : un document refuse ne touche ni les uns
	// ni les autres. Les seuils restent hors de GothicWindowSettings.
	static_assert(std::is_nothrow_copy_assignable<GothicWindowSettings>::value
	              && std::is_nothrow_copy_assignable<GothicFeatureThresholds>::value,
	              "le chargement pose les reglages et les seuils sans pouvoir echouer a mi-chemin");
	GothicWindowSettings    settings   = m_settings;
	GothicFeatureThresholds thresholds = m_thresholds;

	GothicBuildReport load;
	{
		Reader r(load);
		try
		{
			const json doc = json::parse(jsonText);
			if (!doc.is_object())
				r.Error(GothicBuildError::JsonType, std::string(),
				        std::string("the document root must be an object, not ") + doc.type_name());
			else
			{
				switch (detectVersion(doc, r))
				{
				case JsonVersion::V1:
				{
					const json converted = convertV1(doc, r);
					if (!r.Failed())
						readV2(converted, r, kMouchetteSize, settings, thresholds);
					break;
				}
				case JsonVersion::V2:
					readV2(doc, r, kMouchetteSize, settings, thresholds);
					break;
				case JsonVersion::Unsupported:
					break;
				}
			}
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
	if (accepted)
	{
		m_settings   = settings;
		m_thresholds = thresholds;
	}
	m_lastLoadReport = std::move(load);
	return accepted;
}

std::string ParameterizedGothicWindow::ExportJson() const
{
	auto R = [](double v) { return std::round(v * 1e4) / 1e4; };   // arrondi lisible
	auto profileName = [](int p) -> std::string {
		switch (p) { case 0: return "flat"; case 1: return "chamfer";
		             case 3: return "keel"; case 4: return "ogee"; default: return "roll"; }
	};
	const std::string prof = profileName(m_settings.profile);   // profil GLOBAL (émis par champ)

	json j;
	j["schema"] = kV2Schema;

	json &g = j["geometry"];
	g["width"]      = R(m_settings.width);
	g["excess"]     = R(m_settings.excess);
	g["bdOuter"]    = R(m_settings.offsetOuter);
	g["bdInner"]    = R(m_settings.offsetInner);
	g["heightBott"] = R(-m_settings.bodyHeight);   // inverse de LoadFromJson (bodyHeight = -heightBott)
	if (m_settings.maxAngleRad > 1e-6)
		g["kseg"] = (int) std::lround(2.0 * kPi / m_settings.maxAngleRad);

	json &sw = g["subwindows"];
	sw["count"]   = m_settings.subCount;
	sw["arcDown"] = R(m_settings.subDrop);
	sw["excess"]  = R(m_settings.subExcess);
	sw["layout"]  = (m_settings.lancetLayout == 0) ? "uniform" : "prototype";
	// Ecrit dans les deux dispositions : gapFraction borne aussi l'offset interieur.
	sw["gap"] = json{ {"mode", "fraction"}, {"gapFraction", R(m_settings.gapFraction)} };

	json &st = j["style"];
	st["mainArch"]["profile"]["type"] = prof;
	st["mainArch"]["trefoil"] = json{
		{"enabled", m_settings.archTrefoil},
		{"splitParameter", R(m_settings.trefoilSplit)},
		{"foilRadiusFactor", R(m_settings.trefoilFoilRadius)} };
	st["subArch"]["profile"]["type"] = prof;
	st["subArch"]["head"] = json{
		{"type", (m_settings.lancetHead == 0) ? "plain" : "foiled"},
		{"foils", m_settings.lancetHeadFoils} };
	json &ros = st["rosette"];
	ros["present"] = m_settings.rosette;
	ros["profile"]["type"] = prof;
	if (m_settings.rosetteFoils)
		ros["foils"] = json{
			{"count", m_settings.rosetteFoilCount},
			{"type", (m_settings.rosetteFoilType == 1) ? "pointed" : "round"},
			{"pointedness", R(m_settings.rosetteFoilPointed)},
			{"orientation", "standing"} };
	json &fi = st["fillet"];
	fi["fill"] = !m_settings.fillets ? "none" : m_settings.mouchettes ? "mouchette" : "plain";
	fi["mouchette"] = json{
		{"type", (m_settings.mouchetteType == 0) ? "vesica" : (m_settings.mouchetteType == 1) ? "teardrop" : "soufflet"},
		{"size", kMouchetteSize} };

	j["recursion"]["depth"] = m_settings.recursion;
	j["extrusion"]["zHeight"] = R(m_settings.zHeight);

	// Seuils en unites monde, a pleine precision (le JSON les relit a l'identique).
	json &th = j["thresholds"];
	for (const auto &[name, field] : kThresholdFields)
		th[name] = m_thresholds.*field;

	return j.dump(2);
}
