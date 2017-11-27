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

#ifndef COBALT_INPUT_CAMERA_3D_INPUT_POLLER_H_
#define COBALT_INPUT_CAMERA_3D_INPUT_POLLER_H_

#include <map>
#include <utility>

#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "cobalt/input/camera_3d.h"
#include "cobalt/input/input_poller.h"
#include "third_party/glm/glm/mat4x4.hpp"

namespace cobalt {
namespace input {

// Implements a camera that gets its orientation information from hand-
// controlled inputs.
class Camera3DInputPoller : public Camera3D {
 public:
  explicit Camera3DInputPoller(
      const scoped_refptr<input::InputPoller>& input_poller);

  void CreateKeyMapping(int keycode, uint32 camera_axis,
                        float degrees_per_second) override;
  void ClearKeyMapping(int keycode) override;
  void ClearAllKeyMappings() override;
  glm::quat GetOrientation() const override;

  // Returns the camera transforms based on hand-controlled inputs mapped
  // by the functions above
  base::CameraTransform GetCameraTransformAndUpdateOrientation() override;

  void UpdatePerspective(float width_to_height_aspect_ratio,
                         float vertical_fov) override;

  void Reset() override;

 private:
  struct KeycodeMappingInfo {
    KeycodeMappingInfo() : axis(0), degrees_per_second(0.0f) {}
    KeycodeMappingInfo(uint32 axis, float degrees_per_second)
        : axis(axis), degrees_per_second(degrees_per_second) {}

    uint32 axis;
    float degrees_per_second;
  };

  typedef std::map<int, KeycodeMappingInfo> KeycodeMap;

  void AccumulateOrientation();
  void ClampAngles();

  glm::quat orientation() const;

  mutable base::Lock mutex_;

  // The current accumulated camera orientation state.
  float roll_in_radians_;
  float pitch_in_radians_;
  float yaw_in_radians_;

  // The time that the last update to the camera's state has occurred.
  base::optional<base::TimeTicks> last_update_;

  // A map of keys bound to camera movements.
  KeycodeMap keycode_map_;

  // The input poller from which we can check the state of a given key.
  scoped_refptr<input::InputPoller> input_poller_;

  float width_to_height_aspect_ratio_;
  float vertical_fov_;

  DISALLOW_COPY_AND_ASSIGN(Camera3DInputPoller);
};

}  // namespace input
}  // namespace cobalt

#endif  // COBALT_INPUT_CAMERA_3D_INPUT_POLLER_H_
