/*
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

#ifndef GLIMP_STUB_GLES_CONTEXT_IMPL_H_
#define GLIMP_STUB_GLES_CONTEXT_IMPL_H_

#include "glimp/egl/surface.h"
#include "glimp/gles/context_impl.h"
#include "glimp/gles/draw_state.h"
#include "glimp/gles/index_data_type.h"
#include "glimp/shaders/glsl_shader_map_helpers.h"
#include "nb/scoped_ptr.h"

namespace glimp {
namespace gles {

class ContextImplStub : public ContextImpl {
 public:
  ContextImplStub();
  virtual ~ContextImplStub() {}

  ContextImpl::ExtensionList GetExtensions() const override;
  int GetMaxVertexAttribs() const override;
  int GetMaxFragmentTextureUnits() const override;
  int GetMaxCombinedTextureImageUnits() const override;
  int GetMaxTextureSize() const override;
  int GetMaxRenderbufferSize() const override;
  int GetMaxFragmentUniformVectors() const override;
  int GetMaxVertexTextureImageUnits() const override { return 0; }

  nb::scoped_ptr<ProgramImpl> CreateProgram() override;

  nb::scoped_ptr<ShaderImpl> CreateVertexShader() override;
  nb::scoped_ptr<ShaderImpl> CreateFragmentShader() override;

  nb::scoped_ptr<BufferImpl> CreateBuffer() override;

  nb::scoped_ptr<TextureImpl> CreateTexture() override;

  void Flush() override;
  void Finish() override;

  void Clear(bool clear_color,
             bool clear_depth,
             bool clear_stencil,
             const DrawState& draw_state,
             DrawStateDirtyFlags* dirty_flags) override;

  void DrawArrays(DrawMode mode,
                  int first_vertex,
                  int num_vertices,
                  const DrawState& draw_state,
                  DrawStateDirtyFlags* dirty_flags) override;

  void DrawElements(DrawMode mode,
                    int num_vertices,
                    IndexDataType index_data_type,
                    intptr_t index_offset_in_bytes,
                    const DrawState& draw_state,
                    DrawStateDirtyFlags* dirty_flags) override;

  void SwapBuffers(egl::Surface* surface) override;

 private:
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_STUB_GLES_CONTEXT_IMPL_H_
