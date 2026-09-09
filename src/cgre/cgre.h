#pragma once

//
// En-tete parapluie de cgre.
//
// N'INCLURE ICI QUE DES EN-TETES DONT LE .cpp EST BATI. La version historique
// exposait console.h, viewer3D_core.h et quatre examinators dont les sources
// etaient commentees dans CMakeLists.txt : les declarations etaient visibles
// depuis sinaia, les definitions absentes de la bibliotheque. Un `new Cfly ()`
// ou un `Console::getInstance ()` compilait donc proprement et echouait a
// l'edition de liens sur un LNK2019 sans rapport apparent avec la cause. Le
// module annoncait cinq cameras et n'en fournissait qu'une.
//

#include "mesh_renderer.h"
#include "widgets_renderer.h"
#include "capabilities_manager.h"
#include "material_renderer.h"

// diagnostic : journalisation vers l'hote et controle d'erreur GL
#include "diagnostics.h"

// shaders
#include "gl_program.h"
#include "surface_program.h"

// camera
#include "examinator_trackball.h"
