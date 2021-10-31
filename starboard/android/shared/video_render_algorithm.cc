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

#include "starboard/android/shared/video_render_algorithm.h"

#include <algorithm>

#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/common/log.h"

namespace starboard {
namespace android {
namespace shared {

namespace {

const SbTimeMonotonic kBufferTooLateThreshold = -30 * kSbTimeMillisecond;
const SbTimeMonotonic kBufferReadyThreshold = 50 * kSbTimeMillisecond;

jlong GetSystemNanoTime() {
  timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return now.tv_sec * 1000000000LL + now.tv_nsec;
}

}  // namespace

VideoRenderAlgorithm::VideoRenderAlgorithm(VideoDecoder* video_decoder)
    : video_decoder_(video_decoder) {
  SB_DCHECK(video_decoder_);
  video_decoder_->SetPlaybackRate(playback_rate_);
}

void VideoRenderAlgorithm::Render(
    MediaTimeProvider* media_time_provider,
    std::list<scoped_refptr<VideoFrame>>* frames,
    VideoRendererSink::DrawFrameCB draw_frame_cb) {
  SB_DCHECK(media_time_provider);
  SB_DCHECK(frames);
  SB_DCHECK(draw_frame_cb);

  while (frames->size() > 0) {
    if (frames->front()->is_end_of_stream()) {
      frames->pop_front();
      SB_DCHECK(frames->empty())
          << "Expected end of stream output buffer to be the last buffer.";
      break;
    }

    bool is_audio_playing;
    bool is_audio_eos_played;
    bool is_underflow;
    double playback_rate;
    SbTime playback_time = media_time_provider->GetCurrentMediaTime(
        &is_audio_playing, &is_audio_eos_played, &is_underflow, &playback_rate);
    if (!is_audio_playing) {
      break;
    }
    if (playback_rate != playback_rate_) {
      playback_rate_ = playback_rate;
      video_decoder_->SetPlaybackRate(playback_rate);
    }

    if (is_audio_eos_played) {
      // If the audio stream has reached end of stream before the video stream,
      // we should end the video stream immediately by only keeping one frame in
      // the backlog.
      bool popped = false;
      while (frames->size() > 1) {
        frames->pop_front();
        popped = true;
      }
      if (popped) {
        // Let the loop process the end of stream frame, if there is one.
        continue;
      } else {
        break;
      }
    }

    jlong early_us = frames->front()->timestamp() - playback_time;

    auto system_time_ns = GetSystemNanoTime();
    auto unadjusted_frame_release_time_ns =
        system_time_ns + (early_us * kSbTimeNanosecondsPerMicrosecond);

    auto adjusted_release_time_ns =
        video_frame_release_time_helper_.AdjustReleaseTime(
            frames->front()->timestamp(), unadjusted_frame_release_time_ns);

    early_us = (adjusted_release_time_ns - system_time_ns) /
               kSbTimeNanosecondsPerMicrosecond;

    if (early_us < kBufferTooLateThreshold) {
      frames->pop_front();
      ++dropped_frames_;
    } else if (early_us < kBufferReadyThreshold) {
      auto status = draw_frame_cb(frames->front(), adjusted_release_time_ns);
      SB_DCHECK(status == VideoRendererSink::kReleased);
      frames->pop_front();
    } else {
      break;
    }
  }
}

VideoRenderAlgorithm::VideoFrameReleaseTimeHelper::
    VideoFrameReleaseTimeHelper() {
  auto* env = JniEnvExt::Get();
  j_video_frame_release_time_helper_ = env->NewObjectOrAbort(
      "dev/cobalt/media/VideoFrameReleaseTimeHelper", "()V");
  j_video_frame_release_time_helper_ =
      env->ConvertLocalRefToGlobalRef(j_video_frame_release_time_helper_);
  env->CallVoidMethod(j_video_frame_release_time_helper_, "enable", "()V");
}

VideoRenderAlgorithm::VideoFrameReleaseTimeHelper::
    ~VideoFrameReleaseTimeHelper() {
  SB_DCHECK(j_video_frame_release_time_helper_);
  auto* env = JniEnvExt::Get();
  env->CallVoidMethod(j_video_frame_release_time_helper_, "disable", "()V");
  env->DeleteGlobalRef(j_video_frame_release_time_helper_);
  j_video_frame_release_time_helper_ = nullptr;
}

jlong VideoRenderAlgorithm::VideoFrameReleaseTimeHelper::AdjustReleaseTime(
    jlong frame_presentation_time_us,
    jlong unadjusted_release_time_ns) {
  SB_DCHECK(j_video_frame_release_time_helper_);
  auto* env = JniEnvExt::Get();
  return env->CallLongMethodOrAbort(
      j_video_frame_release_time_helper_, "adjustReleaseTime", "(JJ)J",
      frame_presentation_time_us, unadjusted_release_time_ns);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
