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

#ifndef GLIMP_GLES_CONTEXT_IMPL_H_
#define GLIMP_GLES_CONTEXT_IMPL_H_

#include <GLES3/gl3.h>

#include <string>
#include <vector>

#include "glimp/egl/surface.h"
#include "glimp/gles/buffer_impl.h"
#include "glimp/gles/program_impl.h"
#include "glimp/gles/shader_impl.h"
#include "glimp/nb/scoped_ptr.h"

namespace glimp {
namespace gles {

// The ContextImpl class is the interface to the platform-specific backend
// implementation of GL ES Context calls.  Many GL ES methods will ultimately
// end up calling a method on this class.  This class is also responsible for
// acting as a factory for the generation of resource Impl objects as well,
// such as textures or shaders.
class ContextImpl {
 public:
  typedef std::vector<std::string> ExtensionList;

  virtual ~ContextImpl() {}

  // The following methods are called when eglMakeCurrent() is called with
  // different surfaces specified.
  //   https://www.khronos.org/registry/egl/sdk/docs/man/html/eglMakeCurrent.xhtml
  virtual void SetDrawSurface(egl::Surface* surface) = 0;
  virtual void SetReadSurface(egl::Surface* surface) = 0;

  // Called via glScissor.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glScissor.xml
  virtual void SetScissor(int x, int y, int width, int height) = 0;

  // Called via glViewport.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glViewport.xml
  virtual void SetViewport(int x, int y, int width, int height) = 0;

  // Called via glGetString(GL_EXTENSIONS).
  // Note that glimp common code may append its own extensions to the list
  // provided by the implementation.
  //   https://www.khronos.org/opengles/sdk/1.1/docs/man/glGetString.xml
  virtual ExtensionList GetExtensions() const = 0;

  // Called via glCreateProgram.  Must create and return a platform-specific
  // ProgramImpl implementation.  If a NULL scoped_ptr is returned, it is
  // treated as an error.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glCreateProgram.xml
  virtual nb::scoped_ptr<ProgramImpl> CreateProgram() = 0;

  // Called via glCreateShader(GL_VERTEX_SHADER).  Must create a
  // platform-specific ShaderImpl object representing a vertex shader.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glCreateShader.xml
  virtual nb::scoped_ptr<ShaderImpl> CreateVertexShader() = 0;

  // Called via glCreateShader(GL_FRAGMENT_SHADER).  Must create a
  // platform-specific ShaderImpl object representing a fragment shader.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glCreateShader.xml
  virtual nb::scoped_ptr<ShaderImpl> CreateFragmentShader() = 0;

  // Called via glGenBuffers().  Must create a platform-specific BufferImpl
  // object representing a buffer.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glGenBuffers.xml
  virtual nb::scoped_ptr<BufferImpl> CreateBuffer() = 0;

 private:
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_CONTEXT_IMPL_H_
