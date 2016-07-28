//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// winrt/windowadapter.h: ANGLE window adapter

#ifndef COMMON_WINRT_WINDOWADAPTER_H_
#define COMMON_WINRT_WINDOWADAPTER_H_

#include <wrl.h>

#include <Windows.UI.Core.h>  // ABI interface for CoreWindow

#include "common/windowutils.h"

struct IDXGISwapChain;

namespace egl
{

enum AngleWindowType
{
    // Do not reorganize to stay compatible with older versions
    AWT_COREWINDOW = 0,
    AWT_SWAPCHAIN_ADAPTER = 1,

    AWT_NUM_TYPES,
};

// The following template is used to map between Windows Runtime classes
// and their ABI couterparts
template <typename WinRT_T>
struct WindowAdapterABIType;

template <>
struct WindowAdapterABIType<Windows::UI::Core::CoreWindow>
{
    typedef ABI::Windows::UI::Core::ICoreWindow Type;
};

MIDL_INTERFACE("1C9CC283-C168-447A-B11A-98EF5D294EAE")
IWindowAdapter : public IUnknown
{
    typedef Microsoft::WRL::ComPtr<IWindowAdapter> Ptr;

    virtual HRESULT GetType(AngleWindowType *type) = 0;
    virtual HRESULT GetWindow(IUnknown **ptr) = 0;

    template <typename Interface_T>
    HRESULT GetWindowAs(Interface_T **ptr)
    {
        IUnknown *unk = nullptr;
        HRESULT hr = GetWindow(&unk);

        if (SUCCEEDED(hr))
            hr = unk->QueryInterface(ptr);

        if (unk)
            unk->Release();

        return hr;
    }

    template <typename WinRT_T>
    HRESULT GetWindowAsWinRT(WinRT_T^* winrtPtr)
    {
        typedef WindowAdapterABIType<WinRT_T>::Type ABIType;

        Microsoft::WRL::ComPtr<ABIType> ptr;
        HRESULT hr = GetWindowAs(ptr.GetAddressOf());
        if (FAILED(hr))
            return hr;

        *winrtPtr = reinterpret_cast<WinRT_T ^>(ptr.Get());
        return hr;
    }
};

MIDL_INTERFACE("AF0D1922-B84D-4443-9D04-980E7476B16F")
ISwapChainAdapter : public IUnknown
{
    virtual void GetSwapChain(Microsoft::WRL::ComPtr<IDXGISwapChain>* swapChain) = 0;
    virtual void GetWindowSize(INT32* width, INT32* height) = 0;
};

IWindowAdapter::Ptr createWindowAdapter(EGLNativeWindowType window);

bool verifyWindowAccessible(IWindowAdapter::Ptr windowAdapter);

} // namespace egl

#endif // COMMON_WINRT_WINDOWADAPTER_H_
