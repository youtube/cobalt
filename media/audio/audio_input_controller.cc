// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_input_controller.h"

#include "base/bind.h"
#include "base/threading/thread_restrictions.h"
#include "media/base/limits.h"

namespace {
const int kMaxInputChannels = 2;
const int kTimerResetInterval = 1;  // One second.
}

namespace media {

// static
AudioInputController::Factory* AudioInputController::factory_ = NULL;

AudioInputController::AudioInputController(EventHandler* handler,
                                           SyncWriter* sync_writer)
    : creator_loop_(base::MessageLoopProxy::current()),
      handler_(handler),
      stream_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(no_data_timer_(FROM_HERE,
          base::TimeDelta::FromSeconds(kTimerResetInterval),
          this,
          &AudioInputController::DoReportNoDataError)),
      state_(kEmpty),
      sync_writer_(sync_writer) {
  DCHECK(creator_loop_);
}

AudioInputController::~AudioInputController() {
  DCHECK(kClosed == state_ || kCreated == state_ || kEmpty == state_);
}

// static
scoped_refptr<AudioInputController> AudioInputController::Create(
    AudioManager* audio_manager,
    EventHandler* event_handler,
    const AudioParameters& params) {
  DCHECK(audio_manager);

  if (!params.IsValid() || (params.channels() > kMaxInputChannels))
    return NULL;

  if (factory_)
    return factory_->Create(audio_manager, event_handler, params);

  scoped_refptr<AudioInputController> controller(new AudioInputController(
      event_handler, NULL));

  controller->message_loop_ = audio_manager->GetMessageLoop();

  // Create and open a new audio input stream from the existing
  // audio-device thread. Use the default audio-input device.
  std::string device_id = AudioManagerBase::kDefaultDeviceId;
  if (!controller->message_loop_->PostTask(FROM_HERE,
          base::Bind(&AudioInputController::DoCreate, controller,
                     base::Unretained(audio_manager), params, device_id))) {
    controller = NULL;
  }

  return controller;
}

// static
scoped_refptr<AudioInputController> AudioInputController::CreateLowLatency(
    AudioManager* audio_manager,
    EventHandler* event_handler,
    const AudioParameters& params,
    const std::string& device_id,
    SyncWriter* sync_writer) {
  DCHECK(audio_manager);
  DCHECK(sync_writer);

  if (!params.IsValid() || (params.channels() > kMaxInputChannels))
    return NULL;

  // Create the AudioInputController object and ensure that it runs on
  // the audio-manager thread.
  scoped_refptr<AudioInputController> controller(new AudioInputController(
      event_handler, sync_writer));
  controller->message_loop_ = audio_manager->GetMessageLoop();

  // Create and open a new audio input stream from the existing
  // audio-device thread. Use the provided audio-input device.
  if (!controller->message_loop_->PostTask(FROM_HERE,
          base::Bind(&AudioInputController::DoCreate, controller,
                     base::Unretained(audio_manager), params, device_id))) {
    controller = NULL;
  }

  return controller;
}

void AudioInputController::Record() {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioInputController::DoRecord, this));
}

void AudioInputController::Close(const base::Closure& closed_task) {
  DCHECK(!closed_task.is_null());
  message_loop_->PostTaskAndReply(
      FROM_HERE, base::Bind(&AudioInputController::DoClose, this), closed_task);
}

void AudioInputController::DoCreate(AudioManager* audio_manager,
                                    const AudioParameters& params,
                                    const std::string& device_id) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  stream_ = audio_manager->MakeAudioInputStream(params, device_id);

  if (!stream_) {
    // TODO(satish): Define error types.
    handler_->OnError(this, 0);
    return;
  }

  if (stream_ && !stream_->Open()) {
    stream_->Close();
    stream_ = NULL;
    // TODO(satish): Define error types.
    handler_->OnError(this, 0);
    return;
  }

  creator_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioInputController::DoResetNoDataTimer, this));

  state_ = kCreated;
  handler_->OnCreated(this);
}

void AudioInputController::DoRecord() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (state_ != kCreated)
    return;

  {
    base::AutoLock auto_lock(lock_);
    state_ = kRecording;
  }

  stream_->Start(this);
  handler_->OnRecording(this);
}

void AudioInputController::DoClose() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (state_ != kClosed) {
    DoStopCloseAndClearStream(NULL);

    if (LowLatencyMode()) {
      sync_writer_->Close();
    }

    state_ = kClosed;
  }
}

void AudioInputController::DoReportError(int code) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  handler_->OnError(this, code);
}

void AudioInputController::DoReportNoDataError() {
  DCHECK(creator_loop_->BelongsToCurrentThread());

  // Error notifications should be sent on the audio-manager thread.
  int code = 0;
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioInputController::DoReportError, this, code));
}

void AudioInputController::DoResetNoDataTimer() {
  DCHECK(creator_loop_->BelongsToCurrentThread());
  no_data_timer_.Reset();
}

void AudioInputController::OnData(AudioInputStream* stream, const uint8* data,
                                  uint32 size, uint32 hardware_delay_bytes) {
  {
    base::AutoLock auto_lock(lock_);
    if (state_ != kRecording)
      return;
  }

  creator_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioInputController::DoResetNoDataTimer, this));

  // Use SyncSocket if we are in a low-latency mode.
  if (LowLatencyMode()) {
    sync_writer_->Write(data, size);
    sync_writer_->UpdateRecordedBytes(hardware_delay_bytes);
    return;
  }

  handler_->OnData(this, data, size);
}

void AudioInputController::OnClose(AudioInputStream* stream) {
  DVLOG(1) << "AudioInputController::OnClose()";
  // TODO(satish): Sometimes the device driver closes the input stream without
  // us asking for it (may be if the device was unplugged?). Check how to handle
  // such cases here.
}

void AudioInputController::OnError(AudioInputStream* stream, int code) {
  // Handle error on the audio-manager thread.
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &AudioInputController::DoReportError, this, code));
}

void AudioInputController::DoStopCloseAndClearStream(
    base::WaitableEvent *done) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Allow calling unconditionally and bail if we don't have a stream to close.
  if (stream_ != NULL) {
    stream_->Stop();
    stream_->Close();
    stream_ = NULL;
  }

  // Should be last in the method, do not touch "this" from here on.
  if (done != NULL)
    done->Signal();
}

}  // namespace media
