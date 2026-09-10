/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/native_api/codecs/wrapper.h"

#include <jni.h>

#include <memory>
#include <vector>

#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "sdk/android/src/jni/video_codec_info.h"
#include "sdk/android/src/jni/video_decoder_factory_wrapper.h"
#include "sdk/android/src/jni/video_encoder_factory_wrapper.h"
#include "sdk/android/src/jni/video_encoder_wrapper.h"
#include "third_party/jni_zero/jni_zero.h"

namespace webrtc {

SdpVideoFormat JavaToNativeVideoCodecInfo(JNIEnv* jni, jobject codec_info) {
  return jni::VideoCodecInfoToSdpVideoFormat(
      jni, jni_zero::JavaRef<jobject>::CreateLeaky(jni, codec_info));
}

std::unique_ptr<VideoDecoderFactory> JavaToNativeVideoDecoderFactory(
    JNIEnv* jni,
    jobject decoder_factory) {
  return std::make_unique<jni::VideoDecoderFactoryWrapper>(
      jni, jni_zero::JavaRef<jobject>::CreateLeaky(jni, decoder_factory));
}

std::unique_ptr<VideoEncoderFactory> JavaToNativeVideoEncoderFactory(
    JNIEnv* jni,
    jobject encoder_factory) {
  return std::make_unique<jni::VideoEncoderFactoryWrapper>(
      jni, jni_zero::JavaRef<jobject>::CreateLeaky(jni, encoder_factory));
}

std::vector<VideoEncoder::ResolutionBitrateLimits>
JavaToNativeResolutionBitrateLimits(JNIEnv* jni,
                                    const jobjectArray j_bitrate_limits_array) {
  return jni::JavaToNativeResolutionBitrateLimits(
      jni, jni_zero::JavaRef<jobjectArray>::CreateLeaky(
               jni, j_bitrate_limits_array));
}

}  // namespace webrtc
