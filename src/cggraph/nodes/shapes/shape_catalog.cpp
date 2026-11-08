#include "shape_catalog.h"

#include <cgarchi/classical_parametric_catalog.h>
#include <cgarchi/gothic_parametric_catalog.h>
#include "../../../cgmesh/parametric_catalog.h"

namespace cggraph_nodes
{

const std::vector<ParametricShapeEntry> &CatalogShapes ()
{
	static const std::vector<ParametricShapeEntry> entries = [] {
		const std::vector<ParametricShapeEntry> &generic = ParametricShapes ();
		const std::vector<ParametricShapeEntry> &gothic  = cgarchi::GothicParametricShapes ();
		const std::vector<ParametricShapeEntry> &classic = cgarchi::ClassicalParametricShapes ();
		std::vector<ParametricShapeEntry> all;
		all.reserve (generic.size () + gothic.size () + classic.size ());
		all.insert (all.end (), generic.begin (), generic.end ());
		all.insert (all.end (), gothic.begin (), gothic.end ());
		all.insert (all.end (), classic.begin (), classic.end ());
		return all;
	}();
	return entries;
}

const ParametricShapeEntry *FindCatalogShape (const std::string &name)
{
	for (const ParametricShapeEntry &entry : CatalogShapes ())
		if (name == entry.name)
			return &entry;
	return nullptr;
}

std::unique_ptr<IParameterized> MakeCatalogShape (const std::string &name)
{
	const ParametricShapeEntry *entry = FindCatalogShape (name);
	return entry ? entry->make () : nullptr;
}

const ParametricShapeEntry *FindCatalogShapeById (const std::string &id)
{
	for (const ParametricShapeEntry &entry : CatalogShapes ())
		if (id == entry.id)
			return &entry;
	return nullptr;
}

} // namespace cggraph_nodes
