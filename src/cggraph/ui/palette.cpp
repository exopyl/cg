#include "palette.h"

#include "../nodes/catalog.h"

namespace cggraph_ui
{

const char *const kUncategorized = "Sans categorie";

Palette::Palette ()
{
	for (const cggraph_nodes::CatalogEntry &entry : cggraph_nodes::Catalog ())
	{
		const char *category = entry.category != nullptr ? entry.category : kUncategorized;

		PaletteCategory *group = nullptr;
		for (PaletteCategory &candidate : m_categories)
			if (candidate.name == category)
			{
				group = &candidate;
				break;
			}

		if (group == nullptr)
		{
			PaletteCategory created;
			created.name = category;
			m_categories.push_back (created);
			group = &m_categories.back ();
		}

		PaletteItem item;
		item.typeName = entry.typeName;
		item.label = entry.label != nullptr ? entry.label : entry.typeName;
		item.caveat = entry.caveat;
		group->items.push_back (item);
	}
}

std::size_t Palette::GetItemCount () const
{
	std::size_t count = 0;
	for (const PaletteCategory &category : m_categories)
		count += category.items.size ();
	return count;
}

const PaletteItem *Palette::Find (const std::string &typeName) const
{
	for (const PaletteCategory &category : m_categories)
		for (const PaletteItem &item : category.items)
			if (typeName == item.typeName)
				return &item;
	return nullptr;
}

} // namespace cggraph_ui
