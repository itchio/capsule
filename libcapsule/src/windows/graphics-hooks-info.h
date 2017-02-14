#pragma once

// original code: https://github.com/jp9000/obs-studio

struct dxgi_offsets {
	uint32_t present;
	uint32_t resize;
};

struct graphics_offsets {
  struct dxgi_offsets dxgi;
};

struct hook_info {
  /** hooks addresses */
  struct graphics_offsets offsets;
};
