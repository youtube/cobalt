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

#include "starboard/android/shared/jni_utils.h"
#include "starboard/common/log.h"
#include "starboard/once.h"

namespace starboard {
namespace android {
namespace shared {

SB_ONCE_INITIALIZE_FUNCTION(MediaCodecBridgeEradicator,
                            MediaCodecBridgeEradicator::GetInstance);

namespace {

struct EradicateParam {
  explicit EradicateParam(MediaCodecBridgeEradicator* eradicator, 
      jobject j_media_codec_bridge, 
      jobject j_reused_get_output_format_result)
      : eradicator(eradicator), 
        j_media_codec_bridge(j_media_codec_bridge), 
        j_reused_get_output_format_result(j_reused_get_output_format_result) {}
  MediaCodecBridgeEradicator* eradicator;
  jobject j_media_codec_bridge;
  jobject j_reused_get_output_format_result;
};

}  // namespace

bool MediaCodecBridgeEradicator::IsEradicating() {
  ScopedLock scoped_lock(mutex_);
  SB_LOG(INFO) << "j_media_codec_bridge_set_ count=" << j_media_codec_bridge_set_.size();
  return !j_media_codec_bridge_set_.empty();
}

void MediaCodecBridgeEradicator::CheckEradicatingAndMaybeWait() {
  SB_LOG(INFO) << "MediaCodecBridgeEradicator CheckEradicatingAndMaybeWait.";
  while (IsEradicating()) {
    SB_LOG(INFO) << "MediaCodecBridgeEradicator CheckEradicatingAndMaybeWait and wait.";
    condition_variable_.Wait();
  }
}

void MediaCodecBridgeEradicator::Eradicate(jobject j_media_codec_bridge, jobject j_reused_get_output_format_result) {
  SB_LOG(INFO) << "Start to eradicate j_media_codec_bridge. j_media_codec_bridge:" << j_media_codec_bridge;

  // std::unique_ptr<EradicateParam> param = std::make_unique<EradicateParam>(this, j_media_codec_bridge, j_reused_get_output_format_result);
  // std::unique_ptr<EradicateParam> movedParam = std::move(param); // OK: transfer ownership
  EradicateParam* param = new EradicateParam(this, j_media_codec_bridge, j_reused_get_output_format_result);

  SB_LOG(INFO) << "EradicateParam.j_media_codec_bridge:" << param->j_media_codec_bridge;

  SbThread eradicate_thread = SbThreadCreate(0,            // default stack_size.
                          kSbThreadPriorityHigh,           // high priority.
                          kSbThreadNoAffinity,             // default affinity.
                          true,                            // joinable.
                          "MediaCodecBridgeEradicator",
                          &MediaCodecBridgeEradicator::EradicateMediaCodecBridge, param);
}

// static
void* MediaCodecBridgeEradicator::EradicateMediaCodecBridge(void* context) {
  SB_LOG(INFO) << "EradicateMediaCodecBridge starts to work.";
  EradicateParam* param = static_cast<EradicateParam*>(context);
  SB_DCHECK(param != nullptr);

  MediaCodecBridgeEradicator* eradicator = param->eradicator;
  jobject j_media_codec_bridge = param->j_media_codec_bridge;
  SB_LOG(INFO) << "j_media_codec_bridge is " << j_media_codec_bridge;
  jobject j_reused_get_output_format_result = param->j_reused_get_output_format_result;

  {
    // add the j_media_codec_bridge to the set
    ScopedLock scoped_lock(eradicator->mutex_);
    eradicator->j_media_codec_bridge_set_.insert(j_media_codec_bridge);
    SB_LOG(INFO) << "inserted " << j_media_codec_bridge << ", j_media_codec_bridge_set_ count=" << eradicator->j_media_codec_bridge_set_.size();
  }

  JniEnvExt* env = JniEnvExt::Get();

  env->CallVoidMethodOrAbort(j_media_codec_bridge, "stop", "()V");
  env->CallVoidMethodOrAbort(j_media_codec_bridge, "release", "()V");
  env->DeleteGlobalRef(j_media_codec_bridge);
  // j_media_codec_bridge = NULL;

  SB_DCHECK(j_reused_get_output_format_result);
  env->DeleteGlobalRef(j_reused_get_output_format_result);
  // j_reused_get_output_format_result = NULL;

  {
    // delete the j_media_codec_bridge from the set
    ScopedLock scoped_lock(eradicator->mutex_);
    eradicator->j_media_codec_bridge_set_.erase(j_media_codec_bridge);
    SB_LOG(INFO) << "removed " << j_media_codec_bridge << ", j_media_codec_bridge_set_ count=" << eradicator->j_media_codec_bridge_set_.size();

    if (eradicator->j_media_codec_bridge_set_.empty()) {
      eradicator->condition_variable_.Signal();
      SB_LOG(INFO) << "MediaCodecBridgeEradicator signal on condition_variable.";
    } 
  }

  delete param;

  SB_LOG(INFO) << "EradicateMediaCodecBridge ends.";
  return NULL;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard