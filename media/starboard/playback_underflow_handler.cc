// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "media/starboard/playback_underflow_handler.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/to_string.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "media/starboard/playback_underflow_detector.h"

namespace media {

namespace {

// How often to check for underflow while presenting.
constexpr base::TimeDelta kCheckInterval = base::Milliseconds(100);

// How much written data every stream must have ahead of the playback position
// before playback resumes after an underflow.
constexpr base::TimeDelta kResumeThreshold = base::Milliseconds(500);

// A stream with less written data than this ahead of the playback position,
// while waiting on the demuxer, is starved. Non-zero because the clock may stop
// slightly before the end of the written data (e.g. on audio starvation).
constexpr base::TimeDelta kUnderflowMargin = base::Milliseconds(100);

// How long a video-only starvation is tolerated while audio still has data.
// Matches RendererImpl's default video underflow threshold.
constexpr base::TimeDelta kVideoUnderflowDelay = base::Seconds(3);

// While in underflow, a stream with no read pending and no data written for
// this long is considered full, so playback resumes.
constexpr base::TimeDelta kSaturationDelay = base::Seconds(1);

// Pauses and resumes playback on underflow. See PlaybackUnderflowHandler.
class PlaybackUnderflowHandlerImpl : public PlaybackUnderflowHandler {
 public:
  PlaybackUnderflowHandlerImpl(
      Delegate* delegate,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  PlaybackUnderflowHandlerImpl(const PlaybackUnderflowHandlerImpl&) = delete;
  PlaybackUnderflowHandlerImpl& operator=(const PlaybackUnderflowHandlerImpl&) =
      delete;

  ~PlaybackUnderflowHandlerImpl() override;

  // PlaybackUnderflowHandler implementation.
  void OnStartPlayingFrom(bool has_audio, bool has_video) override;
  void OnPresenting() override;
  void OnEndOfStream() override;
  void OnFlush() override;
  void OnBuffersWritten(
      DemuxerStream::Type type,
      const DemuxerStream::DecoderBufferVector& buffers) override;
  bool IsPausedForUnderflow() const override;

 private:
  void StartMonitoring();
  void StopMonitoring();
  void OnCheckTimer();
  void PauseForUnderflow(base::TimeDelta media_time);
  void ResumeFromUnderflow(base::TimeDelta media_time);
  PlaybackUnderflowDetector::ReadState GetReadState(DemuxerStream::Type type);
  base::TimeTicks& GetLastWriteTime(DemuxerStream::Type type);

  const raw_ptr<Delegate> delegate_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  PlaybackUnderflowDetector detector_;
  base::RepeatingTimer check_timer_;
  bool has_audio_ = false;
  bool has_video_ = false;
  // When data was last written for each stream.
  base::TimeTicks last_audio_write_time_;
  base::TimeTicks last_video_write_time_;
};

PlaybackUnderflowHandlerImpl::PlaybackUnderflowHandlerImpl(
    Delegate* delegate,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : delegate_(delegate),
      task_runner_(std::move(task_runner)),
      detector_({.resume_threshold = kResumeThreshold,
                 .underflow_margin = kUnderflowMargin,
                 .video_underflow_delay = kVideoUnderflowDelay,
                 .saturation_delay = kSaturationDelay}) {
  CHECK(delegate_);
  CHECK(task_runner_);
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  check_timer_.SetTaskRunner(task_runner_);
  LOG(INFO) << "PlaybackUnderflowHandlerImpl created: playback is auto-paused "
               "on underflow and resumes once every stream has "
            << kResumeThreshold
            << " of data ahead (check_interval=" << kCheckInterval
            << ", underflow_margin=" << kUnderflowMargin
            << ", video_underflow_delay=" << kVideoUnderflowDelay
            << ", saturation_delay=" << kSaturationDelay << ").";
}

PlaybackUnderflowHandlerImpl::~PlaybackUnderflowHandlerImpl() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  StopMonitoring();
}

void PlaybackUnderflowHandlerImpl::OnStartPlayingFrom(bool has_audio,
                                                      bool has_video) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  has_audio_ = has_audio;
  has_video_ = has_video;
  detector_.Reset(has_audio_, has_video_);
  last_audio_write_time_ = base::TimeTicks();
  last_video_write_time_ = base::TimeTicks();
}

void PlaybackUnderflowHandlerImpl::OnPresenting() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  if (detector_.in_underflow()) {
    // Defensive: presenting again while paused for underflow can only happen
    // if the SbPlayer is ever re-prerolled without a Flush(). The renderer has
    // already reported BUFFERING_HAVE_ENOUGH, so leave underflow and restore
    // the app's playback rate on the SbPlayer.
    LOG(INFO) << "PlaybackUnderflowHandler: presenting while paused for "
                 "underflow, resuming playback.";
    detector_.ClearUnderflow();
    delegate_->ResumeSbPlayer();
  }
  StartMonitoring();
}

void PlaybackUnderflowHandlerImpl::OnEndOfStream() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  if (detector_.in_underflow()) {
    // Defensive: playback ended while paused for underflow (e.g. all
    // remaining frames were already late). Leave underflow so that the
    // buffering state and the app's playback rate aren't left stale.
    LOG(WARNING) << "PlaybackUnderflowHandler: end of stream while paused for "
                    "underflow, leaving underflow.";
    detector_.ClearUnderflow();
    delegate_->ReportBufferingState(BUFFERING_HAVE_ENOUGH,
                                    BUFFERING_CHANGE_REASON_UNKNOWN);
    delegate_->ResumeSbPlayer();
  }
  StopMonitoring();
}

