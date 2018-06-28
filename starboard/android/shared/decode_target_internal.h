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

#ifndef STARBOARD_ANDROID_SHARED_DECODE_TARGET_INTERNAL_H_
#define STARBOARD_ANDROID_SHARED_DECODE_TARGET_INTERNAL_H_

#include <android/native_window.h>
#include <GLES2/gl2.h>
#include <jni.h>

#include "starboard/common/ref_counted.h"
#include "starboard/decode_target.h"

struct SbDecodeTargetPrivate {
  class Data : public starboard::RefCounted<Data> {
   public:
    Data() {}

    // Java objects which wrap the texture.  We hold on to global references
    // to these objects.
    jobject surface_texture;
    jobject surface;
    ANativeWindow* native_window;

    // Publicly accessible information about the decode target.
    SbDecodeTargetInfo info;

   private:
    friend class starboard::RefCounted<Data>;
    ~Data();
  };

  starboard::scoped_refptr<Data> data;
};

#endif  // STARBOARD_ANDROID_SHARED_DECODE_TARGET_INTERNAL_H_
