// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_audio_source.h"

#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

PushableMediaStreamAudioSource::Broker::Broker(
    PushableMediaStreamAudioSource* source)
    : source_(source), main_task_runner_(source->GetTaskRunner()) {
  DCHECK(main_task_runner_);
}

void PushableMediaStreamAudioSource::Broker::OnClientStarted() {
  base::AutoLock locker(lock_);
  DCHECK_GE(num_clients_, 0);
  ++num_clients_;
}

void PushableMediaStreamAudioSource::Broker::OnClientStopped() {
  bool should_stop = false;
  {
    base::AutoLock locker(lock_);
    should_stop = --num_clients_ == 0;
    DCHECK_GE(num_clients_, 0);
  }
  if (should_stop)
    StopSource();
}

bool PushableMediaStreamAudioSource::Broker::IsRunning() {
  base::AutoLock locker(lock_);
  return is_running_;
}

void PushableMediaStreamAudioSource::Broker::PushAudioData(
    scoped_refptr<media::AudioBuffer> data) {
  base::AutoLock locker(lock_);
  if (!source_)
    return;

  source_->PushAudioData(std::move(data));
}

void PushableMediaStreamAudioSource::Broker::StopSource() {
  if (main_task_runner_->RunsTasksInCurrentSequence()) {
    StopSourceOnMain();
  } else {
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &PushableMediaStreamAudioSource::Broker::StopSourceOnMain,
            WrapRefCounted(this)));
  }
}

void PushableMediaStreamAudioSource::Broker::DeliverData(
    scoped_refptr<media::AudioBuffer> data) {
  base::AutoLock locker(lock_);
  if (!source_)
    return;

  source_->DeliverData(std::move(data));
}

void PushableMediaStreamAudioSource::Broker::OnSourceStarted() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!source_)
    return;

  base::AutoLock locker(lock_);
  is_running_ = true;
}

void PushableMediaStreamAudioSource::Broker::OnSourceDestroyedOrStopped() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  base::AutoLock locker(lock_);
  source_ = nullptr;
  is_running_ = false;
}

void PushableMediaStreamAudioSource::Broker::StopSourceOnMain() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!source_)
    return;

  source_->StopSource();
}

PushableMediaStreamAudioSource::PushableMediaStreamAudioSource(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SequencedTaskRunner> audio_task_runner)
    : MediaStreamAudioSource(std::move(main_task_runner), /* is_local */ true),
      audio_task_runner_(std::move(audio_task_runner)),
      broker_(AdoptRef(new Broker(this))) {}

PushableMediaStreamAudioSource::~PushableMediaStreamAudioSource() {
  broker_->OnSourceDestroyedOrStopped();
}

void PushableMediaStreamAudioSource::PushAudioData(
    scoped_refptr<media::AudioBuffer> data) {
  DCHECK(data);

  if (audio_task_runner_->RunsTasksInCurrentSequence()) {
    DeliverData(std::move(data));
    return;
  }

  PostCrossThreadTask(
      *audio_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&PushableMediaStreamAudioSource::Broker::DeliverData,
                          broker_, std::move(data)));
}

void PushableMediaStreamAudioSource::DeliverData(
    scoped_refptr<media::AudioBuffer> data) {
  DCHECK(audio_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(data);

  const int sample_rate = data->sample_rate();
  const int frame_count = data->frame_count();
  const int channel_count = data->channel_count();

  media::AudioParameters params = GetAudioParameters();
  if (!params.IsValid() ||
      params.format() != media::AudioParameters::AUDIO_PCM_LOW_LATENCY ||
      last_channels_ != channel_count || last_sample_rate_ != sample_rate ||
      last_frames_ != frame_count) {
    SetFormat(
        media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               media::ChannelLayoutConfig::Guess(channel_count),
                               sample_rate, frame_count));
    last_channels_ = channel_count;
    last_sample_rate_ = sample_rate;
    last_frames_ = frame_count;
  }

  // If |data|'s sample format has the same memory layout as a media::AudioBus,
  // |audio_bus| will simply wrap it. Otherwise, |data| will be copied and
  // converted into |audio_bus|.
  std::unique_ptr<media::AudioBus> audio_bus =
      media::AudioBuffer::WrapOrCopyToAudioBus(data);

  DeliverDataToTracks(*audio_bus, base::TimeTicks() + data->timestamp());
}

bool PushableMediaStreamAudioSource::EnsureSourceIsStarted() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  broker_->OnSourceStarted();
  return true;
}

void PushableMediaStreamAudioSource::EnsureSourceIsStopped() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  broker_->OnSourceDestroyedOrStopped();
}

}  // namespace blink
