#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "architecture_gothic_io.h"

#include <nlohmann/json.hpp>

using nlohmann::json;

//
// JSON parsing
//

namespace
{
	Vector2d parsePoint2d (const json &j)
	{
		if (!j.is_object() || !j.contains("x") || !j.contains("y"))
			throw std::invalid_argument("Expected a point2d object with x and y");
		return Vector2d(j.at("x").get<double>(), j.at("y").get<double>());
	}

	void parseGap (const json &j, SubwindowParams::Gap &out)
	{
		std::string mode = j.at("mode").get<std::string>();
		if (mode == "fraction")
		{
			out.mode = SubwindowParams::Gap::Mode::Fraction;
			out.gapFraction = j.at("gapFraction").get<double>();
		}
		else if (mode == "absolute")
		{
			out.mode = SubwindowParams::Gap::Mode::Absolute;
			out.absoluteWidth = j.at("absoluteWidth").get<double>();
		}
		else
		{
			throw std::invalid_argument("Unknown gap mode: " + mode);
		}
	}

	FoilsParams parseFoils (const json &j)
	{
		FoilsParams p;
		p.count = j.at("count").get<int>();

		std::string type = j.at("type").get<std::string>();
		if (type == "round")
			p.type = FoilType::Round;
		else if (type == "pointed")
			p.type = FoilType::Pointed;
		else
			throw std::invalid_argument("Unknown foil type: " + type);

		p.pointedness = j.value("pointedness", 0.0);
		p.phi0        = j.value("phi0", 0.0);

		std::string orientation = j.value("orientation", std::string("standing"));
		p.orientLying = (orientation == "lying");

		return p;
	}

	// Returns true iff the trefoil block is present AND enabled. Sets `out` to
	// the parsed parameters (with schema defaults if fields are missing).
	bool parseTrefoilEnabled (const json &j, TrefoilParams &out)
	{
		bool enabled = j.value("enabled", false);
		if (!enabled)
			return false;
		out.splitParameter   = j.value("splitParameter",   0.45);
		out.foilRadiusFactor = j.value("foilRadiusFactor", 0.30);
		// `continuity` is fixed to "tangent" in our implementation : ignored.
		return true;
	}

	// Returns true iff the mouchette block is present AND enabled. Sets `out`.
	bool parseMouchettesEnabled (const json &j, MouchetteParams &out)
	{
		bool enabled = j.value("enabled", false);
		if (!enabled)
			return false;

		std::string type = j.value("type", std::string("vesica"));
		if      (type == "vesica")   out.type = MouchetteType::Vesica;
		else if (type == "teardrop") out.type = MouchetteType::Teardrop;
		else if (type == "soufflet") out.type = MouchetteType::Soufflet;
		else throw std::invalid_argument("Unknown mouchette type: " + type);

		out.radiusFactor = j.value("radiusFactor", 0.18);
		out.rotation     = j.value("rotation",     0.0);
		return true;
	}
}

