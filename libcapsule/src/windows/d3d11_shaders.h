#pragma once

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

static const char pixel_shader_string_overlay[] =
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
  float4 res = diffuseTexture.Sample(textureSampler, input.texCoord); \
  res.a *= 0.4; \
  return res; \
}";

static const char pixel_shader_string_noconv[] =
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

static const char pixel_shader_string_conv[] =
"\
static const float PRECISION_OFFSET = 0.2; \
\
uniform float width; \
uniform float width_i; \
uniform float height; \
uniform float height_i; \
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
  return (0.299 * rgb.r + 0.587 * rgb.g + 0.114 * rgb.b) * ((236.0 - 16.0) / 256.0) + (16.0 / 256.0); \
} \
float rgbToU(float3 rgb) { \
  return -0.169 * rgb.r - 0.331 * rgb.g + 0.499 * rgb.b + 0.5; \
} \
float rgbToV(float3 rgb) { \
  return 0.499 * rgb.r - 0.418 * rgb.g - 0.0813 * rgb.b + 0.5; \
} \
float4 main(VertData input) : SV_Target \
{ \
  float u = input.texCoord.x; \
  float v = input.texCoord.y; \
  float u_val = floor(fmod(u * width * 4.0 + PRECISION_OFFSET, width)) * width_i; \
  float v_val = floor(v * height + PRECISION_OFFSET) * height_i; \
  float2 sample_pos = float2(u_val, v_val); \
  sample_pos.x -= width_i * 1.5; \
  sample_pos.y += height_i * 0.5; \
  float3 samples[4]; \
  samples[0] = diffuseTexture.Sample(textureSampler, sample_pos); \
  sample_pos.x += width_i; \
  samples[1] = diffuseTexture.Sample(textureSampler, sample_pos); \
  sample_pos.x += width_i; \
  samples[2] = diffuseTexture.Sample(textureSampler, sample_pos); \
  sample_pos.x += width_i; \
  samples[3] = diffuseTexture.Sample(textureSampler, sample_pos); \
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
