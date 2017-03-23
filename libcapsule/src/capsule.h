
#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <lab/platform.h>

#include "logging.h"
#include "dynlib.h"
#include "capsule/constants.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LAB_WINDOWS

void CapsuleInstallWindowsHooks();
void CapsuleInstallProcessHooks();
void CapsuleInstallOpenglHooks();
void CapsuleInstallDxgiHooks();
void CapsuleInstallD3d9Hooks();

bool DcCaptureInit();

#endif // LAB_WINDOWS

#ifdef __cplusplus
}
#endif

#ifdef LAB_WINDOWS
// Deviare-InProc, for hooking
#include <NktHookLib.h>

// LoadLibrary, etc.
#define WIN32_WIN32_LEAN_AND_MEAN
#include <windows.h>

extern CNktHookLib cHookMgr;
#endif // LAB_WINDOWS
