
#include "relay.h"
#include <stdio.h>

void createChildPipe(HANDLE *lpH_Rd, HANDLE *lpH_Wr) {
  SECURITY_ATTRIBUTES saAttr;

  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = NULL;

  if (!CreatePipe(lpH_Rd, lpH_Wr, &saAttr, 0)) {
    fprintf(stderr, "Could not create pipe");
    fflush(stderr);
    exit(132);
  }

  if (!SetHandleInformation(*lpH_Rd, HANDLE_FLAG_INHERIT, 0)) {
    fprintf(stderr, "Could not set handle information");
    fflush(stderr);
    exit(132);
  }
}

unsigned int __stdcall relayThread(void *argptr) {
  RelayArgs *args = argptr;

  DWORD dwRead, dwWritten; 
  CHAR *chBuf = (CHAR *) calloc(args->bufsize, 1);
  BOOL bSuccess = TRUE;

  while (bSuccess) {
    bSuccess = ReadFile(args->in, chBuf, args->bufsize, &dwRead, NULL);
    if (bSuccess && dwRead > 0) {
      bSuccess = WriteFile(args->out, chBuf, dwRead, &dwWritten, NULL);
      if (!bSuccess) {
        break;
      }
    }
  }

  return 0;
}
