//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// win32/WindowSubclassApi.cpp: Win32 specific implementation for WindowSubclass
#include "libEGL/Surface.h"

#include <tchar.h>

#include "common/debug.h"
#include "common/system.h"
#include "common/windowutils.h"

#include "libEGL/main.h"
#include "libEGL/Display.h"

#include "libGLESv2/renderer/SwapChain.h"
#include "libGLESv2/main.h"

namespace egl
{

#define kSurfaceProperty _TEXT("Egl::SurfaceOwner")
#define kParentWndProc _TEXT("Egl::SurfaceParentWndProc")

static LRESULT CALLBACK SurfaceWindowProc(
    HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
  if (message == WM_SIZE)
  {
      Surface* surf = reinterpret_cast<Surface*>(GetProp(hwnd, kSurfaceProperty));
      if(surf)
      {
          surf->checkForOutOfDateSwapChain();
      }
  }
  WNDPROC prevWndFunc = reinterpret_cast<WNDPROC >(GetProp(hwnd, kParentWndProc));
  return CallWindowProc(prevWndFunc, hwnd, message, wparam, lparam);
}

class WindowSubclassApi : public WindowSubclass
{
  public:
    WindowSubclassApi(Surface *surface, EGLNativeWindowType window);
    virtual ~WindowSubclassApi();

    virtual bool getWindowSize(EGLint *width, EGLint *height);

    bool initialize();

    bool subclassWindow();
    void unsubclassWindow();

    Surface *mSurface;
    EGLNativeWindowType mWindow;
    bool mWindowSubclassed;
};

WindowSubclassApi::WindowSubclassApi(Surface *surface, EGLNativeWindowType window)
  : mSurface(surface), mWindow(window), mWindowSubclassed(false)
{
}

WindowSubclassApi::~WindowSubclassApi()
{
    unsubclassWindow();
}

bool WindowSubclassApi::getWindowSize(EGLint *width, EGLint *height)
{
    RECT rect = { 0 };
    if (!::GetClientRect(mWindow, &rect))
    {
        return false;
    }

    *width = rect.right - rect.left;
    *height = rect.bottom - rect.top;

    return true;
}

bool WindowSubclassApi::subclassWindow()
{
    HWND wnd = (HWND)mWindow;
    if (!wnd)
    {
        return false;
    }

    DWORD processId;
    DWORD threadId = GetWindowThreadProcessId(wnd, &processId);
    if (processId != GetCurrentProcessId() || threadId != GetCurrentThreadId())
        return false;

    SetLastError(0);
    LONG_PTR oldWndProc = SetWindowLongPtr(
        wnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(SurfaceWindowProc));
    if(oldWndProc == 0 && GetLastError() != ERROR_SUCCESS)
    {
        return false;
    }

    SetProp(wnd, kSurfaceProperty, reinterpret_cast<HANDLE>(this));
    SetProp(wnd, kParentWndProc, reinterpret_cast<HANDLE>(oldWndProc));

    mWindowSubclassed = true;

    return true;
}

void WindowSubclassApi::unsubclassWindow()
{
    HWND wnd = (HWND)mWindow;
    if(!mWindowSubclassed)
    {
        return;
    }

    // un-subclass
    LONG_PTR parentWndFunc = reinterpret_cast<LONG_PTR>(
        GetProp(wnd, kParentWndProc));

    // Check the windowproc is still SurfaceWindowProc.
    // If this assert fails, then it is likely the application has subclassed the
    // hwnd as well and did not unsubclass before destroying its EGL context. The
    // application should be modified to either subclass before initializing the
    // EGL context, or to unsubclass before destroying the EGL context.
    if(parentWndFunc)
    {
        LONG_PTR prevWndFunc = SetWindowLongPtr(wnd, GWLP_WNDPROC, parentWndFunc);
        ASSERT(prevWndFunc == reinterpret_cast<LONG_PTR>(SurfaceWindowProc));
    }

    RemoveProp(wnd, kSurfaceProperty);
    RemoveProp(wnd, kParentWndProc);
    mWindowSubclassed = false;
}

bool WindowSubclassApi::initialize()
{
    if (!egl::verifyWindowAccessible(mWindow))
        return false;

    return true;
}

WindowSubclass *CreateWindowSubclassWin32(
    Surface *surface, EGLNativeWindowType window)
{
    WindowSubclassApi *subclass = new WindowSubclassApi(surface, window);

    if (!subclass->initialize())
    {
        delete subclass;
        subclass = NULL;
    }

    return subclass;
}

} // namespace egl
