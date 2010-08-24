// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_controller.h"

// The following parameters limit the request buffer and packet size from the
// renderer to avoid renderer from requesting too much memory.
static const uint32 kMegabytes = 1024 * 1024;
static const uint32 kMaxHardwareBufferSize = 2 * kMegabytes;
static const int kMaxChannels = 32;
static const int kMaxBitsPerSample = 64;
static const int kMaxSampleRate = 192000;

// Return true if the parameters for creating an audio stream is valid.
// Return false otherwise.
static bool CheckParameters(int channels, int sample_rate,
                            int bits_per_sample, uint32 hardware_buffer_size) {
  if (channels <= 0 || channels > kMaxChannels)
    return false;
  if (sample_rate <= 0 || sample_rate > kMaxSampleRate)
    return false;
  if (bits_per_sample <= 0 || bits_per_sample > kMaxBitsPerSample)
    return false;
  if (hardware_buffer_size <= 0 ||
      hardware_buffer_size > kMaxHardwareBufferSize) {
    return false;
  }
  return true;
}

namespace media {

AudioOutputController::AudioOutputController(EventHandler* handler,
                                             uint32 capacity,
                                             SyncReader* sync_reader)
    : handler_(handler),
      volume_(1.0),
      state_(kEmpty),
      hardware_pending_bytes_(0),
      buffer_capacity_(capacity),
      sync_reader_(sync_reader),
      thread_("AudioOutputControllerThread") {
}

AudioOutputController::~AudioOutputController() {
  DCHECK(kClosed == state_);
}

// static
scoped_refptr<AudioOutputController> AudioOutputController::Create(
    EventHandler* event_handler,
    AudioManager::Format format,
    int channels,
    int sample_rate,
    int bits_per_sample,
    uint32 hardware_buffer_size,
    uint32 buffer_capacity) {

  if (!CheckParameters(channels, sample_rate, bits_per_sample,
                       hardware_buffer_size))
    return NULL;

  // Starts the audio controller thread.
  scoped_refptr<AudioOutputController> controller = new AudioOutputController(
      event_handler, buffer_capacity, NULL);

  // Start the audio controller thread and post a task to create the
  // audio stream.
  controller->thread_.Start();
  controller->thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(controller.get(), &AudioOutputController::DoCreate,
                        format, channels, sample_rate, bits_per_sample,
                        hardware_buffer_size));
  return controller;
}

// static
scoped_refptr<AudioOutputController> AudioOutputController::CreateLowLatency(
    EventHandler* event_handler,
    AudioManager::Format format,
    int channels,
    int sample_rate,
    int bits_per_sample,
    uint32 hardware_buffer_size,
    SyncReader* sync_reader) {

  DCHECK(sync_reader);

  if (!CheckParameters(channels, sample_rate, bits_per_sample,
                       hardware_buffer_size))
    return NULL;

  // Starts the audio controller thread.
  scoped_refptr<AudioOutputController> controller = new AudioOutputController(
      event_handler, 0, sync_reader);

  // Start the audio controller thread and post a task to create the
  // audio stream.
  controller->thread_.Start();
  controller->thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(controller.get(), &AudioOutputController::DoCreate,
                        format, channels, sample_rate, bits_per_sample,
                        hardware_buffer_size));
  return controller;
}

void AudioOutputController::Play() {
  DCHECK(thread_.IsRunning());
  thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioOutputController::DoPlay));
}

void AudioOutputController::Pause() {
  DCHECK(thread_.IsRunning());
  thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioOutputController::DoPause));
}

void AudioOutputController::Flush() {
  DCHECK(thread_.IsRunning());
  thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioOutputController::DoFlush));
}

void AudioOutputController::Close() {
  if (!thread_.IsRunning()) {
    // If the thread is not running make sure we are stopped.
    DCHECK_EQ(kClosed, state_);
    return;
  }

  // Wait for all tasks to complete on the audio thread.
  thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioOutputController::DoClose));
  thread_.Stop();
}

void AudioOutputController::SetVolume(double volume) {
  DCHECK(thread_.IsRunning());
  thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioOutputController::DoSetVolume, volume));
}

void AudioOutputController::EnqueueData(const uint8* data, uint32 size) {
  // Write data to the push source and ask for more data if needed.
  AutoLock auto_lock(lock_);
  push_source_.Write(data, size);
  SubmitOnMoreData_Locked();
}

