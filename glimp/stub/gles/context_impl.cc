/*
 * Copyright 2023 The Cobalt Authors. All Rights Reserved.
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "glimp/stub/gles/context_impl.h"

#include <algorithm>
#include <vector>

#include "glimp/polymorphic_downcast.h"
#include "glimp/rect.h"
#include "glimp/stub/egl/surface_impl.h"
#include "glimp/stub/gles/buffer_impl.h"
#include "glimp/stub/gles/program_impl.h"
#include "glimp/stub/gles/shader_impl.h"
#include "glimp/stub/gles/texture_impl.h"
#include "glimp/tracing/tracing.h"

namespace glimp {
namespace gles {

ContextImplStub::ContextImplStub() {}

ContextImpl::ExtensionList ContextImplStub::GetExtensions() const {
  ContextImpl::ExtensionList extensions;

  // We are able to accept RGBA8 textures as framebuffer attachments (i.e.
  // as render targets).
  extensions.push_back("GL_OES_rgb8_rgba8");

  // This is required in order to render distance field fonts.
  extensions.push_back("GL_OES_standard_derivatives");

  return extensions;
}

int ContextImplStub::GetMaxVertexAttribs() const {
  return 16;
}

int ContextImplStub::GetMaxFragmentTextureUnits() const {
  return 16;
}

int ContextImplStub::GetMaxCombinedTextureImageUnits() const {
  return 0;
}

int ContextImplStub::GetMaxTextureSize() const {
  return 4096;
}

int ContextImplStub::GetMaxRenderbufferSize() const {
  return 4096;
}

int ContextImplStub::GetMaxFragmentUniformVectors() const {
  // GL ES requires that it is at least 16, so we will return that.
  return ProgramImplStub::kMaxUniformsPerShader;
}

std::unique_ptr<ProgramImpl> ContextImplStub::CreateProgram() {
  return std::unique_ptr<ProgramImpl>(new ProgramImplStub());
}

std::unique_ptr<ShaderImpl> ContextImplStub::CreateVertexShader() {
  return std::unique_ptr<ShaderImpl>(
      new ShaderImplStub(ShaderImplStub::kVertex));
}

std::unique_ptr<ShaderImpl> ContextImplStub::CreateFragmentShader() {
  return std::unique_ptr<ShaderImpl>(
      new ShaderImplStub(ShaderImplStub::kFragment));
}

std::unique_ptr<BufferImpl> ContextImplStub::CreateBuffer() {
  return std::unique_ptr<BufferImpl>(new BufferImplStub());
}

std::unique_ptr<TextureImpl> ContextImplStub::CreateTexture() {
  return std::unique_ptr<TextureImpl>(new TextureImplStub());
}

void ContextImplStub::Flush() {
  // Do nothing, we will flush when we swap buffers.
}

void ContextImplStub::Finish() {}

void ContextImplStub::Clear(bool clear_color,
                            bool clear_depth,
                            bool clear_stencil,
                            const DrawState& draw_state,
                            DrawStateDirtyFlags* dirty_flags) {}

void ContextImplStub::DrawArrays(DrawMode mode,
                                 int first_vertex,
                                 int num_vertices,
                                 const DrawState& draw_state,
                                 DrawStateDirtyFlags* dirty_flags) {}

void ContextImplStub::DrawElements(DrawMode mode,
                                   int num_vertices,
                                   IndexDataType index_data_type,
                                   intptr_t index_offset_in_bytes,
                                   const DrawState& draw_state,
                                   DrawStateDirtyFlags* dirty_flags) {}

void ContextImplStub::SwapBuffers(egl::Surface* surface) {}

}  // namespace gles
}  // namespace glimp
