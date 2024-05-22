// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_MANIFEST_DEMUXER_H_
#define MEDIA_FILTERS_MANIFEST_DEMUXER_H_

#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "media/base/container_names.h"
#include "media/base/demuxer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/media_track.h"
#include "media/base/pipeline_status.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/hls_data_source_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

// Declared and defined in manifest_demuxer.cc.
class ManifestDemuxerStream;

// This class provides an interface for ManifestDemuxer::Engine to talk to
// ManifestDemuxer about the internal ChunkDemuxer, without allowing it to call
// any of the methods that the pipeline uses, like Seek or Stop.
class MEDIA_EXPORT ManifestDemuxerEngineHost {
 public:
  virtual ~ManifestDemuxerEngineHost() {}

  // Handle errors.
  virtual void OnError(PipelineStatus error);
};

// A Demuxer designed to allow implementation of media demuxers which don't
// rely on raw media data alone, such as HLS or DASH. This demuxer owns an
// implementation of an engine and handles the seeking and event dispatching
// for the engine so that it can focus on keeping internal manifest states up
// to date.
class MEDIA_EXPORT ManifestDemuxer : public Demuxer, ManifestDemuxerEngineHost {
 public:
  using DelayCallback = base::OnceCallback<void(base::TimeDelta)>;

  class Engine {
   public:
    virtual ~Engine() {}

    // Set for the engine, such as fetching manifests or content.
    virtual void Initialize(ManifestDemuxerEngineHost* demuxer,
                            PipelineStatusCallback status_cb) = 0;

    // Get the name of the engine impl.
    virtual std::string GetName() const = 0;

    // A tick signal indicating that the state of the engine should be
    // checked. `time` is the current player time. `playback_rate` is the
    // current playback rate. `loaded_ranges` is the current set of loaded
    // ranges in the chunk demuxer. `cb` should be called with the amount of
    // time to delay until the next event is requested.
    virtual void OnTimeUpdate(base::TimeDelta time,
                              double playback_rate,
                              DelayCallback cb) = 0;

    // A synchronous seek, mostly intended to reset parts of the chunk
    // demuxer. returns whether the chunk demuxer needs more data.
    virtual bool Seek(base::TimeDelta time) = 0;

    // Start waiting for seek, usually means canceling outstanding events
    // and network fetches.
    virtual void StartWaitingForSeek() = 0;

    // Abort any pending reads, parses, or network requests.
    virtual void AbortPendingReads() = 0;

    // Returns whether this engine supports seeking. Some live stream content
    // can't be seeked.
    virtual bool IsSeekable() = 0;

    // Gets the memory usage of the engine.
    virtual int64_t GetMemoryUsage() const = 0;

    // Stop demuxing and clean up pending CBs.
    virtual void Stop() = 0;
  };

  // ManifestDemuxer takes and keeps ownership of `impl` for the lifetime of
  // both.
  ManifestDemuxer(scoped_refptr<base::SequencedTaskRunner> media_task_runner,
                  std::unique_ptr<Engine> impl,
                  MediaLog* media_log);

  ~ManifestDemuxer() override;

  // `media::Demuxer` implementation
  std::vector<DemuxerStream*> GetAllStreams() override;
  std::string GetDisplayName() const override;
  DemuxerType GetDemuxerType() const override;
  void Initialize(DemuxerHost* host, PipelineStatusCallback status_cb) override;
  void AbortPendingReads() override;
  void StartWaitingForSeek(base::TimeDelta seek_time) override;
  void CancelPendingSeek(base::TimeDelta seek_time) override;
  void Seek(base::TimeDelta time, PipelineStatusCallback status_cb) override;
  bool IsSeekable() const override;
  void Stop() override;
  base::TimeDelta GetStartTime() const override;
  base::Time GetTimelineOffset() const override;
  int64_t GetMemoryUsage() const override;
  void SetPlaybackRate(double rate) override;
  absl::optional<container_names::MediaContainerName> GetContainerForMetrics()
      const override;

  void OnEnabledAudioTracksChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta curr_time,
                                   TrackChangeCB change_completed_cb) override;
  void OnSelectedVideoTrackChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta curr_time,
                                   TrackChangeCB change_completed_cb) override;

  // `ManifestDemuxerEngineHost` implementation
  void OnError(PipelineStatus status) override;

  // Allow unit tests to grab the chunk demuxer.
  ChunkDemuxer* GetChunkDemuxerForTesting();
  bool has_pending_seek_for_testing() const { return !pending_seek_.is_null(); }
  bool has_pending_event_for_testing() const { return has_pending_event_; }
  bool has_next_task_for_testing() const {
    return !cancelable_next_event_.IsCancelled();
  }

 private:
  // This wrapper class allows us to capture the results of Read() and use
  // DecoderBuffer timestamps to update the current media time within the
  // loaded buffer, without having to make modifications to ChunkDemuxer.
  class ManifestDemuxerStream : public DemuxerStream {
   public:
    ~ManifestDemuxerStream() override;
    using WrapperReadCb =
        base::RepeatingCallback<void(DemuxerStream::ReadCB,
                                     DemuxerStream::Status,
                                     DemuxerStream::DecoderBufferVector)>;
    ManifestDemuxerStream(DemuxerStream* stream, WrapperReadCb cb);
    void Read(uint32_t count, DemuxerStream::ReadCB cb) override;
    AudioDecoderConfig audio_decoder_config() override;
    VideoDecoderConfig video_decoder_config() override;
    DemuxerStream::Type type() const override;
    StreamLiveness liveness() const override;
    void EnableBitstreamConverter() override;
    bool SupportsConfigChanges() override;

   private:
    WrapperReadCb read_cb_;
    base::raw_ptr<DemuxerStream> stream_;
  };

  void OnChunkDemuxerInitialized(PipelineStatus init_status);
  void OnChunkDemuxerOpened();
  void OnProgress();
  void OnEncryptedMediaData(EmeInitDataType type,
                            const std::vector<uint8_t>& data);
  void OnDemuxerStreamRead(DemuxerStream::ReadCB wrapped_read_cb,
                           DemuxerStream::Status status,
                           DemuxerStream::DecoderBufferVector buffers);

  // Helper for the `Seek` call, so that returning from an event when a seek
  // is pending can continue the seek process.
  void SeekInternal();
  void OnChunkDemuxerSeeked(PipelineStatus seek_status);
  void OnEngineSeekComplete(base::TimeDelta delay_time);
  void CompletePendingSeek();

  // Allows for both the chunk demuxer and the engine to be required for
  // initialization.
  void OnEngineInitialized(PipelineStatus status);
  void MaybeCompleteInitialize();

  // Trigger the next event, and based on it's expected delay, post a
  // cancellable callback to TriggerEvent again.
  void TriggerEvent();
  void TriggerEventWithTime(DelayCallback cb, base::TimeDelta current_time);
  void OnEngineEventFinished(base::TimeDelta delay_time);

  std::unique_ptr<MediaLog> media_log_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  // Pending callbacks.
  PipelineStatusCallback pending_seek_;
  PipelineStatusCallback pending_init_;

  // Engine implementation
  std::unique_ptr<Engine> impl_;

  // Wrapped chunk demuxer that actually does the parsing and demuxing of the
  // raw data we feed it.
  std::unique_ptr<ChunkDemuxer> chunk_demuxer_;
  base::raw_ptr<DemuxerHost> host_;

  // Updated by seek, and by updates from outgoing frames.
  base::TimeDelta media_time_ = base::Seconds(0);

  // Playback rate helps calculate how often we should check for new data.
  double current_playback_rate_ = 0.0;

  // Keeps a map of demuxer streams to their wrapper implementations which
  // can be used to set the current media time. ChunkDemuxer's streams live
  // forever due to the use of raw pointers in the pipeline, so these must
  // also live for the duration of `this` lifetime.
  base::flat_map<DemuxerStream*, std::unique_ptr<ManifestDemuxerStream>>
      streams_;

  // Flags for the two part asynchronous initialization process.
  bool demuxer_opened_ = false;
  bool engine_impl_ready_ = false;

  // Flag for the two-cb wait for finishing a seek.
  bool seek_waiting_on_engine_ = false;

  // Pending an event. Don't trigger a new event chain while one is in
  // progress.
  bool has_pending_event_ = false;

  // A pending "next event" callback, which can be canceled in the case of a
  // seek or a playback rate change.
  base::CancelableOnceClosure cancelable_next_event_;

  base::WeakPtrFactory<ManifestDemuxer> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_MANIFEST_DEMUXER_H_
