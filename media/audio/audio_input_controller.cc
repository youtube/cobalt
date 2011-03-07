// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_input_controller.h"

#include "base/threading/thread_restrictions.h"
#include "media/base/limits.h"

namespace media {

static const int kMaxInputChannels = 2;

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
    AudioParameters params) {
  if (!params.IsValid() || (params.channels > kMaxInputChannels))
    return NULL;

  if (factory_) {
    return factory_->Create(event_handler, params);
  }

  scoped_refptr<AudioInputController> controller(new AudioInputController(
      event_handler));

  // Start the thread and post a task to create the audio input stream.
  controller->thread_.Start();
  controller->thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(controller.get(), &AudioInputController::DoCreate,
                        params));
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

void AudioInputController::DoCreate(AudioParameters params) {
  stream_ = AudioManager::GetAudioManager()->MakeAudioInputStream(params);

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
    base::AutoLock auto_lock(lock_);
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
