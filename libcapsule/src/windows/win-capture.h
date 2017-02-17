
#pragma once

// numbers of buffers used for async GPU download
#define NUM_BUFFERS 3

void d3d10_capture(void *swap_ptr, void *backbuffer_ptr);
void d3d10_free();

void d3d11_capture(void *swap_ptr, void *backbuffer_ptr);
void d3d11_free();
