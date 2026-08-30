#include "param_set.h"

namespace cggraph
{

ParamEntry *ParamSet::Touch (const std::string &name, ParamRole role, ParamVisibility visibility)
{
	if (name.empty ())
		return nullptr;

	for (ParamEntry &entry : m_entries)
		if (entry.name == name)
		{
			entry.role = role;
			entry.visibility = visibility;
			entry.value = ParamValue ();
			return &entry;
		}

	ParamEntry entry;
	entry.name = name;
	entry.role = role;
	entry.visibility = visibility;
	m_entries.push_back (entry);
	return &m_entries.back ();
}

ParamValue *ParamSet::SetInt (const std::string &name, int value, ParamRole role,
                              ParamVisibility visibility)
{
	ParamEntry *entry = Touch (name, role, visibility);
	if (entry == nullptr)
		return nullptr;
	entry->value.type = ParamType::Int;
	entry->value.intValue = value;
	return &entry->value;
}

ParamValue *ParamSet::SetFloat (const std::string &name, float value, ParamRole role,
                                ParamVisibility visibility)
{
	ParamEntry *entry = Touch (name, role, visibility);
	if (entry == nullptr)
		return nullptr;
	entry->value.type = ParamType::Float;
	entry->value.floatValue = value;
	return &entry->value;
}

ParamValue *ParamSet::SetBool (const std::string &name, bool value, ParamRole role,
                               ParamVisibility visibility)
{
	ParamEntry *entry = Touch (name, role, visibility);
	if (entry == nullptr)
		return nullptr;
	entry->value.type = ParamType::Bool;
	entry->value.boolValue = value;
	return &entry->value;
}

ParamValue *ParamSet::SetString (const std::string &name, const std::string &value, ParamRole role,
                                 ParamVisibility visibility)
{
	ParamEntry *entry = Touch (name, role, visibility);
	if (entry == nullptr)
		return nullptr;
	entry->value.type = ParamType::String;
	entry->value.stringValue = value;
	return &entry->value;
}

ParamValue *ParamSet::SetDriven (const std::string &name, ParamType type, const std::string &expression,
                                 ParamRole role, ParamVisibility visibility)
{
	ParamEntry *entry = Touch (name, role, visibility);
	if (entry == nullptr)
		return nullptr;
	entry->value.kind = ParamKind::Driven;
	entry->value.type = type;
	entry->value.expression = expression;
	return &entry->value;
}

bool ParamSet::UpdateString (const std::string &name, const std::string &value)
{
	for (ParamEntry &entry : m_entries)
		if (entry.name == name)
		{
			if (entry.value.kind != ParamKind::Literal || entry.value.type != ParamType::String)
				return false;
			entry.value.stringValue = value;
			return true;
		}
	return false;
}

void ParamSet::Clear ()
{
	m_entries.clear ();
}

const ParamValue *ParamSet::Find (const std::string &name) const
{
	for (const ParamEntry &entry : m_entries)
		if (entry.name == name)
			return &entry.value;
	return nullptr;
}

ParamValue *ParamSet::Find (const std::string &name)
{
	const ParamSet *self = this;
	return const_cast<ParamValue *> (self->Find (name));
}

const ParamEntry *ParamSet::FindEntry (const std::string &name) const
{
	for (const ParamEntry &entry : m_entries)
		if (entry.name == name)
			return &entry;
	return nullptr;
}

const ParamEntry *ParamSet::FindDriven () const
{
	for (const ParamEntry &entry : m_entries)
		if (entry.value.kind == ParamKind::Driven)
			return &entry;
	return nullptr;
}

} // namespace cggraph