WindowInstance loadInstanceFromJson (const std::string &jsonText)
{
	json doc;
	try
	{
		doc = json::parse(jsonText);
	}
	catch (const json::parse_error &e)
	{
		throw std::runtime_error(std::string("JSON parse error: ") + e.what());
	}

	if (!doc.contains("window") || !doc["window"].is_object())
		throw std::invalid_argument("loadInstanceFromJson: missing 'window' object");

	const json &w = doc["window"];
	WindowInstance result;

	if (w.contains("id")     && w["id"].is_string())     result.id     = w["id"].get<std::string>();
	if (w.contains("label")  && w["label"].is_string())  result.label  = w["label"].get<std::string>();
	if (w.contains("period") && w["period"].is_string()) result.period = w["period"].get<std::string>();

	// basis
	if (!w.contains("basis"))
		throw std::invalid_argument("loadInstanceFromJson: missing window.basis");
	const json &basis = w["basis"];
	result.archBasis.pL = parsePoint2d(basis.at("pL"));
	result.archBasis.pR = parsePoint2d(basis.at("pR"));

	// arch (excess and offset)
	if (!w.contains("arch"))
		throw std::invalid_argument("loadInstanceFromJson: missing window.arch");
	const json &arch = w["arch"];
	result.archBasis.excess = arch.at("excess").get<double>();

	const json &offset = arch.at("offset");
	result.archOffset.outer = offset.at("outer").get<double>();
	result.archOffset.inner = offset.at("inner").get<double>();

	// arch.trefoil (optional, only consumed if enabled=true)
	if (arch.contains("trefoil") && arch["trefoil"].is_object())
		result.hasArchTrefoil = parseTrefoilEnabled(arch["trefoil"], result.archTrefoil);

	// subwindows
	if (!w.contains("subwindows"))
		throw std::invalid_argument("loadInstanceFromJson: missing window.subwindows");
	const json &sw = w["subwindows"];
	result.subwindowParams.count  = sw.at("count").get<int>();
	result.subwindowParams.drop   = sw.value("drop", 0.0);
	result.subwindowParams.excess = sw.at("excess").get<double>();
	parseGap(sw.at("gap"), result.subwindowParams.gap);

	// subwindows.foils (optional in our tolerant parser, required by schema)
	if (sw.contains("foils") && sw["foils"].is_object())
	{
		result.subwindowFoils    = parseFoils(sw["foils"]);
		result.hasSubwindowFoils = true;
	}

	// subwindows.trefoil (optional, only consumed if enabled=true)
	if (sw.contains("trefoil") && sw["trefoil"].is_object())
		result.hasSubwindowTrefoil = parseTrefoilEnabled(sw["trefoil"], result.subwindowTrefoil);

	// rosette and rosette.foils
	if (w.contains("rosette") && w["rosette"].is_object())
	{
		result.hasRosette = true;
		const json &ros = w["rosette"];
		if (ros.contains("foils") && ros["foils"].is_object())
		{
			result.rosetteFoils    = parseFoils(ros["foils"]);
			result.hasRosetteFoils = true;
		}
	}

	// fillets (optional, default disabled in our parser to avoid building
	// dependencies on rosette ; users opt-in with enabled=true).
	if (w.contains("fillets") && w["fillets"].is_object())
	{
		const json &fil = w["fillets"];
		bool enabled = fil.value("enabled", false);
		if (enabled)
		{
			result.hasFillets            = true;
			result.filletsStoneBandWidth = fil.value("stoneBandWidth", 10.0);
		}
	}

	// mouchettes (optional, only consumed if enabled=true)
	if (w.contains("mouchettes") && w["mouchettes"].is_object())
		result.hasMouchettes = parseMouchettesEnabled(w["mouchettes"], result.mouchettes);

	return result;
}

WindowInstance loadInstanceFromFile (const std::string &filePath)
{
	std::ifstream f(filePath);
	if (!f.is_open())
		throw std::runtime_error("loadInstanceFromFile: cannot open: " + filePath);
	std::stringstream ss;
	ss << f.rdbuf();
	return loadInstanceFromJson(ss.str());
}

//
// Geometry construction
//

