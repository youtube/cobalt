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

#include "cobalt/dom/camera_3d_impl.h"
#include "cobalt/input/input_poller.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// 3D camera is used for setting the key mapping.
class Camera3D : public script::Wrappable {
 public:
  enum CameraAxes {
    // Restricted to [0deg, 360deg]
    kDomCameraRoll = Camera3DImpl::kCameraRoll,
    // Restricted to [-90deg, 90deg]
    kDomCameraPitch = Camera3DImpl::kCameraPitch,
    // Restricted to [0deg, 360deg]
    kDomCameraYaw = Camera3DImpl::kCameraYaw,
  };

  explicit Camera3D(const scoped_refptr<input::InputPoller>& input_poller);

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

  // Resets the camera's orientation.
  void Reset();

  // Custom, not in any spec.
  scoped_refptr<Camera3DImpl> impl() { return impl_; }

  DEFINE_WRAPPABLE_TYPE(Camera3D);

 private:
  // We delegate all calls to an Impl version of Camera3D so that all camera
  // state is stored within an object that is *not* a script::Wrappable.  This
  // is important because Camera3DImpl will typically be attached to a render
  // tree, and render trees passed to the rasterizer have the potential to
  // outlive the WebModule that created them, and the Camera3DImpl class is
  // designed for just this.
  scoped_refptr<Camera3DImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(Camera3D);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_CAMERA_3D_H_
