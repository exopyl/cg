#include <cgarchi/parameterized_classical.h>

#include <algorithm>
#include <exception>
#include <string>
#include <memory>

#include <cgarchi/classical_colonnade.h>
#include <cgarchi/classical_entablature.h>
#include <cgarchi/classical_ionic_capital.h>
#include <cgarchi/classical_lathe.h>
#include <cgarchi/classical_orders.h>
#include "polygon_extrude.h"   // appendMesh

namespace cgarchi
{

std::unique_ptr<Mesh> BuildClassicalColumnMesh (const ClassicalColumnSettings &settings,
                                                classical::ColumnProfile *profileOut,
                                                std::vector<classical::PlanConflict> *conflictsOut)
{
	const classical::OrderData &order = classical::ClassicalOrders ().Get (static_cast<classical::Order> (settings.order));

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
	const classical::ColumnProfile profile = classical::BuildColumnProfile (order, options);
	if (conflictsOut)
		*conflictsOut = classical::FindPlanConflicts (profile);

	classical::LatheOptions lathe;
	lathe.radialSegments = settings.radialSegments;
	lathe.scale = settings.module / order.partsPerModule;
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

ParameterizedClassicalColumn::ParameterizedClassicalColumn () { Regenerate (); }

std::vector<Parameter> ParameterizedClassicalColumn::GetParameters ()
{
	return {
		Parameter::MakeEnum ("Order", &m_order, {"Tuscan", "Doric", "Ionic", "Corinthian", "Composite"}),
		Parameter::MakeFloat ("Module", &m_module, 0.1f, 10.f),
		Parameter::MakeBool ("Pedestal", &m_pedestal),
		Parameter::MakeBool ("Entablature", &m_entablature),
		Parameter::MakeEnum ("Doric cornice", &m_doricCornice, {"Dentils", "Mutules"}),
		Parameter::MakeBool ("Ornament envelope", &m_envelope),
		Parameter::MakeBool ("Volutes", &m_volutes),
		Parameter::MakeBool ("Flutes", &m_flutes),
		Parameter::MakeEnum ("Doric flutes", &m_doricFlutes, {"Segment (A)", "Semicircle (B)"}),
		Parameter::MakeInt ("Radial segments", &m_radialSegments, 8, 256),
		Parameter::MakeInt ("Curve segments", &m_curveSegments, 2, 32),
	};
}

void ParameterizedClassicalColumn::Regenerate ()
{
	m_order = std::clamp (m_order, 0, classical::kOrderCount - 1);
	m_doricCornice = std::clamp (m_doricCornice, 0, 1);
	m_doricFlutes = std::clamp (m_doricFlutes, 0, 1);
	m_module = std::clamp (m_module, 0.1f, 10.f);
	m_radialSegments = std::clamp (m_radialSegments, 8, 256);
	m_curveSegments = std::clamp (m_curveSegments, 2, 32);

	delete m_pMesh;
	m_pMesh = nullptr;
	m_lastError.clear ();
	try
	{
		ClassicalColumnSettings settings;
		settings.order = m_order;
		settings.module = m_module;
		settings.pedestal = m_pedestal;
		settings.entablature = m_entablature;
		settings.doricCornice = m_doricCornice;
		settings.envelope = m_envelope;
		settings.volutes = m_volutes;
		settings.flutes = m_flutes;
		settings.doricFlutes = m_doricFlutes;
		settings.radialSegments = m_radialSegments;
		settings.curveSegments = m_curveSegments;
		m_pMesh = BuildClassicalColumnMesh (settings, &m_profile, &m_conflicts).release ();
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
		Parameter::MakeFloat ("Module", &s.module, 0.1f, 10.f),
		Parameter::MakeInt ("Columns", &m_columns, 1, 12),
		Parameter::MakeBool ("Pedestal", &s.pedestal),
		Parameter::MakeEnum ("Doric cornice", &s.doricCornice, {"Dentils", "Mutules"}),
		Parameter::MakeBool ("Ornaments", &m_ornaments),
		Parameter::MakeBool ("Ornament envelope", &s.envelope),
		Parameter::MakeBool ("Volutes", &s.volutes),
		Parameter::MakeBool ("Flutes", &s.flutes),
		Parameter::MakeEnum ("Doric flutes", &s.doricFlutes, {"Segment (A)", "Semicircle (B)"}),
		Parameter::MakeInt ("Radial segments", &s.radialSegments, 8, 256),
		Parameter::MakeInt ("Curve segments", &s.curveSegments, 2, 32),
	};
}

void ParameterizedClassicalColonnade::Regenerate ()
{
	ClassicalColumnSettings &s = m_settings;
	s.order = std::clamp (s.order, 0, classical::kOrderCount - 1);
	s.doricCornice = std::clamp (s.doricCornice, 0, 1);
	s.doricFlutes = std::clamp (s.doricFlutes, 0, 1);
	s.module = std::clamp (s.module, 0.1f, 10.f);
	s.radialSegments = std::clamp (s.radialSegments, 8, 256);
	s.curveSegments = std::clamp (s.curveSegments, 2, 32);
	s.entablature = false;
	m_columns = std::clamp (m_columns, 1, 12);

	delete m_pMesh;
	m_pMesh = nullptr;
	m_lastError.clear ();
	m_warnings.clear ();
	try
	{
		const classical::OrderData &order = classical::ClassicalOrders ().Get (static_cast<classical::Order> (s.order));
		const double scale = s.module / order.partsPerModule;
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
