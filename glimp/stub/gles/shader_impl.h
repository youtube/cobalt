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

#ifndef GLIMP_STUB_GLES_SHADER_IMPL_H_
#define GLIMP_STUB_GLES_SHADER_IMPL_H_

#include <string>

#include "glimp/gles/shader_impl.h"
#include "glimp/gles/uniform_info.h"
#include "glimp/shaders/glsl_shader_map_helpers.h"
#include "nb/ref_counted.h"
#include "starboard/common/log.h"

namespace glimp {
namespace gles {

class ShaderImplStub : public ShaderImpl {
 public:
  enum Type {
    kVertex,
    kFragment,
  };

  explicit ShaderImplStub(Type type);
  ~ShaderImplStub() override {}

  ShaderImpl::CompileResults Compile(const std::string& source) override;

 private:
  Type type_;
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_STUB_GLES_SHADER_IMPL_H_
