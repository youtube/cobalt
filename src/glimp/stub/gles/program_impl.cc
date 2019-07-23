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

#include "glimp/stub/gles/program_impl.h"

#include "glimp/stub/gles/shader_impl.h"
#include "nb/polymorphic_downcast.h"
#include "starboard/common/log.h"

namespace glimp {
namespace gles {

ProgramImplStub::ProgramImplStub()
    : linked_vertex_shader_(NULL), linked_fragment_shader_(NULL) {}

ProgramImpl::LinkResults ProgramImplStub::Link(
    const nb::scoped_refptr<Shader>& vertex_shader,
    const nb::scoped_refptr<Shader>& fragment_shader) {
  linked_vertex_shader_ =
      nb::polymorphic_downcast<ShaderImplStub*>(vertex_shader->impl());
  linked_fragment_shader_ =
      nb::polymorphic_downcast<ShaderImplStub*>(fragment_shader->impl());

  bound_attributes_.clear();

  return ProgramImpl::LinkResults(true);
}

bool ProgramImplStub::BindAttribLocation(unsigned int index, const char* name) {
  SB_DCHECK(linked_vertex_shader_);
  return false;
}

int ProgramImplStub::GetUniformLocation(const char* name) {
  return -1;
}

int ProgramImplStub::GetResourceForBoundAttrib(unsigned int index) const {
  std::map<unsigned int, int>::const_iterator found =
      bound_attributes_.find(index);
  if (found == bound_attributes_.end()) {
    return -1;
  }

  return found->second;
}

}  // namespace gles
}  // namespace glimp
