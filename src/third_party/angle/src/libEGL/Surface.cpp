//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Surface.cpp: Implements the egl::Surface class, representing a drawing surface
// such as the client area of a window, including any back buffers.
// Implements EGLSurface and related functionality. [EGL 1.4] section 2.2 page 3.

#include <tchar.h>

#include "libEGL/Surface.h"

#include "common/debug.h"
#include "common/system.h"
#include "common/windowutils.h"

#include "libGLESv2/Texture.h"
#include "libGLESv2/renderer/SwapChain.h"
#include "libGLESv2/main.h"

#include "libEGL/main.h"
#include "libEGL/Display.h"

namespace egl
{

Surface::Surface(Display *display, const Config *config, EGLNativeWindowType window, EGLint postSubBufferSupported)
    : mDisplay(display), mConfig(config), mWindow(window), mPostSubBufferSupported(postSubBufferSupported), mWindowSubclass(NULL)
{
    mRenderer = mDisplay->getRenderer();
    mSwapChain = NULL;
    mShareHandle = NULL;
    mTexture = NULL;
    mTextureFormat = EGL_NO_TEXTURE;
    mTextureTarget = EGL_NO_TEXTURE;

    mPixelAspectRatio = (EGLint)(1.0 * EGL_DISPLAY_SCALING);   // FIXME: Determine actual pixel aspect ratio
    mRenderBuffer = EGL_BACK_BUFFER;
    mSwapBehavior = EGL_BUFFER_PRESERVED;
    mSwapInterval = -1;
    mWidth = -1;
    mHeight = -1;
    setSwapInterval(1);
}

Surface::Surface(Display *display, const Config *config, HANDLE shareHandle, EGLint width, EGLint height, EGLenum textureFormat, EGLenum textureType)
  : mDisplay(display), mWindow(ANGLE_NO_WINDOW), mConfig(config), mShareHandle(shareHandle), mWidth(width)
  , mHeight(height), mPostSubBufferSupported(EGL_FALSE), mWindowSubclass(NULL)
{
    mRenderer = mDisplay->getRenderer();
    mSwapChain = NULL;
    mTexture = NULL;
    mTextureFormat = textureFormat;
    mTextureTarget = textureType;

    mPixelAspectRatio = (EGLint)(1.0 * EGL_DISPLAY_SCALING);   // FIXME: Determine actual pixel aspect ratio
    mRenderBuffer = EGL_BACK_BUFFER;
    mSwapBehavior = EGL_BUFFER_PRESERVED;
    mSwapInterval = -1;
    setSwapInterval(1);
}

Surface::~Surface()
{
    release();
}

bool Surface::initialize()
{
    if (hasWindow())
        mWindowSubclass = createWindowSubclass(this, mWindow);

    if (!resetSwapChain())
      return false;

    return true;
}

void Surface::release()
{
    if (mWindowSubclass)
    {
        delete mWindowSubclass;
        mWindowSubclass = NULL;
    }

    if (mSwapChain)
    {
      delete mSwapChain;
      mSwapChain = NULL;
    }

    if (mTexture)
    {
        mTexture->releaseTexImage();
        mTexture = NULL;
    }
}

bool Surface::resetSwapChain()
{
    ASSERT(!mSwapChain);

    EGLint width, height;

    if (hasWindow())
    {
        if (!mWindowSubclass || !mWindowSubclass->getWindowSize(&width, &height))
        {
            ASSERT(false);

            ERR("Could not retrieve the window dimensions");
            return error(EGL_BAD_SURFACE, false);
        }
    }
    else
    {
        // non-window surface - size is determined at creation
        width = mWidth;
        height = mHeight;
    }

    mSwapChain = mRenderer->createSwapChain(mWindow, mShareHandle,
                                            mConfig->mRenderTargetFormat,
                                            mConfig->mDepthStencilFormat);
    if (!mSwapChain)
    {
        return error(EGL_BAD_ALLOC, false);
    }

    if (!resetSwapChain(width, height))
    {
        delete mSwapChain;
        mSwapChain = NULL;
        return false;
    }

    return true;
}

bool Surface::resizeSwapChain(EGLint backbufferWidth, EGLint backbufferHeight)
{
    ASSERT(backbufferWidth >= 0 && backbufferHeight >= 0);
    ASSERT(mSwapChain);

    EGLint status = mSwapChain->resize(backbufferWidth, backbufferHeight);

    if (status == EGL_CONTEXT_LOST)
    {
        mDisplay->notifyDeviceLost();
        return false;
    }
    else if (status != EGL_SUCCESS)
    {
        return error(status, false);
    }

    mWidth = backbufferWidth;
    mHeight = backbufferHeight;

    return true;
}

bool Surface::resetSwapChain(EGLint backbufferWidth, EGLint backbufferHeight)
{
    ASSERT(backbufferWidth >= 0 && backbufferHeight >= 0);
    ASSERT(mSwapChain);

    EGLint status = mSwapChain->reset(backbufferWidth, backbufferHeight, mSwapInterval);

    if (status == EGL_CONTEXT_LOST)
    {
        mRenderer->notifyDeviceLost();
        return false;
    }
    else if (status != EGL_SUCCESS)
    {
        return error(status, false);
    }

    mWidth = backbufferWidth;
    mHeight = backbufferHeight;
    mSwapIntervalDirty = false;

    return true;
}

bool Surface::swapRect(EGLint x, EGLint y, EGLint width, EGLint height)
{
    if (!mSwapChain)
    {
        return true;
    }

    if (x + width > mWidth)
    {
        width = mWidth - x;
    }

    if (y + height > mHeight)
    {
        height = mHeight - y;
    }

    if (width == 0 || height == 0)
    {
        return true;
    }

    EGLint status = mSwapChain->swapRect(x, y, width, height);

    if (status == EGL_CONTEXT_LOST)
    {
        mRenderer->notifyDeviceLost();
        return false;
    }
    else if (status != EGL_SUCCESS)
    {
        return error(status, false);
    }

    checkForOutOfDateSwapChain();

    return true;
}

#if defined(ANGLE_WIN32)
WindowSubclass *CreateWindowSubclassWin32(Surface *, EGLNativeWindowType);
#elif defined(ANGLE_WINRT)
WindowSubclass *CreateWindowSubclassWinRT(Surface *, EGLNativeWindowType);
#endif

WindowSubclass *Surface::createWindowSubclass(Surface *surface, EGLNativeWindowType window)
{
#if defined(ANGLE_WIN32)
    return CreateWindowSubclassWin32(surface, window);
#elif defined(ANGLE_WINRT)
    return CreateWindowSubclassWinRT(surface, window);
#endif
    return NULL;
}

bool Surface::hasWindow() const
{
    return (mWindow != ANGLE_NO_WINDOW);
}

bool Surface::compareWindow(EGLNativeWindowType window) const
{
    return (mWindow == window);
}

bool Surface::checkForOutOfDateSwapChain()
{
    if (!mWindowSubclass)
    {
      return false;
    }

    EGLint clientWidth, clientHeight;

    if (!mWindowSubclass->getWindowSize(&clientWidth, &clientHeight))
    {
        ASSERT(false);
        return false;
    }

    // Grow the buffer now, if the window has grown. We need to grow now to avoid losing information.
    bool sizeDirty = clientWidth != getWidth() || clientHeight != getHeight();

    if (mSwapIntervalDirty)
    {
        resetSwapChain(clientWidth, clientHeight);
    }
    else if (sizeDirty)
    {
        resizeSwapChain(clientWidth, clientHeight);
    }

    if (mSwapIntervalDirty || sizeDirty)
    {
        if (static_cast<egl::Surface*>(getCurrentDrawSurface()) == this)
        {
            glMakeCurrent(glGetCurrentContext(), static_cast<egl::Display*>(getCurrentDisplay()), this);
        }

        return true;
    }

    return false;
}

bool Surface::swap()
{
    return swapRect(0, 0, mWidth, mHeight);
}

bool Surface::postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height)
{
    if (!mPostSubBufferSupported)
    {
        // Spec is not clear about how this should be handled.
        return true;
    }

    return swapRect(x, y, width, height);
}

EGLint Surface::getWidth() const
{
    return mWidth;
}

EGLint Surface::getHeight() const
{
    return mHeight;
}

EGLint Surface::isPostSubBufferSupported() const
{
    return mPostSubBufferSupported;
}

rx::SwapChain *Surface::getSwapChain() const
{
    return mSwapChain;
}

void Surface::setSwapInterval(EGLint interval)
{
    if (mSwapInterval == interval)
    {
        return;
    }

    mSwapInterval = interval;
    mSwapInterval = std::max(mSwapInterval, mRenderer->getMinSwapInterval());
    mSwapInterval = std::min(mSwapInterval, mRenderer->getMaxSwapInterval());

    mSwapIntervalDirty = true;
}

EGLenum Surface::getTextureFormat() const
{
    return mTextureFormat;
}

EGLenum Surface::getTextureTarget() const
{
    return mTextureTarget;
}

void Surface::setBoundTexture(gl::Texture2D *texture)
{
    mTexture = texture;
}

gl::Texture2D *Surface::getBoundTexture() const
{
    return mTexture;
}

EGLenum Surface::getFormat() const
{
    return mConfig->mRenderTargetFormat;
}
}