void PlaybackUnderflowHandlerImpl::OnFlush() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  StopMonitoring();
  // Leave underflow so that rate changes from the app are applied again.
  // The renderer reports BUFFERING_HAVE_NOTHING for the flush itself.
  detector_.Reset(has_audio_, has_video_);
}

void PlaybackUnderflowHandlerImpl::OnBuffersWritten(
    DemuxerStream::Type type,
    const DemuxerStream::DecoderBufferVector& buffers) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  detector_.OnBuffersWritten(type, buffers);
  GetLastWriteTime(type) = base::TimeTicks::Now();
}

bool PlaybackUnderflowHandlerImpl::IsPausedForUnderflow() const {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  return detector_.in_underflow();
}

void PlaybackUnderflowHandlerImpl::StartMonitoring() {
  if (check_timer_.IsRunning()) {
    return;
  }
  // Unretained is safe: |check_timer_| is owned by this object and stops on
  // destruction.
  check_timer_.Start(
      FROM_HERE, kCheckInterval,
      base::BindRepeating(&PlaybackUnderflowHandlerImpl::OnCheckTimer,
                          base::Unretained(this)));
}

void PlaybackUnderflowHandlerImpl::StopMonitoring() {
  check_timer_.Stop();
}

void PlaybackUnderflowHandlerImpl::OnCheckTimer() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  // Only look for a new underflow while the app is playing: a player paused by
  // the app is never treated as underflowing. An ongoing underflow is still
  // evaluated while the app is paused, so that it can end. The video underflow
  // delay is cancelled whenever playback isn't checked, so that the time spent
  // paused or inactive doesn't count towards it.
  if (!delegate_->IsPlaybackActive() ||
      (!detector_.in_underflow() && delegate_->IsPausedByApp())) {
    detector_.CancelVideoDeferral();
    return;
  }

  const base::TimeDelta media_time = delegate_->GetCurrentMediaTime();
  switch (detector_.Update(base::TimeTicks::Now(), media_time,
                           GetReadState(DemuxerStream::AUDIO),
                           GetReadState(DemuxerStream::VIDEO))) {
    case PlaybackUnderflowDetector::Transition::kNone:
      break;
    case PlaybackUnderflowDetector::Transition::kUnderflowStarted:
      PauseForUnderflow(media_time);
      break;
    case PlaybackUnderflowDetector::Transition::kUnderflowEnded:
      ResumeFromUnderflow(media_time);
      break;
  }
}

