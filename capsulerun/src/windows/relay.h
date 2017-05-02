
#pragma once

#include "windows.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct RelayArgs {
  int bufsize;
  HANDLE in;
  HANDLE out;
} RelayArgs;

void createChildPipe (HANDLE *lpH_Rd, HANDLE *lpH_Wr);

unsigned int __stdcall relayThread(void *args);

#ifdef __cplusplus
}
#endif // __cplusplus
