
#include "capsule.h"

// #include <d3d11.h>

// typedef HRESULT(*CreateDXGIFactoryType)(REFIID, void**);

void capsule_d3d11_sniff() {
  capsule_log("Sniffing Direct3D11!\n");

  //HRESULT result;
  //IDXGIFactory* factory;
  //IDXGIAdapter* adapter;
  //IDXGIOutput* adapterOutput;
  //unsigned int numModes, i, numerator, denominator, stringLength;
  //DXGI_MODE_DESC* displayModeList;
  //DXGI_ADAPTER_DESC adapterDesc;
  //int error;
  //DXGI_SWAP_CHAIN_DESC swapChainDesc;
  //D3D_FEATURE_LEVEL featureLevel;

  //capsule_log("Getting dxgi.dll");

  //HMODULE dxgiModule = GetModuleHandle("dxgi.dll");
  //if (!dxgiModule) {
  //  return;
  //}

  //capsule_log("Got dxgi.dll\n");

  //CreateDXGIFactoryType _createDXGIFactory = (CreateDXGIFactoryType) GetProcAddress(dxgiModule, "CreateDXGIFactory");
  //if (!_createDXGIFactory) {
  //  return;
  //}

  //capsule_log("Got CreateDXGIFactory");

  //// Create a DirectX graphics interface factory.
  //result = _createDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
  //if (FAILED(result))
  //{
  //  return;
  //}

  //// Use the factory to create an adapter for the primary graphics interface (video card).
  //result = factory->EnumAdapters(0, &adapter);
  //if (FAILED(result))
  //{
  //  return;
  //}

  //// Enumerate the primary adapter output (monitor).
  //result = adapter->EnumOutputs(0, &adapterOutput);
  //if (FAILED(result))
  //{
  //  return;
  //}

  //// Get the number of modes that fit the DXGI_FORMAT_R8G8B8A8_UNORM display format for the adapter output (monitor).
  //result = adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, NULL);
  //if (FAILED(result))
  //{
  //  return;
  //}

  //// Create a list to hold all the possible display modes for this monitor/video card combination.
  //displayModeList = new DXGI_MODE_DESC[numModes];
  //if (!displayModeList)
  //{
  //  return;
  //}

  //// Now fill the display mode list structures.
  //result = adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, displayModeList);
  //if (FAILED(result))
  //{
  //  return;
  //}

  //int screenWidth;
  //int screenHeight;

  //// Now go through all the display modes and find the one that matches the screen width and height.
  //// When a match is found store the numerator and denominator of the refresh rate for that monitor.
  //for (i = 0; i<numModes; i++)
  //{
  //  if (displayModeList[i].Width == (unsigned int)screenWidth)
  //  {
  //    if (displayModeList[i].Height == (unsigned int)screenHeight)
  //    {
  //      numerator = displayModeList[i].RefreshRate.Numerator;
  //      denominator = displayModeList[i].RefreshRate.Denominator;
  //    }
  //  }
  //}

  //// Get the adapter (video card) description.
  //result = adapter->GetDesc(&adapterDesc);
  //if (FAILED(result))
  //{
  //  return;
  //}

  //// Store the dedicated video card memory in megabytes.
  //int m_videoCardMemory = (int)(adapterDesc.DedicatedVideoMemory / 1024 / 1024);

  //LPTSTR m_videoCardDescription = (LPTSTR)calloc(128, 1);

  //// Convert the name of the video card to a character array and store it.
  //error = wcstombs_s(&stringLength, m_videoCardDescription, 128, adapterDesc.Description, 128);
  //if (error != 0)
  //{
  //  return;
  //}

  //// Release the display mode list.
  //delete[] displayModeList;
  //displayModeList = 0;

  //// Release the adapter output.
  //adapterOutput->Release();
  //adapterOutput = 0;

  //// Release the adapter.
  //adapter->Release();
  //adapter = 0;

  //// Release the factory.
  //factory->Release();
  //factory = 0;
}
