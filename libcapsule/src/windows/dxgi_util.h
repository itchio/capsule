
/*
 *  capsule - the game recording and overlay toolkit
 *  Copyright (C) 2017, Amos Wenger
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details:
 * https://github.com/itchio/capsule/blob/master/LICENSE
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#pragma once

#include <ctype.h>
#include <wchar.h>
#include <string>
#include <vector>

#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <dxgi1_3.h>
#include <D3Dcompiler.h>
#include <d3d9.h>
#include <DirectXMath.h>

#include "../capsule/messages_generated.h"
#include "../logging.h"

namespace capsule {
namespace dxgi {

// When logging, it's not very helpful to have long sequences of hex instead of
// the actual names of the objects in question.
// e.g.
// DEFINE_GUID(IID_IDXGIFactory,0x7b7166ec,0x21c7,0x44ae,0xb2,0x1a,0xc9,0xae,0x32,0x1a,0xe3,0x69);
// 

static std::string NameFromIid(IID id) {
	// Adding every MIDL_INTERFACE from d3d11_1.h to make this reporting complete.
	// Doesn't seem useful to do every object from d3d11.h itself.

	if (__uuidof(IUnknown) == id)
		return "IUnknown";

	if (__uuidof(ID3D11DeviceChild) == id)
		return "ID3D11DeviceChild";
	if (__uuidof(ID3DDeviceContextState) == id)
		return "ID3DDeviceContextState";

	if (__uuidof(IDirect3DDevice9) == id)
		return "IDirect3DDevice9";
	if (__uuidof(ID3D10Device) == id)
		return "ID3D10Device";
	if (__uuidof(ID3D11Device) == id)
		return "ID3D11Device";
	if (__uuidof(ID3D11Device1) == id)
		return "ID3D11Device1";
	//if (__uuidof(ID3D11Device2) == id)  for d3d11_2.h when the time comes
	//	return "ID3D11Device2";

	if (__uuidof(ID3D11DeviceContext) == id)
		return "ID3D11DeviceContext";
	if (__uuidof(ID3D11DeviceContext1) == id)
		return "ID3D11DeviceContext1";
	//if (__uuidof(ID3D11DeviceContext2) == id) for d3d11_2.h when the time comes
	//	return "ID3D11DeviceContext2";

	if (__uuidof(ID3D11InfoQueue) == id)
		return "ID3D11InfoQueue";
	if (__uuidof(ID3DUserDefinedAnnotation) == id)
		return "ID3DUserDefinedAnnotation";

	if (__uuidof(ID3D11BlendState) == id)
		return "ID3D11BlendState";
	if (__uuidof(ID3D11BlendState1) == id)
		return "ID3D11BlendState1";
	if (__uuidof(ID3D11RasterizerState) == id)
		return "ID3D11RasterizerState";
	if (__uuidof(ID3D11RasterizerState1) == id)
		return "ID3D11RasterizerState1";

	if (__uuidof(ID3D11Texture2D) == id)	// Used to fetch backbuffer
		return "ID3D11Texture2D";

	// All the DXGI interfaces from dxgi.h, and dxgi1_2.h

	if (__uuidof(IDXGIObject) == id)
		return "IDXGIObject";
	if (__uuidof(IDXGIDeviceSubObject) == id)
		return "IDXGIDeviceSubObject";

	if (__uuidof(IDXGIFactory) == id)
		return "IDXGIFactory";
	if (__uuidof(IDXGIFactory1) == id)
		return "IDXGIFactory1";
	if (__uuidof(IDXGIFactory2) == id)
		return "IDXGIFactory2";

	if (__uuidof(IDXGIDevice) == id)
		return "IDXGIDevice";
	if (__uuidof(IDXGIDevice1) == id)
		return "IDXGIDevice1";
	if (__uuidof(IDXGIDevice2) == id)
		return "IDXGIDevice2";

	if (__uuidof(IDXGISwapChain) == id)
		return "IDXGISwapChain";
	if (__uuidof(IDXGISwapChain1) == id)
		return "IDXGISwapChain1";

	if (__uuidof(IDXGIAdapter) == id)
		return "IDXGIAdapter";
	if (__uuidof(IDXGIAdapter1) == id)
		return "IDXGIAdapter1";
	if (__uuidof(IDXGIAdapter2) == id)
		return "IDXGIAdapter2";

	if (__uuidof(IDXGIOutputDuplication) == id)
		return "IDXGIOutputDuplication";
	if (__uuidof(IDXGIDisplayControl) == id)
		return "IDXGIDisplayControl";

	if (__uuidof(IDXGIOutput) == id)
		return "IDXGIOutput";
	if (__uuidof(IDXGIOutput1) == id)
		return "IDXGIOutput1";
	if (__uuidof(IDXGIResource) == id)
		return "IDXGIResource";
	if (__uuidof(IDXGIResource1) == id)
		return "IDXGIResource1";
	if (__uuidof(IDXGISurface) == id)
		return "IDXGISurface";
	if (__uuidof(IDXGISurface1) == id)
		return "IDXGIResource";
	if (__uuidof(IDXGISurface2) == id)
		return "IDXGISurface2";
	if (__uuidof(IDXGIKeyedMutex) == id)
		return "IDXGIKeyedMutex";

	// For unknown IIDs lets return the hex string.
	// Converting from wchar_t to string using stackoverflow suggestion.

	std::string iidString;
	wchar_t wiid[128];
	if (SUCCEEDED(StringFromGUID2(id, wiid, 128)))
	{
		std::wstring convert = std::wstring(wiid);
		iidString = std::string(convert.begin(), convert.end());
	}
	else
	{
		iidString = "unknown";
	}

	return iidString;
}

static std::string NameFromFormat(DXGI_FORMAT format) {
  if (format == DXGI_FORMAT_UNKNOWN                     ) return "UNKNOWN";
  if (format == DXGI_FORMAT_R32G32B32A32_TYPELESS       ) return "R32G32B32A32_TYPELESS";
  if (format == DXGI_FORMAT_R32G32B32A32_FLOAT          ) return "R32G32B32A32_FLOAT";
  if (format == DXGI_FORMAT_R32G32B32A32_UINT           ) return "R32G32B32A32_UINT";
  if (format == DXGI_FORMAT_R32G32B32A32_SINT           ) return "R32G32B32A32_SINT";
  if (format == DXGI_FORMAT_R32G32B32_TYPELESS          ) return "R32G32B32_TYPELESS";
  if (format == DXGI_FORMAT_R32G32B32_FLOAT             ) return "R32G32B32_FLOAT";
  if (format == DXGI_FORMAT_R32G32B32_UINT              ) return "R32G32B32_UINT";
  if (format == DXGI_FORMAT_R32G32B32_SINT              ) return "R32G32B32_SINT";
  if (format == DXGI_FORMAT_R16G16B16A16_TYPELESS       ) return "R16G16B16A16_TYPELESS";
  if (format == DXGI_FORMAT_R16G16B16A16_FLOAT          ) return "R16G16B16A16_FLOAT";
  if (format == DXGI_FORMAT_R16G16B16A16_UNORM          ) return "R16G16B16A16_UNORM";
  if (format == DXGI_FORMAT_R16G16B16A16_UINT           ) return "R16G16B16A16_UINT";
  if (format == DXGI_FORMAT_R16G16B16A16_SNORM          ) return "R16G16B16A16_SNORM";
  if (format == DXGI_FORMAT_R16G16B16A16_SINT           ) return "R16G16B16A16_SINT";
  if (format == DXGI_FORMAT_R32G32_TYPELESS             ) return "R32G32_TYPELESS";
  if (format == DXGI_FORMAT_R32G32_FLOAT                ) return "R32G32_FLOAT";
  if (format == DXGI_FORMAT_R32G32_UINT                 ) return "R32G32_UINT";
  if (format == DXGI_FORMAT_R32G32_SINT                 ) return "R32G32_SINT";
  if (format == DXGI_FORMAT_R32G8X24_TYPELESS           ) return "R32G8X24_TYPELESS";
  if (format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT        ) return "D32_FLOAT_S8X24_UINT";
  if (format == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS    ) return "R32_FLOAT_X8X24_TYPELESS";
  if (format == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT     ) return "X32_TYPELESS_G8X24_UINT";
  if (format == DXGI_FORMAT_R10G10B10A2_TYPELESS        ) return "R10G10B10A2_TYPELESS";
  if (format == DXGI_FORMAT_R10G10B10A2_UNORM           ) return "R10G10B10A2_UNORM";
  if (format == DXGI_FORMAT_R10G10B10A2_UINT            ) return "R10G10B10A2_UINT";
  if (format == DXGI_FORMAT_R11G11B10_FLOAT             ) return "R11G11B10_FLOAT";
  if (format == DXGI_FORMAT_R8G8B8A8_TYPELESS           ) return "R8G8B8A8_TYPELESS";
  if (format == DXGI_FORMAT_R8G8B8A8_UNORM              ) return "R8G8B8A8_UNORM";
  if (format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB         ) return "R8G8B8A8_UNORM_SRGB";
  if (format == DXGI_FORMAT_R8G8B8A8_UINT               ) return "R8G8B8A8_UINT";
  if (format == DXGI_FORMAT_R8G8B8A8_SNORM              ) return "R8G8B8A8_SNORM";
  if (format == DXGI_FORMAT_R8G8B8A8_SINT               ) return "R8G8B8A8_SINT";
  if (format == DXGI_FORMAT_R16G16_TYPELESS             ) return "R16G16_TYPELESS";
  if (format == DXGI_FORMAT_R16G16_FLOAT                ) return "R16G16_FLOAT";
  if (format == DXGI_FORMAT_R16G16_UNORM                ) return "R16G16_UNORM";
  if (format == DXGI_FORMAT_R16G16_UINT                 ) return "R16G16_UINT";
  if (format == DXGI_FORMAT_R16G16_SNORM                ) return "R16G16_SNORM";
  if (format == DXGI_FORMAT_R16G16_SINT                 ) return "R16G16_SINT";
  if (format == DXGI_FORMAT_R32_TYPELESS                ) return "R32_TYPELESS";
  if (format == DXGI_FORMAT_D32_FLOAT                   ) return "D32_FLOAT";
  if (format == DXGI_FORMAT_R32_FLOAT                   ) return "R32_FLOAT";
  if (format == DXGI_FORMAT_R32_UINT                    ) return "R32_UINT";
  if (format == DXGI_FORMAT_R32_SINT                    ) return "R32_SINT";
  if (format == DXGI_FORMAT_R24G8_TYPELESS              ) return "R24G8_TYPELESS";
  if (format == DXGI_FORMAT_D24_UNORM_S8_UINT           ) return "D24_UNORM_S8_UINT";
  if (format == DXGI_FORMAT_R24_UNORM_X8_TYPELESS       ) return "R24_UNORM_X8_TYPELESS";
  if (format == DXGI_FORMAT_X24_TYPELESS_G8_UINT        ) return "X24_TYPELESS_G8_UINT";
  if (format == DXGI_FORMAT_R8G8_TYPELESS               ) return "R8G8_TYPELESS";
  if (format == DXGI_FORMAT_R8G8_UNORM                  ) return "R8G8_UNORM";
  if (format == DXGI_FORMAT_R8G8_UINT                   ) return "R8G8_UINT";
  if (format == DXGI_FORMAT_R8G8_SNORM                  ) return "R8G8_SNORM";
  if (format == DXGI_FORMAT_R8G8_SINT                   ) return "R8G8_SINT";
  if (format == DXGI_FORMAT_R16_TYPELESS                ) return "R16_TYPELESS";
  if (format == DXGI_FORMAT_R16_FLOAT                   ) return "R16_FLOAT";
  if (format == DXGI_FORMAT_D16_UNORM                   ) return "D16_UNORM";
  if (format == DXGI_FORMAT_R16_UNORM                   ) return "R16_UNORM";
  if (format == DXGI_FORMAT_R16_UINT                    ) return "R16_UINT";
  if (format == DXGI_FORMAT_R16_SNORM                   ) return "R16_SNORM";
  if (format == DXGI_FORMAT_R16_SINT                    ) return "R16_SINT";
  if (format == DXGI_FORMAT_R8_TYPELESS                 ) return "R8_TYPELESS";
  if (format == DXGI_FORMAT_R8_UNORM                    ) return "R8_UNORM";
  if (format == DXGI_FORMAT_R8_UINT                     ) return "R8_UINT";
  if (format == DXGI_FORMAT_R8_SNORM                    ) return "R8_SNORM";
  if (format == DXGI_FORMAT_R8_SINT                     ) return "R8_SINT";
  if (format == DXGI_FORMAT_A8_UNORM                    ) return "A8_UNORM";
  if (format == DXGI_FORMAT_R1_UNORM                    ) return "R1_UNORM";
  if (format == DXGI_FORMAT_R9G9B9E5_SHAREDEXP          ) return "R9G9B9E5_SHAREDEXP";
  if (format == DXGI_FORMAT_R8G8_B8G8_UNORM             ) return "R8G8_B8G8_UNORM";
  if (format == DXGI_FORMAT_G8R8_G8B8_UNORM             ) return "G8R8_G8B8_UNORM";
  if (format == DXGI_FORMAT_BC1_TYPELESS                ) return "BC1_TYPELESS";
  if (format == DXGI_FORMAT_BC1_UNORM                   ) return "BC1_UNORM";
  if (format == DXGI_FORMAT_BC1_UNORM_SRGB              ) return "BC1_UNORM_SRGB";
  if (format == DXGI_FORMAT_BC2_TYPELESS                ) return "BC2_TYPELESS";
  if (format == DXGI_FORMAT_BC2_UNORM                   ) return "BC2_UNORM";
  if (format == DXGI_FORMAT_BC2_UNORM_SRGB              ) return "BC2_UNORM_SRGB";
  if (format == DXGI_FORMAT_BC3_TYPELESS                ) return "BC3_TYPELESS";
  if (format == DXGI_FORMAT_BC3_UNORM                   ) return "BC3_UNORM";
  if (format == DXGI_FORMAT_BC3_UNORM_SRGB              ) return "BC3_UNORM_SRGB";
  if (format == DXGI_FORMAT_BC4_TYPELESS                ) return "BC4_TYPELESS";
  if (format == DXGI_FORMAT_BC4_UNORM                   ) return "BC4_UNORM";
  if (format == DXGI_FORMAT_BC4_SNORM                   ) return "FORMAT_BC4_SNORM";
  if (format == DXGI_FORMAT_BC5_TYPELESS                ) return "BC5_TYPELESS";
  if (format == DXGI_FORMAT_BC5_UNORM                   ) return "BC5_UNORM";
  if (format == DXGI_FORMAT_BC5_SNORM                   ) return "BC5_SNORM";
  if (format == DXGI_FORMAT_B5G6R5_UNORM                ) return "B5G6R5_UNORM";
  if (format == DXGI_FORMAT_B5G5R5A1_UNORM              ) return "B5G5R5A1_UNORM";
  if (format == DXGI_FORMAT_B8G8R8A8_UNORM              ) return "B8G8R8A8_UNORM";
  if (format == DXGI_FORMAT_B8G8R8X8_UNORM              ) return "B8G8R8X8_UNORM";
  if (format == DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM  ) return "R10G10B10_XR_BIAS_A2_UNORM";
  if (format == DXGI_FORMAT_B8G8R8A8_TYPELESS           ) return "B8G8R8A8_TYPELESS";
  if (format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB         ) return "B8G8R8A8_UNORM_SRGB";
  if (format == DXGI_FORMAT_B8G8R8X8_TYPELESS           ) return "B8G8R8X8_TYPELESS";
  if (format == DXGI_FORMAT_B8G8R8X8_UNORM_SRGB         ) return "B8G8R8X8_UNORM_SRGB";
  if (format == DXGI_FORMAT_BC6H_TYPELESS               ) return "BC6H_TYPELESS";
  if (format == DXGI_FORMAT_BC6H_UF16                   ) return "BC6H_UF16";
  if (format == DXGI_FORMAT_BC6H_SF16                   ) return "BC6H_SF16";
  if (format == DXGI_FORMAT_BC7_TYPELESS                ) return "BC7_TYPELESS";
  if (format == DXGI_FORMAT_BC7_UNORM                   ) return "BC7_UNORM";
  if (format == DXGI_FORMAT_BC7_UNORM_SRGB              ) return "BC7_UNORM_SRGB";
  if (format == DXGI_FORMAT_AYUV                        ) return "AYUV";
  if (format == DXGI_FORMAT_Y410                        ) return "Y410";
  if (format == DXGI_FORMAT_Y416                        ) return "Y416";
  if (format == DXGI_FORMAT_NV12                        ) return "NV12";
  if (format == DXGI_FORMAT_P010                        ) return "P010";
  if (format == DXGI_FORMAT_P016                        ) return "P016";
  if (format == DXGI_FORMAT_420_OPAQUE                  ) return "420_OPAQUE";
  if (format == DXGI_FORMAT_YUY2                        ) return "YUY2";
  if (format == DXGI_FORMAT_Y210                        ) return "Y210";
  if (format == DXGI_FORMAT_Y216                        ) return "Y216";
  if (format == DXGI_FORMAT_NV11                        ) return "NV11";
  if (format == DXGI_FORMAT_AI44                        ) return "AI44";
  if (format == DXGI_FORMAT_IA44                        ) return "IA44";
  if (format == DXGI_FORMAT_P8                          ) return "P8";
  if (format == DXGI_FORMAT_A8P8                        ) return "A8P8";
  if (format == DXGI_FORMAT_B4G4R4A4_UNORM              ) return "B4G4R4A4_UNORM";
  // if (format == DXGI_FORMAT_P208                        ) return "P208";
  // if (format == DXGI_FORMAT_V208                        ) return "V208";
  // if (format == DXGI_FORMAT_V408                        ) return "V408";
  if (format == DXGI_FORMAT_FORCE_UINT                  ) return "FORCE_UINT";
  return "<unrecognized format>";
}

static inline DXGI_FORMAT FixFormat(DXGI_FORMAT format) {
	switch ((unsigned long)format) {
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			return DXGI_FORMAT_B8G8R8A8_UNORM;
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			return DXGI_FORMAT_R8G8B8A8_UNORM;
	}

	return format;
}

static inline messages::PixFmt FormatToPixFmt(DXGI_FORMAT format) {
  switch (FixFormat(format)) {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
      return messages::PixFmt_BGRA;
    case DXGI_FORMAT_B8G8R8X8_UNORM:
      return messages::PixFmt_BGRA;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
      return messages::PixFmt_RGBA;
    case DXGI_FORMAT_R10G10B10A2_UNORM:
      return messages::PixFmt_RGB10_A2;
    default:
      Log("Unsupported DXGI format %s", NameFromFormat(format).c_str());
      return messages::PixFmt_UNKNOWN;
  }
}

} // namespace dxgi
} // namespace capsule
