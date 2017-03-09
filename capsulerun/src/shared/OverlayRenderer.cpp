
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

  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_paint(cr);

  cairo_set_line_width(cr, 15);
  cairo_set_source_rgb(cr, 255, 0, 0);
  cairo_move_to(cr, 0, -100);
  cairo_line_to(cr, 100, 100);
  cairo_rel_line_to(cr, -200, 0);
  cairo_close_path(cr);
  cairo_stroke(cr);

  cairo_surface_flush(surface);
  unsigned char *data = cairo_image_surface_get_data(surface);

  FILE *f = fopen("cairo.rawimg", "wb");
  fwrite(data, 256 * 4 * 128, 1, f);
  capsule_log("Wrote raw surface to disk!");
  fclose(f);
}