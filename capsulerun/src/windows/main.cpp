
#include <stdio.h>
#include <NktHookLib.h>

int capsulerun_main (int argc, char **argv) {
  printf("thanks for flying capsule on Windows\n");

  DWORD pid = NktHookLibHelpers::GetCurrentProcessId();
  printf("capsulerun pid: %d", (int) pid);

  return 0;
}
