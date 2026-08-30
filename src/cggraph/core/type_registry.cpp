#include "type_registry.h"

namespace cggraph
{

const TypeDesc *TypeRegistry::Register (const TypeDesc &desc)
{
	if (desc.name.empty ())
		return nullptr;
	if (desc.mutability == TypeDesc::Forkable && desc.clone == nullptr)
		return nullptr;
	if (m_byName.find (desc.name) != m_byName.end ())
		return nullptr;

	m_storage.push_back (desc);
	const TypeDesc *stored = &m_storage.back ();
	m_byName[stored->name] = stored;
	return stored;
}

const TypeDesc *TypeRegistry::Find (const std::string &name) const
{
	auto it = m_byName.find (name);
	return (it == m_byName.end ()) ? nullptr : it->second;
}

} // namespace cggraph
