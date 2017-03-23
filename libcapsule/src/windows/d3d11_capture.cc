
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>

#include <string>

#include "../capture.h"
#include "../logging.h"
#include "../io.h"
#include "win_capture.h"
#include "dxgi_util.h"
#include "d3d11_shaders.h"

namespace capsule {
namespace d3d11 {

struct State {
  ID3D11Device              *device; // do not release
  ID3D11DeviceContext       *context; // do not release
  int                       cx; // framebuffer width
  int                       cy; // framebuffer height
  int                       size_divider;
  DXGI_FORMAT               format; // pixel format
  DXGI_FORMAT               out_format; // pixel format
  bool                      multisampled; // if true, subresource needs to be resolved on GPU before downloading

  bool                      gpu_color_conv; // whether to do color conversion on the GPU

  ID3D11Texture2D                *scale_tex; // texture & resource used for in-GPU scaling
  ID3D11ShaderResourceView       *scale_resource;

  ID3D11VertexShader             *vertex_shader; // shaders used for in-GPU scaling
  ID3D11InputLayout              *vertex_layout;
  ID3D11PixelShader              *pixel_shader;

  ID3D11SamplerState             *sampler_state;
  ID3D11BlendState               *blend_state;
  ID3D11DepthStencilState        *zstencil_state;
  ID3D11RasterizerState          *raster_state;

  ID3D11Buffer                   *vertex_buffer;
  ID3D11Buffer                   *constants;

  ID3D11Texture2D                *copy_surfaces[kNumBuffers]; // staging, CPU_READ_ACCESS
  ID3D11Texture2D                *textures[kNumBuffers]; // default (in-GPU), useful to resolve multisampled backbuffers
  ID3D11RenderTargetView         *render_targets[kNumBuffers];
  bool                           texture_ready[kNumBuffers];
  bool                           texture_mapped[kNumBuffers];
  int64_t                        timestamps[kNumBuffers];
  int                            cur_tex;
  int                            copy_wait;

  int                            overlay_width;  
  int                            overlay_height;  
  unsigned char                  *overlay_pixels;
  ID3D11Texture2D                *overlay_tex;
  ID3D11ShaderResourceView       *overlay_resource;
  ID3D11PixelShader              *overlay_pixel_shader;
  ID3D11BlendState               *overlay_blend_state;
  ID3D11Buffer                   *overlay_vertex_buffer;

