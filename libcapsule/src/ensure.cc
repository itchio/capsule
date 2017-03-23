
#include "ensure.h"
 
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "logging.h"

namespace capsule {

void Ensure(const char *msg, int cond) {
  if (cond) {
    return;
  }
  Log("Assertion failed: %s", msg);
  exit(1);
}

} // namespace capsule
