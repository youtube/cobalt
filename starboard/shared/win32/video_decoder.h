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

#ifndef STARBOARD_SHARED_WIN32_VIDEO_DECODER_H_
#define STARBOARD_SHARED_WIN32_VIDEO_DECODER_H_

#include <D3d11_1.h>
#include <wrl/client.h>

#include <atomic>
#include <list>
#include <memory>

#include "starboard/common/mutex.h"
#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/configuration.h"
#include "starboard/decode_target.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/thread_checker.h"
#include "starboard/shared/win32/decrypting_decoder.h"
#include "starboard/thread.h"

namespace starboard {
namespace shared {
namespace win32 {

class VideoDecoder
    : public ::starboard::shared::starboard::player::filter::VideoDecoder {
 public:
  VideoDecoder(SbMediaVideoCodec video_codec,
               SbPlayerOutputMode output_mode,
               SbDecodeTargetGraphicsContextProvider* graphics_context_provider,
               SbDrmSystem drm_system,
               bool is_hdr_supported = false);
  ~VideoDecoder() override;

  // Queries for support without creating the vp9 decoder. The function caches
  // the result for the first call.  Note that the first call to this function
  // isn't thread safe and is supposed to be called on startup.
  static bool IsHardwareVp9DecoderSupported(bool is_hdr_required = false);

  bool IsHdrSupported() const { return is_hdr_supported_; }

  // Implement VideoDecoder interface.
  void Initialize(const DecoderStatusCB& decoder_status_cb,
                  const ErrorCB& error_cb) override;
  size_t GetPrerollFrameCount() const override;
  SbTime GetPrerollTimeout() const override { return kSbTimeMax; }
  size_t GetMaxNumberOfCachedFrames() const override;

  void WriteInputBuffers(const InputBuffers& input_buffers) override;
  void WriteEndOfStream() override;
  void Reset() override;
  SbDecodeTarget GetCurrentDecodeTarget() override;

 private:
  typedef ::starboard::shared::starboard::media::VideoStreamInfo
      VideoStreamInfo;

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
    Output(SbTime time,
           const RECT& video_area,
           const ComPtr<IMFSample>& video_sample)
        : time(time), video_area(video_area), video_sample(video_sample) {}
    SbTime time;
    RECT video_area;
    ComPtr<IMFSample> video_sample;
  };

  // This function returns false if HDR video is played, but HDR support is
  // lost. Otherwise it returns true. For inherited class it also updates HDMI
  // color metadata and sets HDR mode for Angle, if the video stream is HDR
  // stream.
  virtual bool TryUpdateOutputForHdrVideo(const VideoStreamInfo& stream_info) {
    return true;
  }

  void InitializeCodec();
  void ShutdownCodec();
  static void ReleaseDecodeTargets(void* context);

  void UpdateVideoArea(const ComPtr<IMFMediaType>& media);
  scoped_refptr<VideoFrame> CreateVideoFrame(const ComPtr<IMFSample>& sample);
  void DeleteVideoFrame(VideoFrame* video_frame);
  SbDecodeTarget CreateDecodeTarget();

  void EnsureDecoderThreadRunning();
  void StopDecoderThread();
  void DecoderThreadRun();
  static void* DecoderThreadEntry(void* context);

  ::starboard::shared::starboard::ThreadChecker thread_checker_;

  // These variables will be initialized inside ctor or Initialize() and will
  // not be changed during the life time of this class.
  const SbMediaVideoCodec video_codec_;
  DecoderStatusCB decoder_status_cb_;
  ErrorCB error_cb_;
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

  SbThread decoder_thread_ = kSbThreadInvalid;
  volatile bool decoder_thread_stop_requested_ = false;
  bool decoder_thread_stopped_ = false;
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

  // To workaround the startup hitch for VP9, exercise the decoder for a
  // certain number of frames while prerolling the initial playback.
  int priming_output_count_;

  SbDecodeTarget current_decode_target_ = kSbDecodeTargetInvalid;
  std::list<SbDecodeTarget> prev_decode_targets_;

  bool is_hdr_supported_;
  std::atomic_bool error_occured_ = {false};
};

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_VIDEO_DECODER_H_
