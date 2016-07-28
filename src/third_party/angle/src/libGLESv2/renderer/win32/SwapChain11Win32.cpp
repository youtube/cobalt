#include "precompiled.h"
//
// Copyright (c) 2012-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SwapChain11Api.cpp: Win32 specific implementation for the D3D11 swap chain.

#include "libGLESv2/renderer/SwapChain11.h"

#include "libGLESv2/renderer/renderer11_utils.h"
#include "libGLESv2/renderer/Renderer11.h"

namespace rx
{

class SwapChain11Api : public SwapChain11
{
  public:
    SwapChain11Api(Renderer11 *renderer, EGLNativeWindowType window,
                   HANDLE shareHandle, GLenum backBufferFormat,
                   GLenum depthBufferFormat);

  protected:
    virtual EGLint createSwapChain(int backbufferWidth, int backbufferHeight,
                                   IDXGISwapChain **outSwapChain);
};

SwapChain11Api::SwapChain11Api(
    Renderer11 *renderer, EGLNativeWindowType window, HANDLE shareHandle,
    GLenum backBufferFormat, GLenum depthBufferFormat)
  : SwapChain11(renderer, window, shareHandle, backBufferFormat,
                depthBufferFormat) {}

SwapChain11* CreateSwapChainWin32(
    Renderer11 *renderer, EGLNativeWindowType window, HANDLE shareHandle,
    GLenum backBufferFormat, GLenum depthBufferFormat)
{
    return new SwapChain11Api(renderer, window, shareHandle,
                              backBufferFormat, depthBufferFormat);
}

EGLint SwapChain11Api::createSwapChain(
    int backbufferWidth, int backbufferHeight, IDXGISwapChain **outSwapChain)
{
    // We cannot create a swap chain for an HWND that is owned by a
    // different process
    DWORD currentProcessId = GetCurrentProcessId();
    DWORD wndProcessId;
    GetWindowThreadProcessId((HWND)mWindow, &wndProcessId);

    if (currentProcessId != wndProcessId)
    {
        ERR("Could not create swap chain, window owned by different process");
        return EGL_BAD_NATIVE_WINDOW;
    }

    ID3D11Device *device = getRenderer()->getDevice();
    IDXGIFactory *factory = getRenderer()->getDxgiFactory();
    DXGI_FORMAT format = gl_d3d11::ConvertRenderbufferFormat(mBackBufferFormat);

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {0};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Format = format;
    swapChainDesc.BufferDesc.Width = backbufferWidth;
    swapChainDesc.BufferDesc.Height = backbufferHeight;
    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.Flags = 0;
    swapChainDesc.OutputWindow = mWindow;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;

    IDXGISwapChain *swapChain;
    HRESULT result = factory->CreateSwapChain(device, &swapChainDesc, &swapChain);

    if (SUCCEEDED(result))
    {
        *outSwapChain = swapChain;
        d3d11::ResourceTracker::Track(swapChain);
        return EGL_SUCCESS;
    }
    else
    {
        if (d3d11::isDeviceLostError(result))
            return EGL_CONTEXT_LOST;
        else
            return EGL_BAD_ALLOC;
    }
}

} // namespace rx