WindowGeometry buildGeometryFromInstance (const WindowInstance &inst)
{
	WindowGeometry g;
	g.mainArch    = buildArch(inst.archBasis);
	g.mainOffset  = buildArchOffset(g.mainArch, inst.archOffset);
	// Sub-arches use the same offset as the main arch (schema-silent choice).
	g.subwindows  = buildSubwindows(g.mainArch, inst.archOffset, inst.subwindowParams);
	g.hasRosette  = inst.hasRosette;
	if (g.hasRosette)
		g.rosette = buildRosette(g.mainArch, g.subwindows);

	// Rosette foils, inscribed in the rosette circle.
	if (g.hasRosette && inst.hasRosetteFoils)
	{
		g.rosetteFoils    = buildFoilRing(g.rosette.circle, inst.rosetteFoils);
		g.hasRosetteFoils = true;
	}

	// Subwindow (lancet) foils, inscribed in each lancet's "head circle"
	// (largest circle fitting inside the lancet's inner offset arch).
	if (inst.hasSubwindowFoils)
	{
		g.subwindowFoils.reserve(g.subwindows.lancets.size());
		for (const LancetGeom &l : g.subwindows.lancets)
		{
			Circle head = inscribedCircleOfPointedArch(l.offset.inner);
			g.subwindowFoils.push_back(buildFoilRing(head, inst.subwindowFoils));
		}
		g.hasSubwindowFoils = true;
	}

	// Trefoils (built from the base arches, not the offsets).
	if (inst.hasArchTrefoil)
	{
		g.archTrefoil    = buildTrefoilArch(g.mainArch, inst.archTrefoil);
		g.hasArchTrefoil = true;
	}
	if (inst.hasSubwindowTrefoil)
	{
		g.subwindowTrefoils.reserve(g.subwindows.lancets.size());
		for (const LancetGeom &l : g.subwindows.lancets)
			g.subwindowTrefoils.push_back(buildTrefoilArch(l.arch, inst.subwindowTrefoil));
		g.hasSubwindowTrefoils = true;
	}

	// Fillets : require the rosette and at least one lancet.
	if (inst.hasFillets && g.hasRosette && !g.subwindows.lancets.empty())
	{
		g.fillets    = buildFillets(g.mainOffset, g.subwindows, g.rosette, inst.filletsStoneBandWidth);
		g.hasFillets = true;
	}

	// Mouchettes : one per inter-lancet gap (requires >= 2 lancets).
	if (inst.hasMouchettes && !g.subwindows.lancets.empty())
	{
		g.mouchettes    = buildMouchettes(g.subwindows, inst.mouchettes);
		g.hasMouchettes = !g.mouchettes.empty();
	}

	return g;
}

//
// SVG export
//

namespace
{
	struct Bounds
	{
		double xMin =  std::numeric_limits<double>::infinity();
		double xMax = -std::numeric_limits<double>::infinity();
		double yMin =  std::numeric_limits<double>::infinity();
		double yMax = -std::numeric_limits<double>::infinity();

		void include (Vector2d p)
		{
			if (p.x < xMin) xMin = p.x;
			if (p.x > xMax) xMax = p.x;
			if (p.y < yMin) yMin = p.y;
			if (p.y > yMax) yMax = p.y;
		}

		void includeCircle (const Circle &c)
		{
			include(Vector2d(c.center.x - c.radius, c.center.y - c.radius));
			include(Vector2d(c.center.x + c.radius, c.center.y + c.radius));
		}

		void includeArchSilhouette (const ArchGeom &a)
		{
			include(a.circleL.center);   // foot points
			include(a.circleR.center);
			include(a.apex);
			// Sample the arcs to cover ground anchors when excess != 1.
			include(a.arcLeft.pointAt(0.0));
			include(a.arcRight.pointAt(1.0));
		}
	};

	// Tessellation step (radians per segment) for arc rendering.
	const double SVG_ARC_MAX_ANGLE = 3.14159265358979323846 / 180.0; // 1 deg

	std::string formatPoint (Vector2d p, double yShift)
	{
		// y_canvas = yShift - y_math   (flips around horizontal axis at yShift)
		std::ostringstream os;
		os.setf(std::ios::fixed);
		os.precision(3);
		os << p.x << "," << (yShift - p.y);
		return os.str();
	}

	std::string polylineFromArc (const Arc &arc, double yShift,
	                              const char *stroke, double strokeWidth)
	{
		auto pts = arc.tessellateAdaptive(SVG_ARC_MAX_ANGLE);
		std::ostringstream os;
		os << "  <polyline points=\"";
		for (size_t i = 0; i < pts.size(); ++i)
		{
			if (i > 0) os << " ";
			os << formatPoint(pts[i], yShift);
		}
		os << "\" style=\"fill:none;stroke:" << stroke
		   << ";stroke-width:" << strokeWidth << "\"/>\n";
		return os.str();
	}

