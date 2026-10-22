#include <cgarchi/gothic_parametric_catalog.h>

#include <memory>

#include <cgarchi/parameterized_gothic.h>

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

const std::vector<ParametricShapeEntry> &GothicParametricShapes ()
{
	static const std::vector<ParametricShapeEntry> entries = {
		{ "Gothic Window", &Make<ParameterizedGothicWindow> },
		{ "Gothic Block", &Make<ParameterizedGothicBlock> }
	};
	return entries;
}

} // namespace cgarchi
