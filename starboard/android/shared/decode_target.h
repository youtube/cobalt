// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_DECODE_TARGET_H_
#define STARBOARD_ANDROID_SHARED_DECODE_TARGET_H_

#include <GLES2/gl2.h>
#include <android/native_window.h>
#include <jni.h>

#include "starboard/decode_target.h"
#include "starboard/shared/starboard/decode_target/decode_target_internal.h"

namespace starboard {
namespace android {
namespace shared {

class DecodeTarget final : public SbDecodeTargetPrivate {
 public:
  explicit DecodeTarget(SbDecodeTargetGraphicsContextProvider* provider);

  bool GetInfo(SbDecodeTargetInfo* out_info) final;

  jobject surface_texture() const { return surface_texture_; }
  jobject surface() const { return surface_; }

  void set_dimension(int width, int height) {
    info_.planes[0].width = width;
    info_.planes[0].height = height;
    info_.width = width;
    info_.height = height;
  }

  void set_content_region(
      const SbDecodeTargetInfoContentRegion& content_region) {
    info_.planes[0].content_region = content_region;
  }

 private:
  ~DecodeTarget() final;

  void CreateOnContextRunner();

  // Java objects which wrap the texture.  We hold on to global references
  // to these objects.
  jobject surface_texture_;
  jobject surface_;
  ANativeWindow* native_window_;

  SbDecodeTargetInfo info_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_DECODE_TARGET_H_
