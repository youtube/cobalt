/*
 * Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/input/create_default_camera_3d.h"
#include "third_party/glm/glm/gtc/matrix_transform.hpp"
#include "third_party/glm/glm/gtc/quaternion.hpp"
#include "third_party/glm/glm/gtx/transform.hpp"

namespace cobalt {
namespace input {

Camera3DInputPoller::Camera3DInputPoller(
    const scoped_refptr<input::InputPoller>& input_poller)
    : roll_in_radians_(0.0f),
      pitch_in_radians_(0.0f),
      yaw_in_radians_(0.0f),
      input_poller_(input_poller),
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

glm::quat Camera3DInputPoller::orientation() const {
  return glm::angleAxis(-roll_in_radians_, glm::vec3(0, 0, 1)) *
         glm::angleAxis(-pitch_in_radians_, glm::vec3(1, 0, 0)) *
         glm::angleAxis(-yaw_in_radians_, glm::vec3(0, 1, 0));
}

glm::quat Camera3DInputPoller::GetOrientation() const {
  base::AutoLock lock(mutex_);
  return orientation();
}

base::CameraTransform
Camera3DInputPoller::GetCameraTransformAndUpdateOrientation() {
  base::AutoLock lock(mutex_);
  AccumulateOrientation();

  glm::mat4 view_matrix = glm::mat4_cast(orientation());

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
  roll_in_radians_ = 0.0f;
  pitch_in_radians_ = 0.0f;
  yaw_in_radians_ = 0.0f;
}

void Camera3DInputPoller::SetInput(const scoped_refptr<Camera3D>& other) {
  base::AutoLock lock(mutex_);
  Camera3DInputPoller* other_camera =
      base::polymorphic_downcast<Camera3DInputPoller*>(other.get());
  input_poller_ = other_camera ? other_camera->input_poller_ : nullptr;
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
          target_angle = &roll_in_radians_;
          break;
        case kCameraPitch:
          target_angle = &pitch_in_radians_;
          break;
        case kCameraYaw:
          target_angle = &yaw_in_radians_;
          break;
      }

      // Apply the angle adjustment from the key.
      *target_angle += value *
                       DegreesToRadians(iter->second.degrees_per_second) *
                       static_cast<float>(delta.InSecondsF());
    }

    // Clamp the angles to ensure that they remain in the valid range.
    ClampAngles();
  }
  last_update_ = now;
}

void Camera3DInputPoller::ClampAngles() {
  float kTwoPi = static_cast<float>(M_PI * 2);
  float kPiOverTwo = static_cast<float>(M_PI / 2);

  pitch_in_radians_ =
      std::max(-kPiOverTwo, std::min(kPiOverTwo, pitch_in_radians_));

  roll_in_radians_ = fmod(roll_in_radians_, kTwoPi);
  if (roll_in_radians_ < 0) {
    roll_in_radians_ += kTwoPi;
  }

  yaw_in_radians_ = fmod(yaw_in_radians_, kTwoPi);
  if (yaw_in_radians_ < 0) {
    yaw_in_radians_ += kTwoPi;
  }
}

scoped_refptr<Camera3D> CreatedDefaultCamera3D(
    SbWindow window, const scoped_refptr<InputPoller>& input_poller) {
  return new Camera3DInputPoller(input_poller);
}

}  // namespace input
}  // namespace cobalt
