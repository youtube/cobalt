//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// winrt/WindowSubclassWinRT.cpp: WinRT specific implementation for WindowSubclass
#include "libEGL/Surface.h"

#include <agile.h>
#include <dxgi1_2.h>
#include <math.h>
#include <mutex>
#include <Windows.h>
#include <wrl.h>

#include "common/debug.h"
#include "common/windowutils.h"
#include "common/winrt/windowadapter.h"

using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::UI::Core;

namespace egl
{

EGLint ConvertDipsToPixels(EGLint dips)
{
    static const float dipsPerInch = 96.0f;

    float logicalDpi = DisplayProperties::LogicalDpi;
    // Round to nearest integer.
    return (EGLint)floor(dips * logicalDpi / dipsPerInch + 0.5f);
}

class WindowSubclassWinRT;

ref class CoreWindowSizeHandler sealed
{
  public:
    virtual ~CoreWindowSizeHandler();

  internal:
    CoreWindowSizeHandler(CoreWindow^ coreWindow);

    void OnSizeChanged(CoreWindow^ sender, WindowSizeChangedEventArgs^ args);

    void setWindowSizeDips(EGLint width, EGLint height);
    void getWindowSize(EGLint* width, EGLint* height);

    Platform::Agile<CoreWindow> mCoreWindow;

    // We need to synchronize access to our variables since window
    // size notifications are coming from the UI thread while the variables
    // can be read from a different thread
    std::mutex mMutex;

    EGLint mWidth;
    EGLint mHeight;

    Windows::Foundation::EventRegistrationToken mOnSizeChangedToken;
};

CoreWindowSizeHandler::CoreWindowSizeHandler(CoreWindow^ coreWindow)
    : mCoreWindow(coreWindow)
{
    setWindowSizeDips(mCoreWindow->Bounds.Width, mCoreWindow->Bounds.Height);

    typedef TypedEventHandler<CoreWindow^, WindowSizeChangedEventArgs^> SizeChangeHandler;
    auto eventHandler = ref new SizeChangeHandler(this, &CoreWindowSizeHandler::OnSizeChanged);
    mOnSizeChangedToken = (mCoreWindow->SizeChanged += eventHandler);
}

CoreWindowSizeHandler::~CoreWindowSizeHandler()
{
    mCoreWindow->SizeChanged -= mOnSizeChangedToken;
}

void CoreWindowSizeHandler::OnSizeChanged(
      CoreWindow^ sender, WindowSizeChangedEventArgs^ args)
{
    // Window size is reported in DIPs (Device Independent Pixels)
    setWindowSizeDips(args->Size.Width, args->Size.Height);
}

void CoreWindowSizeHandler::setWindowSizeDips(EGLint width, EGLint height)
{
    width = ConvertDipsToPixels(width);
    height = ConvertDipsToPixels(height);

    {
      std::lock_guard<std::mutex> lock(mMutex);

      mWidth = width;
      mHeight = height;
    }
}

void CoreWindowSizeHandler::getWindowSize(EGLint* width, EGLint* height)
{
    std::lock_guard<std::mutex> lock(mMutex);

    *width = mWidth;
    *height = mHeight;
}

class WindowSubclassWinRT : public WindowSubclass
{
  public:
    WindowSubclassWinRT(IWindowAdapter::Ptr windowAdapter);

    virtual bool getWindowSize(EGLint *width, EGLint *height);

    bool initialize();

    IWindowAdapter::Ptr mWindowAdapter;
    AngleWindowType mWindowType;

    // Exists only if mWindowType == AWT_COREWINDOW
    CoreWindowSizeHandler^ mHandler;
};

WindowSubclassWinRT::WindowSubclassWinRT(IWindowAdapter::Ptr windowAdapter)
    : mWindowAdapter(windowAdapter)
{
    mWindowAdapter->GetType(&mWindowType);

    if (mWindowType == AWT_COREWINDOW)
    {
        CoreWindow^ coreWindow;
        mWindowAdapter->GetWindowAsWinRT(&coreWindow);
        mHandler = ref new CoreWindowSizeHandler(coreWindow);
    }
}

bool WindowSubclassWinRT::getWindowSize(EGLint *width, EGLint *height)
{
    if (mWindowType == AWT_COREWINDOW)
    {
        if (!mHandler)
            return false;

        mHandler->getWindowSize(width, height);
    }
    else if (mWindowType == AWT_SWAPCHAIN_ADAPTER)
    {
        ComPtr<ISwapChainAdapter> swapChainAdapter;
        if (FAILED(mWindowAdapter->GetWindowAs(swapChainAdapter.GetAddressOf())))
            return false;

        swapChainAdapter->GetWindowSize(width, height);
    }

    return true;
}

bool WindowSubclassWinRT::initialize()
{
    if (!egl::verifyWindowAccessible(mWindowAdapter))
        return false;

    return true;
}

WindowSubclass *CreateWindowSubclassWinRT(Surface *, EGLNativeWindowType window)
{
    WindowSubclassWinRT *subclass = new WindowSubclassWinRT(
        createWindowAdapter(window));

    if (!subclass->initialize())
    {
        delete subclass;
        subclass = NULL;
    }

    return subclass;
}

} // namespace egl
