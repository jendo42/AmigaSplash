#pragma once

#ifdef NLOG
	#define LOG_DEBUG(x, ...)
#else
	#include <stdio.h>
	#define LOG_DEBUG(x, ...) printf("DEBUG: " x "\n", ##__VA_ARGS__)
#endif
