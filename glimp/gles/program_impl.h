/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef GLIMP_GLES_PROGRAM_IMPL_H_
#define GLIMP_GLES_PROGRAM_IMPL_H_

#include "glimp/gles/shader.h"
#include "glimp/nb/ref_counted.h"

namespace glimp {
namespace gles {

class ProgramImpl {
 public:
  virtual ~ProgramImpl() {}

  // Ultimately called by glLinkProgram(), this marks the end of the program's
  // setup phase and the beginning of the program's ability to be used.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glLinkProgram.xml
  virtual bool Link(const nb::scoped_refptr<Shader>& vertex_shader,
                    const nb::scoped_refptr<Shader>& fragment_shader) = 0;

 private:
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_PROGRAM_IMPL_H_
