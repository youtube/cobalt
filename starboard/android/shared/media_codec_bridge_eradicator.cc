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

#include "starboard/android/shared/media_codec_bridge_eradicator.h"

#include <pthread.h>

#include "starboard/android/shared/jni_state.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/common/log.h"
#include "starboard/common/once.h"

namespace starboard {
namespace android {
namespace shared {

SB_ONCE_INITIALIZE_FUNCTION(MediaCodecBridgeEradicator,
                            MediaCodecBridgeEradicator::GetInstance);

namespace {

struct EradicateParam {
  EradicateParam(MediaCodecBridgeEradicator* eradicator,
                 jobject j_media_codec_bridge)
      : eradicator(eradicator), j_media_codec_bridge(j_media_codec_bridge) {}

  MediaCodecBridgeEradicator* eradicator;
  jobject j_media_codec_bridge;
};

}  // namespace

bool MediaCodecBridgeEradicator::WaitForPendingDestructions() {
  ScopedLock scoped_lock(mutex_);
  while (!j_media_codec_bridge_set_.empty()) {
    bool signaled =
        condition_variable_.WaitTimed(GetTimeoutSeconds() * 1000 * 1000);
    if (!signaled) {
      // condition_variable_ timed out
      SB_LOG(WARNING)
          << "The child thread that runs "
             "MediaCodecBridgeEradicator::DestroyMediaCodecBridge has not "
             "terminated after ERADICATOR_THREAD_TIMEOUT_MICROSECOND, "
             "potential thread leakage happened.";

      // remove all values in j_media_codec_bridge_set_ to unblock
      // creating new MediaCodecBridge instances.
      j_media_codec_bridge_set_.clear();
      return false;
    }
  }
  return true;
}

bool MediaCodecBridgeEradicator::Destroy(jobject j_media_codec_bridge) {
  // Since the native media_codec_bridge object is about to be destroyed, remove
  // its reference from the Java MediaCodecBridge object to prevent further
  // access. Otherwise it could lead to an application crash.
  // The de-reference has to happen before the java MediaCodecBridge object is
  // destroyed.
  JniEnvExt* env = JniEnvExt::Get();
  env->CallVoidMethodOrAbort(j_media_codec_bridge,
                             "resetNativeMediaCodecBridge", "()V");

  {
    // add the j_media_codec_bridge to the set before the work is started
    ScopedLock scoped_lock(mutex_);
    j_media_codec_bridge_set_.insert(j_media_codec_bridge);
  }

  EradicateParam* param = new EradicateParam(this, j_media_codec_bridge);
  pthread_t thread_id;
  int result = pthread_create(
      &thread_id, nullptr, &MediaCodecBridgeEradicator::DestroyMediaCodecBridge,
      param);

  if (result == 0) {
    pthread_detach(thread_id);
  } else {
    // If it fails to create the thread
    delete param;

    ScopedLock scoped_lock(mutex_);
    j_media_codec_bridge_set_.erase(j_media_codec_bridge);
    if (j_media_codec_bridge_set_.empty()) {
      condition_variable_.Signal();
    }
  }

  return result == 0;
}

void* MediaCodecBridgeEradicator::DestroyMediaCodecBridge(void* context) {
  EradicateParam* param = static_cast<EradicateParam*>(context);
  SB_DCHECK(param != nullptr);

  MediaCodecBridgeEradicator* eradicator = param->eradicator;
  jobject j_media_codec_bridge = param->j_media_codec_bridge;

  delete param;
  param = NULL;

  JniEnvExt* env = JniEnvExt::Get();

  // TODO(cobalt): wrap fields in EradicateParam to JNI smartpointers, and
  // migrate them to jni_zero.
  env->CallVoidMethodOrAbort(j_media_codec_bridge, "stop", "()V");
  env->CallVoidMethodOrAbort(j_media_codec_bridge, "release", "()V");
  env->DeleteGlobalRef(j_media_codec_bridge);

  {
    // the work is finished, delete the j_media_codec_bridge from the set
    ScopedLock scoped_lock(eradicator->mutex_);
    eradicator->j_media_codec_bridge_set_.erase(j_media_codec_bridge);

    if (eradicator->j_media_codec_bridge_set_.empty()) {
      eradicator->condition_variable_.Signal();
    }
  }

  JNIState::GetVM()->DetachCurrentThread();
  return NULL;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
