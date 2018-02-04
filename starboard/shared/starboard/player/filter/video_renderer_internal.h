// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_INTERNAL_H_

#include <list>

#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/mutex.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/media_time_provider.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/video_frame_internal.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm.h"
#include "starboard/shared/starboard/player/filter/video_renderer_internal.h"
#include "starboard/shared/starboard/player/filter/video_renderer_sink.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/thread_checker.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// A class that sits in between the video decoder, the video sink and the
// pipeline to coordinate data transfer between these parties.
class VideoRenderer {
 public:
  typedef VideoDecoder::ErrorCB ErrorCB;

  VideoRenderer(scoped_ptr<VideoDecoder> decoder,
                MediaTimeProvider* media_time_provider,
                scoped_ptr<VideoRenderAlgorithm> algorithm,
                scoped_refptr<VideoRendererSink> sink);
  ~VideoRenderer();

  void Initialize(const ErrorCB& error_cb);
  void SetBounds(int z_index, int x, int y, int width, int height);
  int GetDroppedFrames() const { return algorithm_->GetDroppedFrames(); }

  void WriteSample(const scoped_refptr<InputBuffer>& input_buffer);
  void WriteEndOfStream();

  void Seek(SbMediaTime seek_to_pts);

  bool IsEndOfStreamWritten() const { return end_of_stream_written_; }
  bool IsEndOfStreamPlayed() const;
  bool CanAcceptMoreData() const;
  bool IsSeekingInProgress() const;

  SbDecodeTarget GetCurrentDecodeTarget();

 private:
  typedef std::list<scoped_refptr<VideoFrame>> Frames;

  void OnDecoderStatus(VideoDecoder::Status status,
                       const scoped_refptr<VideoFrame>& frame);
  void Render(VideoRendererSink::DrawFrameCB draw_frame_cb);

  ThreadChecker thread_checker_;
  Mutex mutex_;

  MediaTimeProvider* const media_time_provider_;
  scoped_ptr<VideoRenderAlgorithm> algorithm_;
  scoped_refptr<VideoRendererSink> sink_;

  bool seeking_;
  SbTimeMonotonic absolute_time_of_first_input_;

  Frames frames_;

  SbMediaTime seeking_to_pts_;
  bool end_of_stream_written_;
  bool need_more_input_;

  scoped_ptr<VideoDecoder> decoder_;

  // Our owner will attempt to seek to pts 0 when playback begins.  In
  // general, seeking could require a full reset of the underlying decoder on
  // some platforms, so we make an effort to improve playback startup
  // performance by keeping track of whether we already have a fresh decoder,
  // and can thus avoid doing a full reset.
  bool first_input_written_;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_RENDERER_INTERNAL_H_
