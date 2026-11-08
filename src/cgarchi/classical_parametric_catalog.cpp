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

const char *const kCategory = "classical";

} // namespace

const std::vector<ParametricShapeEntry> &ClassicalParametricShapes ()
{
	static const std::vector<ParametricShapeEntry> entries = {
		{ "shape.classical.column", "Classical Column", kCategory, &Make<ParameterizedClassicalColumn> },
		{ "shape.classical.colonnade", "Classical Colonnade", kCategory, &Make<ParameterizedClassicalColonnade> }
	};
	return entries;
}

} // namespace cgarchi
