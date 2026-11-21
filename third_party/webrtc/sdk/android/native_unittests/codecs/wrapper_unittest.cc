/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
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

#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/make_ref_counted.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"
#include "media/base/media_constants.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "sdk/android/generated_native_unittests_jni/CodecsWrapperTestHelper_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/native_api/jni/jvm.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"
#include "sdk/android/src/jni/video_encoder_wrapper.h"
#include "test/create_test_environment.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {
TEST(JavaCodecsWrapperTest, JavaToNativeVideoCodecInfo) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedJavaLocalRef<jobject> j_video_codec_info =
      jni::Java_CodecsWrapperTestHelper_createTestVideoCodecInfo(env);

  const SdpVideoFormat video_format =
      JavaToNativeVideoCodecInfo(env, j_video_codec_info.obj());

  EXPECT_EQ(webrtc::kH264CodecName, video_format.name);
  const auto it = video_format.parameters.find(webrtc::kH264FmtpProfileLevelId);
  ASSERT_NE(it, video_format.parameters.end());
  EXPECT_EQ(webrtc::kH264ProfileLevelConstrainedBaseline, it->second);
}

TEST(JavaCodecsWrapperTest, JavaToNativeResolutionBitrateLimits) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedJavaLocalRef<jobject> j_fake_encoder =
      jni::Java_CodecsWrapperTestHelper_createFakeVideoEncoder(env);
  const Environment webrtc_env = CreateTestEnvironment();

  auto encoder = jni::JavaToNativeVideoEncoder(
      env, j_fake_encoder, NativeToJavaPointer(&webrtc_env));
  ASSERT_TRUE(encoder);

  // Check that the bitrate limits correctly passed from Java to native.
  const std::vector<VideoEncoder::ResolutionBitrateLimits> bitrate_limits =
      encoder->GetEncoderInfo().resolution_bitrate_limits;
  ASSERT_EQ(bitrate_limits.size(), 1u);
  EXPECT_EQ(bitrate_limits[0].frame_size_pixels, 640 * 360);
  EXPECT_EQ(bitrate_limits[0].min_start_bitrate_bps, 300000);
  EXPECT_EQ(bitrate_limits[0].min_bitrate_bps, 200000);
  EXPECT_EQ(bitrate_limits[0].max_bitrate_bps, 1000000);
}

TEST(JavaCodecsWrapperTest, EncodeNullFrame) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedJavaLocalRef<jobject> j_fake_encoder =
      jni::Java_CodecsWrapperTestHelper_createFakeVideoEncoder(env);
  const Environment webrtc_env = CreateTestEnvironment();

  auto encoder = jni::JavaToNativeVideoEncoder(
      env, j_fake_encoder, NativeToJavaPointer(&webrtc_env));
  ASSERT_TRUE(encoder);

  // Initialize the encoder.
  VideoCodec codec_settings;
  codec_settings.codecType = kVideoCodecVP8;
  codec_settings.width = 640;
  codec_settings.height = 480;
  codec_settings.startBitrate = 300;
  codec_settings.maxFramerate = 30;
  VideoEncoder::Settings settings(VideoEncoder::Capabilities(false), 1, 1200);
  EXPECT_EQ(encoder->InitEncode(&codec_settings, settings),
            WEBRTC_VIDEO_CODEC_OK);

  // Create a frame with a buffer that returns null in ToI420.
  class NullI420Buffer : public VideoFrameBuffer {
   public:
    Type type() const override { return Type::kI420; }
    int width() const override { return 640; }
    int height() const override { return 480; }
    scoped_refptr<I420BufferInterface> ToI420() override { return nullptr; }
  };

  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(make_ref_counted<NullI420Buffer>())
          .set_rtp_timestamp(0)
          .set_timestamp_us(0)
          .set_rotation(kVideoRotation_0)
          .build();

  // Encode the frame. It should be dropped and return WEBRTC_VIDEO_CODEC_OK.
  EXPECT_EQ(encoder->Encode(frame, nullptr), WEBRTC_VIDEO_CODEC_OK);
}
}  // namespace
}  // namespace test
}  // namespace webrtc
