// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_factory.h"

#import <UIKit/UIKit.h>

#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_stub.h"

namespace gl::init {

std::vector<GLImplementationParts> GetAllowedGLImplementations() {
  std::vector<GLImplementationParts> impls;
  impls.emplace_back(gl::ANGLEImplementation::kMetal);
  return impls;
}

bool GetGLWindowSystemBindingInfo(const GLVersionInfo& gl_info,
                                  GLWindowSystemBindingInfo* info) {
  return false;
}

scoped_refptr<GLContext> CreateGLContext(GLShareGroup* share_group,
                                         GLSurface* compatible_surface,
                                         const GLContextAttribs& attribs) {
  switch (GetGLImplementation()) {
    case kGLImplementationEGLANGLE:
      return InitializeGLContext(new GLContextEGL(share_group),
                                 compatible_surface, attribs);
    case kGLImplementationMockGL:
    case kGLImplementationStubGL: {
      scoped_refptr<GLContextStub> stub_context =
          base::MakeRefCounted<GLContextStub>(share_group);
      if (GetGLImplementation() == kGLImplementationStubGL) {
        stub_context->SetUseStubApi(true);
      }
      // The stub ctx needs to be initialized so that the gl::GLContext can
      // store the |compatible_surface|.
      stub_context->Initialize(compatible_surface, attribs);
      return stub_context;
    }
    default:
      NOTREACHED();
  }
}

scoped_refptr<GLSurface> CreateViewGLSurface(GLDisplay* display,
                                             gfx::AcceleratedWidget window) {
  TRACE_EVENT0("gpu", "gl::init::CreateViewGLSurface");
  CHECK_NE(kGLImplementationNone, GetGLImplementation());
  switch (GetGLImplementation()) {
    case kGLImplementationEGLANGLE:
      if (window != gfx::kNullAcceleratedWidget) {
        UIView* view = (__bridge id)(void*)window;
        void* layer = (__bridge void*)view.layer;
        return InitializeGLSurface(
            new NativeViewGLSurfaceEGL(display->GetAs<gl::GLDisplayEGL>(),
                                       layer, /*vsync_provider=*/nullptr));
      } else {
        return InitializeGLSurface(new GLSurfaceStub());
      }
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return InitializeGLSurface(new GLSurfaceStub());
    default:
      NOTREACHED();
  }
}

scoped_refptr<GLSurface> CreateOffscreenGLSurface(GLDisplay* display,
                                                  const gfx::Size& size) {
  TRACE_EVENT0("gpu", "gl::init::CreateOffscreenGLSurface");
  switch (GetGLImplementation()) {
    case kGLImplementationEGLANGLE: {
      GLDisplayEGL* display_egl = display->GetAs<gl::GLDisplayEGL>();
      if (display_egl->IsEGLSurfacelessContextSupported() &&
          size.width() == 0 && size.height() == 0) {
        return InitializeGLSurface(new SurfacelessEGL(display_egl, size));
      } else {
        return InitializeGLSurface(new PbufferGLSurfaceEGL(display_egl, size));
      }
    }
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return InitializeGLSurface(new GLSurfaceStub());
    default:
      NOTREACHED();
  }
}

void SetDisabledExtensionsPlatform(const std::string& disabled_extensions) {
  GLImplementation implementation = GetGLImplementation();
  DCHECK_NE(kGLImplementationNone, implementation);
  // TODO(zmo): Implement this if needs arise.
}

bool InitializeExtensionSettingsOneOffPlatform(GLDisplay* display) {
  GLImplementation implementation = GetGLImplementation();
  DCHECK_NE(kGLImplementationNone, implementation);
  // TODO(zmo): Implement this if needs arise.
  return true;
}

}  // namespace gl::init
