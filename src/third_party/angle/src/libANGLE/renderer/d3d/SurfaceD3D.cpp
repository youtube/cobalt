//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SurfaceD3D.cpp: D3D implementation of an EGL surface
#include <Mfobjects.h>

#include "libANGLE/renderer/d3d/SurfaceD3D.h"

#include "libANGLE/Display.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"
#include "libANGLE/renderer/d3d/RenderTargetD3D.h"
#include "libANGLE/renderer/d3d/SwapChainD3D.h"

#include <tchar.h>
#include <EGL/eglext.h>
#include <algorithm>

// A key for ID3D11DeviceChild. The value should be a bool.
// When set and true, indicates that the NV12 texture passed
// in eglCreatePbufferFromClientBuffer EGL_D3D_TEXTURE_ANGLE should
// draw it's chroma component when a ShaderResourceView is created for it.
// If absent or false, the luma component is drawn.
//
// The value is fetched from ID3D11DeviceChild and stored before
// eglCreatePbufferFromClientBuffer returns.
//
// {3C3A43AB-C69B-46C9-AA8D-B0CFFCD4596D}
static const GUID kCobaltNv12BindChroma = {
  0x3c3a43ab, 0xc69b, 0x46c9,
  { 0xaa, 0x8d, 0xb0, 0xcf, 0xfc, 0xd4, 0x59, 0x6d }
};

// A key for ID3D11DeviceChild. The value should be an IMFDXGIBuffer*.
// When used with an NV12 texture passed in eglCreatePbufferFromClientBuffer
// EGL_D3D_TEXTURE_ANGLE, this interface is asked for the appropriate
// texture in a texture array to use via GetSubresourceIndex.
// This is appropriate for use with IMFTransform video decoders that
// return IMFDXGIBuffer's that have texture array resources.
//
// The value is fetched from ID3D11DeviceChild and stored before
// eglCreatePbufferFromClientBuffer returns.
//
// C62BF18D-B5EE-46B1-9C31-F61BD8AE3B0D
static const GUID kCobaltDxgiBuffer = {
  0Xc62bf18d, 0Xb5ee, 0X46b1,
  {0X9c, 0X31, 0Xf6, 0X1b, 0Xd8, 0Xae, 0X3b, 0X0d }
};

