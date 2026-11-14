/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/session_description.h"

#include <jni.h>

#include <memory>
#include <optional>
#include <string>

#include "api/jsep.h"
#include "rtc_base/logging.h"
#include "sdk/android/generated_peerconnection_jni/SessionDescription_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"

namespace webrtc {
namespace jni {
namespace {
// Maps enum names from SessionDescription.java to SdpType.
SdpType SdpTypeFromJavaEnumName(absl::string_view name) {
  if (name == "offer")
    return SdpType::kOffer;
  if (name == "pranswer")
    return SdpType::kPrAnswer;
  if (name == "answer")
    return SdpType::kAnswer;
  if (name == "rollback")
    return SdpType::kRollback;
  RTC_CHECK(false);
}
}  // namespace

std::unique_ptr<SessionDescriptionInterface> JavaToNativeSessionDescription(
    JNIEnv* jni,
    const JavaRef<jobject>& j_sdp) {
  std::string std_type = JavaToStdString(
      jni, Java_SessionDescription_getTypeInCanonicalForm(jni, j_sdp));
  std::string std_description =
      JavaToStdString(jni, Java_SessionDescription_getDescription(jni, j_sdp));
  return CreateSessionDescription(SdpTypeFromJavaEnumName(std_type),
                                  std_description);
}

ScopedJavaLocalRef<jobject> NativeToJavaSessionDescription(
    JNIEnv* jni,
    const std::string& sdp,
    SdpType type) {
  return Java_SessionDescription_Constructor(
      jni,
      Java_Type_fromCanonicalForm(
          jni, NativeToJavaString(jni, SdpTypeToString(type))),
      NativeToJavaString(jni, sdp));
}

}  // namespace jni
}  // namespace webrtc
