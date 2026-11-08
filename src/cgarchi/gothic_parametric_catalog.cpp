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

const char *const kCategory = "gothic";

} // namespace

const std::vector<ParametricShapeEntry> &GothicParametricShapes ()
{
	static const std::vector<ParametricShapeEntry> entries = {
		{ "shape.gothic.window", "Gothic Window", kCategory, &Make<ParameterizedGothicWindow> },
		{ "shape.gothic.block", "Gothic Block", kCategory, &Make<ParameterizedGothicBlock> }
	};
	return entries;
}

} // namespace cgarchi
