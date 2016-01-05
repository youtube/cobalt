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

}  // namespace gles
}  // namespace glimp
