// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_input_controller.h"

#include "base/bind.h"
#include "base/threading/thread_restrictions.h"
#include "media/base/limits.h"

namespace {
const int kMaxInputChannels = 2;
const int kTimerResetInterval = 1; // One second.
}

namespace media {

// static
AudioInputController::Factory* AudioInputController::factory_ = NULL;

AudioInputController::AudioInputController(EventHandler* handler,
                                           SyncWriter* sync_writer)
    : handler_(handler),
      stream_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(no_data_timer_(FROM_HERE,
          base::TimeDelta::FromSeconds(kTimerResetInterval),
          this,
          &AudioInputController::DoReportNoDataError)),
      state_(kEmpty),
      thread_("AudioInputControllerThread"),
      sync_writer_(sync_writer) {
}

AudioInputController::~AudioInputController() {
  DCHECK(kClosed == state_ || kCreated == state_ || kEmpty == state_);
}

// static
scoped_refptr<AudioInputController> AudioInputController::Create(
    EventHandler* event_handler,
    const AudioParameters& params) {
  if (!params.IsValid() || (params.channels > kMaxInputChannels))
    return NULL;

  if (factory_) {
    return factory_->Create(event_handler, params);
  }

  scoped_refptr<AudioInputController> controller(new AudioInputController(
      event_handler, NULL));

  // Start the thread and post a task to create the audio input stream.
  // Pass an empty string to indicate using default device.
  std::string device_id = AudioManagerBase::kDefaultDeviceId;
  controller->thread_.Start();
  controller->thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &AudioInputController::DoCreate, controller.get(),
      params, device_id));
  return controller;
}

// static
scoped_refptr<AudioInputController> AudioInputController::CreateLowLatency(
    EventHandler* event_handler,
    const AudioParameters& params,
    const std::string& device_id,
    SyncWriter* sync_writer) {
  DCHECK(sync_writer);

  if (!params.IsValid() || (params.channels > kMaxInputChannels))
    return NULL;

  if (!AudioManager::GetAudioManager())
    return NULL;

  // Starts the audio controller thread.
  scoped_refptr<AudioInputController> controller(new AudioInputController(
      event_handler, sync_writer));

  // Start the thread and post a task to create the audio input stream.
  controller->thread_.Start();
  controller->thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &AudioInputController::DoCreate, controller.get(), params, device_id));
  return controller;
}

void AudioInputController::Record() {
  DCHECK(thread_.IsRunning());
  thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &AudioInputController::DoRecord, this));
}

void AudioInputController::Close() {
  if (!thread_.IsRunning()) {
    // If the thread is not running make sure we are stopped.
    DCHECK_EQ(kClosed, state_);
    return;
  }

  // Wait for all tasks to complete on the audio thread.
  thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &AudioInputController::DoClose, this));

  // A ScopedAllowIO object is required to join the thread when calling Stop.
  // This is because as joining threads may be a long operation it's now
  // not allowed in threads without IO access, which is the case of the IO
  // thread (it is missnamed) being used here. This object overrides
  // temporarily this restriction and should be used only in specific
  // infrequent cases where joining is guaranteed to be fast.
  // Bug: http://code.google.com/p/chromium/issues/detail?id=67806
  base::ThreadRestrictions::ScopedAllowIO allow_io_for_thread_join;
  thread_.Stop();
}

void AudioInputController::DoCreate(const AudioParameters& params,
                                    const std::string& device_id) {
  stream_ = AudioManager::GetAudioManager()->MakeAudioInputStream(params,
                                                                  device_id);

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

  thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &AudioInputController::DoResetNoDataTimer, this));
  state_ = kCreated;
  handler_->OnCreated(this);
}

void AudioInputController::DoRecord() {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());

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
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());
  DCHECK_NE(kClosed, state_);

  // |stream_| can be null if creating the device failed in DoCreate().
  if (stream_) {
    stream_->Stop();
    stream_->Close();
    // After stream is closed it is destroyed, so don't keep a reference to it.
    stream_ = NULL;
  }

  if (LowLatencyMode()) {
    sync_writer_->Close();
  }

  // Since the stream is closed at this point there's no other threads reading
  // |state_| so we don't need to lock.
  state_ = kClosed;
}

void AudioInputController::DoReportError(int code) {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());
  handler_->OnError(this, code);
}

void AudioInputController::DoReportNoDataError() {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());
  handler_->OnError(this, 0);
}

void AudioInputController::DoResetNoDataTimer() {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());
  no_data_timer_.Reset();
}

void AudioInputController::OnData(AudioInputStream* stream, const uint8* data,
                                  uint32 size, uint32 hardware_delay_bytes) {
  {
    base::AutoLock auto_lock(lock_);
    if (state_ != kRecording)
      return;
  }

  thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
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
  // TODO(satish): Sometimes the device driver closes the input stream without
  // us asking for it (may be if the device was unplugged?). Check how to handle
  // such cases here.
}

void AudioInputController::OnError(AudioInputStream* stream, int code) {
  // Handle error on the audio controller thread.
  thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &AudioInputController::DoReportError, this, code));
}

}  // namespace media
