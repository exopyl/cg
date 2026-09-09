#pragma once

// _WIN32 et non WIN32 : seul le premier est predefini par le compilateur. Le
// second n'existe que parce que CMake l'injecte dans CMAKE_CXX_FLAGS, variable
// que ce depot ECRASE integralement sous ENABLE_COVERAGE.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define RELEASED		0
#define PRESSED			1

#define LEFT_BUTTON         0
#define RIGHT_BUTTON        1
#define MIDDLE_BUTTON       2

// glad/wgl.h declare l'interface WGL, propre a Windows : elle n'a pas
// d'equivalent ailleurs et ses en-tetes exigent <windows.h>.
#ifdef _WIN32
#include "glad/wgl.h"
#endif
#include "glad/gl.h"
