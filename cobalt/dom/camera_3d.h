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

#include "base/optional.h"
#include "base/synchronization/lock.h"
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

  // Custom, not in any spec.

  // Returns the camera's view-perspective matrix, setup according to the passed
  // in width/height aspect ratio.  It is likely that this function will be
  // called from another thread, like a renderer thread.
  glm::mat4 QueryViewPerspectiveMatrix(float width_to_height_aspect_ratio);

  DEFINE_WRAPPABLE_TYPE(Camera3D);

 private:
  typedef std::map<int, KeycodeMappingInfo> KeycodeMap;

  // Structure to keep track of the current orientation state.
  struct Orientation {
    Orientation() : roll(0.0f), pitch(0.0f), yaw(0.0f) {}

    float roll;
    float pitch;
    float yaw;
  };

  void AccumulateOrientation();

  // The Camera3D's WebAPI will be accessed from the WebModule thread, but
  // the internal camera orientation will be accessed on the renderer thread
  // via QueryViewPerspectiveMatrix().  So, we do need a mutex to guard against
  // these potentially parallel accesses.
  base::Lock mutex_;

  // A map of keys bound to camera movements.
  KeycodeMap keycode_map_;

  // The input poller from which we can check the state of a given key.
  input::InputPoller* input_poller_;

  // The current accumulated camera orientation state.
  Orientation orientation_;

  // The time that the last update to the camera's state has occurred.
  base::optional<base::TimeTicks> last_update_;

  DISALLOW_COPY_AND_ASSIGN(Camera3D);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_CAMERA_3D_H_
