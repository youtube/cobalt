//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// winrt/windowadapter.cpp: WinRT specific window utility functionality
#include "windowadapter.h"

#include <dxgi1_2.h>

#include "common/debug.h"

using namespace Microsoft::WRL;
using namespace Windows::UI::Core;

namespace egl
{

typedef RuntimeClass<RuntimeClassFlags<ClassicCom>, IWindowAdapter> WindowAdapterBase;

class WindowAdapter : public WindowAdapterBase
{
  public:
    WindowAdapter(ComPtr<IUnknown> window, AngleWindowType type);

    // IWindowAdapter methods
    virtual HRESULT GetType(AngleWindowType *type) override;
    virtual HRESULT GetWindow(IUnknown **ptr) override;

  protected:
    virtual ~WindowAdapter() {}

  private:
    AngleWindowType mWindowType;
    ComPtr<IUnknown> mWindow;
};

WindowAdapter::WindowAdapter(ComPtr<IUnknown> window, AngleWindowType type)
    : mWindowType(type)
    , mWindow(window)
{
}

HRESULT WindowAdapter::GetType(AngleWindowType *type)
{
    if (mWindowType < 0 || mWindowType >= AWT_NUM_TYPES)
        return E_FAIL;

    *type = mWindowType;
    return S_OK;
}

HRESULT WindowAdapter::GetWindow(IUnknown **ptr)
{
    *ptr = mWindow.Get();
    if (*ptr)
        (*ptr)->AddRef();

    return S_OK;
}

IWindowAdapter::Ptr createWindowAdapter(EGLNativeWindowType window)
{
    if (window == nullptr)
        return nullptr;

    ComPtr<IUnknown> unk(window);

    // Try a trivial conversion first
    IWindowAdapter::Ptr adapter;
    if (SUCCEEDED(unk.As(&adapter)))
        return adapter;

    // Check if this is a CoreWindow
    ComPtr<WindowAdapterABIType<CoreWindow>::Type> coreWindow;
    if (SUCCEEDED(unk.As(&coreWindow)))
        return Make<WindowAdapter>(unk, AWT_COREWINDOW);

    // Check if this is a DXGI swapchain adapter
    ComPtr<ISwapChainAdapter> swapChainAdapter;
    if (SUCCEEDED(unk.As(&swapChainAdapter)))
      return Make<WindowAdapter>(unk, AWT_SWAPCHAIN_ADAPTER);

    return nullptr;
}

bool verifyWindowAccessible(IWindowAdapter::Ptr windowAdapter)
{
    if (windowAdapter == nullptr)
        return false;

    AngleWindowType windowType;
    if (FAILED(windowAdapter->GetType(&windowType)))
        return false;

    if (windowType == AWT_COREWINDOW)
    {
        CoreWindow^ coreWindow;
        windowAdapter->GetWindowAsWinRT(&coreWindow);
        if (coreWindow != CoreWindow::GetForCurrentThread())
        {
            ERR("CoreWindow doesn't belong to the current thread,"
                "was the function called from the matching UI thread?");
            return false;
        }
    }

    return true;
}

} // namespace egl
