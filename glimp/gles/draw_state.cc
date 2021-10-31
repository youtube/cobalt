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

#include "glimp/gles/draw_state.h"

namespace glimp {
namespace gles {

DirtyUniforms::DirtyUniforms() : all_dirty_(true) {}

bool DirtyUniforms::IsDirty(int location) const {
  return all_dirty_ ||
         std::find(uniforms_dirty_.begin(), uniforms_dirty_.end(), location) !=
             uniforms_dirty_.end();
}

bool DirtyUniforms::AnyDirty() const {
  return all_dirty_ || !uniforms_dirty_.empty();
}

void DirtyUniforms::ClearAll() {
  all_dirty_ = false;
  uniforms_dirty_.clear();
}

void DirtyUniforms::MarkAll() {
  all_dirty_ = true;
}

void DirtyUniforms::Mark(int location) {
  if (!IsDirty(location)) {
    uniforms_dirty_.push_back(location);
  }
}

}  // namespace gles
}  // namespace glimp
