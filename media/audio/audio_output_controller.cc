// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_controller.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "build/build_config.h"
#include "media/audio/shared_memory_util.h"

using base::Time;
using base::TimeDelta;
using base::WaitableEvent;

namespace media {

// Polling-related constants.
const int AudioOutputController::kPollNumAttempts = 3;
const int AudioOutputController::kPollPauseInMilliseconds = 3;

AudioOutputController::AudioOutputController(AudioManager* audio_manager,
                                             EventHandler* handler,
                                             const AudioParameters& params,
                                             SyncReader* sync_reader)
    : audio_manager_(audio_manager),
      handler_(handler),
      stream_(NULL),
      volume_(1.0),
      state_(kEmpty),
      sync_reader_(sync_reader),
      message_loop_(audio_manager->GetMessageLoop()),
      number_polling_attempts_left_(0),
      params_(params),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_this_(this)) {
}

AudioOutputController::~AudioOutputController() {
  DCHECK_EQ(kClosed, state_);

  if (message_loop_->BelongsToCurrentThread()) {
    DoStopCloseAndClearStream(NULL);
  } else {
    // http://crbug.com/120973
    base::ThreadRestrictions::ScopedAllowWait allow_wait;
    WaitableEvent completion(true /* manual reset */,
                             false /* initial state */);
    message_loop_->PostTask(FROM_HERE,
        base::Bind(&AudioOutputController::DoStopCloseAndClearStream,
                   base::Unretained(this),
                   &completion));
    completion.Wait();
  }
}

// static
scoped_refptr<AudioOutputController> AudioOutputController::Create(
    AudioManager* audio_manager,
    EventHandler* event_handler,
    const AudioParameters& params,
    SyncReader* sync_reader) {
  DCHECK(audio_manager);
  DCHECK(sync_reader);

  if (!params.IsValid() || !audio_manager)
    return NULL;

  // Starts the audio controller thread.
  scoped_refptr<AudioOutputController> controller(new AudioOutputController(
      audio_manager, event_handler, params, sync_reader));

  controller->message_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioOutputController::DoCreate, controller));

  return controller;
}

void AudioOutputController::Play() {
  DCHECK(message_loop_);
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioOutputController::DoPlay, this));
}

void AudioOutputController::Pause() {
  DCHECK(message_loop_);
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioOutputController::DoPause, this));
}

void AudioOutputController::Flush() {
  DCHECK(message_loop_);
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioOutputController::DoFlush, this));
}

void AudioOutputController::Close(const base::Closure& closed_task) {
  DCHECK(!closed_task.is_null());
  DCHECK(message_loop_);
  message_loop_->PostTaskAndReply(FROM_HERE, base::Bind(
      &AudioOutputController::DoClose, this), closed_task);
}

void AudioOutputController::SetVolume(double volume) {
  DCHECK(message_loop_);
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioOutputController::DoSetVolume, this, volume));
}

void AudioOutputController::DoCreate() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Close() can be called before DoCreate() is executed.
  if (state_ == kClosed)
    return;
  DCHECK(state_ == kEmpty || state_ == kRecreating) << state_;

  DoStopCloseAndClearStream(NULL);
  stream_ = audio_manager_->MakeAudioOutputStreamProxy(params_);
  if (!stream_) {
    state_ = kError;

    // TODO(hclam): Define error types.
    handler_->OnError(this, 0);
    return;
  }

  if (!stream_->Open()) {
    state_ = kError;
    DoStopCloseAndClearStream(NULL);

    // TODO(hclam): Define error types.
    handler_->OnError(this, 0);
    return;
  }

  // Everything started okay, so register for state change callbacks if we have
  // not already done so.
  if (state_ != kRecreating)
    audio_manager_->AddOutputDeviceChangeListener(this);

  // We have successfully opened the stream. Set the initial volume.
  stream_->SetVolume(volume_);

  // Finally set the state to kCreated.
  State original_state = state_;
  state_ = kCreated;

  // And then report we have been created if we haven't done so already.
  if (original_state != kRecreating)
    handler_->OnCreated(this);
}

void AudioOutputController::DoPlay() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // We can start from created or paused state.
  if (state_ != kCreated && state_ != kPaused) {
    // If a pause is pending drop it.  Otherwise the controller might hang since
    // the corresponding play event has already occurred.
    if (state_ == kPausedWhenStarting)
      state_ = kStarting;
    return;
  }

  state_ = kStarting;

  // Ask for first packet.
  sync_reader_->UpdatePendingBytes(0);

  // Cannot start stream immediately, should give renderer some time
  // to deliver data.
  // TODO(vrk): The polling here and in WaitTillDataReady() is pretty clunky.
  // Refine the API such that polling is no longer needed. (crbug.com/112196)
  number_polling_attempts_left_ = kPollNumAttempts;
  message_loop_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AudioOutputController::PollAndStartIfDataReady,
      weak_this_.GetWeakPtr()),
      TimeDelta::FromMilliseconds(kPollPauseInMilliseconds));
}

