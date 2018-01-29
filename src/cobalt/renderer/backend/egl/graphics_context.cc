// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <GLES2/gl2.h>
#include <algorithm>

#include "cobalt/renderer/backend/egl/graphics_context.h"

#include "base/debug/leak_annotations.h"
#include "base/debug/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/renderer/backend/egl/framebuffer_render_target.h"
#include "cobalt/renderer/backend/egl/graphics_system.h"
#include "cobalt/renderer/backend/egl/pbuffer_render_target.h"
#include "cobalt/renderer/backend/egl/render_target.h"
#include "cobalt/renderer/backend/egl/texture.h"
#include "cobalt/renderer/backend/egl/texture_data.h"
#include "cobalt/renderer/backend/egl/utils.h"

namespace cobalt {
namespace renderer {
namespace backend {

namespace {

bool HasExtension(const char* extension) {
  const char* raw_extension_string =
      reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
  DCHECK(raw_extension_string);

  return strstr(raw_extension_string, extension) != NULL;
}

}  // namespace

GraphicsContextEGL::GraphicsContextEGL(GraphicsSystem* parent_system,
                                       EGLDisplay display, EGLConfig config,
                                       ResourceContext* resource_context)
    : GraphicsContext(parent_system),
      display_(display),
      config_(config),
      is_current_(false) {
#if defined(GLES3_SUPPORTED)
  context_ = CreateGLES3Context(display, config, resource_context->context());
#else
  // Create an OpenGL ES 2.0 context.
  EGLint context_attrib_list[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE,
  };
  context_ =
      eglCreateContext(display, config, EGL_NO_CONTEXT, context_attrib_list);
  CHECK_EQ(EGL_SUCCESS, eglGetError());
#endif

  // Create a dummy EGLSurface object to be assigned as the target surface
  // when we need to make OpenGL calls that do not depend on a surface (e.g.
  // creating a texture).
  null_surface_ = new PBufferRenderTargetEGL(display, config, math::Size(0, 0));
  CHECK(!null_surface_->CreationError());

  ScopedMakeCurrent scoped_current_context(this);

  bgra_format_supported_ = HasExtension("GL_EXT_texture_format_BGRA8888");

  SetupBlitObjects();

  {
    // The current mesa egl drivers leak memory on the first call to glDraw*.
    // Get that first draw out of the way, and do something useful with it.
    ANNOTATE_SCOPED_MEMORY_LEAK;
    ComputeReadPixelsNeedVerticalFlip();
  }
}

GraphicsSystemEGL* GraphicsContextEGL::system_egl() {
  return base::polymorphic_downcast<GraphicsSystemEGL*>(system());
}

bool GraphicsContextEGL::ComputeReadPixelsNeedVerticalFlip() {
  // This computation is expensive, so it is cached the first time that it is
  // computed. Simply return the value if it is already cached.
  if (read_pixels_needs_vertical_flip_) {
    return *read_pixels_needs_vertical_flip_;
  }

  // Create a 1x2 texture with distinct values vertically so that we can test
  // them.  We will blit the texture to an offscreen render target, and then
  // read back the value of the render target's pixels to check if they are
  // flipped or not.  It is found that the results of this test can differ
  // between at least Angle and Mesa GL, so if the results of this test are not
  // taken into account, one of those platforms will read pixels out upside
  // down.
  const int kDummyTextureWidth = 1;
  const int kDummyTextureHeight = 2;

  ScopedMakeCurrent scoped_make_current(this);

  scoped_ptr<FramebufferEGL> framebuffer(new FramebufferEGL(this,
      math::Size(kDummyTextureWidth, kDummyTextureHeight), GL_RGBA, GL_NONE));
  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->gl_handle()));

