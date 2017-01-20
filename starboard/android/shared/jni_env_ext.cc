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

#include "starboard/android/shared/jni_env_ext.h"

#include <android/native_activity.h>
#include <jni.h>

#include "starboard/thread.h"

namespace {

SbThreadLocalKey tls_key_ = kSbThreadLocalKeyInvalid;
ANativeActivity* native_activity_;

void Destroy(void* value) {
  if (value) {
    native_activity_->vm->DetachCurrentThread();
  }
}

}  // namespace

namespace starboard {
namespace android {
namespace shared {

void JniEnvExt::Initialize(ANativeActivity* native_activity) {
  tls_key_ = SbThreadCreateLocalKey(Destroy);
  native_activity_ = native_activity;
}

JniEnvExt* JniEnvExt::Get() {
  JNIEnv* env;
  // Always attach in case someone detached. This is a no-op if still attached.
  native_activity_->vm->AttachCurrentThread(&env, NULL);
  // We don't actually use the value, but any non-NULL means we have to detach.
  SbThreadSetLocalValue(tls_key_, env);
  // The downcast is safe since we only add methods, not fields.
  return static_cast<JniEnvExt*>(env);
}

jobject JniEnvExt::GetActivityObject() {
  return native_activity_->clazz;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
