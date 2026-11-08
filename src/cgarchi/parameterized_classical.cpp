#include <cgarchi/parameterized_classical.h>

#include <algorithm>
#include <exception>
#include <string>
#include <memory>
#include <stdexcept>

#include <cgarchi/classical_colonnade.h>
#include <cgarchi/classical_entablature.h>
#include <cgarchi/classical_ionic_capital.h>
#include <cgarchi/classical_lathe.h>
#include <cgarchi/classical_orders.h>
#include "polygon_extrude.h"   // appendMesh

namespace cgarchi
{

namespace
{

// Ramene chaque reglage de valeur finie dans le domaine que son Parameter
// expose : enumerations dans leurs choix, Module, Radial segments et Curve
// segments dans leurs bornes. Un module NaN reste NaN (std::clamp).
void ClampClassicalColumnSettings (ClassicalColumnSettings &s)
{
	s.order = std::clamp (s.order, 0, classical::kOrderCount - 1);
	s.doricCornice = std::clamp (s.doricCornice, 0, 1);
	s.doricFlutes = std::clamp (s.doricFlutes, 0, 1);
	s.module = std::clamp (s.module, kClassicalModuleMin, kClassicalModuleMax);
	s.radialSegments = std::clamp (s.radialSegments, kClassicalRadialSegmentsMin, kClassicalRadialSegmentsMax);
	s.curveSegments = std::clamp (s.curveSegments, kClassicalCurveSegmentsMin, kClassicalCurveSegmentsMax);
}

// Donnees de l'ordre de `settings` ; leve std::invalid_argument, au nom de
// `caller`, si settings.order est hors de [0, kOrderCount).
const classical::OrderData &CheckedOrder (const ClassicalColumnSettings &settings, const char *caller)
{
	if (settings.order < 0 || settings.order >= classical::kOrderCount)
		throw std::invalid_argument (std::string (caller) + " : order " + std::to_string (settings.order) +
		                             " hors de [0, " + std::to_string (classical::kOrderCount) + ")");
	return classical::ClassicalOrders ().Get (static_cast<classical::Order> (settings.order));
}

// Options du profil de la colonne, communes au maillage et aux cotes.
classical::ColumnOptions ColumnOptionsOf (const classical::OrderData &order, const ClassicalColumnSettings &settings)
{
	classical::ColumnOptions options;
	options.order = order.order;
	options.pedestal = settings.pedestal;
	options.entablature = settings.entablature;
	options.doricCornice = settings.doricCornice ? classical::DoricCornice::Mutules : classical::DoricCornice::Dentils;
	options.ornamentEnvelope = settings.envelope;
	options.curveSegments = settings.curveSegments;
	options.shaftSegments = 4 * settings.curveSegments;
	options.ionicVolutes = settings.volutes && order.volute.has_value ();
	options.flutes = settings.flutes;
	options.doricFlutes = settings.doricFlutes ? classical::FluteSection::Semicircle : classical::FluteSection::Segment60;
	if (order.flutes)
	{
		// Segments d'arc par demi-cannelure : au moins 6 (ecart radial < 5 %
		// de la profondeur, aire creusee a moins de 1 %), au moins
		// curveSegments, et pas plus grossier que la grille reguliere.
		const int count = order.flutes->count;
		options.fluteSamples = std::max ({6, settings.curveSegments, (settings.radialSegments + 2 * count - 1) / (2 * count)});
	}
	return options;
}

// Unites monde par partie de module, celles du tour. Quotient float / int,
// arrondi en float (ecart relatif au quotient exact d'au plus 2^-24) : les cotes
// prennent la meme echelle que les sommets.
double LatheScale (const classical::OrderData &order, const ClassicalColumnSettings &settings)
{
	return settings.module / order.partsPerModule;
}

} // namespace

std::unique_ptr<Mesh> BuildClassicalColumnMesh (const ClassicalColumnSettings &settings,
                                                classical::ColumnProfile *profileOut,
                                                std::vector<classical::PlanConflict> *conflictsOut)
{
	const classical::OrderData &order = CheckedOrder (settings, "BuildClassicalColumnMesh");
	const classical::ColumnOptions options = ColumnOptionsOf (order, settings);
	const classical::ColumnProfile profile = classical::BuildColumnProfile (order, options);
	if (conflictsOut)
		*conflictsOut = classical::FindPlanConflicts (profile);

	classical::LatheOptions lathe;
	lathe.radialSegments = settings.radialSegments;
	lathe.scale = LatheScale (order, settings);
	std::unique_ptr<Mesh> mesh = classical::LatheColumnProfile (profile, lathe);
	if (options.ionicVolutes)
	{
		classical::VoluteOptions volute;
		volute.segmentsPerQuarter = settings.curveSegments;
		volute.scale = lathe.scale;
		for (const std::unique_ptr<Mesh> &bolster : classical::BuildIonicVolutes (order, profile, volute))
			appendMesh (*mesh, *bolster);
	}
	if (profileOut)
		*profileOut = profile;
	return mesh;
}

ClassicalColumnLevels ComputeClassicalColumnLevels (const ClassicalColumnSettings &settings)
{
	const classical::OrderData &order = CheckedOrder (settings, "ComputeClassicalColumnLevels");
	const classical::ColumnProfile profile = classical::BuildColumnProfile (order, ColumnOptionsOf (order, settings));
	const double scale = LatheScale (order, settings);

	const classical::ProfileMember *baseFirst = nullptr, *capitalFirst = nullptr, *capitalLast = nullptr,
	                               *architrave = nullptr;
	for (const classical::ProfileMember &member : profile.members)
	{
		if (member.block == "base" && !baseFirst)
			baseFirst = &member;
		else if (member.block == "chapiteau")
		{
			if (!capitalFirst)
				capitalFirst = &member;
			capitalLast = &member;
		}
		else if (member.block == "architrave" && !architrave)
			architrave = &member;
	}
	if (profile.vertices.empty ())
		throw std::runtime_error ("ComputeClassicalColumnLevels : profil sans sommet : " + order.key);
	if (!baseFirst || !capitalFirst || (settings.entablature && !architrave))
		throw std::runtime_error ("ComputeClassicalColumnLevels : profil sans base, sans chapiteau ou sans architrave : " +
		                          order.key);

	ClassicalColumnLevels levels;
	levels.foot = profile.vertices.front ().y * scale;
	if (settings.pedestal)
		levels.pedestalTop = baseFirst->y0 * scale;
	levels.shaftFoot = profile.shaft.foot * scale;
	levels.shaftTop = profile.shaft.top * scale;
	levels.capitalBottom = capitalFirst->y0 * scale;
	levels.capitalTop = capitalLast->y1 * scale;
	levels.top = profile.height * scale;
	if (settings.entablature)
		levels.entablatureFoot = architrave->y0 * scale;
	return levels;
}

ParameterizedClassicalColumn::ParameterizedClassicalColumn () { Regenerate (); }

std::vector<Parameter> ParameterizedClassicalColumn::GetParameters ()
{
	ClassicalColumnSettings &s = m_settings;
	return {
		Parameter::MakeEnum ("Order", &s.order, {"Tuscan", "Doric", "Ionic", "Corinthian", "Composite"}),
		Parameter::MakeFloat ("Module", &s.module, kClassicalModuleMin, kClassicalModuleMax),
		Parameter::MakeBool ("Pedestal", &s.pedestal),
		Parameter::MakeBool ("Entablature", &s.entablature),
		Parameter::MakeEnum ("Doric cornice", &s.doricCornice, {"Dentils", "Mutules"}),
		Parameter::MakeBool ("Ornament envelope", &s.envelope),
		Parameter::MakeBool ("Volutes", &s.volutes),
		Parameter::MakeBool ("Flutes", &s.flutes),
		Parameter::MakeEnum ("Doric flutes", &s.doricFlutes, {"Segment (A)", "Semicircle (B)"}),
		Parameter::MakeInt ("Radial segments", &s.radialSegments, kClassicalRadialSegmentsMin, kClassicalRadialSegmentsMax),
		Parameter::MakeInt ("Curve segments", &s.curveSegments, kClassicalCurveSegmentsMin, kClassicalCurveSegmentsMax),
	};
}

void ParameterizedClassicalColumn::Regenerate ()
{
	ClampClassicalColumnSettings (m_settings);

	delete m_pMesh;
	m_pMesh = nullptr;
	m_lastError.clear ();
	try
	{
		m_pMesh = BuildClassicalColumnMesh (m_settings, &m_profile, &m_conflicts).release ();
	}
	catch (const std::exception &e)
	{
		m_lastError = e.what ();
		m_profile = {};
		m_conflicts.clear ();
		m_pMesh = new Mesh ();
	}
	m_pMesh->ComputeNormals ();
}

ParameterizedClassicalColonnade::ParameterizedClassicalColonnade () { Regenerate (); }

std::vector<Parameter> ParameterizedClassicalColonnade::GetParameters ()
{
	ClassicalColumnSettings &s = m_settings;
	return {
		Parameter::MakeEnum ("Order", &s.order, {"Tuscan", "Doric", "Ionic", "Corinthian", "Composite"}),
		Parameter::MakeFloat ("Module", &s.module, kClassicalModuleMin, kClassicalModuleMax),
		Parameter::MakeInt ("Columns", &m_columns, 1, 12),
		Parameter::MakeBool ("Pedestal", &s.pedestal),
		Parameter::MakeEnum ("Doric cornice", &s.doricCornice, {"Dentils", "Mutules"}),
		Parameter::MakeBool ("Ornaments", &m_ornaments),
		Parameter::MakeBool ("Ornament envelope", &s.envelope),
		Parameter::MakeBool ("Volutes", &s.volutes),
		Parameter::MakeBool ("Flutes", &s.flutes),
		Parameter::MakeEnum ("Doric flutes", &s.doricFlutes, {"Segment (A)", "Semicircle (B)"}),
		Parameter::MakeInt ("Radial segments", &s.radialSegments, kClassicalRadialSegmentsMin, kClassicalRadialSegmentsMax),
		Parameter::MakeInt ("Curve segments", &s.curveSegments, kClassicalCurveSegmentsMin, kClassicalCurveSegmentsMax),
	};
}

void ParameterizedClassicalColonnade::Regenerate ()
{
	ClassicalColumnSettings &s = m_settings;
	ClampClassicalColumnSettings (s);
	s.entablature = false;
	m_columns = std::clamp (m_columns, 1, 12);

	delete m_pMesh;
	m_pMesh = nullptr;
	m_lastError.clear ();
	m_warnings.clear ();
	try
	{
		const classical::OrderData &order = classical::ClassicalOrders ().Get (static_cast<classical::Order> (s.order));
		const double scale = LatheScale (order, s);
		classical::ColumnProfile columnProfile;
		const std::unique_ptr<Mesh> column = BuildClassicalColumnMesh (s, &columnProfile);
		const classical::ColonnadeLayout layout = classical::LayoutColonnade (order, m_columns);

		auto mesh = std::make_unique<Mesh> ();
		for (double axis : layout.axes)
		{
			Mesh copy (*column);
			for (unsigned i = 0; i < copy.GetNVertices (); ++i)
			{
				float v[3];
				copy.GetVertex (i, v);
				copy.SetVertex (i, v[0] + static_cast<float> (axis * scale), v[1], v[2]);
			}
			appendMesh (*mesh, copy);
		}

		classical::EntablatureOptions entablature;
		entablature.doricCornice = s.doricCornice ? classical::DoricCornice::Mutules : classical::DoricCornice::Dentils;
		entablature.ornaments = m_ornaments;
		entablature.curveSegments = s.curveSegments;
		const classical::ColumnProfile section = classical::BuildEntablatureSection (order, entablature);
		appendMesh (*mesh, *classical::SweepEntablature (section, layout.halfLength, scale, columnProfile.height));

		if (m_ornaments)
		{
			const auto placements = classical::PlaceEntablatureOrnaments (order, layout, section, entablature.doricCornice);
			// Gouttes : 6 cotes au moins, Curve segments au-dela.
			for (const auto &family : classical::BuildOrnamentMeshes (order, layout, placements, columnProfile.height, scale,
			                                                          std::max (6, s.curveSegments)))
				appendMesh (*mesh, *family.second);
		}
		if (mesh->GetNFaces () > kColonnadeFaceWarning)
			m_warnings = std::to_string (mesh->GetNFaces ()) + " faces (au-dela de " + std::to_string (kColonnadeFaceWarning) +
			             ") : reduire Columns, Radial segments ou Curve segments, ou decocher Flutes";
		m_pMesh = mesh.release ();
	}
	catch (const std::exception &e)
	{
		m_lastError = e.what ();
		m_pMesh = new Mesh ();
	}
	m_pMesh->ComputeNormals ();
}

} // namespace cgarchi
