
#include <capsule.h>
#include "win-capture.h"
#include "dxgi-util.h"

#include <d3d11.h>
#include <d3dcompiler.h>

#include <dxgi.h>

// inspired by libobs
struct d3d11_data {
  ID3D11Device              *device; // do not release
  ID3D11DeviceContext       *context; // do not release
  uint32_t                  cx; // framebuffer width
  uint32_t                  cy; // framebuffer height
  uint32_t                  size_divider;
  DXGI_FORMAT               format; // pixel format
  DXGI_FORMAT               out_format; // pixel format
  bool                      multisampled; // if true, subresource needs to be resolved on GPU before downloading

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

  ID3D11Texture2D                *copy_surfaces[NUM_BUFFERS]; // staging, CPU_READ_ACCESS
  ID3D11Texture2D                *textures[NUM_BUFFERS]; // default (in-GPU), useful to resolve multisampled backbuffers
  ID3D11RenderTargetView         *render_targets[NUM_BUFFERS];
  bool                           texture_ready[NUM_BUFFERS];
  bool                           texture_mapped[NUM_BUFFERS];
  int64_t                        timestamps[NUM_BUFFERS];
  int                            cur_tex;
  int                            copy_wait;

  uint32_t                       pitch; // linesize, may not be (width * components) because of alignemnt
};

static struct d3d11_data data = {};

HINSTANCE hD3DCompiler = NULL;
pD3DCompile pD3DCompileFunc = NULL;

static const char vertex_shader_string[] =
"struct VertData \
{ \
  float4 pos : SV_Position; \
  float2 texCoord : TexCoord0; \
}; \
VertData main(VertData input) \
{ \
  VertData output; \
  output.pos = input.pos; \
  output.texCoord = input.texCoord; \
  return output; \
}";

static const char pixel_shader_string_old[] =
"uniform Texture2D diffuseTexture; \
SamplerState textureSampler \
{ \
  AddressU = Clamp; \
  AddressV = Clamp; \
  Filter   = Linear; \
}; \
struct VertData \
{ \
  float4 pos      : SV_Position; \
  float2 texCoord : TexCoord0; \
}; \
float4 main(VertData input) : SV_Target \
{ \
  return diffuseTexture.Sample(textureSampler, input.texCoord); \
}";

static const char pixel_shader_string[] =
"\
uniform float width; \
uniform Texture2D diffuseTexture; \
SamplerState textureSampler \
{ \
  AddressU = Clamp; \
  AddressV = Clamp; \
  Filter   = Linear; \
}; \
struct VertData \
{ \
  float4 pos      : SV_Position; \
  float2 texCoord : TexCoord0; \
}; \
float rgbToY(float3 rgb) { \
  return (0.299 * rgb.r + 0.587 * rgb.g + 0.114 * rgb.b) * (1 - 0.0625) + 0.0625; \
} \
float rgbToU(float3 rgb) { \
  return -0.169 * rgb.r - 0.331 * rgb.g + 0.499 * rgb.b + 0.5; \
} \
float rgbToV(float3 rgb) { \
  return 0.499 * rgb.r - 0.418 * rgb.g - 0.0813 * rgb.b + 0.5; \
} \
float4 main(VertData input) : SV_Target \
{ \
  float width_i = 1.0 / width; \
  float u = input.texCoord.x; \
  float2 sample_pos[4]; \
  float uwrap = fmod(u * 4.0, 1.0); \
  sample_pos[0] = float2(uwrap + 0*width_i, input.texCoord.y); \
  sample_pos[1] = float2(uwrap + 1*width_i, input.texCoord.y); \
  sample_pos[2] = float2(uwrap + 2*width_i, input.texCoord.y); \
  sample_pos[3] = float2(uwrap + 3*width_i, input.texCoord.y); \
  float3 samples[4]; \
  samples[0] = diffuseTexture.Sample(textureSampler, sample_pos[0]); \
  samples[1] = diffuseTexture.Sample(textureSampler, sample_pos[1]); \
  samples[2] = diffuseTexture.Sample(textureSampler, sample_pos[2]); \
  samples[3] = diffuseTexture.Sample(textureSampler, sample_pos[3]); \
  if (u > 0.75) { \
    return float4(0, 0, 0, 1); \
  } else if (u > 0.5) { \
    return float4( \
      rgbToV(samples[0]), \
      rgbToV(samples[1]), \
      rgbToV(samples[2]), \
      rgbToV(samples[3]) \
    ); \
  } else if (u > 0.25) { \
    return float4( \
      rgbToU(samples[0]), \
      rgbToU(samples[1]), \
      rgbToU(samples[2]), \
      rgbToU(samples[3]) \
    ); \
  } else { \
    return float4( \
      rgbToY(samples[0]), \
      rgbToY(samples[1]), \
      rgbToY(samples[2]), \
      rgbToY(samples[3]) \
    ); \
  } \
}";

