
#include "OverlayRenderer.h"

#include <stdio.h>
#include <capsulerun_macros.h>

OverlayRenderer::OverlayRenderer() {
  capsule_log("Hello from OverlayRenderer!");

  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_status_t status;

  surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 256, 128);
  status = cairo_surface_status(surface);
  if (status != CAIRO_STATUS_SUCCESS) {
    capsule_log("Could not create cairo surface: error %d", status);
  }

  cr = cairo_create(surface);
  status = cairo_surface_status(surface);
  if (status != CAIRO_STATUS_SUCCESS) {
    capsule_log("Could not create cairo ceontext: error %d", status);
  }
}