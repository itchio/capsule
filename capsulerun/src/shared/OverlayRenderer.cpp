
#include "OverlayRenderer.h"

#include <stdio.h>
#include <capsulerun_macros.h>

#include <cairo-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <string>

using namespace std;

OverlayRenderer::OverlayRenderer(capsule_args_t *args_in) {
  capsule_log("Hello from OverlayRenderer!");

  args = args_in;

  FT_Library ftlib;
  FT_Error ftstatus;
  FT_Face ftface;

  cairo_font_face_t *ct;
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_status_t status;

  capsule_log("libpath: %s", args->libpath);
  string filename = string(args->libpath);
#ifdef WIN32
  filename.append("\\");
#else
  filename.append("/");
#endif
  filename.append("Lato-Regular.ttf");
  capsule_log("filename: %s", filename.c_str());

  ftstatus = FT_Init_FreeType(&ftlib);
  if (ftstatus != 0) {
    capsule_log("Could not open freetype: %d", ftstatus);
  }

  ftstatus = FT_New_Face(ftlib, filename.c_str(), 0, &ftface);
  if (ftstatus != 0) {
    capsule_log("Could not open font face: %d", ftstatus);
  }

  ct = cairo_ft_font_face_create_for_ft_face(ftface, 0);

  surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 256, 128);
  status = cairo_surface_status(surface);
  if (status != CAIRO_STATUS_SUCCESS) {
    capsule_log("Could not create cairo surface: error %d", status);
    return;
  }

  cr = cairo_create(surface);
  status = cairo_surface_status(surface);
  if (status != CAIRO_STATUS_SUCCESS) {
    capsule_log("Could not create cairo ceontext: error %d", status);
    return;
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

  cairo_set_source_rgb(cr, 0, 255, 0);
  cairo_set_font_face(cr, ct);
  cairo_set_font_size(cr, 18.0);
  cairo_move_to(cr, 12, 64);
  cairo_show_text(cr, "recording @ 60FPS");

  cairo_surface_flush(surface);
  unsigned char *data = cairo_image_surface_get_data(surface);

  FILE *f = fopen("cairo.rawimg", "wb");
  fwrite(data, 256 * 4 * 128, 1, f);
  capsule_log("Wrote raw surface to disk!");
  fclose(f);
}