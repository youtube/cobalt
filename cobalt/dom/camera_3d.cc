/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/camera_3d.h"

namespace cobalt {
namespace dom {

Camera3D::Camera3D(const scoped_refptr<input::InputPoller>& input_poller)
    : impl_(new Camera3DImpl(input_poller)) {}

void Camera3D::CreateKeyMapping(int keycode, uint32 camera_axis,
                                float degrees_per_second) {
  impl_->CreateKeyMapping(keycode, camera_axis, degrees_per_second);
}

void Camera3D::ClearKeyMapping(int keycode) { impl_->ClearKeyMapping(keycode); }

void Camera3D::ClearAllKeyMappings() { impl_->ClearAllKeyMappings(); }

void Camera3D::Reset() { impl_->Reset(); }

}  // namespace dom
}  // namespace cobalt
