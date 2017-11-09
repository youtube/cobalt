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

#ifndef STARBOARD_SHARED_WIN32_VIDEO_DECODER_H_
#define STARBOARD_SHARED_WIN32_VIDEO_DECODER_H_

#include <wrl/client.h>

#include <deque>
#include <memory>

#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/configuration.h"
#include "starboard/decode_target.h"
#include "starboard/mutex.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/thread_checker.h"
#include "starboard/shared/win32/decrypting_decoder.h"
#include "starboard/shared/win32/dx_context_video_decoder.h"
#include "starboard/shared/win32/video_texture.h"
#include "starboard/thread.h"

namespace starboard {
namespace shared {
namespace win32 {

class VideoDecoder
    : public
        ::starboard::shared::starboard::player::filter::HostedVideoDecoder {
 public:
  typedef ::starboard::shared::starboard::player::InputBuffer InputBuffer;
  typedef ::starboard::shared::starboard::player::VideoFrame VideoFrame;

  VideoDecoder(SbMediaVideoCodec video_codec,
               SbPlayerOutputMode output_mode,
               SbDecodeTargetGraphicsContextProvider* graphics_context_provider,
               SbDrmSystem drm_system);
  ~VideoDecoder() SB_OVERRIDE;

  // Implement HostedVideoDecoder interface.
  void SetHost(Host* host) SB_OVERRIDE;
  void Initialize(const Closure& error_cb) SB_OVERRIDE;
  void WriteInputBuffer(const scoped_refptr<InputBuffer>& input_buffer)
      SB_OVERRIDE;
  void WriteEndOfStream() SB_OVERRIDE;
  void Reset() SB_OVERRIDE;
  SbDecodeTarget GetCurrentDecodeTarget() SB_OVERRIDE;

 private:
  struct Event {
    enum Type {
      kWriteInputBuffer,
      kWriteEndOfStream,
    };
    Type type;
    scoped_refptr<InputBuffer> input_buffer;
  };

  void InitializeCodec();
  void ShutdownCodec();
  void UpdateVideoArea(Microsoft::WRL::ComPtr<IMFMediaType> media);

  void EnsureDecoderThreadRunning();
  void StopDecoderThread();
  scoped_refptr<VideoFrame> DecoderThreadCreateFrame(
      Microsoft::WRL::ComPtr<IMFSample> sample);
  void DecoderThreadRun();
  static void* DecoderThreadEntry(void* context);

  ::starboard::shared::starboard::ThreadChecker thread_checker_;

  // These variables will be initialized inside ctor or SetHost() and will not
  // be changed during the life time of this class.
  const SbMediaVideoCodec video_codec_;
  Closure error_cb_;
  Host* host_;
  SbDrmSystem const drm_system_;

  VideoBltInterfaces video_texture_interfaces_;
  HardwareDecoderContext decoder_context_;
  scoped_ptr<DecryptingDecoder> decoder_;
  RECT video_area_;

  SbThread decoder_thread_;
  volatile bool decoder_thread_stop_requested_;
  bool decoder_thread_stopped_;
  Mutex thread_lock_;
  std::deque<std::unique_ptr<Event> > thread_events_;
};

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_VIDEO_DECODER_H_
