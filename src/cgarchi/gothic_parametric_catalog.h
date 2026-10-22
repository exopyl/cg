#pragma once

#include <vector>

#include "parametric_catalog.h"   // ParametricShapeEntry

namespace cgarchi
{

// Formes parametriques gothiques sans ressource : « Gothic Window », puis
// « Gothic Block ». Elles ne figurent pas dans ParametricShapes () ; la
// recherche qui reunit les deux listes vit chez leurs consommateurs
// (cggraph_nodes : shapes/shape_catalog.h). Ordre stable.
const std::vector<ParametricShapeEntry> &GothicParametricShapes ();

} // namespace cgarchi
