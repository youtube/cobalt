// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_input_controller.h"

namespace {

const int kMaxSampleRate = 192000;
const int kMaxBitsPerSample = 64;
const int kMaxInputChannels = 2;
const int kMaxSamplesPerPacket = kMaxSampleRate;

}  // namespace

namespace media {

AudioInputController::AudioInputController(EventHandler* handler)
    : handler_(handler),
      state_(kEmpty),
      thread_("AudioInputControllerThread") {
}

AudioInputController::~AudioInputController() {
  DCHECK(kClosed == state_ || kCreated == state_ || kEmpty == state_);
}

// static
scoped_refptr<AudioInputController> AudioInputController::Create(
    EventHandler* event_handler,
    AudioManager::Format format,
    int channels,
    int sample_rate,
    int bits_per_sample,
    int samples_per_packet) {
  if ((channels > kMaxInputChannels) || (channels <= 0) ||
      (sample_rate > kMaxSampleRate) || (sample_rate <= 0) ||
      (bits_per_sample > kMaxBitsPerSample) || (bits_per_sample <= 0) ||
      (samples_per_packet > kMaxSamplesPerPacket) || (samples_per_packet < 0))
    return NULL;

  scoped_refptr<AudioInputController> controller = new AudioInputController(
      event_handler);

  // Start the thread and post a task to create the audio input stream.
  controller->thread_.Start();
  controller->thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(controller.get(), &AudioInputController::DoCreate,
                        format, channels, sample_rate, bits_per_sample,
                        samples_per_packet));
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

void AudioInputController::DoCreate(AudioManager::Format format, int channels,
                                    int sample_rate, int bits_per_sample,
                                    uint32 samples_per_packet) {
  stream_ = AudioManager::GetAudioManager()->MakeAudioInputStream(
      format, channels, sample_rate, bits_per_sample, samples_per_packet);

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
}

void AudioInputController::OnError(AudioInputStream* stream, int code) {
  // Handle error on the audio controller thread.
  thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioInputController::DoReportError, code));
}

}  // namespace media
