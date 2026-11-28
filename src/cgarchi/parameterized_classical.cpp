#include <cgarchi/parameterized_classical.h>

#include <algorithm>
#include <array>
#include <exception>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <utility>

#include <cgarchi/classical_acanthus.h>
#include <cgarchi/classical_acanthus_detail.h>
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
	s.capitalOrnaments = std::clamp (s.capitalOrnaments, 0, kCapitalOrnamentsCount - 1);
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

// Conseil de la banniere de la colonnade sur Capital ornaments : un niveau de
// moins (Heavy : Light ; Light : None), si l'ornement du chapiteau est construit.
std::string CapitalOrnamentsAdvice (const classical::OrderData &order, const ClassicalColumnSettings &settings)
{
	if (order.order != classical::Order::Corinthian || !settings.volutes)
		return {};
	switch (static_cast<classical::OrnamentDetail> (settings.capitalOrnaments))
	{
	case classical::OrnamentDetail::Heavy:
		return ", ou regler Capital ornaments a Light";
	case classical::OrnamentDetail::Light:
		return ", ou regler Capital ornaments a None";
	default:
		return {};
	}
}

// Ajoute a `dest` les sommets (convertis en float) et les triangles de
// l'ornement, indices decales du nombre de sommets de `dest` : meme maillage
// que appendMesh (dest, *BuildCorinthianOrnament (...).mesh), sans construire
// le maillage intermediaire.
void AppendOrnament (Mesh &dest, const classical::detail::OrnamentGeometry &ornament)
{
	const unsigned destNV = dest.GetNVertices ();
	const unsigned destNF = dest.GetNFaces ();
	const std::size_t srcNV = ornament.positions.size ();
	const std::size_t srcNF = ornament.triangles.size ();
	std::vector<float> verts;
	verts.reserve (3 * (destNV + srcNV));
	for (unsigned i = 0; i < destNV; ++i)
	{
		float v[3];
		dest.GetVertex (i, v);
		verts.insert (verts.end (), {v[0], v[1], v[2]});
	}
	for (const std::array<double, 3> &p : ornament.positions)
		verts.insert (verts.end (), {static_cast<float> (p[0]), static_cast<float> (p[1]), static_cast<float> (p[2])});
	std::vector<unsigned> faces;
	faces.reserve (3 * (destNF + srcNF));
	for (unsigned i = 0; i < destNF; ++i)
		for (unsigned j = 0; j < 3; ++j)
			faces.push_back (static_cast<unsigned> (dest.GetFaceVertex (i, j)));
	for (const std::array<unsigned, 3> &t : ornament.triangles)
		faces.insert (faces.end (), {t[0] + destNV, t[1] + destNV, t[2] + destNV});
	dest.SetVertices (static_cast<unsigned> (destNV + srcNV), verts.data ());
	dest.SetFaces (static_cast<unsigned> (destNF + srcNF), 3, faces.data ());
}

} // namespace

