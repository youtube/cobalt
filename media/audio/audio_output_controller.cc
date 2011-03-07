// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_controller.h"

#include "base/message_loop.h"

namespace media {

// Signal a pause in low-latency mode.
static const int kPauseMark = -1;

AudioOutputController::AudioOutputController(EventHandler* handler,
                                             uint32 capacity,
                                             SyncReader* sync_reader)
    : handler_(handler),
      stream_(NULL),
      volume_(1.0),
      state_(kEmpty),
      buffer_(0, capacity),
      pending_request_(false),
      sync_reader_(sync_reader),
      message_loop_(NULL) {
}

AudioOutputController::~AudioOutputController() {
  DCHECK(kClosed == state_);
}

// static
scoped_refptr<AudioOutputController> AudioOutputController::Create(
    EventHandler* event_handler,
    AudioParameters params,
    uint32 buffer_capacity) {

  if (!params.IsValid())
    return NULL;

  if (!AudioManager::GetAudioManager())
    return NULL;

  // Starts the audio controller thread.
  scoped_refptr<AudioOutputController> controller(new AudioOutputController(
      event_handler, buffer_capacity, NULL));

  controller->message_loop_ =
      AudioManager::GetAudioManager()->GetMessageLoop();
  controller->message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(controller.get(), &AudioOutputController::DoCreate,
                        params));
  return controller;
}

// static
scoped_refptr<AudioOutputController> AudioOutputController::CreateLowLatency(
    EventHandler* event_handler,
    AudioParameters params,
    SyncReader* sync_reader) {

  DCHECK(sync_reader);

  if (!params.IsValid())
    return NULL;

  if (!AudioManager::GetAudioManager())
    return NULL;

  // Starts the audio controller thread.
  scoped_refptr<AudioOutputController> controller(new AudioOutputController(
      event_handler, 0, sync_reader));

  controller->message_loop_ =
      AudioManager::GetAudioManager()->GetMessageLoop();
  controller->message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(controller.get(), &AudioOutputController::DoCreate,
                        params));
  return controller;
}

void AudioOutputController::Play() {
  DCHECK(message_loop_);
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioOutputController::DoPlay));
}

void AudioOutputController::Pause() {
  DCHECK(message_loop_);
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioOutputController::DoPause));
}

void AudioOutputController::Flush() {
  DCHECK(message_loop_);
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioOutputController::DoFlush));
}

void AudioOutputController::Close(Task* closed_task) {
  DCHECK(closed_task);
  DCHECK(message_loop_);
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioOutputController::DoClose, closed_task));
}

void AudioOutputController::SetVolume(double volume) {
  DCHECK(message_loop_);
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioOutputController::DoSetVolume, volume));
}

void AudioOutputController::EnqueueData(const uint8* data, uint32 size) {
  // Write data to the push source and ask for more data if needed.
  base::AutoLock auto_lock(lock_);
  pending_request_ = false;
  // If |size| is set to 0, it indicates that the audio source doesn't have
  // more data right now, and so it doesn't make sense to send additional
  // request.
  if (size) {
    buffer_.Append(data, size);
    SubmitOnMoreData_Locked();
  }
}

void AudioOutputController::DoCreate(AudioParameters params) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Close() can be called before DoCreate() is executed.
  if (state_ == kClosed)
    return;
  DCHECK(state_ == kEmpty);

  if (!AudioManager::GetAudioManager())
    return;

  stream_ = AudioManager::GetAudioManager()->MakeAudioOutputStreamProxy(params);
  if (!stream_) {
    // TODO(hclam): Define error types.
    handler_->OnError(this, 0);
    return;
  }

  if (!stream_->Open()) {
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
    base::AutoLock auto_lock(lock_);
    SubmitOnMoreData_Locked();
  }
}

void AudioOutputController::DoPlay() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // We can start from created or paused state.
  if (state_ != kCreated && state_ != kPaused)
    return;
  state_ = kPlaying;

  // We start the AudioOutputStream lazily.
  stream_->Start(this);

  // Tell the event handler that we are now playing.
  handler_->OnPlaying(this);
}

void AudioOutputController::DoPause() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // We can pause from started state.
  if (state_ != kPlaying)
    return;
  state_ = kPaused;

  // Then we stop the audio device. This is not the perfect solution because
  // it discards all the internal buffer in the audio device.
  // TODO(hclam): Actually pause the audio device.
  stream_->Stop();

  if (LowLatencyMode()) {
    // Send a special pause mark to the low-latency audio thread.
    sync_reader_->UpdatePendingBytes(kPauseMark);
  }

  handler_->OnPaused(this);
}

void AudioOutputController::DoFlush() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // TODO(hclam): Actually flush the audio device.

  // If we are in the regular latency mode then flush the push source.
  if (!sync_reader_) {
    if (state_ != kPaused)
      return;
    base::AutoLock auto_lock(lock_);
    buffer_.Clear();
  }
}

void AudioOutputController::DoClose(Task* closed_task) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (state_ != kClosed) {
    // |stream_| can be null if creating the device failed in DoCreate().
    if (stream_) {
      stream_->Stop();
      stream_->Close();
      // After stream is closed it is destroyed, so don't keep a reference to
      // it.
      stream_ = NULL;
    }

    if (LowLatencyMode()) {
      sync_reader_->Close();
    }

    state_ = kClosed;
  }

  closed_task->Run();
  delete closed_task;
}

void AudioOutputController::DoSetVolume(double volume) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Saves the volume to a member first. We may not be able to set the volume
  // right away but when the stream is created we'll set the volume.
  volume_ = volume;

  if (state_ != kPlaying && state_ != kPaused && state_ != kCreated)
    return;

  stream_->SetVolume(volume_);
}

void AudioOutputController::DoReportError(int code) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  if (state_ != kClosed)
    handler_->OnError(this, code);
}

uint32 AudioOutputController::OnMoreData(
    AudioOutputStream* stream, uint8* dest,
    uint32 max_size, AudioBuffersState buffers_state) {
  // If regular latency mode is used.
  if (!sync_reader_) {
    base::AutoLock auto_lock(lock_);

    // Save current buffers state.
    buffers_state_ = buffers_state;

    if (state_ != kPlaying) {
      // Don't read anything. Save the number of bytes in the hardware buffer.
      return 0;
    }

    uint32 size = buffer_.Read(dest, max_size);
    buffers_state_.pending_bytes += size;
    SubmitOnMoreData_Locked();
    return size;
  }

  // Low latency mode.
  uint32 size =  sync_reader_->Read(dest, max_size);
  sync_reader_->UpdatePendingBytes(buffers_state.total_bytes() + size);
  return size;
}

void AudioOutputController::OnError(AudioOutputStream* stream, int code) {
  // Handle error on the audio controller thread.
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioOutputController::DoReportError, code));
}

void AudioOutputController::SubmitOnMoreData_Locked() {
  lock_.AssertAcquired();

  if (buffer_.forward_bytes() > buffer_.forward_capacity())
    return;

  if (pending_request_)
    return;
  pending_request_ = true;

  AudioBuffersState buffers_state = buffers_state_;
  buffers_state.pending_bytes += buffer_.forward_bytes();

  // If we need more data then call the event handler to ask for more data.
  // It is okay that we don't lock in this block because the parameters are
  // correct and in the worst case we are just asking more data than needed.
  base::AutoUnlock auto_unlock(lock_);
  handler_->OnMoreData(this, buffers_state);
}

}  // namespace media