namespace rx
{

SurfaceD3D::SurfaceD3D(const egl::SurfaceState &state,
                       RendererD3D *renderer,
                       egl::Display *display,
                       EGLNativeWindowType window,
                       EGLenum buftype,
                       EGLClientBuffer clientBuffer,
                       const egl::AttributeMap &attribs)
    : SurfaceImpl(state),
      mRenderer(renderer),
      mDisplay(display),
      mFixedSize(window == nullptr || attribs.get(EGL_FIXED_SIZE_ANGLE, EGL_FALSE) == EGL_TRUE),
      mOrientation(static_cast<EGLint>(attribs.get(EGL_SURFACE_ORIENTATION_ANGLE, 0))),
      mRenderTargetFormat(state.config->renderTargetFormat),
      mDepthStencilFormat(state.config->depthStencilFormat),
      mSwapChain(nullptr),
      mSwapIntervalDirty(true),
      mNativeWindow(renderer->createNativeWindow(window, state.config, attribs)),
      mWidth(static_cast<EGLint>(attribs.get(EGL_WIDTH, 0))),
      mHeight(static_cast<EGLint>(attribs.get(EGL_HEIGHT, 0))),
      mSwapInterval(1),
      mShareHandle(0),
      mD3DTexture(nullptr),
      mBuftype(buftype),
      mDXGIBuffer(nullptr),
      mBindChroma(false)
{
    if (window != nullptr && !mFixedSize)
    {
        mWidth  = -1;
        mHeight = -1;
    }

    switch (buftype)
    {
        case EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE:
            mShareHandle = static_cast<HANDLE>(clientBuffer);
            break;

        case EGL_D3D_TEXTURE_ANGLE:
        {
            mD3DTexture = static_cast<IUnknown *>(clientBuffer);
            ASSERT(mD3DTexture != nullptr);
            mD3DTexture->AddRef();
            mRenderer->getD3DTextureInfo(state.config, mD3DTexture, &mWidth, &mHeight,
                                         &mRenderTargetFormat);

            UINT out;
            HRESULT hr = static_cast<ID3D11DeviceChild*>(mD3DTexture)->
                GetPrivateData(kCobaltNv12BindChroma, &out, nullptr);
            mBindChroma = (SUCCEEDED(hr)) && (out != 0);

            out = sizeof(mDXGIBuffer);
            hr = static_cast<ID3D11DeviceChild*>(mD3DTexture)->
                GetPrivateData(kCobaltDxgiBuffer, &out, &mDXGIBuffer);
            if (SUCCEEDED(hr) && mDXGIBuffer != nullptr) {
              mDXGIBuffer->AddRef();
            }
            break;
        }

        default:
            break;
    }
}

SurfaceD3D::~SurfaceD3D()
{
    releaseSwapChain();
    SafeDelete(mNativeWindow);
    SafeRelease(mD3DTexture);
    SafeRelease(mDXGIBuffer);
}

void SurfaceD3D::releaseSwapChain()
{
    SafeDelete(mSwapChain);
}

egl::Error SurfaceD3D::initialize(const DisplayImpl *displayImpl)
{
    if (mNativeWindow->getNativeWindow())
    {
        if (!mNativeWindow->initialize())
        {
            return egl::Error(EGL_BAD_SURFACE);
        }
    }

    if (mBuftype == EGL_D3D_TEXTURE_ANGLE)
    {
        ID3D11Texture2D* d3Texture = static_cast<ID3D11Texture2D*>(mD3DTexture);

        D3D11_TEXTURE2D_DESC texture_desc;
        d3Texture->GetDesc(&texture_desc);

        if (texture_desc.Format == DXGI_FORMAT_NV12)
        {
            // NV12 textures cannot be rendered to,
            // so don't proceed to making a swap chain.
            return egl::Error(EGL_SUCCESS);
        }
    }

    egl::Error error = resetSwapChain();
    if (error.isError())
    {
        return error;
    }

    return egl::Error(EGL_SUCCESS);
}

FramebufferImpl *SurfaceD3D::createDefaultFramebuffer(const gl::FramebufferState &data)
{
    return mRenderer->createDefaultFramebuffer(data);
}

egl::Error SurfaceD3D::bindTexImage(gl::Texture *, EGLint)
{
    return egl::Error(EGL_SUCCESS);
}

egl::Error SurfaceD3D::releaseTexImage(EGLint)
{
    return egl::Error(EGL_SUCCESS);
}

egl::Error SurfaceD3D::getSyncValues(EGLuint64KHR *ust, EGLuint64KHR *msc, EGLuint64KHR *sbc)
{
    return mSwapChain->getSyncValues(ust, msc, sbc);
}

egl::Error SurfaceD3D::resetSwapChain()
{
    ASSERT(!mSwapChain);

    int width;
    int height;

    if (!mFixedSize)
    {
        RECT windowRect;
        if (!mNativeWindow->getClientRect(&windowRect))
        {
            ASSERT(false);

            return egl::Error(EGL_BAD_SURFACE, "Could not retrieve the window dimensions");
        }

        width = windowRect.right - windowRect.left;
        height = windowRect.bottom - windowRect.top;
    }
    else
    {
        // non-window surface - size is determined at creation
        width = mWidth;
        height = mHeight;
    }

    mSwapChain =
        mRenderer->createSwapChain(mNativeWindow, mShareHandle, mD3DTexture, mRenderTargetFormat,
                                   mDepthStencilFormat, mOrientation, mState.config->samples);
    if (!mSwapChain)
    {
        return egl::Error(EGL_BAD_ALLOC);
    }

    egl::Error error = resetSwapChain(width, height);
    if (error.isError())
    {
        SafeDelete(mSwapChain);
        return error;
    }

    return egl::Error(EGL_SUCCESS);
}

egl::Error SurfaceD3D::resizeSwapChain(int backbufferWidth, int backbufferHeight)
{
    ASSERT(backbufferWidth >= 0 && backbufferHeight >= 0);
    ASSERT(mSwapChain);

    EGLint status = mSwapChain->resize(std::max(1, backbufferWidth), std::max(1, backbufferHeight));

    if (status == EGL_CONTEXT_LOST)
    {
        mDisplay->notifyDeviceLost();
        return egl::Error(status);
    }
    else if (status != EGL_SUCCESS)
    {
        return egl::Error(status);
    }

    mWidth = backbufferWidth;
    mHeight = backbufferHeight;

    return egl::Error(EGL_SUCCESS);
}

egl::Error SurfaceD3D::resetSwapChain(int backbufferWidth, int backbufferHeight)
{
    ASSERT(backbufferWidth >= 0 && backbufferHeight >= 0);
    ASSERT(mSwapChain);

    EGLint status = mSwapChain->reset(std::max(1, backbufferWidth), std::max(1, backbufferHeight), mSwapInterval);

    if (status == EGL_CONTEXT_LOST)
    {
        mRenderer->notifyDeviceLost();
        return egl::Error(status);
    }
    else if (status != EGL_SUCCESS)
    {
        return egl::Error(status);
    }

    mWidth = backbufferWidth;
    mHeight = backbufferHeight;
    mSwapIntervalDirty = false;

    return egl::Error(EGL_SUCCESS);
}

egl::Error SurfaceD3D::swapRect(EGLint x, EGLint y, EGLint width, EGLint height)
{
    if (!mSwapChain)
    {
        return egl::Error(EGL_SUCCESS);
    }

    if (x + width > mWidth)
    {
        width = mWidth - x;
    }

    if (y + height > mHeight)
    {
        height = mHeight - y;
    }

    if (width != 0 && height != 0)
    {
        EGLint status = mSwapChain->swapRect(x, y, width, height);

        if (status == EGL_CONTEXT_LOST)
        {
            mRenderer->notifyDeviceLost();
            return egl::Error(status);
        }
        else if (status != EGL_SUCCESS)
        {
            return egl::Error(status);
        }
    }

    checkForOutOfDateSwapChain();

    return egl::Error(EGL_SUCCESS);
}

bool SurfaceD3D::checkForOutOfDateSwapChain()
{
    RECT client;
    int clientWidth = getWidth();
    int clientHeight = getHeight();
    bool sizeDirty = false;
    if (!mFixedSize && !mNativeWindow->isIconic())
    {
        // The window is automatically resized to 150x22 when it's minimized, but the swapchain shouldn't be resized
        // because that's not a useful size to render to.
        if (!mNativeWindow->getClientRect(&client))
        {
            ASSERT(false);
            return false;
        }

        // Grow the buffer now, if the window has grown. We need to grow now to avoid losing information.
        clientWidth = client.right - client.left;
        clientHeight = client.bottom - client.top;
        sizeDirty = clientWidth != getWidth() || clientHeight != getHeight();
    }

    bool wasDirty = (mSwapIntervalDirty || sizeDirty);

    if (mSwapIntervalDirty)
    {
        resetSwapChain(clientWidth, clientHeight);
    }
    else if (sizeDirty)
    {
        resizeSwapChain(clientWidth, clientHeight);
    }

    return wasDirty;
}

egl::Error SurfaceD3D::swap(const DisplayImpl *displayImpl)
{
    return swapRect(0, 0, mWidth, mHeight);
}

egl::Error SurfaceD3D::postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height)
{
    return swapRect(x, y, width, height);
}

