
#include "../src/shared/io.h" // oh no.

typedef struct encoder_private_s {
  capsule_io_t *io;
} encoder_private_t;

int capsule_hotkey_init(encoder_private_t *p);
void capsule_run_app();

