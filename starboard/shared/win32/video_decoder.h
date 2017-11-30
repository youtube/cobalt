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

#include <D3d11_1.h>
#include <wrl/client.h>

#include <list>
#include <memory>

#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/configuration.h"
#include "starboard/decode_target.h"
#include "starboard/mutex.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/thread_checker.h"
#include "starboard/shared/win32/decrypting_decoder.h"
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
  ~VideoDecoder() override;

  // Implement HostedVideoDecoder interface.
  void SetHost(Host* host) override;
  size_t GetPrerollFrameCount() const override;
  void Initialize(const Closure& error_cb) override;
  void WriteInputBuffer(const scoped_refptr<InputBuffer>& input_buffer)
      override;
  void WriteEndOfStream() override;
  void Reset() override;
  SbDecodeTarget GetCurrentDecodeTarget() override;

 private:
  template <typename T>
  using ComPtr = Microsoft::WRL::ComPtr<T>;

  struct Event {
    enum Type {
      kWriteInputBuffer,
      kWriteEndOfStream,
    };
    Type type;
    scoped_refptr<InputBuffer> input_buffer;
  };

  struct Output {
    Output(SbMediaTime time, const RECT& video_area,
           const ComPtr<IMFSample>& video_sample)
        : time(time), video_area(video_area), video_sample(video_sample) {}
    SbMediaTime time;
    RECT video_area;
    ComPtr<IMFSample> video_sample;
  };

  void InitializeCodec();
  void ShutdownCodec();
  static void AllocateDecodeTargets(void* context);
  static void ReleaseDecodeTargets(void* context);

  void UpdateVideoArea(const ComPtr<IMFMediaType>& media);
  scoped_refptr<VideoFrame> CreateVideoFrame(const ComPtr<IMFSample>& sample);
  static void DeleteVideoFrame(void* context, void* native_texture);
  static void CreateDecodeTargetHelper(void* context);
  SbDecodeTarget CreateDecodeTarget();

  void EnsureDecoderThreadRunning();
  void StopDecoderThread();
  void DecoderThreadRun();
  static void* DecoderThreadEntry(void* context);

  ::starboard::shared::starboard::ThreadChecker thread_checker_;

  // These variables will be initialized inside ctor or SetHost() and will not
  // be changed during the life time of this class.
  const SbMediaVideoCodec video_codec_;
  Closure error_cb_;
  Host* host_;
  SbDecodeTargetGraphicsContextProvider* graphics_context_provider_;
  SbDrmSystem const drm_system_;

  // These are platform-specific objects required to create and use a codec.
  ComPtr<ID3D11Device> d3d_device_;
  ComPtr<IMFDXGIDeviceManager> device_manager_;
  ComPtr<ID3D11VideoDevice1> video_device_;
  ComPtr<ID3D11VideoContext> video_context_;
  ComPtr<ID3D11VideoProcessorEnumerator> video_enumerator_;
  ComPtr<ID3D11VideoProcessor> video_processor_;

  scoped_ptr<DecryptingDecoder> decoder_;
  RECT video_area_;

  SbThread decoder_thread_;
  volatile bool decoder_thread_stop_requested_;
  bool decoder_thread_stopped_;
  Mutex thread_lock_;
  std::list<std::unique_ptr<Event> > thread_events_;

  // This structure shadows the list of outstanding frames held by the host.
  // When a new output is added to this structure, the host should be notified
  // of a new VideoFrame. When the host deletes the VideoFrame, the delete
  // callback is used to update this structure. The VideoDecoder may need to
  // delete outputs without notifying the host. In such a situation, the host's
  // VideoFrames will be invalid if they still require the IMFSample; it's
  // possible that the VideoFrame was converted to a texture already, so it
  // will continue to be valid since the IMFSample is no longer needed.
  Mutex outputs_reset_lock_;
  std::list<Output> thread_outputs_;

  Mutex decode_target_lock_;
  SbDecodeTarget current_decode_target_;
  std::list<SbDecodeTarget> prev_decode_targets_;
};

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_VIDEO_DECODER_H_
