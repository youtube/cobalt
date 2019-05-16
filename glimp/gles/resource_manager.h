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

#ifndef GLIMP_GLES_RESOURCE_MANAGER_H_
#define GLIMP_GLES_RESOURCE_MANAGER_H_

#include "glimp/gles/buffer.h"
#include "glimp/gles/framebuffer.h"
#include "glimp/gles/program.h"
#include "glimp/gles/ref_counted_resource_map.h"
#include "glimp/gles/renderbuffer.h"
#include "glimp/gles/shader.h"
#include "glimp/gles/texture.h"
#include "nb/ref_counted.h"
#include "starboard/common/mutex.h"

namespace glimp {
namespace gles {

// The ResourceManager is responsible for managing the set of all GL ES
// resources used by contexts.  It manages the assignment of IDs to resources
// like textures, buffers, shaders, programs and so on.  When it is specified
// that a context should share resources with another upon construction, it is
// this ResourceManager object that is shared between them, and thus it must
// also be thread-safe.
class ResourceManager : public nb::RefCountedThreadSafe<ResourceManager> {
 public:
  ~ResourceManager();

  uint32_t RegisterProgram(const nb::scoped_refptr<Program>& program);
  nb::scoped_refptr<Program> GetProgram(uint32_t id);
  nb::scoped_refptr<Program> DeregisterProgram(uint32_t id);

  uint32_t RegisterShader(const nb::scoped_refptr<Shader>& shader);
  nb::scoped_refptr<Shader> GetShader(uint32_t id);
  nb::scoped_refptr<Shader> DeregisterShader(uint32_t id);

  uint32_t RegisterBuffer(const nb::scoped_refptr<Buffer>& buffer);
  nb::scoped_refptr<Buffer> GetBuffer(uint32_t id);
  nb::scoped_refptr<Buffer> DeregisterBuffer(uint32_t id);

  uint32_t RegisterTexture(const nb::scoped_refptr<Texture>& texture);
  nb::scoped_refptr<Texture> GetTexture(uint32_t id);
  nb::scoped_refptr<Texture> DeregisterTexture(uint32_t id);

  uint32_t RegisterFramebuffer(
      const nb::scoped_refptr<Framebuffer>& framebuffer);
  nb::scoped_refptr<Framebuffer> GetFramebuffer(uint32_t id);
  nb::scoped_refptr<Framebuffer> DeregisterFramebuffer(uint32_t id);

  uint32_t RegisterRenderbuffer(
      const nb::scoped_refptr<Renderbuffer>& renderbuffer);
  nb::scoped_refptr<Renderbuffer> GetRenderbuffer(uint32_t id);
  nb::scoped_refptr<Renderbuffer> DeregisterRenderbuffer(uint32_t id);

 private:
  starboard::Mutex mutex_;

  RefCountedResourceMap<Program> programs_;
  RefCountedResourceMap<Shader> shaders_;
  RefCountedResourceMap<Buffer> buffers_;
  RefCountedResourceMap<Texture> textures_;
  RefCountedResourceMap<Framebuffer> framebuffers_;
  RefCountedResourceMap<Renderbuffer> renderbuffers_;
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_RESOURCE_MANAGER_H_