  intptr_t                       pitch; // linesize, may not be (width * components) because of alignemnt
};

static State state = {};

HINSTANCE hD3DCompiler = NULL;
pD3DCompile pD3DCompileFunc = NULL;

/**
 * Copy the format, size, and multisampling information of the
 * D3D11 backbuffer to our internal state
 */
static bool InitFormat(IDXGISwapChain *swap, HWND *window) {
  DXGI_SWAP_CHAIN_DESC desc;
  HRESULT hr;

  hr = swap->GetDesc(&desc);
  if (FAILED(hr)) {
    Log("InitFormat: swap->GetDesc failed");
    return false;
  }
  state.format = dxgi::FixFormat(desc.BufferDesc.Format);
  state.out_format = DXGI_FORMAT_R8G8B8A8_UNORM;
  state.multisampled = desc.SampleDesc.Count > 1;
  *window = desc.OutputWindow;
  state.cx = desc.BufferDesc.Width;
  state.cy = desc.BufferDesc.Height;
  state.size_divider = capture::GetState()->settings.size_divider;

  Log("Backbuffer: %ux%u (%s) format = %s",
    state.cx, state.cy,
    state.multisampled ? "multisampled" : "",
    dxgi::NameFromFormat(desc.BufferDesc.Format).c_str()
  );

  return true;
}

/**
 * Release a COM resource, if it's non-null
 */
static inline void SafeRelease(IUnknown *p) {
  if (p) {
    p->Release();
  }
}

/**
 * Create a staging surface (used to download data from the GPU to the CPU)
 */
static bool CreateStageSurface(ID3D11Texture2D **tex) {
  HRESULT hr;

  D3D11_TEXTURE2D_DESC desc      = {};
  desc.Width                     = state.cx / state.size_divider;
  desc.Height                    = state.cy / state.size_divider;
  desc.Format                    = state.out_format;
  desc.MipLevels                 = 1;
  desc.ArraySize                 = 1;
  desc.SampleDesc.Count          = 1;
  desc.Usage                     = D3D11_USAGE_STAGING;
  desc.CPUAccessFlags            = D3D11_CPU_ACCESS_READ;

  hr = state.device->CreateTexture2D(&desc, nullptr, tex);
  if (FAILED(hr)) {
    Log("CreateStageSurface: failed to create texture (%x)", hr);
    return false;
  }

  return true;
}

/**
 * Create an on-GPU texture (used to resolve multisampled backbuffers)
 */
static bool CreateTex(int cx, int cy, ID3D11Texture2D **tex, bool out) {
  HRESULT hr;

  D3D11_TEXTURE2D_DESC desc                 = {};
  desc.Width                                = cx;
  desc.Height                               = cy;
  desc.MipLevels                            = 1;
  desc.ArraySize                            = 1;
  desc.BindFlags                            = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  if (out) {
    desc.Format                               = state.out_format;
  } else {
    desc.Format                               = state.format;
  }
  desc.SampleDesc.Count                     = 1;
  desc.Usage                                = D3D11_USAGE_DEFAULT;
  desc.MiscFlags                            = 0;

  hr = state.device->CreateTexture2D(&desc, nullptr, tex);
  if (FAILED(hr)) {
    Log("CreateTex: failed to create texture (%x)", hr);
    return false;
  }

  return true;
}

/**
 * Initiate both on-GPU textures and copy surfaces (staging).
 */
static bool ShmemInitBuffers(size_t idx) {
  bool success;

  success = CreateStageSurface(&state.copy_surfaces[idx]);
  if (!success) {
    Log("ShmemInitBuffers: failed to create copy surface");
    return false;
  }

  // for the first texture, retrieve pitch (linesize), too
  if (idx == 0) {
    D3D11_MAPPED_SUBRESOURCE map = {};
    HRESULT hr;

    hr = state.context->Map(state.copy_surfaces[idx], 0,
      D3D11_MAP_READ, 0, &map);

    if (FAILED(hr)) {
      Log("d3d11_shmem_init_buffers: failed to get pitch (%x)", hr);
      return false;
    }

    state.pitch = map.RowPitch;
    state.context->Unmap(state.copy_surfaces[idx], 0);
  }

  success = CreateTex(state.cx / state.size_divider, state.cy / state.size_divider, &state.textures[idx], true);

  if (!success) {
    Log("ShmemInitBuffers: failed to create texture");
    return false;
  }

  HRESULT hr = state.device->CreateRenderTargetView(state.textures[idx], nullptr, &state.render_targets[idx]);
  if (FAILED(hr)) {
    Log("ShmemInitBuffers: failed to create rendertarget view (%x)", hr);
  }

  success = CreateTex(state.cx, state.cy, &state.scale_tex, false);

  if (!success) {
    Log("ShmemInitBuffers: failed to create scale texture");
    return false;
  }

  D3D11_SHADER_RESOURCE_VIEW_DESC res_desc = {};
  res_desc.Format = state.format;
  res_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  res_desc.Texture2D.MipLevels = 1;

  hr = state.device->CreateShaderResourceView(state.scale_tex, &res_desc, &state.scale_resource);
  if (FAILED(hr)) {
    Log("d3d11_shmem_init_buffers: failed to create rendertarget view (%x)", hr);
    return false;
  }

  return true;
}

static bool ShmemInit(HWND window) {
  for (size_t i = 0; i < kNumBuffers; i++) {
    if (!ShmemInitBuffers(i)) {
      return false;
    }
  }

  Log("d3d11 memory capture initialize successful");
  return true;
}

static bool InitColorconvShader() {
  HRESULT hr;

  D3D11_BUFFER_DESC bd;
  memset(&bd, 0, sizeof(bd));

  // keep in sync with pixel_shader_string_conv
  int constant_size = sizeof(float) * 2;

  float width = (float) (state.cx / state.size_divider);
  float width_i = 1.0f / width;
  float height = (float) (state.cy / state.size_divider);
  float height_i = 1.0f / height;
  struct {
    float width;
    float width_i;
    float height;
    float height_i;
    int64_t pad1;
    int64_t pad2;
  } initial_data = {
    width,
    width_i,
    height,
    height_i,
    0,
    0,
  };

  Log("InitColorconvShader: constant size = %d", constant_size);

  bd.ByteWidth = (constant_size+15) & 0xFFFFFFF0; // align
  bd.Usage = D3D11_USAGE_DYNAMIC;
  bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  hr = state.device->CreateBuffer(&bd, nullptr, &state.constants);
  if (FAILED(hr)) {
    Log("InitColorconvShader: failed to create constant buffer (%x)", hr);
    return false;
  }

  {
    D3D11_MAPPED_SUBRESOURCE map;
    HRESULT hr;

    hr = state.context->Map(state.constants, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
    if (FAILED(hr)) {
      Log("InitColorconvShader: could not lock constants buffer (%x)", hr);
      return false;
    }

    memcpy(map.pData, &initial_data, bd.ByteWidth);
    state.context->Unmap(state.constants, 0);
  }

  return true;
}

static bool LoadShaders() {
  hD3DCompiler = LoadLibraryA(D3DCOMPILER_DLL_A);

  if (!hD3DCompiler) {
    Log("LoadShaders: couldn't load %s", D3DCOMPILER_DLL_A);
    hD3DCompiler = LoadLibraryA("d3dcompiler_42.dll");
    if (!hD3DCompiler) {
      Log("LoadShaders: couldn't load d3dcompiler_42.dll, bailing");
      return false;
    }
  }

  pD3DCompileFunc = (pD3DCompile) GetProcAddress(hD3DCompiler, "D3DCompile");
  if (pD3DCompileFunc == NULL) {
    Log("LoadShaders: D3DCompile was NULL");
    return false;
  }

  HRESULT hr;
  ID3D10Blob *blob;

  // Set up vertex shader

  uint8_t vertex_shader_data[4096];
  size_t vertex_shader_size = 0;
  D3D11_INPUT_ELEMENT_DESC input_desc[2];

  hr = pD3DCompileFunc(vertex_shader_string, sizeof(vertex_shader_string),
    "vertex_shader_string", nullptr, nullptr, "main",
    "vs_4_0", D3D10_SHADER_OPTIMIZATION_LEVEL1, 0, &blob, nullptr);

  if (FAILED(hr)) {
    Log("LoadShaders: failed to compile vertex shader (%x)", hr);
    return false;
  }

  vertex_shader_size = (size_t)blob->GetBufferSize();
  memcpy(vertex_shader_data, blob->GetBufferPointer(), blob->GetBufferSize());
  blob->Release();

  hr = state.device->CreateVertexShader(vertex_shader_data, vertex_shader_size, nullptr, &state.vertex_shader);

  if (FAILED(hr)) {
    Log("LoadShaders: failed to create vertex shader (%x)", hr);
    return false;
  }

  input_desc[0].SemanticName = "SV_Position";
  input_desc[0].SemanticIndex = 0;
  input_desc[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  input_desc[0].InputSlot = 0;
  input_desc[0].AlignedByteOffset = 0;
  input_desc[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
  input_desc[0].InstanceDataStepRate = 0;

  input_desc[1].SemanticName = "TEXCOORD";
  input_desc[1].SemanticIndex = 0;
  input_desc[1].Format = DXGI_FORMAT_R32G32_FLOAT;
  input_desc[1].InputSlot = 0;
  input_desc[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
  input_desc[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
  input_desc[1].InstanceDataStepRate = 0;

  hr = state.device->CreateInputLayout(input_desc, 2, vertex_shader_data, vertex_shader_size, &state.vertex_layout);

  if (FAILED(hr)) {
    Log("LoadShaders: failed to create layout (%x)", hr);
    return false;
  }

  // Set up pixel shader

  {
    uint8_t pixel_shader_data[16384];
    size_t pixel_shader_size = 0;
    ID3D10Blob *error_msg = nullptr;

    const char *pixel_shader_string;
    size_t pixel_shader_string_size;

    if (state.gpu_color_conv) {
      pixel_shader_string = pixel_shader_string_conv;
      pixel_shader_string_size = sizeof(pixel_shader_string_conv);
    } else {
      pixel_shader_string = pixel_shader_string_noconv;
      pixel_shader_string_size = sizeof(pixel_shader_string_noconv);
    }

    hr = pD3DCompileFunc(pixel_shader_string, pixel_shader_string_size,
      "pixel_shader_string", nullptr, nullptr, "main",
      "ps_4_0", D3D10_SHADER_OPTIMIZATION_LEVEL1, 0, &blob, &error_msg);

    if (FAILED(hr)) {
      Log("LoadShaders: failed to compile pixel shader (%x)", hr);
      Log("LoadShaders: %s", (char*) error_msg->GetBufferPointer());
      return false;
    }

    pixel_shader_size = (size_t)blob->GetBufferSize();
    memcpy(pixel_shader_data, blob->GetBufferPointer(), blob->GetBufferSize());
    blob->Release();

    hr = state.device->CreatePixelShader(pixel_shader_data, pixel_shader_size, nullptr, &state.pixel_shader);

    if (FAILED(hr)) {
      Log("LoadShaders: failed to create pixel shader (%x)", hr);
      return false;
    }

    if (state.gpu_color_conv) {
      if (!InitColorconvShader()) {
        return false;
      }
    }
  }

  // Set up overlay pixel shader
  {
    uint8_t pixel_shader_data[16384];
    size_t pixel_shader_size = 0;
    ID3D10Blob *error_msg = nullptr;

    const char *pixel_shader_string;
    size_t pixel_shader_string_size;

    pixel_shader_string = pixel_shader_string_overlay;
    pixel_shader_string_size = sizeof(pixel_shader_string_overlay);

    hr = pD3DCompileFunc(pixel_shader_string, pixel_shader_string_size,
      "pixel_shader_string", nullptr, nullptr, "main",
      "ps_4_0", D3D10_SHADER_OPTIMIZATION_LEVEL1, 0, &blob, &error_msg);

    if (FAILED(hr)) {
      Log("LoadShaders: failed to compile pixel shader (%x)", hr);
      Log("LoadShaders: %s", (char*) error_msg->GetBufferPointer());
      return false;
    }

    pixel_shader_size = (size_t)blob->GetBufferSize();
    memcpy(pixel_shader_data, blob->GetBufferPointer(), blob->GetBufferSize());
    blob->Release();

    hr = state.device->CreatePixelShader(pixel_shader_data, pixel_shader_size, nullptr, &state.overlay_pixel_shader);

    if (FAILED(hr)) {
      Log("LoadShaders: failed to create pixel shader (%x)", hr);
      return false;
    }
  }

  return true;
}

static bool InitDrawStates(void)
{
  HRESULT hr;

  D3D11_SAMPLER_DESC sampler_desc = {};
  sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

  hr = state.device->CreateSamplerState(&sampler_desc, &state.sampler_state);
  if (FAILED(hr)) {
    Log("InitDrawStates: failed to create sampler state (%x)", hr);
    return false;
  }

  {
    D3D11_BLEND_DESC blend_desc = {};

    for (size_t i = 0; i < 8; i++) {
      if (i == 0) {
        blend_desc.RenderTarget[i].BlendEnable = TRUE;
        blend_desc.RenderTarget[i].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blend_desc.RenderTarget[i].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blend_desc.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
        blend_desc.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
        blend_desc.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ZERO;
        blend_desc.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
      }
      blend_desc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }

    hr = state.device->CreateBlendState(&blend_desc, &state.overlay_blend_state);
    if (FAILED(hr)) {
      Log("InitBlendState: failed to create overlay blend state (%x)", hr);
      return false;
    }
  }

  {
    D3D11_BLEND_DESC blend_desc = {};

    for (size_t i = 0; i < 8; i++) {
      blend_desc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }

    hr = state.device->CreateBlendState(&blend_desc, &state.blend_state);
    if (FAILED(hr)) {
      Log("InitBlendState: failed to create blend state (%x)", hr);
      return false;
    }
  }

  D3D11_DEPTH_STENCIL_DESC zstencil_desc = {};

  hr = state.device->CreateDepthStencilState(&zstencil_desc, &state.zstencil_state);
  if (FAILED(hr)) {
    Log("InitZstencilState: failed to create zstencil state (%x)", hr);
    return false;
  }

  D3D11_RASTERIZER_DESC raster_desc = {};

  raster_desc.FillMode = D3D11_FILL_SOLID;
  raster_desc.CullMode = D3D11_CULL_NONE;

  hr = state.device->CreateRasterizerState(&raster_desc, &state.raster_state);
  if (FAILED(hr)) {
    Log("InitRasterState: failed to create raster state (%x)", hr);
    return false;
  }

  return true;
}

struct vertex {
  struct {
    float x, y, z, w;
  } pos;
  struct {
    float u, v;
  } tex;
};

static bool InitQuadVbo(void)
{
  HRESULT hr;
  const vertex verts[4] = {
    { { -1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },
    { { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
    { { 1.0f,   1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
    { { 1.0f,  -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } }
  };

  D3D11_BUFFER_DESC desc;
  desc.ByteWidth = sizeof(vertex) * 4;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = 0;

  D3D11_SUBRESOURCE_DATA srd = {};
  srd.pSysMem = (const void*) verts;

  hr = state.device->CreateBuffer(&desc, &srd, &state.vertex_buffer);
  if (FAILED(hr)) {
    Log("InitQuadVbo: failed to create vertex buffer (%x)", hr);
    return false;
  }

  return true;
}

static bool InitOverlayVbo(void)
{
  // d3d11 coordinate system: (0, 0) = bottom-left
  float cx = (float) state.cx;
  float cy = (float) state.cy;
  float width = (float) state.overlay_width;
  float height = (float) state.overlay_height;
  float x = cx - width;
  float y = 0;

  float l = x / cx * 2.0f - 1.0f;
  float r = (x + width) / cx * 2.0f - 1.0f;
  float b = y / cy * 2.0f - 1.0f;
  float t = (y + height) / cy * 2.0f - 1.0f;

  Log("Overlay vbo: left = %.2f, right = %.2f, top = %.2f, bottom = %.2f", l, r, t, b);

  HRESULT hr;
  const vertex verts[4] = {
    { { l, t, 0.0f, 1.0f }, { 0.0f, 0.0f } },
    { { l, b, 0.0f, 1.0f }, { 0.0f, 1.0f } },
    { { r, t, 0.0f, 1.0f }, { 1.0f, 0.0f } },
    { { r, b, 0.0f, 1.0f }, { 1.0f, 1.0f } }
  };

  D3D11_BUFFER_DESC desc;
  desc.ByteWidth = sizeof(vertex) * 4;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = 0;

  D3D11_SUBRESOURCE_DATA srd = {};
  srd.pSysMem = (const void*) verts;

  hr = state.device->CreateBuffer(&desc, &srd, &state.overlay_vertex_buffer);
  if (FAILED(hr)) {
    Log("InitOverlayVbo: failed to create vertex buffer (%x)", hr);
    return false;
  }

  return true;
}

static bool InitOverlayTexture(void)
{
  state.overlay_width = 256;
  state.overlay_height = 128;
  state.overlay_pixels = (unsigned char*) malloc(state.overlay_width * 4 * state.overlay_height);

  HRESULT hr;
  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = state.overlay_width;
  desc.Height = state.overlay_height;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DYNAMIC;
  desc.MiscFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  D3D11_SUBRESOURCE_DATA srd = {};
  srd.SysMemPitch = state.overlay_width * 4;
  srd.pSysMem = (const void *) state.overlay_pixels;

  hr = state.device->CreateTexture2D(&desc, &srd, &state.overlay_tex);
  if (FAILED(hr)) {
    Log("InitOverlayTexture: failed to create texture (%x)", hr);
    return false;
  }

  D3D11_SHADER_RESOURCE_VIEW_DESC res_desc = {};
  res_desc.Format = desc.Format;
  res_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  res_desc.Texture2D.MipLevels = 1;

  hr = state.device->CreateShaderResourceView(state.overlay_tex, &res_desc, &state.overlay_resource);
  if (FAILED(hr)) {
    Log("InitOverlayTexture: failed to create rendertarget view (%x)", hr);
    return false;
  }

  return true;
}

static void Init(IDXGISwapChain *swap) {
  Log("Initializing D3D11 capture");

  bool success = true;
  HWND window;
  HRESULT hr;

  hr = swap->GetDevice(__uuidof(ID3D11Device), (void**)&state.device);
  if (FAILED(hr)) {
    Log("Init: failed to get device from swap (%x)", hr);
    return;
  }
  state.device->Release();

  state.device->GetImmediateContext(&state.context);
  state.context->Release();

  state.gpu_color_conv = capture::GetState()->settings.gpu_color_conv;

  InitFormat(swap, &window);

  if (!InitOverlayTexture() || !InitOverlayVbo()) {
    exit(1337);
    return;
  }

  if (!LoadShaders() || !InitDrawStates() || !InitQuadVbo()) {
    exit(1337);
    return;
  }

  success = ShmemInit(window);

  if (!success) {
    Free();
  }
}

static inline void CopyTexture (ID3D11Resource *dst, ID3D11Resource *src) {
  if (state.multisampled) {
    state.context->ResolveSubresource(dst, 0, src, 0, state.format);
  } else {
    state.context->CopyResource(dst, src);
  }
}

#define MAX_RENDER_TARGETS             D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT
#define MAX_SO_TARGETS                 4
#define MAX_CLASS_INSTS                256

struct DeviceState {
  ID3D11GeometryShader           *geom_shader;
  ID3D11InputLayout              *vertex_layout;
  D3D11_PRIMITIVE_TOPOLOGY       topology;
  ID3D11Buffer                   *vertex_buffer;
  UINT                           vb_stride;
  UINT                           vb_offset;
  ID3D11BlendState               *blend_state;
  float                          blend_factor[4];
  UINT                           sample_mask;
  ID3D11DepthStencilState        *zstencil_state;
  UINT                           zstencil_ref;
  ID3D11RenderTargetView         *render_targets[MAX_RENDER_TARGETS];
  ID3D11DepthStencilView         *zstencil_view;
  ID3D11SamplerState             *sampler_state;
  ID3D11PixelShader              *pixel_shader;
  ID3D11ShaderResourceView       *resource;
  ID3D11RasterizerState          *raster_state;
  UINT                           num_viewports;
  D3D11_VIEWPORT                 *viewports;
  ID3D11Buffer                   *stream_output_targets[MAX_SO_TARGETS];
  ID3D11VertexShader             *vertex_shader;
  ID3D11ClassInstance            *gs_class_instances[MAX_CLASS_INSTS];
  ID3D11ClassInstance            *ps_class_instances[MAX_CLASS_INSTS];
  ID3D11ClassInstance            *vs_class_instances[MAX_CLASS_INSTS];
  UINT                           gs_class_inst_count;
  UINT                           ps_class_inst_count;
  UINT                           vs_class_inst_count;
};

// TODO: save PSSetConstantBuffers ?
static void SaveState(DeviceState *save) {
  save->gs_class_inst_count = MAX_CLASS_INSTS;
  save->ps_class_inst_count = MAX_CLASS_INSTS;
  save->vs_class_inst_count = MAX_CLASS_INSTS;

  state.context->GSGetShader(&save->geom_shader, save->gs_class_instances, &save->gs_class_inst_count);
  state.context->IAGetInputLayout(&save->vertex_layout);
  state.context->IAGetPrimitiveTopology(&save->topology);
  state.context->IAGetVertexBuffers(0, 1, &save->vertex_buffer, &save->vb_stride, &save->vb_offset);
  state.context->OMGetBlendState(&save->blend_state, save->blend_factor, &save->sample_mask);
  state.context->OMGetDepthStencilState(&save->zstencil_state, &save->zstencil_ref);
  state.context->OMGetRenderTargets(MAX_RENDER_TARGETS, save->render_targets, &save->zstencil_view);
  state.context->PSGetSamplers(0, 1, &save->sampler_state);
  state.context->PSGetShader(&save->pixel_shader, save->ps_class_instances, &save->ps_class_inst_count);
  state.context->PSGetShaderResources(0, 1, &save->resource);
  state.context->RSGetState(&save->raster_state);
  state.context->RSGetViewports(&save->num_viewports, nullptr);
  if (save->num_viewports) {
    save->viewports = (D3D11_VIEWPORT*)malloc(sizeof(D3D11_VIEWPORT) * save->num_viewports);
    state.context->RSGetViewports(&save->num_viewports, save->viewports);
  }
  state.context->SOGetTargets(MAX_SO_TARGETS, save->stream_output_targets);
  state.context->VSGetShader(&save->vertex_shader, save->vs_class_instances, &save->vs_class_inst_count);
}

#define SO_APPEND ((UINT)-1)

static void RestoreState(DeviceState *save) {
  UINT so_offsets[MAX_SO_TARGETS] = { SO_APPEND, SO_APPEND, SO_APPEND, SO_APPEND };

  state.context->GSSetShader(save->geom_shader, save->gs_class_instances, save->gs_class_inst_count);
  state.context->IASetInputLayout(save->vertex_layout);
  state.context->IASetPrimitiveTopology(save->topology);
  state.context->IASetVertexBuffers(0, 1, &save->vertex_buffer, &save->vb_stride, &save->vb_offset);
  state.context->OMSetBlendState(save->blend_state, save->blend_factor, save->sample_mask);
  state.context->OMSetDepthStencilState(save->zstencil_state, save->zstencil_ref);
  state.context->OMSetRenderTargets(MAX_RENDER_TARGETS, save->render_targets, save->zstencil_view);
  state.context->PSSetSamplers(0, 1, &save->sampler_state);
  state.context->PSSetShader(save->pixel_shader, save->ps_class_instances, save->ps_class_inst_count);
  state.context->PSSetShaderResources(0, 1, &save->resource);
  state.context->RSSetState(save->raster_state);
  state.context->RSSetViewports(save->num_viewports, save->viewports);
  state.context->SOSetTargets(MAX_SO_TARGETS, save->stream_output_targets, so_offsets);
  state.context->VSSetShader(save->vertex_shader, save->vs_class_instances, save->vs_class_inst_count);
  SafeRelease(save->geom_shader);
  SafeRelease(save->vertex_layout);
  SafeRelease(save->vertex_buffer);
  SafeRelease(save->blend_state);
  SafeRelease(save->zstencil_state);

  for (size_t i = 0; i < MAX_RENDER_TARGETS; i++)
    SafeRelease(save->render_targets[i]);

  SafeRelease(save->zstencil_view);
  SafeRelease(save->sampler_state);
  SafeRelease(save->pixel_shader);
  SafeRelease(save->resource);
  SafeRelease(save->raster_state);

  for (size_t i = 0; i < MAX_SO_TARGETS; i++) {
    SafeRelease(save->stream_output_targets[i]);
  }

  SafeRelease(save->vertex_shader);

  for (size_t i = 0; i < save->gs_class_inst_count; i++) {
    save->gs_class_instances[i]->Release();
  }
  for (size_t i = 0; i < save->ps_class_inst_count; i++) {
    save->ps_class_instances[i]->Release();
  }
  for (size_t i = 0; i < save->vs_class_inst_count; i++) {
    save->vs_class_instances[i]->Release();
  }
  free(save->viewports);
  memset(save, 0, sizeof(*save));
}


static void SetupPipeline(ID3D11RenderTargetView *target, ID3D11ShaderResourceView *resource) {
  const float factor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
  D3D11_VIEWPORT viewport = { 0 };
  UINT stride = sizeof(vertex);
  void *emptyptr = nullptr;
  UINT zero = 0;

  viewport.Width = (float)state.cx / state.size_divider;
  viewport.Height = (float)state.cy / state.size_divider;
  viewport.MaxDepth = 1.0f;

  state.context->GSSetShader(nullptr, nullptr, 0);
  state.context->IASetInputLayout(state.vertex_layout);
  state.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  state.context->IASetVertexBuffers(0, 1, &state.vertex_buffer, &stride, &zero);
  state.context->OMSetBlendState(state.blend_state, factor, 0xFFFFFFFF);
  state.context->OMSetDepthStencilState(state.zstencil_state, 0);
  state.context->OMSetRenderTargets(1, &target, nullptr);
  state.context->PSSetSamplers(0, 1, &state.sampler_state);
  state.context->PSSetShader(state.pixel_shader, nullptr, 0);
  state.context->PSSetShaderResources(0, 1, &resource);
  if (state.gpu_color_conv) {
    state.context->PSSetConstantBuffers(0, 1, &state.constants);
  }
  state.context->RSSetState(state.raster_state);
  state.context->RSSetViewports(1, &viewport);
  state.context->SOSetTargets(1, (ID3D11Buffer**)&emptyptr, &zero);
  state.context->VSSetShader(state.vertex_shader, nullptr, 0);
}

static void SetupOverlayPipeline(ID3D11ShaderResourceView *resource) {
  const float factor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
  D3D11_VIEWPORT viewport = { 0 };
  UINT stride = sizeof(vertex);
  void *emptyptr = nullptr;
  UINT zero = 0;

  viewport.Width = (float)state.cx;
  viewport.Height = (float)state.cy;
  viewport.MaxDepth = 1.0f;

  state.context->GSSetShader(nullptr, nullptr, 0);
  state.context->IASetInputLayout(state.vertex_layout);
  state.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  state.context->IASetVertexBuffers(0, 1, &state.overlay_vertex_buffer, &stride, &zero);
  state.context->OMSetBlendState(state.overlay_blend_state, factor, 0xFFFFFFFF);
  state.context->OMSetDepthStencilState(state.zstencil_state, 0);
  // state.context->OMSetRenderTargets(1, &target, nullptr);
  state.context->PSSetSamplers(0, 1, &state.sampler_state);
  state.context->PSSetShader(state.overlay_pixel_shader, nullptr, 0);
  state.context->PSSetShaderResources(0, 1, &state.overlay_resource);
  state.context->RSSetState(state.raster_state);
  state.context->RSSetViewports(1, &viewport);
  state.context->SOSetTargets(1, (ID3D11Buffer**)&emptyptr, &zero);
  // TODO: overlay shader
  state.context->VSSetShader(state.vertex_shader, nullptr, 0);
}

static inline void ScaleTexture(ID3D11RenderTargetView *target,
		ID3D11ShaderResourceView *resource) {
	static DeviceState save = {0};

	SaveState(&save);
	SetupPipeline(target, resource);
	state.context->Draw(4, 0);
	RestoreState(&save);
}

static inline void UpdateOverlayTexture() {
  D3D11_MAPPED_SUBRESOURCE map;
  HRESULT hr;

  hr = state.context->Map(state.overlay_tex, 0,
    D3D11_MAP_WRITE_DISCARD, 0, &map);

  if (FAILED(hr)) {
    Log("Could not map overlay tex");
    return;
  }

  size_t pixels_size = state.overlay_width * 4 * state.overlay_height;
  for (int y = 0; y < state.overlay_height; y++) {
    for (int x = 0; x < state.overlay_width; x++) {
      int i = (y * state.overlay_width + x) * 4;
      state.overlay_pixels[i]     = (state.overlay_pixels[i]     + 1) % 256;
      state.overlay_pixels[i + 1] = (state.overlay_pixels[i + 1] + 1) % 256;
      state.overlay_pixels[i + 2] = (state.overlay_pixels[i + 2] + 1) % 256;
      // state.overlay_pixels[i + 3] = (state.overlay_pixels[i + 3] + 1) % 256;
    }
  }
  memcpy(map.pData, state.overlay_pixels, pixels_size);

  state.context->Unmap(state.overlay_tex, 0);
}

static inline void DrawOverlayInternal() {
  UpdateOverlayTexture();

	static DeviceState save = {0};

	SaveState(&save);
	SetupOverlayPipeline(state.overlay_resource);
	state.context->Draw(4, 0);
	RestoreState(&save);
}

static inline void ShmemQueueCopy() {
	for (size_t i = 0; i < kNumBuffers; i++) {
		D3D11_MAPPED_SUBRESOURCE map;
		HRESULT hr;

		if (state.texture_ready[i]) {
			state.texture_ready[i] = false;

			hr = state.context->Map(state.copy_surfaces[i], 0,
					D3D11_MAP_READ, 0, &map);
			if (SUCCEEDED(hr)) {
				state.texture_mapped[i] = true;
        auto timestamp = state.timestamps[i];
        io::WriteVideoFrame(timestamp, (char *) map.pData, (state.cy / state.size_divider) * state.pitch);
			}
			break;
		}
	}
}

static inline void ShmemCapture (ID3D11Resource* backbuffer) {
  int next_tex;

  ShmemQueueCopy();

  next_tex = (state.cur_tex + 1) % kNumBuffers;

  state.timestamps[state.cur_tex] = capture::FrameTimestamp();

  // always using scale
  CopyTexture(state.scale_tex, backbuffer);
  ScaleTexture(state.render_targets[state.cur_tex], state.scale_resource);

  if (state.copy_wait < kNumBuffers - 1) {
    state.copy_wait++;
  } else {
    ID3D11Texture2D *src = state.textures[next_tex];
    ID3D11Texture2D *dst = state.copy_surfaces[next_tex];

    // obs has locking here, we don't
    state.context->Unmap(dst, 0);
    state.texture_mapped[next_tex] = false;

    CopyTexture(dst, src);
    state.texture_ready[next_tex] = true;
  }

  state.cur_tex = next_tex;
}

void Capture(void *swap_ptr, void *backbuffer_ptr) {
  static bool first_frame = true;

  if (!capture::Ready()) {
    if (!capture::Active() && !first_frame) {
      first_frame = true;
      Free();
    }

    return;
  }

  IDXGIResource *dxgi_backbuffer = (IDXGIResource*)backbuffer_ptr;
  IDXGISwapChain *swap = (IDXGISwapChain*)swap_ptr;

  if (!state.device) {
    Init(swap);
  }

  HRESULT hr;

  ID3D11Resource *backbuffer;

  hr = dxgi_backbuffer->QueryInterface(__uuidof(ID3D11Resource), (void**) &backbuffer);
  if (FAILED(hr)) {
    Log("Capture: failed to get backbuffer");
    return;
  }

  if (first_frame) {
    PixFmt pix_fmt;
    if (state.gpu_color_conv) {
      pix_fmt = kPixFmtYuv444P;
    } else {
      pix_fmt = dxgi::FormatToPixFmt(state.format);
    }
    io::WriteVideoFormat(
      state.cx / state.size_divider,
      state.cy / state.size_divider,
      pix_fmt,
      false /* no vflip */,
      state.pitch
    );
    first_frame = false;
  }

  ShmemCapture(backbuffer);
  backbuffer->Release();
}

void DrawOverlay () {
  if (!state.device) {
    return;
  }

  DrawOverlayInternal();
}

void Free() {
  SafeRelease(state.scale_tex);
  SafeRelease(state.scale_resource);
  SafeRelease(state.vertex_shader);
  SafeRelease(state.vertex_layout);
  SafeRelease(state.pixel_shader);
  SafeRelease(state.sampler_state);
  SafeRelease(state.blend_state);
  SafeRelease(state.zstencil_state);
  SafeRelease(state.raster_state);
  SafeRelease(state.vertex_buffer);
  SafeRelease(state.constants);

  for (size_t i = 0; i < kNumBuffers; i++) {
    if (state.copy_surfaces[i]) {
      if (state.texture_mapped[i]) {
        state.context->Unmap(state.copy_surfaces[i], 0);
      }
      state.copy_surfaces[i]->Release();
    }

    SafeRelease(state.textures[i]);
    SafeRelease(state.render_targets[i]);
  }

  memset(&state, 0, sizeof(state));

  Log("----------------- d3d11 capture freed ----------------");
}

} // namespace d3d11
} // namespace capsule
