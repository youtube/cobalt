#include "libGLESv2/precompiled.h"
//
// Copyright (c) 2012-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SwapChain11WinRT.cpp: WinRT specific implementation for the D3D11 swap chain.
#include "libGLESv2/renderer/SwapChain11.h"

#include <dxgi1_2.h>
#include <wrl.h>

#include "common/debug.h"
#include "common/windowutils.h"
#include "common/winrt/windowadapter.h"
#include "libGLESv2/renderer/renderer11_utils.h"
#include "libGLESv2/renderer/Renderer11.h"

using namespace Microsoft::WRL;
using namespace Windows::UI::Core;

namespace rx
{
class SwapChain11WinRT : public SwapChain11
{
  public:
    SwapChain11WinRT(Renderer11 *renderer, EGLNativeWindowType window,
                     HANDLE shareHandle, GLenum backBufferFormat,
                     GLenum depthBufferFormat);

  protected:
    virtual EGLint createSwapChain(int backbufferWidth, int backbufferHeight,
                                   IDXGISwapChain **outSwapChain);

    // Keep a reference to the window pointer
    ComPtr<egl::IWindowAdapter> mWindowAdapter;
};

SwapChain11WinRT::SwapChain11WinRT(
    Renderer11 *renderer, EGLNativeWindowType window, HANDLE shareHandle,
    GLenum backBufferFormat, GLenum depthBufferFormat)
  : SwapChain11(renderer, window, shareHandle, backBufferFormat,
                depthBufferFormat)
{
    mWindowAdapter = egl::createWindowAdapter(window);
}

EGLint SwapChain11WinRT::createSwapChain(
      int backbufferWidth, int backbufferHeight, IDXGISwapChain **outSwapChain)
{
    if (!egl::verifyWindowAccessible(mWindowAdapter))
        return EGL_BAD_NATIVE_WINDOW;

    egl::AngleWindowType windowType;
    mWindowAdapter->GetType(&windowType);

    ID3D11Device *device = getRenderer()->getDevice();
    DXGI_FORMAT format = gl_d3d11::ConvertRenderbufferFormat(mBackBufferFormat);

    ComPtr<IDXGIFactory2> factory;
    getRenderer()->getDxgiFactory()->QueryInterface(factory.GetAddressOf());

    if (windowType == egl::AWT_COREWINDOW)
    {
        ComPtr<IUnknown> windowUnk;
        if (FAILED(mWindowAdapter->GetWindow(&windowUnk)))
            return EGL_BAD_NATIVE_WINDOW;

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = 2;
        swapChainDesc.Flags = 0;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Format = format;
        swapChainDesc.Width = backbufferWidth;
        swapChainDesc.Height = backbufferHeight;
        swapChainDesc.Stereo = false;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

        IDXGISwapChain1* swapChain1 = NULL;
        HRESULT result = factory->CreateSwapChainForCoreWindow(
            device, windowUnk.Get(), &swapChainDesc, NULL, &swapChain1);

        if (SUCCEEDED(result))
        {
            *outSwapChain = swapChain1;
            d3d11::ResourceTracker::Track(swapChain1);
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
    else if (windowType == egl::AWT_SWAPCHAIN_ADAPTER)
    {
        // A swap chain was already created externally
        ComPtr<egl::ISwapChainAdapter> swapChainAdapter;
        if (FAILED(mWindowAdapter->GetWindowAs(swapChainAdapter.GetAddressOf())))
        {
            ERR("The supplied window type is not a DXGI swap chain");
            return EGL_BAD_NATIVE_WINDOW;
        }

        ComPtr<IDXGISwapChain> swapChain;
        swapChainAdapter->GetSwapChain(&swapChain);

        *outSwapChain = swapChain.Get();
        // Note that we are incrementing the reference count here to take
        // ownership of the resource
        (*outSwapChain)->AddRef();

        d3d11::ResourceTracker::Track(*outSwapChain);
        return EGL_SUCCESS;
    }
    else
    {
        ERR("Unsupported EGL_EXT_WINDOW_TYPE: %d", windowType);
        return EGL_BAD_NATIVE_WINDOW;
    }
}

SwapChain11* CreateSwapChainWinRT(
    Renderer11 *renderer, EGLNativeWindowType window, HANDLE shareHandle,
    GLenum backBufferFormat, GLenum depthBufferFormat)
{
    return new SwapChain11WinRT(
        renderer, window, shareHandle, backBufferFormat, depthBufferFormat);
}

} // namespace rx
