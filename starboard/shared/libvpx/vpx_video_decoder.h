// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_LIBVPX_VPX_VIDEO_DECODER_H_
#define STARBOARD_SHARED_LIBVPX_VPX_VIDEO_DECODER_H_

#include "starboard/common/ref_counted.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/queue.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/video_frame_internal.h"
#include "starboard/thread.h"

#define VPX_CODEC_DISABLE_COMPAT 1
#include "third_party/libvpx/vpx/vp8dx.h"
#include "third_party/libvpx/vpx/vpx_decoder.h"

namespace starboard {
namespace shared {
namespace vpx {

class VideoDecoder : public starboard::player::filter::HostedVideoDecoder {
 public:
  typedef starboard::player::InputBuffer InputBuffer;
  typedef starboard::player::VideoFrame VideoFrame;

  VideoDecoder(SbMediaVideoCodec video_codec,
               SbPlayerOutputMode output_mode,
               SbDecodeTargetGraphicsContextProvider*
                   decode_target_graphics_context_provider);
  ~VideoDecoder() SB_OVERRIDE;

  void SetHost(Host* host) SB_OVERRIDE;
  size_t GetPrerollFrameCount() const SB_OVERRIDE { return 8; }
  void WriteInputBuffer(const scoped_refptr<InputBuffer>& input_buffer)
      SB_OVERRIDE;
  void WriteEndOfStream() SB_OVERRIDE;
  void Reset() SB_OVERRIDE;

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

  void ReportError();

  // The following three functions are only called on the decoder thread except
  // that TeardownCodec() can also be called on other threads when the decoder
  // thread is not running.
  void InitializeCodec();
  void TeardownCodec();
  void DecodeOneBuffer(const scoped_refptr<InputBuffer>& input_buffer);

  SbDecodeTarget GetCurrentDecodeTarget() SB_OVERRIDE;

  bool UpdateDecodeTarget(const scoped_refptr<VideoFrame>& frame);

  // |host_| will be initialized inside SetHost() and will not be changed during
  // the life time of this class.
  Host* host_;

  Queue<Event> queue_;

  int current_frame_width_;
  int current_frame_height_;
  scoped_ptr<vpx_codec_ctx> context_;

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
};

}  // namespace vpx
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_LIBVPX_VPX_VIDEO_DECODER_H_
