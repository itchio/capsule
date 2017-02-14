
#pragma once

// original code: https://github.com/jp9000/obs-studio

#define DUMMY_WNDCLASS "get_addrs_wndclass"

#ifdef __cplusplus
extern "C" {
#else
#if defined(_MSC_VER) && !defined(inline)
#define inline __inline
#endif
#endif

static inline uint32_t vtable_offset(HMODULE module, void *cls,
		unsigned int offset)
{
	uintptr_t *vtable = *(uintptr_t**)cls;
	return (uint32_t)(vtable[offset] - (uintptr_t)module);
}

extern void get_dxgi_offsets(struct dxgi_offsets *offsets);

#ifdef __cplusplus
}
#endif