	std::string circleSvg (const Circle &c, double yShift,
	                        const char *stroke, double strokeWidth,
	                        const char *fill = "none")
	{
		std::ostringstream os;
		os.setf(std::ios::fixed);
		os.precision(3);
		os << "  <circle cx=\"" << c.center.x
		   << "\" cy=\"" << (yShift - c.center.y)
		   << "\" r=\""  << c.radius
		   << "\" style=\"fill:" << fill
		   << ";stroke:" << stroke
		   << ";stroke-width:" << strokeWidth << "\"/>\n";
		return os.str();
	}
}

std::string toSvg (const WindowGeometry &geom)
{
	// Compute the bounding box.
	Bounds b;
	b.includeArchSilhouette(geom.mainOffset.outer);
	b.includeArchSilhouette(geom.mainOffset.inner);
	for (const auto &l : geom.subwindows.lancets)
	{
		b.includeArchSilhouette(l.offset.outer);
		b.includeArchSilhouette(l.offset.inner);
	}
	if (geom.hasRosette)
		b.includeCircle(geom.rosette.circle);
	if (geom.hasArchTrefoil)
	{
		b.includeCircle(geom.archTrefoil.foilLeft.circle);
		b.includeCircle(geom.archTrefoil.foilRight.circle);
	}
	if (geom.hasSubwindowTrefoils)
	{
		for (const TrefoilArchGeom &t : geom.subwindowTrefoils)
		{
			b.includeCircle(t.foilLeft.circle);
			b.includeCircle(t.foilRight.circle);
		}
	}
	if (geom.hasMouchettes)
	{
		for (const MouchetteGeom &m : geom.mouchettes)
			for (const Arc &arc : m.contour)
				b.includeCircle(arc.circle);
	}

	// Padding so the strokes don't get clipped.
	const double pad = std::max(10.0, 0.05 * std::max(b.xMax - b.xMin, b.yMax - b.yMin));
	double xMin = b.xMin - pad;
	double yMin = b.yMin - pad;
	double xMax = b.xMax + pad;
	double yMax = b.yMax + pad;
	double w = xMax - xMin;
	double h = yMax - yMin;
	double yShift = yMax + yMin;   // y_canvas = yShift - y_math

	// Header.
	std::ostringstream os;
	os.setf(std::ios::fixed);
	os.precision(3);
	os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	os << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
	   << "viewBox=\"" << xMin << " " << (yShift - yMax) << " " << w << " " << h << "\" "
	   << "preserveAspectRatio=\"xMidYMid meet\">\n";
	os << "  <rect x=\"" << xMin << "\" y=\"" << (yShift - yMax)
	   << "\" width=\"" << w << "\" height=\"" << h
	   << "\" style=\"fill:#fafafa\"/>\n";

	// Stone face (outer offset of main arch).
	const char *stoneStroke = "#444";
	const char *innerStroke = "#888";
	const char *rosStroke   = "#a04040";
	double      sw          = std::max(0.5, 0.0025 * w);

	os << "  <!-- main arch outer offset -->\n";
	os << polylineFromArc(geom.mainOffset.outer.arcLeft,  yShift, stoneStroke, sw);
	os << polylineFromArc(geom.mainOffset.outer.arcRight, yShift, stoneStroke, sw);

	os << "  <!-- main arch inner offset -->\n";
	os << polylineFromArc(geom.mainOffset.inner.arcLeft,  yShift, innerStroke, sw);
	os << polylineFromArc(geom.mainOffset.inner.arcRight, yShift, innerStroke, sw);

	os << "  <!-- lancets -->\n";
	for (const auto &l : geom.subwindows.lancets)
	{
		os << polylineFromArc(l.offset.outer.arcLeft,  yShift, stoneStroke, sw);
		os << polylineFromArc(l.offset.outer.arcRight, yShift, stoneStroke, sw);
		os << polylineFromArc(l.offset.inner.arcLeft,  yShift, innerStroke, sw);
		os << polylineFromArc(l.offset.inner.arcRight, yShift, innerStroke, sw);
	}

	if (geom.hasRosette)
	{
		os << "  <!-- rosette -->\n";
		os << circleSvg(geom.rosette.circle, yShift, rosStroke, sw);
	}

	// Foils
	const char *foilStroke = "#666";
	auto emitFoilRing = [&](const FoilRing &ring) {
		for (const auto &foil : ring.foils)
		{
			if (std::holds_alternative<RoundFoil>(foil))
			{
				const RoundFoil &rf = std::get<RoundFoil>(foil);
				os << circleSvg(rf.circle, yShift, foilStroke, sw);
			}
			else
			{
				const PointedFoil &pf = std::get<PointedFoil>(foil);
				os << polylineFromArc(pf.arcLeft,  yShift, foilStroke, sw);
				os << polylineFromArc(pf.arcRight, yShift, foilStroke, sw);
			}
		}
	};

	if (geom.hasRosetteFoils)
	{
		os << "  <!-- rosette foils -->\n";
		emitFoilRing(geom.rosetteFoils);
	}

	if (geom.hasSubwindowFoils)
	{
		os << "  <!-- lancet foils -->\n";
		for (const FoilRing &ring : geom.subwindowFoils)
			emitFoilRing(ring);
	}

	// Trefoil decorations : draw the foil semicircles (the upper main arcs are
	// already part of the main offset rendering, so they need not be redrawn).
	const char *trefoilStroke = "#3060a0";
	auto emitTrefoilFoils = [&](const TrefoilArchGeom &t) {
		os << polylineFromArc(t.foilLeft.arc,  yShift, trefoilStroke, sw);
		os << polylineFromArc(t.foilRight.arc, yShift, trefoilStroke, sw);
	};

	if (geom.hasArchTrefoil)
	{
		os << "  <!-- main arch trefoil -->\n";
		emitTrefoilFoils(geom.archTrefoil);
	}

	if (geom.hasSubwindowTrefoils)
	{
		os << "  <!-- lancet trefoils -->\n";
		for (const TrefoilArchGeom &t : geom.subwindowTrefoils)
			emitTrefoilFoils(t);
	}

	// Fillets : 3 polylines per fillet outlining the curved triangle.
	if (geom.hasFillets)
	{
		const char *filletStroke = "#308040";
		os << "  <!-- fillets -->\n";
		for (const FilletGeom &fil : geom.fillets.fillets)
		{
			os << polylineFromArc(fil.arcOuter,   yShift, filletStroke, sw);
			os << polylineFromArc(fil.arcSub,     yShift, filletStroke, sw);
			os << polylineFromArc(fil.arcRosette, yShift, filletStroke, sw);
		}
	}

	// Mouchettes : 2 polylines (Vesica) or 3 (Teardrop / Soufflet) per mouchette.
	if (geom.hasMouchettes)
	{
		const char *mouchetteStroke = "#a060a0";
		os << "  <!-- mouchettes -->\n";
		for (const MouchetteGeom &mou : geom.mouchettes)
			for (const Arc &arc : mou.contour)
				os << polylineFromArc(arc, yShift, mouchetteStroke, sw);
	}

	os << "</svg>\n";
	return os.str();
}

void writeSvgToFile (const WindowGeometry &geom, const std::string &filePath)
{
	std::filesystem::path p(filePath);
	if (p.has_parent_path())
		std::filesystem::create_directories(p.parent_path());

	std::ofstream f(filePath);
	if (!f.is_open())
		throw std::runtime_error("writeSvgToFile: cannot open: " + filePath);
	f << toSvg(geom);
}
