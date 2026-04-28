#pragma once

#include "version.h"

#define STR(x) #x
#define XSTR(x) STR(x)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define countof(x) (sizeof(x) / sizeof(x[0]))