void PlaybackUnderflowHandlerImpl::PauseForUnderflow(
    base::TimeDelta media_time) {
  LOG(INFO) << "PlaybackUnderflowHandler detected underflow at " << media_time
            << " (" << detector_.GetDebugString() << ", audio_read_pending="
            << base::ToString(delegate_->IsReadPending(DemuxerStream::AUDIO))
            << ", video_read_pending="
            << base::ToString(delegate_->IsReadPending(DemuxerStream::VIDEO))
            << "), pausing playback.";
  // Stop the media clock. The app's rate is restored in ResumeFromUnderflow().
  delegate_->PauseSbPlayer();
  // Underflow only starts after the demuxer failed to provide data in time (a
  // read was pending when the stream fell behind).
  delegate_->ReportBufferingState(BUFFERING_HAVE_NOTHING, DEMUXER_UNDERFLOW);
}

void PlaybackUnderflowHandlerImpl::ResumeFromUnderflow(
    base::TimeDelta media_time) {
  LOG(INFO) << "PlaybackUnderflowHandler recovered from underflow at "
            << media_time << " (" << detector_.GetDebugString()
            << ", paused_by_app=" << base::ToString(delegate_->IsPausedByApp())
            << "), resuming playback.";
  // Report first, so that the renderer's buffering state is HAVE_ENOUGH as
  // soon as possible after leaving underflow.
  delegate_->ReportBufferingState(BUFFERING_HAVE_ENOUGH,
                                  BUFFERING_CHANGE_REASON_UNKNOWN);
  delegate_->ResumeSbPlayer();
}

PlaybackUnderflowDetector::ReadState PlaybackUnderflowHandlerImpl::GetReadState(
    DemuxerStream::Type type) {
  return {.read_pending = delegate_->IsReadPending(type),
          .last_write_time = GetLastWriteTime(type)};
}

base::TimeTicks& PlaybackUnderflowHandlerImpl::GetLastWriteTime(
    DemuxerStream::Type type) {
  switch (type) {
    case DemuxerStream::AUDIO:
      return last_audio_write_time_;
    case DemuxerStream::VIDEO:
      return last_video_write_time_;
    default:
      NOTREACHED();
  }
}

// Used when underflow handling is disabled: does nothing, and never reports
// being paused for underflow.
class NullPlaybackUnderflowHandler : public PlaybackUnderflowHandler {
 public:
  NullPlaybackUnderflowHandler() {
    LOG(INFO) << "NullPlaybackUnderflowHandler created: playback is not "
                 "auto-paused on underflow (default; enable with "
                 "Media.HandlePlaybackUnderflow).";
  }

  NullPlaybackUnderflowHandler(const NullPlaybackUnderflowHandler&) = delete;
  NullPlaybackUnderflowHandler& operator=(const NullPlaybackUnderflowHandler&) =
      delete;

  ~NullPlaybackUnderflowHandler() override = default;

  // PlaybackUnderflowHandler implementation.
  void OnStartPlayingFrom(bool has_audio, bool has_video) override {}
  void OnPresenting() override {}
  void OnEndOfStream() override {}
  void OnFlush() override {}
  void OnBuffersWritten(
      DemuxerStream::Type type,
      const DemuxerStream::DecoderBufferVector& buffers) override {}
  bool IsPausedForUnderflow() const override { return false; }
};

}  // namespace

// static
std::unique_ptr<PlaybackUnderflowHandler> PlaybackUnderflowHandler::Create(
    bool enabled,
    Delegate* delegate,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  if (!enabled) {
    return std::make_unique<NullPlaybackUnderflowHandler>();
  }
  return std::make_unique<PlaybackUnderflowHandlerImpl>(delegate,
                                                        std::move(task_runner));
}

}  // namespace media
