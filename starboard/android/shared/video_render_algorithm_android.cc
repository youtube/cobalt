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

#include "starboard/android/shared/video_render_algorithm_android.h"

#include <algorithm>
#include <list>
#include <optional>

#include "build/build_config.h"
#include "build/buildflag.h"
#include "cobalt/android/jni_headers/VideoFrameReleaseTimeHelper_jni.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/shared/starboard/features.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {

using jni_zero::AttachCurrentThread;
using jni_zero::ScopedJavaGlobalRef;
using jni_zero::ScopedJavaLocalRef;

namespace {

const int64_t kBufferTooLateThreshold = -32'000;  // -32ms
const int64_t kBufferReadyThreshold = 50'000;     // 50ms

// The following are only used with |kMediaDeadlineDrivenVideoOutput|.
// The longest time to go without rendering. The thread that renders is also
// woken up by every decoded frame, so this only bounds how late a change of the
// media time that was not anticipated, e.g. of the playback rate, is noticed.
const int64_t kMaxTimeToNextRender = 50'000;  // 50ms
const int64_t kMinTimeToNextRender = 1'000;   // 1ms
// Render this much after the front frame becomes ready, so that it is ready
// for sure by then, instead of right when it becomes ready.
const int64_t kTimeToNextRenderMargin = 1'000;  // 1ms

jlong GetSystemNanoTime() {
  timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return now.tv_sec * 1000000000LL + now.tv_nsec;
}

}  // namespace

VideoRenderAlgorithmAndroid::VideoRenderAlgorithmAndroid(
    MediaCodecVideoDecoder* video_decoder,
    VideoFrameTracker* frame_tracker)
    : video_decoder_(video_decoder),
      frame_tracker_(frame_tracker),
#if !BUILDFLAG(IS_STARBOARD)
      release_frames_after_audio_starts_(features::FeatureList::IsEnabled(
          features::kReleaseVideoFramesAfterAudioStarts)) {
#else
      release_frames_after_audio_starts_(false) {
#endif
  SB_CHECK(video_decoder_);
  video_decoder_->SetPlaybackRate(playback_rate_);
}

void VideoRenderAlgorithmAndroid::Render(
    MediaTimeProvider* media_time_provider,
    std::list<scoped_refptr<VideoFrame>>* frames,
    VideoRendererSink::DrawFrameCB draw_frame_cb) {
  SB_CHECK(media_time_provider);
  SB_CHECK(frames);
  SB_CHECK(draw_frame_cb);

  // How long this function doesn't have to be called again, unless a frame is
  // added to |frames|. With no frame left to release, that is as long as
  // allowed. Reset where the loop is left because it has to check again soon.
  std::optional<int64_t> time_to_next_render_us = kMaxTimeToNextRender;

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
    int64_t playback_time = media_time_provider->GetCurrentMediaTime(
        &is_audio_playing, &is_audio_eos_played, &is_underflow, &playback_rate);

    if (release_frames_after_audio_starts_) {
      // After the first frame, stop rendering if audio isn't playing, or if
      // audio playback hasn't advanced past the current seek_to_time (or
      // initial time 0). This ensures video doesn't run ahead if audio is
      // stalled or hasn't consumed frames yet.
      if (first_frame_released_ &&
          (!is_audio_playing || playback_time == seek_to_time_)) {
        // Nothing tells when the audio starts to play.
        time_to_next_render_us = std::nullopt;
        break;
      }
    } else {
      if (!is_audio_playing) {
        time_to_next_render_us = std::nullopt;
        break;
      }
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
        time_to_next_render_us = std::nullopt;
        break;
      }
    }

    jlong early_us = (frames->front()->timestamp() - playback_time) /
                     (playback_rate != 0 ? playback_rate : 1);

    auto system_time_ns = GetSystemNanoTime();
    auto unadjusted_frame_release_time_ns = system_time_ns + (early_us * 1000);

    auto adjusted_release_time_ns =
        video_frame_release_time_helper_.AdjustReleaseTime(
            frames->front()->timestamp(), unadjusted_frame_release_time_ns,
            playback_rate);

    early_us = (adjusted_release_time_ns - system_time_ns) / 1000;

    if (early_us < kBufferTooLateThreshold) {
      frames->pop_front();
      ++dropped_frames_;
    } else if (early_us < kBufferReadyThreshold) {
      [[maybe_unused]] auto status =
          draw_frame_cb(frames->front(), adjusted_release_time_ns);
      SB_DCHECK_EQ(status, VideoRendererSink::kReleased);
      frames->pop_front();
      first_frame_released_ = true;
    } else {
      if (playback_rate == 0) {
        // Paused. Nothing tells when the playback resumes.
        time_to_next_render_us = std::nullopt;
      } else {
        // The front frame becomes ready when it is |kBufferReadyThreshold|
        // early.
        time_to_next_render_us = std::clamp<int64_t>(
            early_us - kBufferReadyThreshold + kTimeToNextRenderMargin,
            kMinTimeToNextRender, kMaxTimeToNextRender);
      }
      break;
    }
  }

  if (video_decoder_->deadline_driven_video_output()) {
    video_decoder_->set_time_to_next_render(time_to_next_render_us);
  }
}

void VideoRenderAlgorithmAndroid::Seek(int64_t seek_to_time) {
  if (frame_tracker_) {
    frame_tracker_->Seek(seek_to_time);
  }
  first_frame_released_ = false;
  seek_to_time_ = seek_to_time;
}

int VideoRenderAlgorithmAndroid::GetDroppedFrames() {
  if (frame_tracker_) {
    return frame_tracker_->UpdateAndGetDroppedFrames();
  }
  return dropped_frames_;
}

VideoRenderAlgorithmAndroid::VideoFrameReleaseTimeHelper::
    VideoFrameReleaseTimeHelper() {
  JNIEnv* env = AttachCurrentThread();

  j_video_frame_release_time_helper_.Reset(
      Java_VideoFrameReleaseTimeHelper_Constructor(env));

  Java_VideoFrameReleaseTimeHelper_enable(env,
                                          j_video_frame_release_time_helper_);
}

VideoRenderAlgorithmAndroid::VideoFrameReleaseTimeHelper::
    ~VideoFrameReleaseTimeHelper() {
  SB_CHECK(j_video_frame_release_time_helper_);
  Java_VideoFrameReleaseTimeHelper_disable(AttachCurrentThread(),
                                           j_video_frame_release_time_helper_);
}

jlong VideoRenderAlgorithmAndroid::VideoFrameReleaseTimeHelper::
    AdjustReleaseTime(jlong frame_presentation_time_us,
                      jlong unadjusted_release_time_ns,
                      double playback_rate) {
  SB_CHECK(j_video_frame_release_time_helper_);
  return Java_VideoFrameReleaseTimeHelper_adjustReleaseTime(
      AttachCurrentThread(), j_video_frame_release_time_helper_,
      frame_presentation_time_us, unadjusted_release_time_ns, playback_rate);
}

}  // namespace starboard
