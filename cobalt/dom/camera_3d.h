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

#ifndef COBALT_DOM_CAMERA_3D_H_
#define COBALT_DOM_CAMERA_3D_H_

#include <map>
#include <utility>

#include "cobalt/input/input_poller.h"
#include "cobalt/script/wrappable.h"
#include "third_party/glm/glm/mat4x4.hpp"

namespace cobalt {
namespace dom {

// 3D camera is used for setting the key mapping.
class Camera3D : public script::Wrappable {
 public:
  enum CameraAxes {
    // Restricted to [0deg, 360deg]
    kDomCameraRoll = 0x00,
    // Restricted to [-90deg, 90deg]
    kDomCameraPitch = 0x01,
    // Restricted to [0deg, 360deg]
    kDomCameraYaw = 0x02,
  };

  struct KeycodeMappingInfo {
    KeycodeMappingInfo() : axis(0), degrees_per_second(0.0f) {}
    KeycodeMappingInfo(uint32 axis, float degrees_per_second)
        : axis(axis), degrees_per_second(degrees_per_second) {}

    uint32 axis;
    float degrees_per_second;
  };

  explicit Camera3D(input::InputPoller* input_poller);

  // Creates a mapping between the specified keyCode and the specified camera
  // axis, such that while the key is pressed, the cameraAxis will rotate at a
  // constant rate of degrees_per_second.
  void CreateKeyMapping(int keycode, uint32 camera_axis,
                        float degrees_per_second);
  // Clears any key mapping associated with the specified keyCode, if they
  // exist.
  void ClearKeyMapping(int keycode);
  // Clears all key mappings created by previous calls to |CreateKeyMapping|.
  void ClearAllKeyMappings();

  glm::mat4 CalculateViewMatrix(float seconds);

  DEFINE_WRAPPABLE_TYPE(Camera3D);

 private:
  typedef std::map<int, KeycodeMappingInfo> KeycodeMap;
  KeycodeMap keycode_map_;
  input::InputPoller* input_poller_;

  DISALLOW_COPY_AND_ASSIGN(Camera3D);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_CAMERA_3D_H_
