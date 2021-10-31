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

#include "cobalt/dom/camera_3d.h"

#include "third_party/glm/glm/gtx/euler_angles.hpp"

#include "base/basictypes.h"
#include "base/time/time.h"
#include "cobalt/dom/device_orientation_event.h"

namespace cobalt {
namespace dom {

Camera3D::Camera3D(const scoped_refptr<input::Camera3D>& impl)
    : impl_(impl),
      last_event_alpha_(720.0),
      last_event_beta_(720.0),
      last_event_gamma_(720.0) {}

void Camera3D::CreateKeyMapping(int keycode, uint32 camera_axis,
                                float degrees_per_second) {
  impl_->CreateKeyMapping(keycode, camera_axis, degrees_per_second);
}

void Camera3D::ClearKeyMapping(int keycode) { impl_->ClearKeyMapping(keycode); }

void Camera3D::ClearAllKeyMappings() { impl_->ClearAllKeyMappings(); }

void Camera3D::Reset() { impl_->Reset(); }

void Camera3D::StartOrientationEvents(
    const base::WeakPtr<EventTarget>& target) {
  if (!impl()) {
    return;
  }
  orientation_event_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSecondsD(1.0 / ORIENTATION_POLL_FREQUENCY_HZ),
      base::Bind(&Camera3D::FireOrientationEvent, base::Unretained(this),
                 target));
}

void Camera3D::StopOrientationEvents() { orientation_event_timer_.Stop(); }

namespace {

// These produces angles within the ranges: alpha in [-180,180], beta in
// [-90,90], gamma in [-180,180].
static void QuaternionToIntrinsicZXY(const glm::dquat& q, double* intrinsic_z,
                                     double* intrinsic_x, double* intrinsic_y) {
  *intrinsic_z = atan2(2 * (q.x * q.y + q.w * q.z),
                       q.w * q.w - q.x * q.x + q.y * q.y - q.z * q.z);
  *intrinsic_x = asin(-2 * (q.y * q.z - q.w * q.x));
  *intrinsic_y = atan2(2 * (q.x * q.z + q.w * q.y),
                       q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z);
}

}  // namespace

void Camera3D::FireOrientationEvent(base::WeakPtr<EventTarget> target) {
  glm::dquat quaternion = glm::normalize(
      glm::dquat(impl()->GetOrientation()) *
      // The API assumes a different initial orientation (straight down instead
      // of ahead), requiring a previous rotation of 90 degrees.
      glm::angleAxis(M_PI / 2, glm::dvec3(1.0f, 0.0f, 0.0f)));

  double intrinsic_z = 0.0f, intrinsic_x = 0.0f, intrinsic_y = 0.0f;
  QuaternionToIntrinsicZXY(quaternion, &intrinsic_z, &intrinsic_x,
                           &intrinsic_y);

  // Adjust to the angle ranges specified by the Device Orientation API.
  // They should be: alpha in [0,360], beta in [-180,180], gamma in [-90,90].
  double alpha = input::Camera3D::RadiansToDegrees(intrinsic_z);
  double beta = input::Camera3D::RadiansToDegrees(intrinsic_x);
  double gamma = -input::Camera3D::RadiansToDegrees(intrinsic_y);

  if (gamma > 90 || gamma < -90) {
    gamma = copysign(180, gamma) - gamma;
    // Represent excess gamma as rotations along other axes.
    beta = 180 - beta;
    alpha = 180 - alpha;
  }

  alpha = fmod(360 + alpha, 360);
  beta = beta >= 180 ? beta - 360 : beta;

  // Filter event based the time elapsed and the angle deltas since the last
  // event.
  base::TimeTicks now = base::TimeTicks::Now();
  if (!last_event_time_ ||
      (now - *last_event_time_).InSecondsF() >
          1.0 / ORIENTATION_EVENT_MIN_FREQUENCY_HZ ||
      fabs(alpha - last_event_alpha_) >
          ORIENTATION_EVENT_DELTA_THRESHOLD_DEGREES ||
      fabs(beta - last_event_beta_) >
          ORIENTATION_EVENT_DELTA_THRESHOLD_DEGREES ||
      fabs(gamma - last_event_gamma_) >
          ORIENTATION_EVENT_DELTA_THRESHOLD_DEGREES) {
    DeviceOrientationEventInit init;
    init.set_absolute(false);
    init.set_alpha(alpha);
    init.set_beta(beta);
    init.set_gamma(gamma);

    target->DispatchEvent(
        new DeviceOrientationEvent("deviceorientation", init));
    last_event_time_ = now;
    last_event_alpha_ = alpha;
    last_event_beta_ = beta;
    last_event_gamma_ = gamma;
  }
}

}  // namespace dom
}  // namespace cobalt