/**
 * Copy the format, size, and multisampling information of the
 * D3D11 backbuffer to our internal state
 */
static bool d3d11_init_format(IDXGISwapChain *swap, HWND *window) {
  DXGI_SWAP_CHAIN_DESC desc;
  HRESULT hr;

  hr = swap->GetDesc(&desc);
  if (FAILED(hr)) {
    capsule_log("d3d11_init_format: swap->GetDesc failed");
    return false;
  }
  data.format = fix_dxgi_format(desc.BufferDesc.Format);
  data.out_format = DXGI_FORMAT_R8G8B8A8_UNORM;
  data.multisampled = desc.SampleDesc.Count > 1;
  *window = desc.OutputWindow;
  data.cx = desc.BufferDesc.Width;
  data.cy = desc.BufferDesc.Height;
  data.size_divider = 2; // testing
  // data.size_divider = 1;

  capsule_log("Backbuffer: %ux%u (%s) format = %s",
    data.cx, data.cy,
    data.multisampled ? "multisampled" : "",
    name_from_dxgi_format(desc.BufferDesc.Format).c_str()
  );

  return true;
}

/**
 * Release a COM resource, if it's non-null
 */
static inline void safe_release(IUnknown *p) {
  if (p) {
    p->Release();
  }
}

/**
 * Create a staging surface (used to download data from the GPU to the CPU)
 */
static bool create_d3d11_stage_surface(ID3D11Texture2D **tex) {
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
    capsule_log("create_d3d11_stage_surface: failed to create texture (%x)", hr);
    return false;
  }

  return true;
}

/**
 * Create an on-GPU texture (used to resolve multisampled backbuffers)
 */
static bool create_d3d11_tex(uint32_t cx, uint32_t cy, ID3D11Texture2D **tex, bool out) {
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
    capsule_log("create_d3d11_tex: failed to create texture (%x)", hr);
    return false;
  }

  return true;
}

/**
 * Initiate both on-GPU textures and copy surfaces (staging).
 */
static bool d3d11_shmem_init_buffers(size_t idx) {
  bool success;

  success = create_d3d11_stage_surface(&data.copy_surfaces[idx]);
  if (!success) {
    capsule_log("d3d11_shmem_init_buffers: failed to create copy surface");
    return false;
  }

  // for the first texture, retrieve pitch (linesize), too
  if (idx == 0) {
    D3D11_MAPPED_SUBRESOURCE map = {};
    HRESULT hr;

    hr = data.context->Map(data.copy_surfaces[idx], 0,
      D3D11_MAP_READ, 0, &map);

    if (FAILED(hr)) {
      capsule_log("d3d11_shmem_init_buffers: failed to get pitch (%x)", hr);
      return false;
    }

    data.pitch = map.RowPitch;
    data.context->Unmap(data.copy_surfaces[idx], 0);
  }

  success = create_d3d11_tex(data.cx / data.size_divider, data.cy / data.size_divider, &data.textures[idx], true);

  if (!success) {
    capsule_log("d3d11_shmem_init_buffers: failed to create texture");
    return false;
  }

  HRESULT hr = data.device->CreateRenderTargetView(data.textures[idx], nullptr, &data.render_targets[idx]);
  if (FAILED(hr)) {
    capsule_log("d3d11_shmem_init_buffers: failed to create rendertarget view (%x)", hr);
  }

  success = create_d3d11_tex(data.cx, data.cy, &data.scale_tex, false);

  if (!success) {
    capsule_log("d3d11_shmem_init_buffers: failed to create scale texture");
    return false;
  }

  D3D11_SHADER_RESOURCE_VIEW_DESC res_desc = {};
  res_desc.Format = data.format;
  res_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  res_desc.Texture2D.MipLevels = 1;

  hr = data.device->CreateShaderResourceView(data.scale_tex, &res_desc, &data.scale_resource);
  if (FAILED(hr)) {
    capsule_log("d3d11_shmem_init_buffers: failed to create rendertarget view (%x)", hr);
  }

  return true;
}