  {
    // Create a 2-pixel texture and then immediately blit it to our 2-pixel
    // framebuffer render target.
    GLuint texture;
    GL_CALL(glGenTextures(1, &texture));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, texture));

    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL_CALL(
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CALL(
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    uint8_t pixels[8];
    pixels[0] = 0;
    pixels[1] = 0;
    pixels[2] = 0;
    pixels[3] = 0;
    pixels[4] = 255;
    pixels[5] = 255;
    pixels[6] = 255;
    pixels[7] = 255;

    GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kDummyTextureWidth,
                         kDummyTextureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         pixels));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));

    Blit(texture, 0, 0, kDummyTextureWidth, kDummyTextureHeight);
    GL_CALL(glDeleteTextures(1, &texture));
    GL_CALL(glFinish());
  }

  // Now read back the texture data using glReadPixels().
  uint32_t out_data[kDummyTextureWidth * kDummyTextureHeight];
  GL_CALL(glReadPixels(0, 0, kDummyTextureWidth, kDummyTextureHeight, GL_RGBA,
                       GL_UNSIGNED_BYTE, out_data));

  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));

  // Ensure the data is in one of two possible states, flipped or not flipped.
  DCHECK((out_data[0] == 0x00000000 && out_data[1] == 0xFFFFFFFF) ||
         (out_data[0] == 0xFFFFFFFF && out_data[1] == 0x00000000));

  // Finally check if the data we read back was flipped or not and cache the
  // result.
  read_pixels_needs_vertical_flip_ = out_data[1] == 0x00000000;
  return *read_pixels_needs_vertical_flip_;
}

void GraphicsContextEGL::SetupBlitObjects() {
  // Setup shaders used when blitting the current texture.
  blit_program_ = glCreateProgram();

  blit_vertex_shader_ = glCreateShader(GL_VERTEX_SHADER);
  const char* blit_vertex_shader_source =
      "attribute vec2 a_position;"
      "attribute vec2 a_tex_coord;"
      "varying vec2 v_tex_coord;"
      "void main() {"
      "  gl_Position = vec4(a_position.x, a_position.y, 0, 1);"
      "  v_tex_coord = a_tex_coord;"
      "}";
  int blit_vertex_shader_source_length = strlen(blit_vertex_shader_source);
  GL_CALL(glShaderSource(blit_vertex_shader_, 1, &blit_vertex_shader_source,
                         &blit_vertex_shader_source_length));
  GL_CALL(glCompileShader(blit_vertex_shader_));
  GL_CALL(glAttachShader(blit_program_, blit_vertex_shader_));

  blit_fragment_shader_ = glCreateShader(GL_FRAGMENT_SHADER);
  const char* blit_fragment_shader_source =
      "precision mediump float;"
      "varying vec2 v_tex_coord;"
      "uniform sampler2D tex;"
      "void main() {"
      "  gl_FragColor = texture2D(tex, v_tex_coord);"
      "}";
  int blit_fragment_shader_source_length = strlen(blit_fragment_shader_source);
  GL_CALL(glShaderSource(blit_fragment_shader_, 1, &blit_fragment_shader_source,
                         &blit_fragment_shader_source_length));
  GL_CALL(glCompileShader(blit_fragment_shader_));
  GL_CALL(glAttachShader(blit_program_, blit_fragment_shader_));

  GL_CALL(glBindAttribLocation(
      blit_program_, kBlitPositionAttribute, "a_position"));
  GL_CALL(glBindAttribLocation(
      blit_program_, kBlitTexcoordAttribute, "a_tex_coord"));

  GL_CALL(glLinkProgram(blit_program_));

  // Setup a vertex buffer that can blit a quad with a full texture, to be used
  // by Frame::BlitToRenderTarget().
  struct QuadVertex {
    float position_x;
    float position_y;
    float tex_coord_u;
    float tex_coord_v;
  };
  const QuadVertex kBlitQuadVerts[4] = {
      {-1.0f, -1.0f, 0.0f, 1.0f},
      {-1.0f, 1.0f, 0.0f, 0.0f},
      {1.0f, -1.0f, 1.0f, 1.0f},
      {1.0f, 1.0, 1.0f, 0.0f},
  };
  GL_CALL(glGenBuffers(1, &blit_vertex_buffer_));
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, blit_vertex_buffer_));
  GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(kBlitQuadVerts), kBlitQuadVerts,
                       GL_STATIC_DRAW));
}

GraphicsContextEGL::~GraphicsContextEGL() {
  MakeCurrent();
  GL_CALL(glFinish());
  GL_CALL(glDeleteBuffers(1, &blit_vertex_buffer_));
  GL_CALL(glDeleteProgram(blit_program_));
  GL_CALL(glDeleteShader(blit_fragment_shader_));
  GL_CALL(glDeleteShader(blit_vertex_shader_));

  ReleaseCurrentContext();

  null_surface_ = NULL;

  EGL_CALL(eglDestroyContext(display_, context_));
}

