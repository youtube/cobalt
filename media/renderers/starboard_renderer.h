// TODO: License

#ifndef MEDIA_RENDERERS_STARBOARD_RENDERER_H_
#define MEDIA_RENDERERS_STARBOARD_RENDERER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_resource.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer.h"
#include "media/base/renderer_client.h"
#include "media/renderers/sbplayer_bridge.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

class VideoOverlayFactory;
class VideoRendererSink;

class MEDIA_EXPORT StarboardRenderer final : public Renderer,
                                             private SbPlayerBridge::Host {
 public:
  explicit StarboardRenderer(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      VideoRendererSink* video_renderer_sink);

  ~StarboardRenderer() final;

  // Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) final;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) final {
    NOTIMPLEMENTED();
  }
  void SetLatencyHint(absl::optional<base::TimeDelta> latency_hint) final {
    NOTIMPLEMENTED();
  }
  void SetPreservesPitch(bool preserves_pitch) final { NOTIMPLEMENTED(); }
  void SetWasPlayedWithUserActivation(
      bool was_played_with_user_activation) final {
    NOTIMPLEMENTED();
  }
  void Flush(base::OnceClosure flush_cb) final;
  void StartPlayingFrom(base::TimeDelta time) final;
  void SetPlaybackRate(double playback_rate) final;
  void SetVolume(float volume) final;
  base::TimeDelta GetMediaTime() final;
  void OnSelectedVideoTracksChanged(
      const std::vector<DemuxerStream*>& enabled_tracks,
      base::OnceClosure change_completed_cb) final {
    NOTIMPLEMENTED();
  }
  void OnEnabledAudioTracksChanged(
      const std::vector<DemuxerStream*>& enabled_tracks,
      base::OnceClosure change_completed_cb) final {
    NOTIMPLEMENTED();
  }
  RendererType GetRendererType() final {
    // Reuse `kRendererImpl` to avoid introducing a new renderer type.
    return RendererType::kRendererImpl;
  }

 private:
  void CreatePlayerBridge(PipelineStatusCallback init_cb);
  void UpdateDecoderConfig(DemuxerStream* stream);
  void OnDemuxerStreamRead(DemuxerStream* stream,
                           DemuxerStream::Status status,
                           DemuxerStream::DecoderBufferVector buffers);

  void OnNeedData(DemuxerStream::Type type,
                  int max_number_of_buffers_to_write) final;
  void OnPlayerStatus(SbPlayerState state) final;
  void OnPlayerError(SbPlayerError error, const std::string& message) final;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Video frame overlays are rendered onto this sink.
  // Rendering of a new overlay is only needed when video natural size changes.
  raw_ptr<VideoRendererSink> video_renderer_sink_ = nullptr;

  raw_ptr<MediaResource> media_resource_ = nullptr;
  raw_ptr<DemuxerStream> audio_stream_ = nullptr;
  raw_ptr<DemuxerStream> video_stream_ = nullptr;
  raw_ptr<RendererClient> client_ = nullptr;

  // Overlay factory used to create overlays for video frames rendered
  // by the remote renderer.
  std::unique_ptr<VideoOverlayFactory> video_overlay_factory_;

  DefaultSbPlayerInterface sbplayer_interface_;
  const base::TimeDelta audio_write_duration_local_ = base::Milliseconds(500);
  const base::TimeDelta audio_write_duration_remote_ = base::Seconds(10);
  const int max_audio_samples_per_write_ = 1;

  base::Lock lock_;
  std::unique_ptr<SbPlayerBridge> player_bridge_;

  bool player_bridge_initialized_ = false;
  std::optional<base::TimeDelta> playing_start_from_time_;

  std::optional<base::OnceClosure> pending_flush_cb_;

  base::TimeDelta audio_write_duration_ = audio_write_duration_local_;

  bool audio_read_in_progress_ = false;
  bool video_read_in_progress_ = false;

  // TODO: Consider calling `void OnWaiting(WaitingReason reason)` on `client_`.
  // TODO: Shall we call `void OnVideoFrameRateChange(absl::optional<int> fps)`
  //       on `client_`?
  double playback_rate_ = 0.;
  float volume_ = 1.0f;

  uint32_t last_video_frames_decoded_ = 0, last_video_frames_dropped_ = 0;

  base::WeakPtrFactory<StarboardRenderer> weak_factory_{this};
  base::WeakPtr<StarboardRenderer> weak_this_;

  StarboardRenderer(const StarboardRenderer&) = delete;
  StarboardRenderer& operator=(const StarboardRenderer&) = delete;
};

}  // namespace media

#endif  // MEDIA_RENDERERS_STARBOARD_RENDERER_H_