std::unique_ptr<Mesh> BuildClassicalColumnMesh (const ClassicalColumnSettings &settings,
                                                classical::ColumnProfile *profileOut,
                                                std::vector<classical::PlanConflict> *conflictsOut)
{
	const classical::OrderData &order = CheckedOrder (settings, "BuildClassicalColumnMesh");
	if (settings.capitalOrnaments < 0 || settings.capitalOrnaments >= kCapitalOrnamentsCount)
		throw std::invalid_argument ("BuildClassicalColumnMesh : capitalOrnaments " +
		                             std::to_string (settings.capitalOrnaments) + " hors de [0, " +
		                             std::to_string (kCapitalOrnamentsCount) + ")");
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
	// « Volutes » porte tout l'ornement sculpte du chapiteau : decoche, le
	// corinthien est rendu par sa corbeille seule.
	if (order.order == classical::Order::Corinthian && settings.volutes &&
	    settings.capitalOrnaments != static_cast<int> (classical::OrnamentDetail::None))
	{
		classical::AcanthusOptions acanthus;
		acanthus.detail = static_cast<classical::OrnamentDetail> (settings.capitalOrnaments);
		acanthus.scale = lathe.scale;
		acanthus.radialSegments = lathe.radialSegments;
		AppendOrnament (*mesh, classical::detail::BuildCorinthianOrnamentGeometry (order, profile, acanthus));
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

ParameterizedClassicalColumn::ParameterizedClassicalColumn ()
{
	m_settings.capitalOrnaments = static_cast<int> (classical::OrnamentDetail::Light);
	Regenerate ();
}

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
		Parameter::MakeEnum ("Capital ornaments", &s.capitalOrnaments, {"None", "Light", "Heavy"}),
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

std::unique_ptr<Mesh> AssembleColonnadeMesh (const Mesh &column, const std::vector<double> &axes, double scale,
                                             const std::vector<const Mesh *> &rest)
{
	std::size_t vertices = axes.size () * column.GetNVertices (), faces = axes.size () * column.GetNFaces ();
	for (const Mesh *piece : rest)
	{
		vertices += piece->GetNVertices ();
		faces += piece->GetNFaces ();
	}
	std::vector<float> positions;
	std::vector<unsigned int> indices;
	positions.reserve (3 * vertices);
	indices.reserve (3 * faces);
	const auto appendFaces = [&indices] (const Mesh &piece, unsigned int offset) {
		for (unsigned int f = 0; f < piece.GetNFaces (); ++f)
			for (unsigned int k = 0; k < 3; ++k)
				indices.push_back (static_cast<unsigned int> (piece.GetFaceVertex (f, k)) + offset);
	};

	const std::vector<float> &source = column.GetVertices ();
	for (const double axis : axes)
	{
		const unsigned int offset = static_cast<unsigned int> (positions.size () / 3);
		const float dx = static_cast<float> (axis * scale);
		for (std::size_t i = 0; i < source.size (); i += 3)
		{
			positions.push_back (source[i] + dx);
			positions.push_back (source[i + 1]);
			positions.push_back (source[i + 2]);
		}
		appendFaces (column, offset);
	}
	for (const Mesh *piece : rest)
	{
		const unsigned int offset = static_cast<unsigned int> (positions.size () / 3);
		const std::vector<float> &p = piece->GetVertices ();
		positions.insert (positions.end (), p.begin (), p.end ());
		appendFaces (*piece, offset);
	}

	auto mesh = std::make_unique<Mesh> ();
	mesh->SetVertices (static_cast<unsigned int> (positions.size () / 3), positions.data ());
	mesh->SetFaces (static_cast<unsigned int> (indices.size () / 3), 3, indices.data ());
	return mesh;
}

ParameterizedClassicalColonnade::ParameterizedClassicalColonnade ()
{
	m_settings.capitalOrnaments = static_cast<int> (classical::OrnamentDetail::Light);
	Regenerate ();
}

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
		Parameter::MakeEnum ("Capital ornaments", &s.capitalOrnaments, {"None", "Light", "Heavy"}),
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

		classical::EntablatureOptions entablature;
		entablature.doricCornice = s.doricCornice ? classical::DoricCornice::Mutules : classical::DoricCornice::Dentils;
		entablature.ornaments = m_ornaments;
		entablature.curveSegments = s.curveSegments;
		const classical::ColumnProfile section = classical::BuildEntablatureSection (order, entablature);
		const std::unique_ptr<Mesh> swept = classical::SweepEntablature (section, layout.halfLength, scale, columnProfile.height);
		std::vector<const Mesh *> rest {swept.get ()};

		std::vector<std::pair<classical::OrnamentKind, std::unique_ptr<Mesh>>> ornaments;
		if (m_ornaments)
		{
			const auto placements = classical::PlaceEntablatureOrnaments (order, layout, section, entablature.doricCornice);
			// Gouttes : 6 cotes au moins, Curve segments au-dela.
			ornaments = classical::BuildOrnamentMeshes (order, layout, placements, columnProfile.height, scale, std::max (6, s.curveSegments));
			for (const auto &family : ornaments)
				rest.push_back (family.second.get ());
		}
		std::unique_ptr<Mesh> mesh = AssembleColonnadeMesh (*column, layout.axes, scale, rest);
		if (mesh->GetNFaces () > kColonnadeFaceWarning)
			m_warnings = std::to_string (mesh->GetNFaces ()) + " faces (au-dela de " + std::to_string (kColonnadeFaceWarning) +
			             ") : reduire Columns, Radial segments ou Curve segments, decocher Flutes" + CapitalOrnamentsAdvice (order, s);
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
