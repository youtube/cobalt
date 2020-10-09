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

#ifndef GLIMP_STUB_GLES_PROGRAM_IMPL_H_
#define GLIMP_STUB_GLES_PROGRAM_IMPL_H_

#include <map>

#include "glimp/gles/program_impl.h"
#include "glimp/gles/shader.h"
#include "glimp/gles/uniform_info.h"
#include "glimp/stub/gles/shader_impl.h"
#include "nb/ref_counted.h"

namespace glimp {
namespace gles {

class ProgramImplStub : public ProgramImpl {
 public:
  static const int kMaxUniformsPerShader = 16;

  ProgramImplStub();
  ~ProgramImplStub() override {}

  ProgramImpl::LinkResults Link(
      const nb::scoped_refptr<Shader>& vertex_shader,
      const nb::scoped_refptr<Shader>& fragment_shader) override;

  bool BindAttribLocation(unsigned int index, const char* name) override;

  int GetUniformLocation(const char* name) override;

  // Returns the location of the shader attribute that was previously bound
  // to |index| in a call to BindAttribLocation().
  int GetResourceForBoundAttrib(unsigned int index) const;

  ShaderImplStub* linked_vertex_shader() const { return linked_vertex_shader_; }

  ShaderImplStub* linked_fragment_shader() const {
    return linked_fragment_shader_;
  }

 private:
  ShaderImplStub* linked_vertex_shader_;
  ShaderImplStub* linked_fragment_shader_;

  std::map<unsigned int, int> bound_attributes_;
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_STUB_GLES_PROGRAM_IMPL_H_