void GraphicsContextEGL::SafeEglMakeCurrent(RenderTargetEGL* surface) {
  // In some EGL implementations, like Angle, the first time we make current on
  // a thread can result in global allocations being made that are never freed.
  ANNOTATE_SCOPED_MEMORY_LEAK;

  EGLSurface egl_surface = surface->GetSurface();

  // This should only be used with egl surfaces (not framebuffer objects).
  DCHECK_NE(egl_surface, EGL_NO_SURFACE);
  DCHECK_EQ(surface->GetPlatformHandle(), 0);

  if (surface->is_surface_bad()) {
    // A surface may become invalid in the middle of shutdown processing. If
    // this is a known bad surface, then bind the null surface just as if this
    // is the first time it is found to be bad.
    egl_surface = null_surface_->GetSurface();
    EGL_CALL(eglMakeCurrent(display_, egl_surface, egl_surface, context_));
    return;
  }

  eglMakeCurrent(display_, egl_surface, egl_surface, context_);
  EGLint make_current_error = eglGetError();
  if (make_current_error != EGL_SUCCESS) {
    LOG(ERROR) << "eglMakeCurrent ERROR: " << make_current_error;
    if (make_current_error == EGL_BAD_ALLOC ||
        make_current_error == EGL_BAD_NATIVE_WINDOW) {
      LOG(ERROR) << "eglMakeCurrent raised either EGL_BAD_ALLOC or "
                    "EGL_BAD_NATIVE_WINDOW, continuing with null surface "
                    "under the assumption that our window surface has become "
                    "invalid due to a suspend or shutdown being triggered.";
      surface->set_surface_bad();
      egl_surface = null_surface_->GetSurface();
      EGL_CALL(eglMakeCurrent(display_, egl_surface, egl_surface, context_));
    } else {
      NOTREACHED() << "Unexpected error when calling eglMakeCurrent().";
    }
  }
}

void GraphicsContextEGL::MakeCurrentWithSurface(RenderTargetEGL* surface) {
  DCHECK_NE(EGL_NO_SURFACE, surface) <<
      "Use ReleaseCurrentContext().";

  // In some EGL implementations, like Angle, the first time we make current on
  // a thread can result in global allocations being made that are never freed.
  ANNOTATE_SCOPED_MEMORY_LEAK;

  EGLSurface egl_surface = surface->GetSurface();
  if (egl_surface != EGL_NO_SURFACE) {
    SafeEglMakeCurrent(surface);

    // Minimize calls to glBindFramebuffer. Normally, nothing keeps their
    // framebuffer object bound, so 0 is normally bound at this point --
    // unless the previous MakeCurrentWithSurface bound a framebuffer object.
    if (current_surface_ && current_surface_->GetPlatformHandle() != 0) {
      GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    }
  } else {
    // This is a framebuffer object, and it does not have an EGL surface. It
    // must be bound using glBindFramebuffer. Use the null surface's EGLSurface
    // with eglMakeCurrent to avoid polluting the previous EGLSurface target.
    DCHECK_NE(surface->GetPlatformHandle(), 0);

    // Since we don't care about what surface is backing the default
    // framebuffer, don't change draw surfaces unless we simply don't have one
    // already.
    if (!IsCurrent()) {
      egl_surface = null_surface_->GetSurface();
      EGL_CALL(eglMakeCurrent(display_, egl_surface, egl_surface, context_));
    }
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, surface->GetPlatformHandle()));
  }

  if (surface->IsWindowRenderTarget() && !surface->has_been_made_current()) {
    SecurityClear();
  }
  surface->set_has_been_made_current();

  is_current_ = true;
  current_surface_ = surface;
}

void GraphicsContextEGL::ResetCurrentSurface() {
  if (is_current_ && current_surface_) {
    if (current_surface_->GetSurface() == EGL_NO_SURFACE) {
      EGLSurface egl_surface = null_surface_->GetSurface();
      EGL_CALL(eglMakeCurrent(display_, egl_surface, egl_surface, context_));
    } else {
      SafeEglMakeCurrent(current_surface_);
    }

    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER,
        current_surface_->GetPlatformHandle()));
  }
}