void AudioOutputController::PollAndStartIfDataReady() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Being paranoid: do nothing if state unexpectedly changed.
  if ((state_ != kStarting) && (state_ != kPausedWhenStarting))
    return;

  bool pausing = (state_ == kPausedWhenStarting);
  // If we are ready to start the stream, start it.
  // Of course we may have to stop it immediately...
  if (--number_polling_attempts_left_ == 0 ||
      pausing ||
      sync_reader_->DataReady()) {
    StartStream();
    if (pausing) {
      DoPause();
    }
  } else {
    message_loop_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&AudioOutputController::PollAndStartIfDataReady,
        weak_this_.GetWeakPtr()),
        TimeDelta::FromMilliseconds(kPollPauseInMilliseconds));
  }
}

void AudioOutputController::StartStream() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  state_ = kPlaying;

  // We start the AudioOutputStream lazily.
  stream_->Start(this);

  // Tell the event handler that we are now playing.
  handler_->OnPlaying(this);
}

void AudioOutputController::DoPause() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (stream_) {
    // Then we stop the audio device. This is not the perfect solution
    // because it discards all the internal buffer in the audio device.
    // TODO(hclam): Actually pause the audio device.
    stream_->Stop();
  }

  switch (state_) {
    case kStarting:
      // We were asked to pause while starting. There is delayed task that will
      // try starting playback, and there is no way to remove that task from the
      // queue. If we stop now that task will be executed anyway.
      // Delay pausing, let delayed task to do pause after it start playback.
      state_ = kPausedWhenStarting;
      break;
    case kPlaying:
      state_ = kPaused;

      // Send a special pause mark to the low-latency audio thread.
      sync_reader_->UpdatePendingBytes(kPauseMark);

      handler_->OnPaused(this);
      break;
    default:
      return;
  }
}

void AudioOutputController::DoFlush() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // TODO(hclam): Actually flush the audio device.
}

void AudioOutputController::DoClose() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (state_ != kClosed) {
    DoStopCloseAndClearStream(NULL);
    sync_reader_->Close();
    state_ = kClosed;
  }
}

void AudioOutputController::DoSetVolume(double volume) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Saves the volume to a member first. We may not be able to set the volume
  // right away but when the stream is created we'll set the volume.
  volume_ = volume;

  switch (state_) {
    case kCreated:
    case kStarting:
    case kPausedWhenStarting:
    case kPlaying:
    case kPaused:
      stream_->SetVolume(volume_);
      break;
    default:
      return;
  }
}

void AudioOutputController::DoReportError(int code) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (state_ != kClosed)
    handler_->OnError(this, code);
}

int AudioOutputController::OnMoreData(AudioBus* dest,
                                      AudioBuffersState buffers_state) {
  return OnMoreIOData(NULL, dest, buffers_state);
}

int AudioOutputController::OnMoreIOData(AudioBus* source,
                                        AudioBus* dest,
                                        AudioBuffersState buffers_state) {
  TRACE_EVENT0("audio", "AudioOutputController::OnMoreIOData");

  {
    // Check state and do nothing if we are not playing.
    // We are on the hardware audio thread, so lock is needed.
    base::AutoLock auto_lock(lock_);
    if (state_ != kPlaying) {
      return 0;
    }
  }

  int frames = sync_reader_->Read(source, dest);
  sync_reader_->UpdatePendingBytes(
      buffers_state.total_bytes() + frames * params_.GetBytesPerFrame());
  return frames;
}

void AudioOutputController::WaitTillDataReady() {
#if defined(OS_WIN) || defined(OS_MACOSX)
  base::Time start = base::Time::Now();
  // Wait for up to 1.5 seconds for DataReady().  1.5 seconds was chosen because
  // it's larger than the playback time of the WaveOut buffer size using the
  // minimum supported sample rate: 4096 / 3000 = ~1.4 seconds.  Even a client
  // expecting real time playout should be able to fill in this time.
  const base::TimeDelta max_wait = base::TimeDelta::FromMilliseconds(1500);
  while (!sync_reader_->DataReady() &&
         ((base::Time::Now() - start) < max_wait)) {
    base::PlatformThread::YieldCurrentThread();
  }
#else
  // WaitTillDataReady() is deprecated and should not be used.
  CHECK(false);
#endif
}

void AudioOutputController::OnError(AudioOutputStream* stream, int code) {
  // Handle error on the audio controller thread.
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioOutputController::DoReportError, this, code));
}

void AudioOutputController::DoStopCloseAndClearStream(WaitableEvent* done) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Allow calling unconditionally and bail if we don't have a stream_ to close.
  if (stream_) {
    stream_->Stop();
    stream_->Close();
    stream_ = NULL;

    audio_manager_->RemoveOutputDeviceChangeListener(this);
    audio_manager_ = NULL;

    weak_this_.InvalidateWeakPtrs();
  }

  // Should be last in the method, do not touch "this" from here on.
  if (done)
    done->Signal();
}

void AudioOutputController::OnDeviceChange() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // We should always have a stream by this point.
  CHECK(stream_);

  // Preserve the original state and shutdown the stream.
  State original_state = state_;
  stream_->Stop();
  stream_->Close();
  stream_ = NULL;

  // Recreate the stream, exit if we ran into an error.
  state_ = kRecreating;
  DoCreate();
  if (!stream_ || state_ == kError)
    return;

  // Get us back to the original state or an equivalent state.
  switch (original_state) {
    case kStarting:
    case kPlaying:
      DoPlay();
      return;
    case kCreated:
    case kPausedWhenStarting:
    case kPaused:
      // From the outside these three states are equivalent.
      return;
    default:
      NOTREACHED() << "Invalid original state.";
  }
}

}  // namespace media
