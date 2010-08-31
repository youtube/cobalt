// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_input_controller.h"
#include "media/base/limits.h"

namespace {

const int kMaxInputChannels = 2;
const int kMaxSamplesPerPacket = media::Limits::kMaxSampleRate;

}  // namespace

namespace media {

// static
AudioInputController::Factory* AudioInputController::factory_ = NULL;

AudioInputController::AudioInputController(EventHandler* handler)
    : handler_(handler),
      stream_(NULL),
      state_(kEmpty),
      thread_("AudioInputControllerThread") {
}

AudioInputController::~AudioInputController() {
  DCHECK(kClosed == state_ || kCreated == state_ || kEmpty == state_);
}

// static
scoped_refptr<AudioInputController> AudioInputController::Create(
    EventHandler* event_handler,
    AudioParameters params,
    int samples_per_packet) {
  if (!params.IsValid() ||
      (params.channels > kMaxInputChannels) ||
      (samples_per_packet > kMaxSamplesPerPacket) || (samples_per_packet < 0))
    return NULL;

  if (factory_) {
    return factory_->Create(event_handler, params, samples_per_packet);
  }

  scoped_refptr<AudioInputController> controller = new AudioInputController(
      event_handler);

  // Start the thread and post a task to create the audio input stream.
  controller->thread_.Start();
  controller->thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(controller.get(), &AudioInputController::DoCreate,
                        params, samples_per_packet));
  return controller;
}

void AudioInputController::Record() {
  DCHECK(thread_.IsRunning());
  thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioInputController::DoRecord));
}

void AudioInputController::Close() {
  if (!thread_.IsRunning()) {
    // If the thread is not running make sure we are stopped.
    DCHECK_EQ(kClosed, state_);
    return;
  }

  // Wait for all tasks to complete on the audio thread.
  thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioInputController::DoClose));
  thread_.Stop();
}

void AudioInputController::DoCreate(AudioParameters params,
                                    uint32 samples_per_packet) {
  stream_ = AudioManager::GetAudioManager()->MakeAudioInputStream(
      params, samples_per_packet);

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

  state_ = kCreated;
  handler_->OnCreated(this);
}

void AudioInputController::DoRecord() {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());

  if (state_ != kCreated)
    return;

  {
    AutoLock auto_lock(lock_);
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

  // Since the stream is closed at this point there's no other threads reading
  // |state_| so we don't need to lock.
  state_ = kClosed;
}

void AudioInputController::DoReportError(int code) {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());
  handler_->OnError(this, code);
}

void AudioInputController::OnData(AudioInputStream* stream, const uint8* data,
                                  uint32 size) {
  {
    AutoLock auto_lock(lock_);
    if (state_ != kRecording)
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
  thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioInputController::DoReportError, code));
}

}  // namespace media
