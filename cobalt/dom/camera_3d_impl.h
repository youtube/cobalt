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

#ifndef COBALT_DOM_CAMERA_3D_IMPL_H_
#define COBALT_DOM_CAMERA_3D_IMPL_H_

#include <map>
#include <utility>

#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "cobalt/input/input_poller.h"
#include "third_party/glm/glm/mat4x4.hpp"

namespace cobalt {
namespace dom {

// 3D camera is used for setting the key mapping.
class Camera3DImpl : public base::RefCountedThreadSafe<Camera3DImpl> {
 public:
  enum CameraAxes {
    // Restricted to [0deg, 360deg]
    kCameraRoll = 0x00,
    // Restricted to [-90deg, 90deg]
    kCameraPitch = 0x01,
    // Restricted to [0deg, 360deg]
    kCameraYaw = 0x02,
  };

  explicit Camera3DImpl(const scoped_refptr<input::InputPoller>& input_poller);

  void CreateKeyMapping(int keycode, uint32 camera_axis,
                        float degrees_per_second);
  void ClearKeyMapping(int keycode);
  void ClearAllKeyMappings();

  void Reset();

  // Returns the camera's view-perspective matrix, setup according to the passed
  // in width/height aspect ratio.  It is likely that this function will be
  // called from another thread, like a renderer thread.
  glm::mat4 QueryViewPerspectiveMatrix(float width_to_height_aspect_ratio);

 private:
  struct KeycodeMappingInfo {
    KeycodeMappingInfo() : axis(0), degrees_per_second(0.0f) {}
    KeycodeMappingInfo(uint32 axis, float degrees_per_second)
        : axis(axis), degrees_per_second(degrees_per_second) {}

    uint32 axis;
    float degrees_per_second;
  };

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
  scoped_refptr<input::InputPoller> input_poller_;

  // The current accumulated camera orientation state.
  Orientation orientation_;

  // The time that the last update to the camera's state has occurred.
  base::optional<base::TimeTicks> last_update_;

  DISALLOW_COPY_AND_ASSIGN(Camera3DImpl);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_CAMERA_3D_IMPL_H_
