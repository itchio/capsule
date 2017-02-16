
#pragma once

#include <ctype.h>
#include <wchar.h>
#include <string>
#include <vector>

#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <D3Dcompiler.h>
#include <d3d9.h>
#include <DirectXMath.h>

// When logging, it's not very helpful to have long sequences of hex instead of
// the actual names of the objects in question.
// e.g.
// DEFINE_GUID(IID_IDXGIFactory,0x7b7166ec,0x21c7,0x44ae,0xb2,0x1a,0xc9,0xae,0x32,0x1a,0xe3,0x69);
// 

static std::string NameFromIID(IID id) {
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

static inline DXGI_FORMAT fix_dxgi_format(DXGI_FORMAT format) {
	switch ((unsigned long)format) {
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			return DXGI_FORMAT_B8G8R8A8_UNORM;
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			return DXGI_FORMAT_R8G8B8A8_UNORM;
	}

	return format;
}
