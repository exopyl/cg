#include <cgarchi/classical_parametric_catalog.h>

#include <memory>

#include <cgarchi/parameterized_classical.h>

namespace cgarchi
{

namespace
{

template <class T>
std::unique_ptr<IParameterized> Make ()
{
	return std::unique_ptr<IParameterized> (new T ());
}

} // namespace

const std::vector<ParametricShapeEntry> &ClassicalParametricShapes ()
{
	static const std::vector<ParametricShapeEntry> entries = {
		{ "Classical Column", &Make<ParameterizedClassicalColumn> },
		{ "Classical Colonnade", &Make<ParameterizedClassicalColonnade> }
	};
	return entries;
}

} // namespace cgarchi
