#pragma once

// Outils de test de la colonnade partages par K62 (tu_cgarchi_classical_colonnade.cpp) et par l'instrument de cout
// (tu_cgarchi_classical_colonnade_cost.cpp) : pieces de la colonnade telles que
// ParameterizedClassicalColonnade::Regenerate les construit avec ses reglages par defaut, et assemblage de reference
// par appendMesh successifs.

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include <cgarchi/classical_acanthus.h>
#include <cgarchi/classical_colonnade.h>
#include <cgarchi/classical_entablature.h>
#include <cgarchi/classical_orders.h>
#include <cgarchi/parameterized_classical.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/polygon_extrude.h"

namespace colonnade_test
{

struct ColonnadePieces
{
	std::unique_ptr<Mesh> column;
	std::vector<double> axes;
	double scale = 1.0;
	std::unique_ptr<Mesh> entablature;
	std::vector<std::pair<cgarchi::classical::OrnamentKind, std::unique_ptr<Mesh>>> ornaments;

	// Entablement balaye, puis les familles d'ornements, dans l'ordre de BuildOrnamentMeshes.
	std::vector<const Mesh *> Rest () const
	{
		std::vector<const Mesh *> rest {entablature.get ()};
		for (const auto &family : ornaments)
			rest.push_back (family.second.get ());
		return rest;
	}
};

// Reglages par defaut de la colonnade (ornements d'entablement, Capital ornaments Light) ; `level` >= 0 remplace
// Capital ornaments.
inline ColonnadePieces MakeColonnadePieces (int order, int columns, int level = -1)
{
	using namespace cgarchi::classical;
	cgarchi::ClassicalColumnSettings s;
	s.order = order;
	s.capitalOrnaments = level >= 0 ? level : static_cast<int> (OrnamentDetail::Light);
	s.entablature = false;
	const OrderData &data = ClassicalOrders ().Get (static_cast<Order> (order));
	ColonnadePieces p;
	p.scale = s.module / data.partsPerModule;
	ColumnProfile profile;
	p.column = cgarchi::BuildClassicalColumnMesh (s, &profile);
	const ColonnadeLayout layout = LayoutColonnade (data, columns);
	p.axes = layout.axes;
	EntablatureOptions entablature;
	entablature.doricCornice = s.doricCornice ? DoricCornice::Mutules : DoricCornice::Dentils;
	entablature.ornaments = true;
	entablature.curveSegments = s.curveSegments;
	const ColumnProfile section = BuildEntablatureSection (data, entablature);
	p.entablature = SweepEntablature (section, layout.halfLength, p.scale, profile.height);
	const auto placements = PlaceEntablatureOrnaments (data, layout, section, entablature.doricCornice);
	p.ornaments = BuildOrnamentMeshes (data, layout, placements, profile.height, p.scale, std::max (6, s.curveSegments));
	return p;
}

// Assemblage de reference : une copie translatee de la colonne par axe, appendMesh par appendMesh, puis les autres
// pieces.
inline std::unique_ptr<Mesh> ReferenceAssembly (const ColonnadePieces &p)
{
	auto mesh = std::make_unique<Mesh> ();
	for (const double axis : p.axes)
	{
		Mesh copy (*p.column);
		for (unsigned i = 0; i < static_cast<unsigned> (copy.GetNVertices ()); ++i)
		{
			float v[3];
			copy.GetVertex (i, v);
			copy.SetVertex (i, v[0] + static_cast<float> (axis * p.scale), v[1], v[2]);
		}
		appendMesh (*mesh, copy);
	}
	for (const Mesh *piece : p.Rest ())
		appendMesh (*mesh, *piece);
	return mesh;
}

} // namespace colonnade_test
