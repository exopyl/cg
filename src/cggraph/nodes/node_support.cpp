#include "node_support.h"

namespace cggraph_nodes
{

namespace
{

const cggraph::ParamValue *Literal (const cggraph::ParamSet &params, const std::string &name,
                                    cggraph::ParamType type)
{
	const cggraph::ParamEntry *entry = params.FindEntry (name);
	if (entry == nullptr || entry->visibility != cggraph::ParamVisibility::Public)
		return nullptr;
	if (entry->value.kind != cggraph::ParamKind::Literal || entry->value.type != type)
		return nullptr;
	return &entry->value;
}

} // namespace

int GetInt (const cggraph::ParamSet &params, const std::string &name, int fallback)
{
	const cggraph::ParamValue *value = Literal (params, name, cggraph::ParamType::Int);
	return value != nullptr ? value->intValue : fallback;
}

float GetFloat (const cggraph::ParamSet &params, const std::string &name, float fallback)
{
	const cggraph::ParamValue *value = Literal (params, name, cggraph::ParamType::Float);
	return value != nullptr ? value->floatValue : fallback;
}

bool GetBool (const cggraph::ParamSet &params, const std::string &name, bool fallback)
{
	const cggraph::ParamValue *value = Literal (params, name, cggraph::ParamType::Bool);
	return value != nullptr ? value->boolValue : fallback;
}

std::string GetString (const cggraph::ParamSet &params, const std::string &name,
                       const std::string &fallback)
{
	const cggraph::ParamValue *value = Literal (params, name, cggraph::ParamType::String);
	return value != nullptr ? value->stringValue : fallback;
}

} // namespace cggraph_nodes
