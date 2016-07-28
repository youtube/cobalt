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

#include "glimp/gles/resource_manager.h"

namespace glimp {
namespace gles {

ResourceManager::~ResourceManager() {
  if (!programs_.empty()) {
    SB_DLOG(WARNING) << "Un-deleted gl programs exist upon shutdown.";
  }

  if (!shaders_.empty()) {
    SB_DLOG(WARNING) << "Un-deleted gl shaders exist upon shutdown.";
  }

  if (!buffers_.empty()) {
    SB_DLOG(WARNING) << "Un-deleted gl buffers exist upon shutdown.";
  }

  if (!textures_.empty()) {
    SB_DLOG(WARNING) << "Un-deleted gl textures exist upon shutdown.";
  }

  if (!framebuffers_.empty()) {
    SB_DLOG(WARNING) << "Un-deleted gl framebuffers exist upon shutdown.";
  }

  if (!renderbuffers_.empty()) {
    SB_DLOG(WARNING) << "Un-deleted gl renderbuffers exist upon shutdown.";
  }
}

uint32_t ResourceManager::RegisterProgram(
    const nb::scoped_refptr<Program>& program) {
  starboard::ScopedLock lock(mutex_);
  return programs_.RegisterResource(program);
}

nb::scoped_refptr<Program> ResourceManager::GetProgram(uint32_t id) {
  starboard::ScopedLock lock(mutex_);
  return programs_.GetResource(id);
}

nb::scoped_refptr<Program> ResourceManager::DeregisterProgram(uint32_t id) {
  starboard::ScopedLock lock(mutex_);
  return programs_.DeregisterResource(id);
}

uint32_t ResourceManager::RegisterShader(
    const nb::scoped_refptr<Shader>& shader) {
  starboard::ScopedLock lock(mutex_);
  return shaders_.RegisterResource(shader);
}

nb::scoped_refptr<Shader> ResourceManager::GetShader(uint32_t id) {
  starboard::ScopedLock lock(mutex_);
  return shaders_.GetResource(id);
}

nb::scoped_refptr<Shader> ResourceManager::DeregisterShader(uint32_t id) {
  starboard::ScopedLock lock(mutex_);
  return shaders_.DeregisterResource(id);
}

uint32_t ResourceManager::RegisterBuffer(
    const nb::scoped_refptr<Buffer>& buffer) {
  starboard::ScopedLock lock(mutex_);
  return buffers_.RegisterResource(buffer);
}

nb::scoped_refptr<Buffer> ResourceManager::GetBuffer(uint32_t id) {
  starboard::ScopedLock lock(mutex_);
  return buffers_.GetResource(id);
}

nb::scoped_refptr<Buffer> ResourceManager::DeregisterBuffer(uint32_t id) {
  starboard::ScopedLock lock(mutex_);
  return buffers_.DeregisterResource(id);
}

uint32_t ResourceManager::RegisterTexture(
    const nb::scoped_refptr<Texture>& texture) {
  starboard::ScopedLock lock(mutex_);
  return textures_.RegisterResource(texture);
}

nb::scoped_refptr<Texture> ResourceManager::GetTexture(uint32_t id) {
  starboard::ScopedLock lock(mutex_);
  return textures_.GetResource(id);
}

nb::scoped_refptr<Texture> ResourceManager::DeregisterTexture(uint32_t id) {
  starboard::ScopedLock lock(mutex_);
  return textures_.DeregisterResource(id);
}

uint32_t ResourceManager::RegisterFramebuffer(
    const nb::scoped_refptr<Framebuffer>& framebuffer) {
  starboard::ScopedLock lock(mutex_);
  return framebuffers_.RegisterResource(framebuffer);
}

nb::scoped_refptr<Framebuffer> ResourceManager::GetFramebuffer(uint32_t id) {
  starboard::ScopedLock lock(mutex_);
  return framebuffers_.GetResource(id);
}

nb::scoped_refptr<Framebuffer> ResourceManager::DeregisterFramebuffer(
    uint32_t id) {
  starboard::ScopedLock lock(mutex_);
  return framebuffers_.DeregisterResource(id);
}

uint32_t ResourceManager::RegisterRenderbuffer(
    const nb::scoped_refptr<Renderbuffer>& renderbuffer) {
  starboard::ScopedLock lock(mutex_);
  return renderbuffers_.RegisterResource(renderbuffer);
}

nb::scoped_refptr<Renderbuffer> ResourceManager::GetRenderbuffer(uint32_t id) {
  starboard::ScopedLock lock(mutex_);
  return renderbuffers_.GetResource(id);
}

nb::scoped_refptr<Renderbuffer> ResourceManager::DeregisterRenderbuffer(
    uint32_t id) {
  starboard::ScopedLock lock(mutex_);
  return renderbuffers_.DeregisterResource(id);
}

}  // namespace gles
}  // namespace glimp
