// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_INIT_GL_FACTORY_H_
#define UI_GL_INIT_GL_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_format.h"
#include "ui/gl/gpu_preference.h"
#include "ui/gl/init/gl_init_export.h"
#include "ui/gl/presenter.h"

namespace gl {

class GLContext;
class GLShareGroup;
class GLSurface;

struct GLContextAttribs;
struct GLVersionInfo;

namespace init {

// Returns a list of allowed GL implementations. The default implementation will
// be the first item.
GL_INIT_EXPORT std::vector<GLImplementationParts> GetAllowedGLImplementations();

// Initializes GL bindings and extension settings.
// |gpu_preference| specifies which GPU to use on a multi-GPU system.
// If its value is kDefault, use the default GPU of the system.
GL_INIT_EXPORT GLDisplay* InitializeGLOneOff(gl::GpuPreference gpu_preference);

// Initializes GL bindings without initializing extension settings.
// |gpu_preference| specifies which GPU to use on a multi-GPU system.
// If its value is kDefault, use the default GPU of the system.
GL_INIT_EXPORT GLDisplay* InitializeGLNoExtensionsOneOff(
    bool init_bindings,
    gl::GpuPreference gpu_preference);

// Initializes GL bindings - load dlls and get proc address according to gl
// command line switch.
GL_INIT_EXPORT bool InitializeStaticGLBindingsOneOff();

// Initialize plaiform dependent extension settings, including bindings,
// capabilities, etc.
GL_INIT_EXPORT bool InitializeExtensionSettingsOneOffPlatform(
    GLDisplay* display);

// Initializes GL bindings using the provided parameters. This might be required
// for use in tests.
GL_INIT_EXPORT bool InitializeStaticGLBindingsImplementation(
    GLImplementationParts impl,
    bool fallback_to_software_gl);

// Initializes GL platform using the provided parameters. This might be required
// for use in tests. This should be called only after GL bindings are initilzed
// successfully.
// |gpu_preference| specifies which GPU to use on a multi-GPU system.
// If its value is kDefault, use the default GPU of the system.
GL_INIT_EXPORT GLDisplay* InitializeGLOneOffPlatformImplementation(
    bool fallback_to_software_gl,
    bool disable_gl_drawing,
    bool init_extensions,
    gl::GpuPreference gpu_preference);

// Does the same as the above, but returns a cached display if one is already
// initialized for the requested GPU.
GL_INIT_EXPORT GLDisplay* GetOrInitializeGLOneOffPlatformImplementation(
    bool fallback_to_software_gl,
    bool disable_gl_drawing,
    bool init_extensions,
    gl::GpuPreference gpu_preference);

// Clears GL bindings and resets GL implementation.
// Calling this function a second time on the same |display| is a no-op.
GL_INIT_EXPORT void ShutdownGL(GLDisplay* display, bool due_to_fallback);

// Return information about the GL window system binding implementation (e.g.,
// EGL, GLX, WGL). Returns true if the information was retrieved successfully.
GL_INIT_EXPORT bool GetGLWindowSystemBindingInfo(
    const GLVersionInfo& gl_info,
    GLWindowSystemBindingInfo* info);

// Creates a GL context that is compatible with the given surface.
// |share_group|, if non-null, is a group of contexts which the internally
// created OpenGL context shares textures and other resources.
GL_INIT_EXPORT scoped_refptr<GLContext> CreateGLContext(
    GLShareGroup* share_group,
    GLSurface* compatible_surface,
    const GLContextAttribs& attribs);

// Creates a GL surface that renders directly to a view.
GL_INIT_EXPORT scoped_refptr<GLSurface> CreateViewGLSurface(
    GLDisplay* display,
    gfx::AcceleratedWidget window);

#if BUILDFLAG(IS_OZONE)
// Creates a GL surface that renders directly into a window with surfaceless
// semantics - there is no default framebuffer and the primary surface must
// be presented as an overlay. If surfaceless mode is not supported or
// enabled it will return a null pointer.
GL_INIT_EXPORT scoped_refptr<Presenter> CreateSurfacelessViewGLSurface(
    GLDisplay* display,
    gfx::AcceleratedWidget window);
#endif  // BUILDFLAG(IS_OZONE)

// Creates a GL surface used for offscreen rendering.
GL_INIT_EXPORT scoped_refptr<GLSurface> CreateOffscreenGLSurface(
    GLDisplay* display,
    const gfx::Size& size);

GL_INIT_EXPORT scoped_refptr<GLSurface> CreateOffscreenGLSurfaceWithFormat(
    GLDisplay* display,
    const gfx::Size& size,
    GLSurfaceFormat format);

// Set platform dependent disabled extensions and re-initialize extension
// bindings.
GL_INIT_EXPORT void SetDisabledExtensionsPlatform(
    const std::string& disabled_extensions);

// Disable ANGLE and force to use native or other GL implementation.
GL_INIT_EXPORT void DisableANGLE();

}  // namespace init
}  // namespace gl

#endif  // UI_GL_INIT_GL_FACTORY_H_