void GraphicsContextEGL::MakeCurrent() {
  // Some GL drivers do *not* handle switching contexts in the middle of a
  // frame very well, so with this change we avoid making a new surface
  // current if we don't actually care about what surface is current.
  if (!IsCurrent()) {
    MakeCurrentWithSurface(null_surface_);
  }
}

void GraphicsContextEGL::ReleaseCurrentContext() {
  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
  EGL_CALL(eglMakeCurrent(
      display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

  current_surface_ = NULL;
  is_current_ = false;
}

scoped_ptr<TextureEGL> GraphicsContextEGL::CreateTexture(
    scoped_ptr<TextureDataEGL> texture_data) {
  return make_scoped_ptr(
      new TextureEGL(this, texture_data.Pass(), bgra_format_supported_));
}

scoped_ptr<TextureEGL> GraphicsContextEGL::CreateTextureFromRawMemory(
    const scoped_refptr<ConstRawTextureMemoryEGL>& raw_texture_memory,
    intptr_t offset, const math::Size& size, GLenum format,
    int pitch_in_bytes) {
  const RawTextureMemoryEGL* texture_memory =
      &(raw_texture_memory->raw_texture_memory());

  return make_scoped_ptr(new TextureEGL(this, texture_memory, offset, size,
                                        format, pitch_in_bytes,
                                        bgra_format_supported_));
}

scoped_refptr<RenderTarget> GraphicsContextEGL::CreateOffscreenRenderTarget(
    const math::Size& dimensions) {
  scoped_refptr<RenderTarget> render_target(new PBufferRenderTargetEGL(
      display_, config_, dimensions));

  if (render_target->CreationError()) {
    return scoped_refptr<RenderTarget>();
  } else {
    return render_target;
  }
}

scoped_refptr<RenderTarget>
  GraphicsContextEGL::CreateDownloadableOffscreenRenderTarget(
      const math::Size& dimensions) {
  scoped_refptr<RenderTarget> render_target(new FramebufferRenderTargetEGL(
      this, dimensions));

  if (render_target->CreationError()) {
    return scoped_refptr<RenderTarget>();
  } else {
    return render_target;
  }
}

void GraphicsContextEGL::InitializeDebugContext() {
  ComputeReadPixelsNeedVerticalFlip();
}

namespace {
void VerticallyFlipPixels(uint8_t* pixels, int pitch_in_bytes, int height) {
  int half_height = height / 2;
  for (int row = 0; row < half_height; ++row) {
    uint8_t* top_row = pixels + row * pitch_in_bytes;
    uint8_t* bottom_row =
        pixels + (height - 1 - row) * pitch_in_bytes;
    for (int i = 0; i < pitch_in_bytes; ++i) {
      std::swap(top_row[i], bottom_row[i]);
    }
  }
}
}  // namespace

scoped_array<uint8_t> GraphicsContextEGL::DownloadPixelDataAsRGBA(
    const scoped_refptr<RenderTarget>& render_target) {
  TRACE_EVENT0("cobalt::renderer",
               "GraphicsContextEGL::DownloadPixelDataAsRGBA()");
  ScopedMakeCurrent scoped_current_context(this);

  int pitch_in_bytes =
      render_target->GetSize().width() * BytesPerPixelForGLFormat(GL_RGBA);
  scoped_array<uint8_t> pixels(
      new uint8_t[render_target->GetSize().height() * pitch_in_bytes]);

  if (render_target->GetPlatformHandle() == 0) {
    // Need to bind the PBufferSurface to a framebuffer object in order to
    // read its pixels.
    PBufferRenderTargetEGL* pbuffer_render_target =
        base::polymorphic_downcast<PBufferRenderTargetEGL*>
            (render_target.get());

    scoped_ptr<TextureEGL> texture(new TextureEGL(this, pbuffer_render_target));
    DCHECK(texture->GetSize() == render_target->GetSize());

    // This shouldn't be strictly necessary as glReadPixels() should implicitly
    // call glFinish(), however it doesn't hurt to be safe and guard against
    // potentially different implementations.  Performance is not an issue
    // in this function, because it is only used by tests to verify rendered
    // output.
    GL_CALL(glFinish());

    GLuint texture_framebuffer;
    GL_CALL(glGenFramebuffers(1, &texture_framebuffer));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, texture_framebuffer));

    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, texture->gl_handle(), 0));
    DCHECK_EQ(GL_FRAMEBUFFER_COMPLETE,
              glCheckFramebufferStatus(GL_FRAMEBUFFER));

    GL_CALL(glReadPixels(0, 0, texture->GetSize().width(),
                         texture->GetSize().height(), GL_RGBA, GL_UNSIGNED_BYTE,
                         pixels.get()));

    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL_CALL(glDeleteFramebuffers(1, &texture_framebuffer));
  } else {
    // The render target is a framebuffer object, so just bind it and read.
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER,
                              render_target->GetPlatformHandle()));
    GL_CALL(glReadPixels(0, 0, render_target->GetSize().width(),
                         render_target->GetSize().height(), GL_RGBA,
                         GL_UNSIGNED_BYTE, pixels.get()));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
  }

  // Vertically flip the resulting pixel data before returning so that the 0th
  // pixel is at the top-left.  While computing this is not a fast procedure,
  // this entire function is only intended to be used in debug/test code. This
  // is lazily computed and cached during the first call, so that subsequent
  // calls can simply return the result.
  if (ComputeReadPixelsNeedVerticalFlip()) {
    // Some platforms, like the Mesa Gallium EGL implementation on Linux, seem
    // to return already flipped pixels.  So in that case, we flip them again
    // before returning here.
    VerticallyFlipPixels(pixels.get(), pitch_in_bytes,
                         render_target->GetSize().height());
  }

  return pixels.Pass();
}

