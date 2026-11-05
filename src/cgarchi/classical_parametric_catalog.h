#pragma once

#include <vector>

#include "parametric_catalog.h"   // ParametricShapeEntry

namespace cgarchi
{

// Formes parametriques de l'architecture classique, sans ressource (les donnees
// des cinq ordres sont embarquees) : « Classical Column », puis « Classical
// Colonnade ». Elles ne figurent
// ni dans ParametricShapes () ni dans GothicParametricShapes () ; la reunion
// vit chez les consommateurs (cggraph_nodes : shapes/shape_catalog.h). Ordre
// stable.
const std::vector<ParametricShapeEntry> &ClassicalParametricShapes ();

} // namespace cgarchi
