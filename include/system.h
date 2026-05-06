#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include <intuition/intuition.h>

#include "buffer.h"

// Performs low-level system initialization sequence
//  - program argument preparation / parsing,
//  - executable path reconstruction,
//  - tooset parameters load
//  - yada, yada, yada, ...
bool sys_init();
void sys_cleanup();
const char *sys_commandline();

// @returns `true` if running on AGA machine
bool sys_isaga();
bool sys_isdisabled();
bool sys_isforbidden();
void sys_setfilter(bool enabled);
void sys_vblank();

uint32_t sys_getpath(BPTR lock, buffer_t *buffer);
const char *sys_workdirpath();
const char *sys_exepath();
