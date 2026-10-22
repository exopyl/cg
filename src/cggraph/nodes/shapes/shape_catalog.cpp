#include "shape_catalog.h"

#include <cgarchi/gothic_parametric_catalog.h>
#include "../../../cgmesh/parametric_catalog.h"

namespace cggraph_nodes
{

const std::vector<ParametricShapeEntry> &CatalogShapes ()
{
	static const std::vector<ParametricShapeEntry> entries = [] {
		const std::vector<ParametricShapeEntry> &generic = ParametricShapes ();
		const std::vector<ParametricShapeEntry> &gothic  = cgarchi::GothicParametricShapes ();
		std::vector<ParametricShapeEntry> all;
		all.reserve (generic.size () + gothic.size ());
		all.insert (all.end (), generic.begin (), generic.end ());
		all.insert (all.end (), gothic.begin (), gothic.end ());
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

} // namespace cggraph_nodes
