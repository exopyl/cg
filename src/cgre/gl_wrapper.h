#pragma once

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define RELEASED		0
#define PRESSED			1

#define LEFT_BUTTON         0
#define RIGHT_BUTTON        1
#define MIDDLE_BUTTON       2

#include "glad/wgl.h"
#include "glad/gl.h"
