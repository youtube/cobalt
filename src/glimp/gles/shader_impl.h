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

#ifndef GLIMP_GLES_SHADER_IMPL_H_
#define GLIMP_GLES_SHADER_IMPL_H_

#include <string>

namespace glimp {
namespace gles {

class ShaderImpl {
 public:
  // Return value from the Compile() method.  Can be used to communicate an
  // error message to the client.
  struct CompileResults {
    explicit CompileResults(bool success) : success(success) {}
    CompileResults(bool success, const std::string& info_log)
        : success(success), info_log(info_log) {}
    bool success;
    std::string info_log;
  };

  virtual ~ShaderImpl() {}

  // Called when glCompileShader() is called.  The source passed in is whatever
  // source was last set by glShaderSource().  This method should return true
  // on success and false on failure, along with an information message if
  // desired.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glCompileShader.xml
  virtual CompileResults Compile(const std::string& source) = 0;

 private:
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_SHADER_IMPL_H_
