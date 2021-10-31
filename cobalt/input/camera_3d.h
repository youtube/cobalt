// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_INPUT_CAMERA_3D_H_
#define COBALT_INPUT_CAMERA_3D_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/camera_transform.h"
#include "cobalt/input/input_poller.h"
#include "starboard/window.h"
#include "third_party/glm/glm/gtc/quaternion.hpp"

namespace cobalt {
namespace input {

// Obtains camera parameters from hardware input to render a 3D scene.
// Interface to the Web module and Renderer module. Some settings may not apply
// depending on the specific hardware implementation, so their setter methods
// have empty default implementations.
class Camera3D : public base::RefCountedThreadSafe<Camera3D> {
 public:
  // The rotations around the axes are intrinsic, meaning they are applied in
  // sequence, each on the rotated frame produced by the previous one.
  enum CameraAxes {
    // Applied around the Z axis. Restricted to [0deg, 360deg).
    kCameraRoll = 0x00,
    // Applied around the X axis. Restricted to [-90deg, 90deg)
    kCameraPitch = 0x01,
    // Applied around the Y axis. Restricted to [0deg, 360deg)
    kCameraYaw = 0x02,
  };

  // Functions to map input keys to camera controls.
  virtual void CreateKeyMapping(int keycode, uint32 camera_axis,
                                float degrees_per_second) {}
  virtual void ClearKeyMapping(int keycode) {}
  virtual void ClearAllKeyMappings() {}

  // Return the camera's current view direction.
  virtual glm::quat GetOrientation() const = 0;

  // The above methods are meant as an interface for the Camera3D's WebAPI on
  // the WebModule thread, but the methods below will be accessed on the
  // renderer thread. So, all the methods on this class may be synchronized to
  // guard against these potentially parallel accesses. It is expected that the
  // renderer will call them at a higher frequency than the WebModule.

  // Updates the camera's perspective parameters.
  virtual void UpdatePerspective(float width_to_height_aspect_ratio,
                                 float vertical_fov) {}

  // Returns the camera's view and projection matrices, setup according to the
  // latest available orientation and perspective information. This queries the
  // orientation from hardware inputs, and leaves it in the state of the class
  // to be queried by GetOrientation().
  virtual base::CameraTransform GetCameraTransformAndUpdateOrientation() = 0;

  // Resets camera to default orientation.
  virtual void Reset() {}

  // Adopt input object from the given Camera3D.
  virtual void SetInput(const scoped_refptr<Camera3D>& other) {}

  virtual ~Camera3D() {}

  template <typename FloatType>
  static FloatType DegreesToRadians(FloatType degrees) {
    return (degrees / 360) * 2 * static_cast<FloatType>(M_PI);
  }

  template <typename FloatType>
  static FloatType RadiansToDegrees(FloatType radians) {
    return radians * 180 / static_cast<FloatType>(M_PI);
  }
};

}  // namespace input
}  // namespace cobalt

#endif  // COBALT_INPUT_CAMERA_3D_H_