rx::SwapChainD3D *SurfaceD3D::getSwapChain() const
{
    return mSwapChain;
}

void SurfaceD3D::setSwapInterval(EGLint interval)
{
    if (mSwapInterval == interval)
    {
        return;
    }

    mSwapInterval = interval;
    mSwapIntervalDirty = true;
}

EGLint SurfaceD3D::getWidth() const
{
    return mWidth;
}

EGLint SurfaceD3D::getHeight() const
{
    return mHeight;
}

EGLint SurfaceD3D::isPostSubBufferSupported() const
{
    // post sub buffer is always possible on D3D surfaces
    return EGL_TRUE;
}

EGLint SurfaceD3D::getSwapBehavior() const
{
    return EGL_BUFFER_PRESERVED;
}

egl::Error SurfaceD3D::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    if (attribute == EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE)
    {
        *value = mSwapChain->getShareHandle();
    }
    else if (attribute == EGL_DXGI_KEYED_MUTEX_ANGLE)
    {
        *value = mSwapChain->getKeyedMutex();
    }
    else UNREACHABLE();

    return egl::Error(EGL_SUCCESS);
}

gl::Error SurfaceD3D::getAttachmentRenderTarget(GLenum binding,
                                                const gl::ImageIndex &imageIndex,
                                                FramebufferAttachmentRenderTarget **rtOut)
{
    if (binding == GL_BACK)
    {
        *rtOut = mSwapChain->getColorRenderTarget();
    }
    else
    {
        *rtOut = mSwapChain->getDepthStencilRenderTarget();
    }
    return gl::NoError();
}

WindowSurfaceD3D::WindowSurfaceD3D(const egl::SurfaceState &state,
                                   RendererD3D *renderer,
                                   egl::Display *display,
                                   EGLNativeWindowType window,
                                   const egl::AttributeMap &attribs)
    : SurfaceD3D(state,
                 renderer,
                 display,
                 window,
                 0,
                 static_cast<EGLClientBuffer>(0),
                 attribs)
{
}

WindowSurfaceD3D::~WindowSurfaceD3D()
{
}

PbufferSurfaceD3D::PbufferSurfaceD3D(const egl::SurfaceState &state,
                                     RendererD3D *renderer,
                                     egl::Display *display,
                                     EGLenum buftype,
                                     EGLClientBuffer clientBuffer,
                                     const egl::AttributeMap &attribs)
    : SurfaceD3D(state,
                 renderer,
                 display,
                 static_cast<EGLNativeWindowType>(0),
                 buftype,
                 clientBuffer,
                 attribs)
{
}

PbufferSurfaceD3D::~PbufferSurfaceD3D()
{
}

}  // namespace rc
