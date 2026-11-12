/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/timestamp_aligner.h"

#include <jni.h>

#include <cstdint>

#include "api/environment/environment.h"
#include "sdk/android/generated_video_jni/TimestampAligner_jni.h"
#include "sdk/android/native_api/jni/java_types.h"

namespace webrtc {
namespace jni {
namespace {

class TimestampAlignerWithClock {
 public:
  explicit TimestampAlignerWithClock(const Environment& env) : env_(env) {}

  int64_t TranslateTimestampNs(int64_t camera_time_ns) {
    return aligner_.TranslateTimestamp(
               /*capture_time_us=*/camera_time_ns / 1'000,
               /*system_time_us=*/env_.clock().TimeInMicroseconds()) *
           1'000;
  }

 private:
  const Environment env_;
  TimestampAligner aligner_;
};

}  // namespace

static jlong JNI_TimestampAligner_CreateTimestampAligner(JNIEnv* env,
                                                         jlong webrtcEnvRef) {
  return NativeToJavaPointer(new TimestampAlignerWithClock(
      *reinterpret_cast<const Environment*>(webrtcEnvRef)));
}

static void JNI_TimestampAligner_ReleaseTimestampAligner(
    JNIEnv* env,
    jlong timestamp_aligner) {
  delete reinterpret_cast<TimestampAlignerWithClock*>(timestamp_aligner);
}

static jlong JNI_TimestampAligner_TranslateTimestamp(JNIEnv* env,
                                                     jlong timestamp_aligner,
                                                     jlong camera_time_ns) {
  return reinterpret_cast<TimestampAlignerWithClock*>(timestamp_aligner)
      ->TranslateTimestampNs(camera_time_ns);
}

}  // namespace jni
}  // namespace webrtc
