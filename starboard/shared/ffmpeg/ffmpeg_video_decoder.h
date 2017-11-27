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

#ifndef STARBOARD_SHARED_FFMPEG_FFMPEG_VIDEO_DECODER_H_
#define STARBOARD_SHARED_FFMPEG_FFMPEG_VIDEO_DECODER_H_

#include "starboard/common/ref_counted.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/queue.h"
#include "starboard/shared/ffmpeg/ffmpeg_common.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/video_frame_internal.h"
#include "starboard/thread.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

class VideoDecoder : public starboard::player::filter::HostedVideoDecoder {
 public:
  typedef starboard::player::InputBuffer InputBuffer;
  typedef starboard::player::VideoFrame VideoFrame;

  VideoDecoder(SbMediaVideoCodec video_codec,
               SbPlayerOutputMode output_mode,
               SbDecodeTargetGraphicsContextProvider*
                   decode_target_graphics_context_provider);
  ~VideoDecoder() override;

  void SetHost(Host* host) override;
  size_t GetPrerollFrameCount() const override { return 8; }
  void WriteInputBuffer(const scoped_refptr<InputBuffer>& input_buffer)
      override;
  void WriteEndOfStream() override;
  void Reset() override;

  bool is_valid() const { return codec_context_ != NULL; }

 private:
  enum EventType {
    kInvalid,
    kReset,
    kWriteInputBuffer,
    kWriteEndOfStream,
  };

  struct Event {
    EventType type;
    // |input_buffer| is only used when |type| is kWriteInputBuffer.
    scoped_refptr<InputBuffer> input_buffer;

    explicit Event(EventType type = kInvalid) : type(type) {
      SB_DCHECK(type != kWriteInputBuffer);
    }

    explicit Event(const scoped_refptr<InputBuffer>& input_buffer)
        : type(kWriteInputBuffer), input_buffer(input_buffer) {}
  };

  static void* ThreadEntryPoint(void* context);
  void DecoderThreadFunc();

  bool DecodePacket(AVPacket* packet);
  void InitializeCodec();
  void TeardownCodec();
  SbDecodeTarget GetCurrentDecodeTarget() override;

  bool UpdateDecodeTarget(const scoped_refptr<VideoFrame>& frame);

  // These variables will be initialized inside ctor or SetHost() and will not
  // be changed during the life time of this class.
  const SbMediaVideoCodec video_codec_;
  Host* host_;

  Queue<Event> queue_;

  // The AV related classes will only be created and accessed on the decoder
  // thread.
  AVCodecContext* codec_context_;
  AVFrame* av_frame_;

  bool stream_ended_;
  bool error_occured_;

  // Working thread to avoid lengthy decoding work block the player thread.
  SbThread decoder_thread_;

  // Decode-to-texture related state.
  SbPlayerOutputMode output_mode_;

  SbDecodeTargetGraphicsContextProvider*
      decode_target_graphics_context_provider_;

  // If decode-to-texture is enabled, then we store the decode target texture
  // inside of this |decode_target_| member.
  SbDecodeTarget decode_target_;

  // GetCurrentDecodeTarget() needs to be called from an arbitrary thread
  // to obtain the current decode target (which ultimately ends up being a
  // copy of |decode_target_|), we need to safe-guard access to |decode_target_|
  // and we do so through this mutex.
  Mutex decode_target_mutex_;
  // Mutex frame_mutex_;

  // int frame_last_rendered_pts_;
  // scoped_refptr<VideoFrame> frame_;
};

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_FFMPEG_FFMPEG_VIDEO_DECODER_H_
