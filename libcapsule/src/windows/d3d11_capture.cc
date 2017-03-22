
#include <capsule.h>
#include "win_capture.h"
#include "dxgi_util.h"

#include "./d3d11_shaders.h"

#include <d3d11.h>
#include <d3dcompiler.h>

#include <dxgi.h>

#include <string>

// inspired by libobs
struct d3d11_data {
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

  ID3D11Texture2D                *copy_surfaces[capsule::kNumBuffers]; // staging, CPU_READ_ACCESS
  ID3D11Texture2D                *textures[capsule::kNumBuffers]; // default (in-GPU), useful to resolve multisampled backbuffers
  ID3D11RenderTargetView         *render_targets[capsule::kNumBuffers];
  bool                           texture_ready[capsule::kNumBuffers];
  bool                           texture_mapped[capsule::kNumBuffers];
  int64_t                        timestamps[capsule::kNumBuffers];
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

static struct d3d11_data data = {};

HINSTANCE hD3DCompiler = NULL;
pD3DCompile pD3DCompileFunc = NULL;

/**
 * Copy the format, size, and multisampling information of the
 * D3D11 backbuffer to our internal state
 */
static bool D3d11InitFormat(IDXGISwapChain *swap, HWND *window) {
  DXGI_SWAP_CHAIN_DESC desc;
  HRESULT hr;

  hr = swap->GetDesc(&desc);
  if (FAILED(hr)) {
    CapsuleLog("D3d11InitFormat: swap->GetDesc failed");
    return false;
  }
  data.format = FixDxgiFormat(desc.BufferDesc.Format);
  data.out_format = DXGI_FORMAT_R8G8B8A8_UNORM;
  data.multisampled = desc.SampleDesc.Count > 1;
  *window = desc.OutputWindow;
  data.cx = desc.BufferDesc.Width;
  data.cy = desc.BufferDesc.Height;
  data.size_divider = capdata.settings.size_divider;

  CapsuleLog("Backbuffer: %ux%u (%s) format = %s",
    data.cx, data.cy,
    data.multisampled ? "multisampled" : "",
    NameFromDxgiFormat(desc.BufferDesc.Format).c_str()
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
static bool CreateD3d11StageSurface(ID3D11Texture2D **tex) {
  HRESULT hr;

  D3D11_TEXTURE2D_DESC desc      = {};
  desc.Width                     = data.cx / data.size_divider;
  desc.Height                    = data.cy / data.size_divider;
  desc.Format                    = data.out_format;
  desc.MipLevels                 = 1;
  desc.ArraySize                 = 1;
  desc.SampleDesc.Count          = 1;
  desc.Usage                     = D3D11_USAGE_STAGING;
  desc.CPUAccessFlags            = D3D11_CPU_ACCESS_READ;

  hr = data.device->CreateTexture2D(&desc, nullptr, tex);
  if (FAILED(hr)) {
    CapsuleLog("CreateD3d11StageSurface: failed to create texture (%x)", hr);
    return false;
  }

  return true;
}

/**
 * Create an on-GPU texture (used to resolve multisampled backbuffers)
 */
static bool CreateD3d11Tex(int cx, int cy, ID3D11Texture2D **tex, bool out) {
  HRESULT hr;

  D3D11_TEXTURE2D_DESC desc                 = {};
  desc.Width                                = cx;
  desc.Height                               = cy;
  desc.MipLevels                            = 1;
  desc.ArraySize                            = 1;
  desc.BindFlags                            = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  if (out) {
    desc.Format                               = data.out_format;
  } else {
    desc.Format                               = data.format;
  }
  desc.SampleDesc.Count                     = 1;
  desc.Usage                                = D3D11_USAGE_DEFAULT;
  desc.MiscFlags                            = 0;

  hr = data.device->CreateTexture2D(&desc, nullptr, tex);
  if (FAILED(hr)) {
    CapsuleLog("CreateD3d11Tex: failed to create texture (%x)", hr);
    return false;
  }

  return true;
}

/**
 * Initiate both on-GPU textures and copy surfaces (staging).
 */
static bool D3d11ShmemInitBuffers(size_t idx) {
  bool success;

  success = CreateD3d11StageSurface(&data.copy_surfaces[idx]);
  if (!success) {
    CapsuleLog("D3d11ShmemInitBuffers: failed to create copy surface");
    return false;
  }

  // for the first texture, retrieve pitch (linesize), too
  if (idx == 0) {
    D3D11_MAPPED_SUBRESOURCE map = {};
    HRESULT hr;

    hr = data.context->Map(data.copy_surfaces[idx], 0,
      D3D11_MAP_READ, 0, &map);

    if (FAILED(hr)) {
      CapsuleLog("d3d11_shmem_init_buffers: failed to get pitch (%x)", hr);
      return false;
    }

    data.pitch = map.RowPitch;
    data.context->Unmap(data.copy_surfaces[idx], 0);
  }

  success = CreateD3d11Tex(data.cx / data.size_divider, data.cy / data.size_divider, &data.textures[idx], true);

  if (!success) {
    CapsuleLog("D3d11ShmemInitBuffers: failed to create texture");
    return false;
  }

  HRESULT hr = data.device->CreateRenderTargetView(data.textures[idx], nullptr, &data.render_targets[idx]);
  if (FAILED(hr)) {
    CapsuleLog("D3d11ShmemInitBuffers: failed to create rendertarget view (%x)", hr);
  }

  success = CreateD3d11Tex(data.cx, data.cy, &data.scale_tex, false);

  if (!success) {
    CapsuleLog("D3d11ShmemInitBuffers: failed to create scale texture");
    return false;
  }

  D3D11_SHADER_RESOURCE_VIEW_DESC res_desc = {};
  res_desc.Format = data.format;
  res_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  res_desc.Texture2D.MipLevels = 1;

  hr = data.device->CreateShaderResourceView(data.scale_tex, &res_desc, &data.scale_resource);
  if (FAILED(hr)) {
    CapsuleLog("d3d11_shmem_init_buffers: failed to create rendertarget view (%x)", hr);
    return false;
  }

  return true;
}

static bool D3d11ShmemInit(HWND window) {
  for (size_t i = 0; i < capsule::kNumBuffers; i++) {
    if (!D3d11ShmemInitBuffers(i)) {
      return false;
    }
  }

  CapsuleLog("d3d11 memory capture initialize successful");
  return true;
}

static bool D3d11InitColorconvShader() {
  HRESULT hr;

  D3D11_BUFFER_DESC bd;
  memset(&bd, 0, sizeof(bd));

  // keep in sync with pixel_shader_string_conv
  int constant_size = sizeof(float) * 2;

  float width = (float) (data.cx / data.size_divider);
  float width_i = 1.0f / width;
  float height = (float) (data.cy / data.size_divider);
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

  CapsuleLog("D3d11InitColorconvShader: constant size = %d", constant_size);

  bd.ByteWidth = (constant_size+15) & 0xFFFFFFF0; // align
  bd.Usage = D3D11_USAGE_DYNAMIC;
  bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  hr = data.device->CreateBuffer(&bd, nullptr, &data.constants);
  if (FAILED(hr)) {
    CapsuleLog("D3d11InitColorconvShader: failed to create constant buffer (%x)", hr);
    return false;
  }

  {
    D3D11_MAPPED_SUBRESOURCE map;
    HRESULT hr;

    hr = data.context->Map(data.constants, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
    if (FAILED(hr)) {
      CapsuleLog("D3d11InitColorconvShader: could not lock constants buffer (%x)", hr);
      return false;
    }

    memcpy(map.pData, &initial_data, bd.ByteWidth);
    data.context->Unmap(data.constants, 0);
  }

  return true;
}

static bool D3d11LoadShaders() {
  hD3DCompiler = LoadLibraryA(D3DCOMPILER_DLL_A);

  if (!hD3DCompiler) {
    CapsuleLog("D3d11LoadShaders: couldn't load %s", D3DCOMPILER_DLL_A);
    hD3DCompiler = LoadLibraryA("d3dcompiler_42.dll");
    if (!hD3DCompiler) {
      CapsuleLog("D3d11LoadShaders: couldn't load d3dcompiler_42.dll, bailing");
      return false;
    }
  }

  pD3DCompileFunc = (pD3DCompile) GetProcAddress(hD3DCompiler, "D3DCompile");
  if (pD3DCompileFunc == NULL) {
    CapsuleLog("D3d11LoadShaders: D3DCompile was NULL");
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
    CapsuleLog("D3d11LoadShaders: failed to compile vertex shader (%x)", hr);
    return false;
  }

  vertex_shader_size = (size_t)blob->GetBufferSize();
  memcpy(vertex_shader_data, blob->GetBufferPointer(), blob->GetBufferSize());
  blob->Release();

  hr = data.device->CreateVertexShader(vertex_shader_data, vertex_shader_size, nullptr, &data.vertex_shader);

  if (FAILED(hr)) {
    CapsuleLog("D3d11LoadShaders: failed to create vertex shader (%x)", hr);
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

  hr = data.device->CreateInputLayout(input_desc, 2, vertex_shader_data, vertex_shader_size, &data.vertex_layout);

  if (FAILED(hr)) {
    CapsuleLog("D3d11LoadShaders: failed to create layout (%x)", hr);
    return false;
  }

  // Set up pixel shader

  {
    uint8_t pixel_shader_data[16384];
    size_t pixel_shader_size = 0;
    ID3D10Blob *error_msg = nullptr;

    const char *pixel_shader_string;
    size_t pixel_shader_string_size;

    if (data.gpu_color_conv) {
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
      CapsuleLog("D3d11LoadShaders: failed to compile pixel shader (%x)", hr);
      CapsuleLog("D3d11LoadShaders: %s", (char*) error_msg->GetBufferPointer());
      return false;
    }

    pixel_shader_size = (size_t)blob->GetBufferSize();
    memcpy(pixel_shader_data, blob->GetBufferPointer(), blob->GetBufferSize());
    blob->Release();

    hr = data.device->CreatePixelShader(pixel_shader_data, pixel_shader_size, nullptr, &data.pixel_shader);

    if (FAILED(hr)) {
      CapsuleLog("D3d11LoadShaders: failed to create pixel shader (%x)", hr);
      return false;
    }

    if (data.gpu_color_conv) {
      if (!D3d11InitColorconvShader()) {
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
      CapsuleLog("D3d11LoadShaders: failed to compile pixel shader (%x)", hr);
      CapsuleLog("D3d11LoadShaders: %s", (char*) error_msg->GetBufferPointer());
      return false;
    }

    pixel_shader_size = (size_t)blob->GetBufferSize();
    memcpy(pixel_shader_data, blob->GetBufferPointer(), blob->GetBufferSize());
    blob->Release();

    hr = data.device->CreatePixelShader(pixel_shader_data, pixel_shader_size, nullptr, &data.overlay_pixel_shader);

    if (FAILED(hr)) {
      CapsuleLog("D3d11LoadShaders: failed to create pixel shader (%x)", hr);
      return false;
    }
  }

  return true;
}

static bool D3d11InitDrawStates(void)
{
  HRESULT hr;

  D3D11_SAMPLER_DESC sampler_desc = {};
  sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

  hr = data.device->CreateSamplerState(&sampler_desc, &data.sampler_state);
  if (FAILED(hr)) {
    CapsuleLog("D3d11InitDrawStates: failed to create sampler state (%x)", hr);
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

    hr = data.device->CreateBlendState(&blend_desc, &data.overlay_blend_state);
    if (FAILED(hr)) {
      CapsuleLog("D3d11InitBlendState: failed to create overlay blend state (%x)", hr);
      return false;
    }
  }

  {
    D3D11_BLEND_DESC blend_desc = {};

    for (size_t i = 0; i < 8; i++) {
      blend_desc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }

    hr = data.device->CreateBlendState(&blend_desc, &data.blend_state);
    if (FAILED(hr)) {
      CapsuleLog("D3d11InitBlendState: failed to create blend state (%x)", hr);
      return false;
    }
  }

  D3D11_DEPTH_STENCIL_DESC zstencil_desc = {};

  hr = data.device->CreateDepthStencilState(&zstencil_desc, &data.zstencil_state);
  if (FAILED(hr)) {
    CapsuleLog("D3d11InitZstencilState: failed to create zstencil state (%x)", hr);
    return false;
  }

  D3D11_RASTERIZER_DESC raster_desc = {};

  raster_desc.FillMode = D3D11_FILL_SOLID;
  raster_desc.CullMode = D3D11_CULL_NONE;

  hr = data.device->CreateRasterizerState(&raster_desc, &data.raster_state);
  if (FAILED(hr)) {
    CapsuleLog("D3d11InitRasterState: failed to create raster state (%x)", hr);
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

static bool D3d11InitQuadVbo(void)
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

  hr = data.device->CreateBuffer(&desc, &srd, &data.vertex_buffer);
  if (FAILED(hr)) {
    CapsuleLog("D3d11InitQuadVbo: failed to create vertex buffer (%x)", hr);
    return false;
  }

  return true;
}

static bool D3d11InitOverlayVbo(void)
{
  // d3d11 coordinate system: (0, 0) = bottom-left
  float cx = (float) data.cx;
  float cy = (float) data.cy;
  float width = (float) data.overlay_width;
  float height = (float) data.overlay_height;
  float x = cx - width;
  float y = 0;

  float l = x / cx * 2.0f - 1.0f;
  float r = (x + width) / cx * 2.0f - 1.0f;
  float b = y / cy * 2.0f - 1.0f;
  float t = (y + height) / cy * 2.0f - 1.0f;

  CapsuleLog("Overlay vbo: left = %.2f, right = %.2f, top = %.2f, bottom = %.2f", l, r, t, b);

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

  hr = data.device->CreateBuffer(&desc, &srd, &data.overlay_vertex_buffer);
  if (FAILED(hr)) {
    CapsuleLog("D3d11InitOverlayVbo: failed to create vertex buffer (%x)", hr);
    return false;
  }

  return true;
}

static bool D3d11InitOverlayTexture(void)
{
  data.overlay_width = 256;
  data.overlay_height = 128;
  data.overlay_pixels = (unsigned char*) malloc(data.overlay_width * 4 * data.overlay_height);

  HRESULT hr;
  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = data.overlay_width;
  desc.Height = data.overlay_height;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DYNAMIC;
  desc.MiscFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  D3D11_SUBRESOURCE_DATA srd = {};
  srd.SysMemPitch = data.overlay_width * 4;
  srd.pSysMem = (const void *) data.overlay_pixels;

  hr = data.device->CreateTexture2D(&desc, &srd, &data.overlay_tex);
  if (FAILED(hr)) {
    CapsuleLog("D3d11InitOverlayTexture: failed to create texture (%x)", hr);
    return false;
  }

  D3D11_SHADER_RESOURCE_VIEW_DESC res_desc = {};
  res_desc.Format = desc.Format;
  res_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  res_desc.Texture2D.MipLevels = 1;

  hr = data.device->CreateShaderResourceView(data.overlay_tex, &res_desc, &data.overlay_resource);
  if (FAILED(hr)) {
    CapsuleLog("D3d11InitOverlayTexture: failed to create rendertarget view (%x)", hr);
    return false;
  }

  return true;
}

static void D3d11Init(IDXGISwapChain *swap) {
  CapsuleLog("Initializing D3D11 capture");

  bool success = true;
  HWND window;
  HRESULT hr;

  hr = swap->GetDevice(__uuidof(ID3D11Device), (void**)&data.device);
  if (FAILED(hr)) {
    CapsuleLog("D3d11Init: failed to get device from swap (%x)", hr);
    return;
  }
  data.device->Release();

  data.device->GetImmediateContext(&data.context);
  data.context->Release();

  data.gpu_color_conv = capdata.settings.gpu_color_conv;

  D3d11InitFormat(swap, &window);

  if (!D3d11InitOverlayTexture() || !D3d11InitOverlayVbo()) {
    exit(1337);
    return;
  }

  if (!D3d11LoadShaders() || !D3d11InitDrawStates() || !D3d11InitQuadVbo()) {
    exit(1337);
    return;
  }

  success = D3d11ShmemInit(window);

  if (!success) {
    D3d11Free();
  }
}

static inline void D3d11CopyTexture (ID3D11Resource *dst, ID3D11Resource *src) {
  if (data.multisampled) {
    data.context->ResolveSubresource(dst, 0, src, 0, data.format);
  } else {
    data.context->CopyResource(dst, src);
  }
}

#define MAX_RENDER_TARGETS             D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT
#define MAX_SO_TARGETS                 4
#define MAX_CLASS_INSTS                256

struct d3d11_state {
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
static void D3d11SaveState(struct d3d11_state *state) {
  state->gs_class_inst_count = MAX_CLASS_INSTS;
  state->ps_class_inst_count = MAX_CLASS_INSTS;
  state->vs_class_inst_count = MAX_CLASS_INSTS;

  data.context->GSGetShader(&state->geom_shader, state->gs_class_instances, &state->gs_class_inst_count);
  data.context->IAGetInputLayout(&state->vertex_layout);
  data.context->IAGetPrimitiveTopology(&state->topology);
  data.context->IAGetVertexBuffers(0, 1, &state->vertex_buffer, &state->vb_stride, &state->vb_offset);
  data.context->OMGetBlendState(&state->blend_state, state->blend_factor, &state->sample_mask);
  data.context->OMGetDepthStencilState(&state->zstencil_state, &state->zstencil_ref);
  data.context->OMGetRenderTargets(MAX_RENDER_TARGETS, state->render_targets, &state->zstencil_view);
  data.context->PSGetSamplers(0, 1, &state->sampler_state);
  data.context->PSGetShader(&state->pixel_shader, state->ps_class_instances, &state->ps_class_inst_count);
  data.context->PSGetShaderResources(0, 1, &state->resource);
  data.context->RSGetState(&state->raster_state);
  data.context->RSGetViewports(&state->num_viewports, nullptr);
  if (state->num_viewports) {
    state->viewports = (D3D11_VIEWPORT*)malloc(sizeof(D3D11_VIEWPORT) * state->num_viewports);
    data.context->RSGetViewports(&state->num_viewports, state->viewports);
  }
  data.context->SOGetTargets(MAX_SO_TARGETS, state->stream_output_targets);
  data.context->VSGetShader(&state->vertex_shader, state->vs_class_instances, &state->vs_class_inst_count);
}

#define SO_APPEND ((UINT)-1)

static void D3d11RestoreState(struct d3d11_state *state) {
  UINT so_offsets[MAX_SO_TARGETS] = { SO_APPEND, SO_APPEND, SO_APPEND, SO_APPEND };

  data.context->GSSetShader(state->geom_shader, state->gs_class_instances, state->gs_class_inst_count);
  data.context->IASetInputLayout(state->vertex_layout);
  data.context->IASetPrimitiveTopology(state->topology);
  data.context->IASetVertexBuffers(0, 1, &state->vertex_buffer, &state->vb_stride, &state->vb_offset);
  data.context->OMSetBlendState(state->blend_state, state->blend_factor, state->sample_mask);
  data.context->OMSetDepthStencilState(state->zstencil_state, state->zstencil_ref);
  data.context->OMSetRenderTargets(MAX_RENDER_TARGETS, state->render_targets, state->zstencil_view);
  data.context->PSSetSamplers(0, 1, &state->sampler_state);
  data.context->PSSetShader(state->pixel_shader, state->ps_class_instances, state->ps_class_inst_count);
  data.context->PSSetShaderResources(0, 1, &state->resource);
  data.context->RSSetState(state->raster_state);
  data.context->RSSetViewports(state->num_viewports, state->viewports);
  data.context->SOSetTargets(MAX_SO_TARGETS, state->stream_output_targets, so_offsets);
  data.context->VSSetShader(state->vertex_shader, state->vs_class_instances, state->vs_class_inst_count);
  SafeRelease(state->geom_shader);
  SafeRelease(state->vertex_layout);
  SafeRelease(state->vertex_buffer);
  SafeRelease(state->blend_state);
  SafeRelease(state->zstencil_state);

  for (size_t i = 0; i < MAX_RENDER_TARGETS; i++)
    SafeRelease(state->render_targets[i]);

  SafeRelease(state->zstencil_view);
  SafeRelease(state->sampler_state);
  SafeRelease(state->pixel_shader);
  SafeRelease(state->resource);
  SafeRelease(state->raster_state);

  for (size_t i = 0; i < MAX_SO_TARGETS; i++) {
    SafeRelease(state->stream_output_targets[i]);
  }

  SafeRelease(state->vertex_shader);

  for (size_t i = 0; i < state->gs_class_inst_count; i++) {
    state->gs_class_instances[i]->Release();
  }
  for (size_t i = 0; i < state->ps_class_inst_count; i++) {
    state->ps_class_instances[i]->Release();
  }
  for (size_t i = 0; i < state->vs_class_inst_count; i++) {
    state->vs_class_instances[i]->Release();
  }
  free(state->viewports);
  memset(state, 0, sizeof(*state));
}


static void D3d11SetupPipeline(ID3D11RenderTargetView *target, ID3D11ShaderResourceView *resource) {
  const float factor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
  D3D11_VIEWPORT viewport = { 0 };
  UINT stride = sizeof(vertex);
  void *emptyptr = nullptr;
  UINT zero = 0;

  viewport.Width = (float)data.cx / data.size_divider;
  viewport.Height = (float)data.cy / data.size_divider;
  viewport.MaxDepth = 1.0f;

  data.context->GSSetShader(nullptr, nullptr, 0);
  data.context->IASetInputLayout(data.vertex_layout);
  data.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  data.context->IASetVertexBuffers(0, 1, &data.vertex_buffer, &stride, &zero);
  data.context->OMSetBlendState(data.blend_state, factor, 0xFFFFFFFF);
  data.context->OMSetDepthStencilState(data.zstencil_state, 0);
  data.context->OMSetRenderTargets(1, &target, nullptr);
  data.context->PSSetSamplers(0, 1, &data.sampler_state);
  data.context->PSSetShader(data.pixel_shader, nullptr, 0);
  data.context->PSSetShaderResources(0, 1, &resource);
  if (data.gpu_color_conv) {
    data.context->PSSetConstantBuffers(0, 1, &data.constants);
  }
  data.context->RSSetState(data.raster_state);
  data.context->RSSetViewports(1, &viewport);
  data.context->SOSetTargets(1, (ID3D11Buffer**)&emptyptr, &zero);
  data.context->VSSetShader(data.vertex_shader, nullptr, 0);
}

static void D3d11SetupOverlayPipeline(ID3D11ShaderResourceView *resource) {
  const float factor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
  D3D11_VIEWPORT viewport = { 0 };
  UINT stride = sizeof(vertex);
  void *emptyptr = nullptr;
  UINT zero = 0;

  viewport.Width = (float)data.cx;
  viewport.Height = (float)data.cy;
  viewport.MaxDepth = 1.0f;

  data.context->GSSetShader(nullptr, nullptr, 0);
  data.context->IASetInputLayout(data.vertex_layout);
  data.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  data.context->IASetVertexBuffers(0, 1, &data.overlay_vertex_buffer, &stride, &zero);
  data.context->OMSetBlendState(data.overlay_blend_state, factor, 0xFFFFFFFF);
  data.context->OMSetDepthStencilState(data.zstencil_state, 0);
  // data.context->OMSetRenderTargets(1, &target, nullptr);
  data.context->PSSetSamplers(0, 1, &data.sampler_state);
  data.context->PSSetShader(data.overlay_pixel_shader, nullptr, 0);
  data.context->PSSetShaderResources(0, 1, &data.overlay_resource);
  data.context->RSSetState(data.raster_state);
  data.context->RSSetViewports(1, &viewport);
  data.context->SOSetTargets(1, (ID3D11Buffer**)&emptyptr, &zero);
  // TODO: overlay shader
  data.context->VSSetShader(data.vertex_shader, nullptr, 0);
}

static inline void D3d11ScaleTexture(ID3D11RenderTargetView *target,
		ID3D11ShaderResourceView *resource) {
	static struct d3d11_state old_state = {0};

	D3d11SaveState(&old_state);
	D3d11SetupPipeline(target, resource);
	data.context->Draw(4, 0);
	D3d11RestoreState(&old_state);
}

static inline void D3d11UpdateOverlayTexture() {
  D3D11_MAPPED_SUBRESOURCE map;
  HRESULT hr;

  hr = data.context->Map(data.overlay_tex, 0,
    D3D11_MAP_WRITE_DISCARD, 0, &map);

  if (FAILED(hr)) {
    CapsuleLog("Could not map overlay tex");
    return;
  }

  size_t pixels_size = data.overlay_width * 4 * data.overlay_height;
  for (int y = 0; y < data.overlay_height; y++) {
    for (int x = 0; x < data.overlay_width; x++) {
      int i = (y * data.overlay_width + x) * 4;
      data.overlay_pixels[i]     = (data.overlay_pixels[i]     + 1) % 256;
      data.overlay_pixels[i + 1] = (data.overlay_pixels[i + 1] + 1) % 256;
      data.overlay_pixels[i + 2] = (data.overlay_pixels[i + 2] + 1) % 256;
      // data.overlay_pixels[i + 3] = (data.overlay_pixels[i + 3] + 1) % 256;
    }
  }
  memcpy(map.pData, data.overlay_pixels, pixels_size);

  data.context->Unmap(data.overlay_tex, 0);
}

static inline void D3d11DrawOverlayInternal() {
  D3d11UpdateOverlayTexture();

	static struct d3d11_state old_state = {0};

	D3d11SaveState(&old_state);
	D3d11SetupOverlayPipeline(data.overlay_resource);
	data.context->Draw(4, 0);
	D3d11RestoreState(&old_state);
}

static inline void D3d11ShmemQueueCopy() {
	for (size_t i = 0; i < capsule::kNumBuffers; i++) {
		D3D11_MAPPED_SUBRESOURCE map;
		HRESULT hr;

		if (data.texture_ready[i]) {
			data.texture_ready[i] = false;

			hr = data.context->Map(data.copy_surfaces[i], 0,
					D3D11_MAP_READ, 0, &map);
			if (SUCCEEDED(hr)) {
				data.texture_mapped[i] = true;
        auto timestamp = data.timestamps[i];
        capsule::io::WriteVideoFrame(timestamp, (char *) map.pData, (data.cy / data.size_divider) * data.pitch);
			}
			break;
		}
	}
}

static inline void D3d11ShmemCapture (ID3D11Resource* backbuffer) {
  int next_tex;

  D3d11ShmemQueueCopy();

  next_tex = (data.cur_tex + 1) % capsule::kNumBuffers;

  data.timestamps[data.cur_tex] = capsule::FrameTimestamp();

  // always using scale
  D3d11CopyTexture(data.scale_tex, backbuffer);
  D3d11ScaleTexture(data.render_targets[data.cur_tex], data.scale_resource);

  if (data.copy_wait < capsule::kNumBuffers - 1) {
    data.copy_wait++;
  } else {
    ID3D11Texture2D *src = data.textures[next_tex];
    ID3D11Texture2D *dst = data.copy_surfaces[next_tex];

    // obs has locking here, we don't
    data.context->Unmap(dst, 0);
    data.texture_mapped[next_tex] = false;

    D3d11CopyTexture(dst, src);
    data.texture_ready[next_tex] = true;
  }

  data.cur_tex = next_tex;
}

void D3d11Capture(void *swap_ptr, void *backbuffer_ptr) {
  static bool first_frame = true;

  if (!capsule::CaptureReady()) {
    if (!capsule::CaptureActive() && !first_frame) {
      first_frame = true;
      D3d11Free();
    }

    return;
  }

  IDXGIResource *dxgi_backbuffer = (IDXGIResource*)backbuffer_ptr;
  IDXGISwapChain *swap = (IDXGISwapChain*)swap_ptr;

  if (!data.device) {
    D3d11Init(swap);
  }

  HRESULT hr;

  ID3D11Resource *backbuffer;

  hr = dxgi_backbuffer->QueryInterface(__uuidof(ID3D11Resource), (void**) &backbuffer);
  if (FAILED(hr)) {
    CapsuleLog("D3d11Capture: failed to get backbuffer");
    return;
  }

  if (first_frame) {
    capsule::PixFmt pix_fmt;
    if (data.gpu_color_conv) {
      pix_fmt = capsule::kPixFmtYuv444P;
    } else {
      pix_fmt = DxgiFormatToPixFmt(data.format);
    }
    capsule::io::WriteVideoFormat(
      data.cx / data.size_divider,
      data.cy / data.size_divider,
      pix_fmt,
      0 /* no vflip */,
      data.pitch
    );
    first_frame = false;
  }

  D3d11ShmemCapture(backbuffer);
  backbuffer->Release();
}

void D3d11DrawOverlay () {
  if (!data.device) {
    return;
  }

  D3d11DrawOverlayInternal();
}

void D3d11Free() {
  SafeRelease(data.scale_tex);
  SafeRelease(data.scale_resource);
  SafeRelease(data.vertex_shader);
  SafeRelease(data.vertex_layout);
  SafeRelease(data.pixel_shader);
  SafeRelease(data.sampler_state);
  SafeRelease(data.blend_state);
  SafeRelease(data.zstencil_state);
  SafeRelease(data.raster_state);
  SafeRelease(data.vertex_buffer);
  SafeRelease(data.constants);

  for (size_t i = 0; i < capsule::kNumBuffers; i++) {
    if (data.copy_surfaces[i]) {
      if (data.texture_mapped[i]) {
        data.context->Unmap(data.copy_surfaces[i], 0);
      }
      data.copy_surfaces[i]->Release();
    }

    SafeRelease(data.textures[i]);
    SafeRelease(data.render_targets[i]);
  }

  memset(&data, 0, sizeof(data));

  CapsuleLog("----------------- d3d11 capture freed ----------------");
}
