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

#include "base/android/jni_android.h"
#include "starboard/common/size.h"
#include "starboard/decode_target.h"
#include "starboard/shared/starboard/decode_target/decode_target_internal.h"

namespace starboard {

class DecodeTarget final : public SbDecodeTargetPrivate {
 public:
  explicit DecodeTarget(SbDecodeTargetGraphicsContextProvider* provider);

  bool GetInfo(SbDecodeTargetInfo* out_info) final;

  jobject surface_texture() const { return surface_texture_.obj(); }
  jobject surface() const { return surface_.obj(); }

  void set_dimension(const Size& size) {
    info_.planes[0].width = size.width;
    info_.planes[0].height = size.height;
    info_.width = size.width;
    info_.height = size.height;
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
  base::android::ScopedJavaGlobalRef<jobject> surface_texture_;
  base::android::ScopedJavaGlobalRef<jobject> surface_;
  ANativeWindow* native_window_;

  SbDecodeTargetInfo info_;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_DECODE_TARGET_H_