void GraphicsContextEGL::Finish() {
  ScopedMakeCurrent scoped_current_context(this);
  GL_CALL(glFinish());
}

void GraphicsContextEGL::Blit(GLuint texture, int x, int y, int width,
                              int height) {
  // Render a texture to the specified output rectangle on the render target.
  GL_CALL(glViewport(x, y, width, height));
  GL_CALL(glScissor(x, y, width, height));

  GL_CALL(glUseProgram(blit_program_));

  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, blit_vertex_buffer_));
  GL_CALL(glVertexAttribPointer(kBlitPositionAttribute, 2, GL_FLOAT, GL_FALSE,
                                sizeof(float) * 4, 0));
  GL_CALL(glVertexAttribPointer(kBlitTexcoordAttribute, 2, GL_FLOAT, GL_FALSE,
                                sizeof(float) * 4,
                                reinterpret_cast<GLvoid*>(sizeof(float) * 2)));
  GL_CALL(glEnableVertexAttribArray(kBlitPositionAttribute));
  GL_CALL(glEnableVertexAttribArray(kBlitTexcoordAttribute));

  GL_CALL(glActiveTexture(GL_TEXTURE0));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, texture));

  GL_CALL(glDisable(GL_BLEND));

  GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));

  GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));

  GL_CALL(glDisableVertexAttribArray(kBlitTexcoordAttribute));
  GL_CALL(glDisableVertexAttribArray(kBlitPositionAttribute));
  GL_CALL(glUseProgram(0));
}

void GraphicsContextEGL::SwapBuffers(RenderTargetEGL* surface) {
  TRACE_EVENT0("cobalt::renderer", "GraphicsContextEGL::SwapBuffers()");
  if (surface->is_surface_bad()) {
    // The surface may become invalid during shutdown.
    return;
  }

  // SwapBuffers should have no effect for offscreen render targets. The
  // current implementation of eglSwapBuffers() does nothing for PBuffers,
  // so only check for framebuffer render targets.
  if (surface->GetPlatformHandle() == 0) {
    eglSwapInterval(display_, COBALT_EGL_SWAP_INTERVAL);
    eglSwapBuffers(display_, surface->GetSurface());
    EGLint swap_err = eglGetError();
    if (swap_err != EGL_SUCCESS) {
      LOG(WARNING) << "Marking surface bad after swap error "
                   << std::hex << swap_err;
      surface->set_surface_bad();
      return;
    }
  }

  surface->increment_swap_count();
  if (surface->IsWindowRenderTarget() && surface->swap_count() <= 2) {
    surface->set_cleared_on_swap(true);
    SecurityClear();
  } else {
    surface->set_cleared_on_swap(false);
  }
}

void GraphicsContextEGL::SecurityClear() {
  // Explicitly clear the screen to transparent to ensure that data from a
  // previous use of the surface is not visible.
  GL_CALL(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
  GL_CALL(glClear(GL_COLOR_BUFFER_BIT));
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt
