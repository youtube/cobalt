// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_VIDEO_DECODER_H_
#define MEDIA_MOJO_CLIENTS_MOJO_VIDEO_DECODER_H_

#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "media/base/status.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"
#include "media/mojo/mojom/video_decoder.mojom.h"
#include "media/video/video_decode_accelerator.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/color_space.h"

namespace base {
class SequencedTaskRunner;
}

namespace media {

class GpuVideoAcceleratorFactories;
class MediaLog;
class MojoDecoderBufferWriter;
class MojoVideoFrameHandleReleaser;

extern const char kMojoVideoDecoderInitialPlaybackSuccessCodecCounterUMA[];

extern const char kMojoVideoDecoderInitialPlaybackErrorCodecCounterUMA[];

// How many frames the decoder needs to process before reporting:
// kMojoVideoDecoderInitialPlaybackSuccessCodecCounterUMA
extern const int kMojoDecoderInitialPlaybackFrameCount;

// A VideoDecoder, for use in the renderer process, that proxies to a
// mojom::VideoDecoder. It is assumed that the other side will be implemented by
// MojoVideoDecoderService, running in the GPU process, and that the remote
// decoder will be hardware accelerated.
class MojoVideoDecoder final : public VideoDecoder,
                               public mojom::VideoDecoderClient {
 public:
  MojoVideoDecoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      GpuVideoAcceleratorFactories* gpu_factories,
      MediaLog* media_log,
      mojo::PendingRemote<mojom::VideoDecoder> pending_remote_decoder,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space);

  MojoVideoDecoder(const MojoVideoDecoder&) = delete;
  MojoVideoDecoder& operator=(const MojoVideoDecoder&) = delete;

  ~MojoVideoDecoder() final;

  // Decoder implementation
  bool IsPlatformDecoder() const final;
  bool SupportsDecryption() const final;
  VideoDecoderType GetDecoderType() const final;

  // VideoDecoder implementation.
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) final;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) final;
  void Reset(base::OnceClosure closure) final;
  bool NeedsBitstreamConversion() const final;
  bool CanReadWithoutStalling() const final;
  int GetMaxDecodeRequests() const final;
  bool IsOptimizedForRTC() const final;

  // mojom::VideoDecoderClient implementation.
  void OnVideoFrameDecoded(
      const scoped_refptr<VideoFrame>& frame,
      bool can_read_without_stalling,
      const absl::optional<base::UnguessableToken>& release_token) final;
  void OnWaiting(WaitingReason reason) final;
  void RequestOverlayInfo(bool restart_for_transitions) final;

  void set_writer_capacity_for_testing(uint32_t capacity) {
    writer_capacity_ = capacity;
  }

 private:
  void FailInit(InitCB init_cb, Status err);
  void OnInitializeDone(const Status& status,
                        bool needs_bitstream_conversion,
                        int32_t max_decode_requests,
                        VideoDecoderType decoder_type);
  void OnDecodeDone(uint64_t decode_id, const Status& status);
  void OnResetDone();

  void InitAndBindRemoteDecoder(base::OnceClosure complete_cb);
  void OnChannelTokenReady(media::mojom::CommandBufferIdPtr command_buffer_id,
                           base::OnceClosure complete_cb,
                           const base::UnguessableToken& channel_token);
  void InitAndConstructRemoteDecoder(
      media::mojom::CommandBufferIdPtr command_buffer_id,
      base::OnceClosure complete_cb);
  void InitializeRemoteDecoder(const VideoDecoderConfig& config,
                               bool low_delay,
                               absl::optional<base::UnguessableToken> cdm_id);

  // Forwards |overlay_info| to the remote decoder.
  void OnOverlayInfoChanged(const OverlayInfo& overlay_info);

  // Cleans up callbacks and blocks future calls.
  void Stop();

  void ReportInitialPlaybackErrorUMA();

  // Task runner that the decoder runs on (media thread).
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  // Used to pass the remote decoder from the constructor (on the main thread)
  // to Initialize() (on the media thread).
  mojo::PendingRemote<mojom::VideoDecoder> pending_remote_decoder_;

  // Manages VideoFrame destruction callbacks.
  scoped_refptr<MojoVideoFrameHandleReleaser> mojo_video_frame_handle_releaser_;

  GpuVideoAcceleratorFactories* gpu_factories_ = nullptr;

  // Raw pointer is safe since both `this` and the `media_log` are owned by
  // WebMediaPlayerImpl with the correct declaration order.
  MediaLog* media_log_ = nullptr;

  InitCB init_cb_;
  OutputCB output_cb_;
  WaitingCB waiting_cb_;
  uint64_t decode_counter_ = 0;
  std::map<uint64_t, DecodeCB> pending_decodes_;
  base::OnceClosure reset_cb_;

  // DecodeBuffer/VideoFrame timestamps for histogram/tracing purposes. Must be
  // large enough to account for any amount of frame reordering.
  base::MRUCache<int64_t, base::TimeTicks> timestamps_;

  mojo::Remote<mojom::VideoDecoder> remote_decoder_;
  std::unique_ptr<MojoDecoderBufferWriter> mojo_decoder_buffer_writer_;

  uint32_t writer_capacity_ = 0;

  bool remote_decoder_bound_ = false;
  bool has_connection_error_ = false;
  mojo::AssociatedReceiver<mojom::VideoDecoderClient> client_receiver_{this};
  RequestOverlayInfoCB request_overlay_info_cb_;
  bool overlay_info_requested_ = false;
  gfx::ColorSpace target_color_space_;

  bool initialized_ = false;
  bool needs_bitstream_conversion_ = false;
  bool can_read_without_stalling_ = true;
  VideoDecoderType decoder_type_ = VideoDecoderType::kUnknown;

  // True if UMA metrics of success/failure after first few seconds of playback
  // have been already reported.
  bool initial_playback_outcome_reported_ = false;
  int total_frames_decoded_ = 0;
  int32_t max_decode_requests_ = 1;

  base::WeakPtr<MojoVideoDecoder> weak_this_;
  base::WeakPtrFactory<MojoVideoDecoder> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_VIDEO_DECODER_H_
