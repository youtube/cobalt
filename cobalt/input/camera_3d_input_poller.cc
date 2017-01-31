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

#include "cobalt/input/camera_3d_input_poller.h"

#include <algorithm>
#include <cmath>

#include "cobalt/input/create_default_camera_3d.h"
#include "third_party/glm/glm/gtc/matrix_transform.hpp"
#include "third_party/glm/glm/gtc/quaternion.hpp"
#include "third_party/glm/glm/gtx/transform.hpp"

namespace cobalt {
namespace input {

Camera3DInputPoller::Camera3DInputPoller(
    const scoped_refptr<input::InputPoller>& input_poller)
    : input_poller_(input_poller),
      width_to_height_aspect_ratio_(16.0f / 9.0f),
      vertical_fov_(60.0f) {}

void Camera3DInputPoller::CreateKeyMapping(int keycode, uint32 camera_axis,
                                           float degrees_per_second) {
  base::AutoLock lock(mutex_);
  keycode_map_[keycode] = KeycodeMappingInfo(camera_axis, degrees_per_second);
}

void Camera3DInputPoller::ClearKeyMapping(int keycode) {
  base::AutoLock lock(mutex_);
  keycode_map_.erase(keycode);
}

void Camera3DInputPoller::ClearAllKeyMappings() {
  base::AutoLock lock(mutex_);
  keycode_map_.clear();
}

glm::quat Camera3DInputPoller::GetOrientation() const {
  base::AutoLock lock(mutex_);
  return orientation_;
}

base::CameraTransform
Camera3DInputPoller::GetCameraTransformAndUpdateOrientation() {
  base::AutoLock lock(mutex_);
  AccumulateOrientation();

  glm::mat4 view_matrix = glm::mat4_cast(orientation_);

  const float kNearZ = 0.01f;
  const float kFarZ = 1000.0f;
  glm::mat4 projection_matrix = glm::perspectiveRH(
      vertical_fov_, width_to_height_aspect_ratio_, kNearZ, kFarZ);

  return base::CameraTransform(projection_matrix, view_matrix);
}

void Camera3DInputPoller::UpdatePerspective(float width_to_height_aspect_ratio,
                                            float vertical_fov) {
  width_to_height_aspect_ratio_ = width_to_height_aspect_ratio;
  vertical_fov_ = vertical_fov;
}

void Camera3DInputPoller::Reset() {
  base::AutoLock lock(mutex_);
  orientation_ = glm::quat();
}

void Camera3DInputPoller::AccumulateOrientation() {
  if (!input_poller_) {
    // Nothing to do if no input poller was provided.
    return;
  }

  base::TimeTicks now = base::TimeTicks::Now();
  if (last_update_) {
    base::TimeDelta delta = now - *last_update_;
    // Cap out the maximum time delta that we will accumulate changes over, to
    // avoid a random extra long time delta that completely changes the camera
    // orientation.
    const base::TimeDelta kMaxTimeDelta = base::TimeDelta::FromMilliseconds(40);
    if (delta > kMaxTimeDelta) {
      delta = kMaxTimeDelta;
    }

    // Accumulate new rotation from all mapped inputs.
    float roll = 0.0f, pitch = 0.0f, yaw = 0.0f;

    for (KeycodeMap::const_iterator iter = keycode_map_.begin();
         iter != keycode_map_.end(); ++iter) {
      // If the key does not have analog output, the AnalogInput() method will
      // always return 0.0f, so check this first, and if it is indeed 0,
      // fallback to a check to see if the button is digital and pressed.
      float value = input_poller_->AnalogInput(static_cast<SbKey>(iter->first));
      if (value == 0.0f) {
        value = input_poller_->IsPressed(static_cast<SbKey>(iter->first))
                    ? 1.0f
                    : 0.0f;
      }

      // Get a pointer to the camera axis angle that this key is bound to.
      float* target_angle;
      switch (iter->second.axis) {
        case kCameraRoll:
          target_angle = &roll;
          break;
        case kCameraPitch:
          target_angle = &pitch;
          break;
        case kCameraYaw:
          target_angle = &yaw;
          break;
      }

      // Apply the angle adjustment from the key.
      *target_angle += value * iter->second.degrees_per_second *
                       static_cast<float>(delta.InSecondsF());
    }

    // New rotation applied from the right to keep it in global axes.
    // Note that we invert the rotation angles since this transform is applied
    // to the objects in our scene, and if the camera moves right, the objects,
    // relatively, would move right.
    orientation_ *=
        glm::angleAxis(-DegreesToRadians(roll), glm::vec3(0, 0, 1)) *
        glm::angleAxis(-DegreesToRadians(pitch), glm::vec3(1, 0, 0)) *
        glm::angleAxis(-DegreesToRadians(yaw), glm::vec3(0, 1, 0));
  }
  last_update_ = now;
}

scoped_refptr<Camera3D> CreatedDefaultCamera3D(
    SbWindow window, const scoped_refptr<InputPoller>& input_poller) {
  UNREFERENCED_PARAMETER(window);
  return new Camera3DInputPoller(input_poller);
}

}  // namespace input
}  // namespace cobalt
