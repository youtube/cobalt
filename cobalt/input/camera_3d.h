// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "base/memory/ref_counted.h"
#include "cobalt/base/camera_transform.h"
#include "cobalt/input/input_poller.h"
#include "starboard/window.h"

namespace cobalt {
namespace input {

// Obtains camera parameters from hardware input to render a 3D scene.
// Interface to the Web module and Renderer module. Some settings may not apply
// depending on the specfic hardware implementation, so their setter methods
// have empty default implementations.
class Camera3D : public base::RefCountedThreadSafe<Camera3D> {
 public:
  enum CameraAxes {
    // Restricted to [0deg, 360deg]
    kCameraRoll = 0x00,
    // Restricted to [-90deg, 90deg]
    kCameraPitch = 0x01,
    // Restricted to [0deg, 360deg]
    kCameraYaw = 0x02,
  };

  // Functions to map input keys to camera controls.
  virtual void CreateKeyMapping(int keycode, uint32 camera_axis,
                                float degrees_per_second) {
    UNREFERENCED_PARAMETER(keycode);
    UNREFERENCED_PARAMETER(camera_axis);
    UNREFERENCED_PARAMETER(degrees_per_second);
  }
  virtual void ClearKeyMapping(int keycode) { UNREFERENCED_PARAMETER(keycode); }
  virtual void ClearAllKeyMappings() {}

  // Return the camera's current view direction.
  virtual base::CameraOrientation GetOrientation() const = 0;

  // The above methods are meant as an interface for the Camera3D's WebAPI on
  // the WebModule thread, but the methods below will be accessed on the
  // renderer thread. So, all the methods on this class may be synchronized to
  // guard against these potentially parallel accesses. It is expected that the
  // renderer will call them at a higher frequency than the WebModule.

  // Updates the camera's perspective parameters.
  virtual void UpdatePerspective(float width_to_height_aspect_ratio,
                                 float vertical_fov) {
    UNREFERENCED_PARAMETER(width_to_height_aspect_ratio);
    UNREFERENCED_PARAMETER(vertical_fov);
  }

  // Returns the camera's view and projection matrices, setup according to the
  // latest available orientation and perspective information. This queries the
  // orientation from hardware inputs, and leaves it in the state of the class
  // to be queried by GetOrientation().
  virtual base::CameraTransform GetCameraTransformAndUpdateOrientation() = 0;

  // Resets camera to default orientation.
  virtual void Reset() {}

  virtual ~Camera3D() {}
};

}  // namespace input
}  // namespace cobalt

#endif  // COBALT_INPUT_CAMERA_3D_H_
