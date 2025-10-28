// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef INTERNAL_STARBOARD_DARWIN_INCLUDES_GLES2_GL2_H_
#define INTERNAL_STARBOARD_DARWIN_INCLUDES_GLES2_GL2_H_

#include <GLES2/gl2platform.h>
#include <OpenGLES/ES2/gl.h>

extern "C" GL_APICALL void GL_APIENTRY eaglBindFramebuffer(GLenum target,
                                                           GLuint framebuffer);

#endif  // INTERNAL_STARBOARD_DARWIN_INCLUDES_GLES2_GL2_H_
