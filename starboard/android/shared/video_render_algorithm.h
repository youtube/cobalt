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

#ifndef STARBOARD_ANDROID_SHARED_VIDEO_RENDER_ALGORITHM_H_
#define STARBOARD_ANDROID_SHARED_VIDEO_RENDER_ALGORITHM_H_

#include <list>

#include "base/android/jni_android.h"
#include "starboard/android/shared/video_decoder.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm.h"

namespace starboard {

class VideoRenderAlgorithmAndroid : public VideoRenderAlgorithm {
 public:
  VideoRenderAlgorithmAndroid(MediaCodecVideoDecoder* video_decoder,
                              VideoFrameTracker* frame_tracker);

  void Render(MediaTimeProvider* media_time_provider,
              std::list<scoped_refptr<VideoFrame>>* frames,
              VideoRendererSink::DrawFrameCB draw_frame_cb) override;
  void Seek(int64_t seek_to_time) override;
  int GetDroppedFrames() override;

 private:
  class VideoFrameReleaseTimeHelper {
   public:
    VideoFrameReleaseTimeHelper();
    ~VideoFrameReleaseTimeHelper();
    jlong AdjustReleaseTime(jlong frame_presentation_time_us,
                            jlong unadjusted_release_time_ns,
                            double playback_rate);

   private:
    base::android::ScopedJavaGlobalRef<jobject>
        j_video_frame_release_time_helper_;
  };

  MediaCodecVideoDecoder* video_decoder_ = nullptr;
  VideoFrameTracker* frame_tracker_;
  double playback_rate_ = 1.0;
  VideoFrameReleaseTimeHelper video_frame_release_time_helper_;
  int dropped_frames_ = 0;
  bool first_frame_released_ = false;
  int64_t seek_to_time_ = 0;  // microseconds

  const bool release_frames_after_audio_starts_;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_RENDER_ALGORITHM_H_