void AudioOutputController::DoCreate(AudioManager::Format format, int channels,
                                     int sample_rate, int bits_per_sample,
                                     uint32 hardware_buffer_size) {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());
  DCHECK_EQ(kEmpty, state_);

  // Create the stream in the first place.
  stream_ = AudioManager::GetAudioManager()->MakeAudioOutputStream(
      format, channels, sample_rate, bits_per_sample);

  if (!stream_) {
    // TODO(hclam): Define error types.
    handler_->OnError(this, 0);
    return;
  }

  if (stream_ && !stream_->Open(hardware_buffer_size)) {
    stream_->Close();
    stream_ = NULL;

    // TODO(hclam): Define error types.
    handler_->OnError(this, 0);
    return;
  }
  // We have successfully opened the stream. Set the initial volume.
  stream_->SetVolume(volume_);

  // Finally set the state to kCreated.
  state_ = kCreated;

  // And then report we have been created.
  handler_->OnCreated(this);

  // If in normal latency mode then start buffering.
  if (!LowLatencyMode()) {
    AutoLock auto_lock(lock_);
    SubmitOnMoreData_Locked();
  }
}

void AudioOutputController::DoPlay() {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());

  // We can start from created or paused state.
  if (state_ != kCreated && state_ != kPaused)
    return;

  State old_state;
  // Update the |state_| to kPlaying.
  {
    AutoLock auto_lock(lock_);
    old_state = state_;
    state_ = kPlaying;
  }

  // We start the AudioOutputStream lazily.
  stream_->Start(this);

  // Tell the event handler that we are now playing.
  handler_->OnPlaying(this);
}

void AudioOutputController::DoPause() {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());

  // We can pause from started state.
  if (state_ != kPlaying)
    return;

  // Sets the |state_| to kPaused so we don't draw more audio data.
  {
    AutoLock auto_lock(lock_);
    state_ = kPaused;
  }

  // Then we stop the audio device. This is not the perfect solution because
  // it discards all the internal buffer in the audio device.
  // TODO(hclam): Actually pause the audio device.
  stream_->Stop();

  handler_->OnPaused(this);
}

void AudioOutputController::DoFlush() {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());

  if (state_ != kPaused)
    return;

  // TODO(hclam): Actually flush the audio device.

  // If we are in the regular latency mode then flush the push source.
  if (!sync_reader_) {
    AutoLock auto_lock(lock_);
    push_source_.ClearAll();
  }
}

void AudioOutputController::DoClose() {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());
  DCHECK_NE(kClosed, state_);

  // |stream_| can be null if creating the device failed in DoCreate().
  if (stream_) {
    stream_->Stop();
    stream_->Close();
    // After stream is closed it is destroyed, so don't keep a reference to it.
    stream_ = NULL;
  }

  // Update the current state. Since the stream is closed at this point
  // there's no other threads reading |state_| so we don't need to lock.
  state_ = kClosed;
}

void AudioOutputController::DoSetVolume(double volume) {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());

  // Saves the volume to a member first. We may not be able to set the volume
  // right away but when the stream is created we'll set the volume.
  volume_ = volume;

  if (state_ != kPlaying && state_ != kPaused && state_ != kCreated)
    return;

  stream_->SetVolume(volume_);
}

void AudioOutputController::DoReportError(int code) {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());
  handler_->OnError(this, code);
}

uint32 AudioOutputController::OnMoreData(AudioOutputStream* stream,
                                         void* dest,
                                         uint32 max_size,
                                         uint32 pending_bytes) {
  // If regular latency mode is used.
  if (!sync_reader_) {
    AutoLock auto_lock(lock_);

    // Record the callback time.
    last_callback_time_ = base::Time::Now();

    if (state_ != kPlaying) {
      // Don't read anything. Save the number of bytes in the hardware buffer.
      hardware_pending_bytes_ = pending_bytes;
      return 0;
    }

    // Push source doesn't need to know the stream and number of pending bytes.
    // So just pass in NULL and 0.
    uint32 size = push_source_.OnMoreData(NULL, dest, max_size, 0);
    hardware_pending_bytes_ = pending_bytes + size;
    SubmitOnMoreData_Locked();
    return size;
  }

  // Low latency mode.
  uint32 size =  sync_reader_->Read(dest, max_size);
  sync_reader_->UpdatePendingBytes(pending_bytes + size);
  return size;
}

void AudioOutputController::OnClose(AudioOutputStream* stream) {
  // Push source doesn't need to know the stream so just pass in NULL.
  if (LowLatencyMode()) {
    sync_reader_->Close();
  } else {
    AutoLock auto_lock(lock_);
    push_source_.OnClose(NULL);
  }
}

void AudioOutputController::OnError(AudioOutputStream* stream, int code) {
  // Handle error on the audio controller thread.
  thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioOutputController::DoReportError, code));
}

void AudioOutputController::SubmitOnMoreData_Locked() {
  lock_.AssertAcquired();

  if (push_source_.UnProcessedBytes() > buffer_capacity_)
    return;

  base::Time timestamp = last_callback_time_;
  uint32 pending_bytes = hardware_pending_bytes_ +
      push_source_.UnProcessedBytes();

  // If we need more data then call the event handler to ask for more data.
  // It is okay that we don't lock in this block because the parameters are
  // correct and in the worst case we are just asking more data than needed.
  AutoUnlock auto_unlock(lock_);
  handler_->OnMoreData(this, timestamp, pending_bytes);
}

}  // namespace media
