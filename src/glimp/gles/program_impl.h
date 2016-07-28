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

#include <string>

#include "glimp/gles/shader.h"
#include "nb/ref_counted.h"

namespace glimp {
namespace gles {

class ProgramImpl {
 public:
  // Return value from the Link() method.  Can be used to communicate an
  // error message to the client.
  struct LinkResults {
    explicit LinkResults(bool success) : success(success) {}
    LinkResults(bool success, const std::string& info_log)
        : success(success), info_log(info_log) {}
    bool success;
    std::string info_log;
  };

  virtual ~ProgramImpl() {}

  // Ultimately called by glLinkProgram(), this marks the end of the program's
  // setup phase and the beginning of the program's ability to be used.
  // This method should return true on success and false on failure, along
  // with an information message if desired.
  // Upon successful linking, BindAttribLocation() will be called for each
  // existing attribute binding, so any exiting bindings should be cleared
  // within Link().
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glLinkProgram.xml
  virtual LinkResults Link(
      const nb::scoped_refptr<Shader>& vertex_shader,
      const nb::scoped_refptr<Shader>& fragment_shader) = 0;

  // Binds the attribute with the specified |name| to the specified index.
  // Implementations can take this opportunity to resolve |name| to a
  // platform-specific attribute identifier so that a by-name lookup does not
  // need to occur when a draw command is issued.  This function will only
  // be called after Link() is called.  If a re-link is performed, this command
  // is re-issued by Program for all existing bindings.
  // Returns true on success and false if the attribute name could not be found.
  // This method will be called when glBindAttribLocation() is called.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glBindAttribLocation.xml
  virtual bool BindAttribLocation(unsigned int index, const char* name) = 0;

  // Returns the location of the specified uniform within the shader.  Texture
  // samplers are included in this definition of "uniform".  This will later
  // be used to reference the uniform, and will be eventually set in the
  // DrawState that is passed into draw calls.  This method should return
  // -1 if there is no uniform by the given |name|.
  // This method will be called when glGetUniformLocation() is called.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glGetUniformLocation.xml
  virtual int GetUniformLocation(const char* name) = 0;

 private:
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_PROGRAM_IMPL_H_