static bool d3d11_shmem_init(HWND window) {
  for (size_t i = 0; i < NUM_BUFFERS; i++) {
    if (!d3d11_shmem_init_buffers(i)) {
      return false;
    }
  }

  capsule_log("d3d11 memory capture initialize successful");
  return true;
}

static bool d3d11_load_shaders() {
  hD3DCompiler = LoadLibraryA(D3DCOMPILER_DLL_A);

  if (!hD3DCompiler) {
    capsule_log("d3d11_load_shaders: couldn't load %s", D3DCOMPILER_DLL_A);
    hD3DCompiler = LoadLibraryA("d3dcompiler_42.dll");
    if (!hD3DCompiler) {
      capsule_log("d3d11_load_shaders: couldn't load d3dcompiler_42.dll, bailing");
      return false;
    }
  }

  pD3DCompileFunc = (pD3DCompile) GetProcAddress(hD3DCompiler, "D3DCompile");
  if (pD3DCompileFunc == NULL) {
    capsule_log("d3d11_load_shaders: D3DCompile was NULL");
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
    capsule_log("d3d11_load_shaders: failed to compile vertex shader (%x)", hr);
    return false;
  }

  vertex_shader_size = (size_t)blob->GetBufferSize();
  memcpy(vertex_shader_data, blob->GetBufferPointer(), blob->GetBufferSize());
  blob->Release();

  hr = data.device->CreateVertexShader(vertex_shader_data, vertex_shader_size, nullptr, &data.vertex_shader);

  if (FAILED(hr)) {
    capsule_log("d3d11_load_shaders: failed to create vertex shader (%x)", hr);
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
    capsule_log("d3d11_load_shaders: failed to create layout (%x)", hr);
    return false;
  }

  // Set up pixel shader

  uint8_t pixel_shader_data[16384];
  size_t pixel_shader_size = 0;
  ID3D10Blob *error_msg = nullptr;

  hr = pD3DCompileFunc(pixel_shader_string, sizeof(pixel_shader_string),
    "pixel_shader_string", nullptr, nullptr, "main",
    "ps_4_0", D3D10_SHADER_OPTIMIZATION_LEVEL1, 0, &blob, &error_msg);

  if (FAILED(hr)) {
    capsule_log("d3d11_load_shaders: failed to compile pixel shader (%x)", hr);
    capsule_log("d3d11_load_shaders: %s", (char*) error_msg->GetBufferPointer());
    return false;
  }

  pixel_shader_size = (size_t)blob->GetBufferSize();
  memcpy(pixel_shader_data, blob->GetBufferPointer(), blob->GetBufferSize());
  blob->Release();

  hr = data.device->CreatePixelShader(pixel_shader_data, pixel_shader_size, nullptr, &data.pixel_shader);

  if (FAILED(hr)) {
    capsule_log("d3d11_load_shaders: failed to create pixel shader (%x)", hr);
    return false;
  }

  D3D11_BUFFER_DESC bd;
  memset(&bd, 0, sizeof(bd));

  // keep in sync with shader string
  int constant_size = sizeof(float);

  capsule_log("d3d11_load_shaders: constant size = %d", constant_size);

  bd.ByteWidth = (constant_size+15) & 0xFFFFFFF0; // align
  bd.Usage = D3D11_USAGE_DYNAMIC;
  bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  hr = data.device->CreateBuffer(&bd, nullptr, &data.constants);
  if (FAILED(hr)) {
    capsule_log("d3d11_load_shaders: failed to create constant buffer (%x)", hr);
    return false;
  }

  struct {
    float size;
    int64_t pad1;
    int64_t pad2;
    int64_t pad3;
  } initial_data = {
    data.cx / data.size_divider,
    0,
    0,
    0
  };

  {
    D3D11_MAPPED_SUBRESOURCE map;
    HRESULT hr;

    hr = data.context->Map(data.constants, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
    if (FAILED(hr)) {
      capsule_log("d3d11_load_shaders: could not lock constants buffer (%x)", hr);
      return false;
    }

    memcpy(map.pData, &initial_data, bd.ByteWidth);
    data.context->Unmap(data.constants, 0);
  }

  return true;
}

static bool d3d11_init_draw_states(void)
{
  HRESULT hr;

  D3D11_SAMPLER_DESC sampler_desc = {};
  sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

  hr = data.device->CreateSamplerState(&sampler_desc, &data.sampler_state);
  if (FAILED(hr)) {
    capsule_log("d3d11_init_draw_states: failed to create sampler state (%x)", hr);
    return false;
  }

  D3D11_BLEND_DESC blend_desc = {};

  for (size_t i = 0; i < 8; i++) {
    blend_desc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
  }

  hr = data.device->CreateBlendState(&blend_desc, &data.blend_state);
  if (FAILED(hr)) {
    capsule_log("d3d11_init_blend_state: failed to create blend state (%x)", hr);
    return false;
  }

  D3D11_DEPTH_STENCIL_DESC zstencil_desc = {};

  hr = data.device->CreateDepthStencilState(&zstencil_desc, &data.zstencil_state);
  if (FAILED(hr)) {
    capsule_log("d3d11_init_zstencil_state: failed to create zstencil state (%x)", hr);
    return false;
  }

  D3D11_RASTERIZER_DESC raster_desc = {};

  raster_desc.FillMode = D3D11_FILL_SOLID;
  raster_desc.CullMode = D3D11_CULL_NONE;

  hr = data.device->CreateRasterizerState(&raster_desc, &data.raster_state);
  if (FAILED(hr)) {
    capsule_log("d3d11_init_raster_state: failed to create raster state (%x)", hr);
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

static bool d3d11_init_quad_vbo(void)
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
    capsule_log("d3d11_init_vertex_buffer: failed to create vertex buffer (%x)", hr);
    return false;
  }

  return true;
}

static void d3d11_init(IDXGISwapChain *swap) {
  capsule_log("Initializing D3D11 capture");

  bool success = true;
  HWND window;
  HRESULT hr;

  hr = swap->GetDevice(__uuidof(ID3D11Device), (void**)&data.device);
  if (FAILED(hr)) {
    capsule_log("d3d11_init: failed to get device from swap (%x)", hr);
    return;
  }
  data.device->Release();

  data.device->GetImmediateContext(&data.context);
  data.context->Release();

  d3d11_init_format(swap, &window);

  if (!d3d11_load_shaders() || !d3d11_init_draw_states() || !d3d11_init_quad_vbo()) {
    exit(1337);
    return;
  }

  success = d3d11_shmem_init(window);

  if (!success) {
    d3d11_free();
  }
}

static inline void d3d11_copy_texture (ID3D11Resource *dst, ID3D11Resource *src) {
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
static void d3d11_save_state(struct d3d11_state *state) {
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

static void d3d11_restore_state(struct d3d11_state *state) {
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
  safe_release(state->geom_shader);
  safe_release(state->vertex_layout);
  safe_release(state->vertex_buffer);
  safe_release(state->blend_state);
  safe_release(state->zstencil_state);

  for (size_t i = 0; i < MAX_RENDER_TARGETS; i++)
    safe_release(state->render_targets[i]);

  safe_release(state->zstencil_view);
  safe_release(state->sampler_state);
  safe_release(state->pixel_shader);
  safe_release(state->resource);
  safe_release(state->raster_state);

  for (size_t i = 0; i < MAX_SO_TARGETS; i++)
    safe_release(state->stream_output_targets[i]);

  safe_release(state->vertex_shader);

  for (size_t i = 0; i < state->gs_class_inst_count; i++)
    state->gs_class_instances[i]->Release();
  for (size_t i = 0; i < state->ps_class_inst_count; i++)
    state->ps_class_instances[i]->Release();
  for (size_t i = 0; i < state->vs_class_inst_count; i++)
    state->vs_class_instances[i]->Release();
  free(state->viewports);
  memset(state, 0, sizeof(*state));
}


static void d3d11_setup_pipeline(ID3D11RenderTargetView *target, ID3D11ShaderResourceView *resource) {
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
  data.context->PSSetConstantBuffers(0, 1, &data.constants);
  data.context->RSSetState(data.raster_state);
  data.context->RSSetViewports(1, &viewport);
  data.context->SOSetTargets(1, (ID3D11Buffer**)&emptyptr, &zero);
  data.context->VSSetShader(data.vertex_shader, nullptr, 0);
}

static inline void d3d11_scale_texture(ID3D11RenderTargetView *target,
		ID3D11ShaderResourceView *resource) {
	static struct d3d11_state old_state = {0};

	d3d11_save_state(&old_state);
	d3d11_setup_pipeline(target, resource);
	data.context->Draw(4, 0);
	d3d11_restore_state(&old_state);
}

static inline void d3d11_shmem_queue_copy() {
	for (size_t i = 0; i < NUM_BUFFERS; i++) {
		D3D11_MAPPED_SUBRESOURCE map;
		HRESULT hr;

		if (data.texture_ready[i]) {
			data.texture_ready[i] = false;

			hr = data.context->Map(data.copy_surfaces[i], 0,
					D3D11_MAP_READ, 0, &map);
			if (SUCCEEDED(hr)) {
				data.texture_mapped[i] = true;
        auto timestamp = data.timestamps[i];
        capsule_write_video_frame(timestamp, (char *) map.pData, (data.cy / data.size_divider) * data.pitch);
			}
			break;
		}
	}
}

static inline void d3d11_shmem_capture (ID3D11Resource* backbuffer) {
  int next_tex;

  d3d11_shmem_queue_copy();

  next_tex = (data.cur_tex + 1) % NUM_BUFFERS;

  data.timestamps[data.cur_tex] = capsule_frame_timestamp();

  // always using scale
  d3d11_copy_texture(data.scale_tex, backbuffer);
  d3d11_scale_texture(data.render_targets[data.cur_tex], data.scale_resource);

  if (data.copy_wait < NUM_BUFFERS - 1) {
    data.copy_wait++;
  } else {
    ID3D11Texture2D *src = data.textures[next_tex];
    ID3D11Texture2D *dst = data.copy_surfaces[next_tex];

    // obs has locking here, we don't
    data.context->Unmap(dst, 0);
    data.texture_mapped[next_tex] = false;

    d3d11_copy_texture(dst, src);
    data.texture_ready[next_tex] = true;
  }

  data.cur_tex = next_tex;
}

void d3d11_capture(void *swap_ptr, void *backbuffer_ptr) {
  static bool first_frame = true;

  if (!capsule_capture_ready()) {
    if (!capsule_capture_active() && !first_frame) {
      first_frame = true;
      d3d11_free();
    }

    return;
  }

  IDXGIResource *dxgi_backbuffer = (IDXGIResource*)backbuffer_ptr;
  IDXGISwapChain *swap = (IDXGISwapChain*)swap_ptr;

  if (!data.device) {
    d3d11_init(swap);
  }

  HRESULT hr;

  ID3D11Resource *backbuffer;

  hr = dxgi_backbuffer->QueryInterface(__uuidof(ID3D11Resource), (void**) &backbuffer);
  if (FAILED(hr)) {
    capsule_log("d3d11_capture: failed to get backbuffer");
    return;
  }

  if (first_frame) {
    auto pix_fmt = dxgi_format_to_pix_fmt(data.format);
    capsule_write_video_format(
      data.cx / data.size_divider,
      data.cy / data.size_divider,
      pix_fmt,
      0 /* no vflip */,
      data.pitch
    );
    first_frame = false;
  }

  d3d11_shmem_capture(backbuffer);
  backbuffer->Release();
}

#define SAFE_RELEASE(x) if ((x)) { (x)->Release(); }

void d3d11_free() {
  SAFE_RELEASE(data.scale_tex);
  SAFE_RELEASE(data.scale_resource);
  SAFE_RELEASE(data.vertex_shader);
  SAFE_RELEASE(data.vertex_layout);
  SAFE_RELEASE(data.pixel_shader);
  SAFE_RELEASE(data.sampler_state);
  SAFE_RELEASE(data.blend_state);
  SAFE_RELEASE(data.zstencil_state);
  SAFE_RELEASE(data.raster_state);
  SAFE_RELEASE(data.vertex_buffer);
  SAFE_RELEASE(data.constants);

  for (size_t i = 0; i < NUM_BUFFERS; i++) {
    if (data.copy_surfaces[i]) {
      if (data.texture_mapped[i]) {
        data.context->Unmap(data.copy_surfaces[i], 0);
      }
      data.copy_surfaces[i]->Release();
    }

    SAFE_RELEASE(data.textures[i]);
    SAFE_RELEASE(data.render_targets[i]);
  }

  memset(&data, 0, sizeof(data));

  capsule_log("----------------- d3d11 capture freed ----------------");
}
