// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_device.h"

#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "media/audio/audio_output_controller.h"
#include "media/audio/audio_util.h"
#include "media/audio/shared_memory_util.h"

namespace media {

// Takes care of invoking the render callback on the audio thread.
// An instance of this class is created for each capture stream in
// OnStreamCreated().
class AudioOutputDevice::AudioThreadCallback
    : public AudioDeviceThread::Callback {
 public:
  AudioThreadCallback(const AudioParameters& audio_parameters,
                      base::SharedMemoryHandle memory,
                      int memory_length,
                      AudioRendererSink::RenderCallback* render_callback);
  virtual ~AudioThreadCallback();

  virtual void MapSharedMemory() OVERRIDE;

  // Called whenever we receive notifications about pending data.
  virtual void Process(int pending_data) OVERRIDE;

 private:
  AudioRendererSink::RenderCallback* render_callback_;
  DISALLOW_COPY_AND_ASSIGN(AudioThreadCallback);
};

AudioOutputDevice::AudioOutputDevice(
    AudioOutputIPC* ipc,
    const scoped_refptr<base::MessageLoopProxy>& io_loop)
    : ScopedLoopObserver(io_loop),
      callback_(NULL),
      ipc_(ipc),
      stream_id_(0),
      play_on_start_(true),
      is_started_(false) {
  CHECK(ipc_);
}

void AudioOutputDevice::Initialize(const AudioParameters& params,
                             RenderCallback* callback) {
  CHECK_EQ(0, stream_id_) <<
      "AudioOutputDevice::Initialize() must be called before Start()";

  CHECK(!callback_);  // Calling Initialize() twice?

  audio_parameters_ = params;
  callback_ = callback;
}

AudioOutputDevice::~AudioOutputDevice() {
  // The current design requires that the user calls Stop() before deleting
  // this class.
  CHECK_EQ(0, stream_id_);
}

void AudioOutputDevice::Start() {
  DCHECK(callback_) << "Initialize hasn't been called";
  message_loop()->PostTask(FROM_HERE,
      base::Bind(&AudioOutputDevice::CreateStreamOnIOThread, this,
                 audio_parameters_));
}

void AudioOutputDevice::Stop() {
  {
    base::AutoLock auto_lock(audio_thread_lock_);
    audio_thread_.Stop(MessageLoop::current());
  }

  message_loop()->PostTask(FROM_HERE,
      base::Bind(&AudioOutputDevice::ShutDownOnIOThread, this));
}

void AudioOutputDevice::Play() {
  message_loop()->PostTask(FROM_HERE,
      base::Bind(&AudioOutputDevice::PlayOnIOThread, this));
}

void AudioOutputDevice::Pause(bool flush) {
  message_loop()->PostTask(FROM_HERE,
      base::Bind(&AudioOutputDevice::PauseOnIOThread, this, flush));
}

bool AudioOutputDevice::SetVolume(double volume) {
  if (volume < 0 || volume > 1.0)
    return false;

  if (!message_loop()->PostTask(FROM_HERE,
          base::Bind(&AudioOutputDevice::SetVolumeOnIOThread, this, volume))) {
    return false;
  }

  return true;
}

void AudioOutputDevice::CreateStreamOnIOThread(const AudioParameters& params) {
  DCHECK(message_loop()->BelongsToCurrentThread());
  // Make sure we don't create the stream more than once.
  DCHECK_EQ(0, stream_id_);
  if (stream_id_)
    return;

  stream_id_ = ipc_->AddDelegate(this);
  ipc_->CreateStream(stream_id_, params);
}

void AudioOutputDevice::PlayOnIOThread() {
  DCHECK(message_loop()->BelongsToCurrentThread());
  if (stream_id_ && is_started_)
    ipc_->PlayStream(stream_id_);
  else
    play_on_start_ = true;
}

void AudioOutputDevice::PauseOnIOThread(bool flush) {
  DCHECK(message_loop()->BelongsToCurrentThread());
  if (stream_id_ && is_started_) {
    ipc_->PauseStream(stream_id_);
    if (flush)
      ipc_->FlushStream(stream_id_);
  } else {
    // Note that |flush| isn't relevant here since this is the case where
    // the stream is first starting.
    play_on_start_ = false;
  }
}

void AudioOutputDevice::ShutDownOnIOThread() {
  DCHECK(message_loop()->BelongsToCurrentThread());

  // Make sure we don't call shutdown more than once.
  if (stream_id_) {
    is_started_ = false;

    if (ipc_) {
      ipc_->CloseStream(stream_id_);
      ipc_->RemoveDelegate(stream_id_);
    }

    stream_id_ = 0;
  }

  // We can run into an issue where ShutDownOnIOThread is called right after
  // OnStreamCreated is called in cases where Start/Stop are called before we
  // get the OnStreamCreated callback.  To handle that corner case, we call
  // Stop(). In most cases, the thread will already be stopped.
  // Another situation is when the IO thread goes away before Stop() is called
  // in which case, we cannot use the message loop to close the thread handle
  // and can't not rely on the main thread existing either.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  audio_thread_.Stop(NULL);
  audio_callback_.reset();
}

void AudioOutputDevice::SetVolumeOnIOThread(double volume) {
  DCHECK(message_loop()->BelongsToCurrentThread());
  if (stream_id_)
    ipc_->SetVolume(stream_id_, volume);
}

void AudioOutputDevice::OnStateChanged(AudioOutputIPCDelegate::State state) {
  DCHECK(message_loop()->BelongsToCurrentThread());

  // Do nothing if the stream has been closed.
  if (!stream_id_)
    return;

  if (state == AudioOutputIPCDelegate::kError) {
    DLOG(WARNING) << "AudioOutputDevice::OnStateChanged(kError)";
    // Don't dereference the callback object if the audio thread
    // is stopped or stopping.  That could mean that the callback
    // object has been deleted.
    // TODO(tommi): Add an explicit contract for clearing the callback
    // object.  Possibly require calling Initialize again or provide
    // a callback object via Start() and clear it in Stop().
    if (!audio_thread_.IsStopped())
      callback_->OnRenderError();
  }
}

void AudioOutputDevice::OnStreamCreated(
    base::SharedMemoryHandle handle,
    base::SyncSocket::Handle socket_handle,
    int length) {
  DCHECK(message_loop()->BelongsToCurrentThread());
  DCHECK_GE(length, audio_parameters_.GetBytesPerBuffer());
#if defined(OS_WIN)
  DCHECK(handle);
  DCHECK(socket_handle);
#else
  DCHECK_GE(handle.fd, 0);
  DCHECK_GE(socket_handle, 0);
#endif

  // We should only get this callback if stream_id_ is valid.  If it is not,
  // the IPC layer should have closed the shared memory and socket handles
  // for us and not invoked the callback.  The basic assertion is that when
  // stream_id_ is 0 the AudioOutputDevice instance is not registered as a
  // delegate and hence it should not receive callbacks.
  DCHECK(stream_id_);

  base::AutoLock auto_lock(audio_thread_lock_);

  DCHECK(audio_thread_.IsStopped());
  audio_callback_.reset(new AudioOutputDevice::AudioThreadCallback(
      audio_parameters_, handle, length, callback_));
  audio_thread_.Start(audio_callback_.get(), socket_handle,
      "AudioOutputDevice");

  // We handle the case where Play() and/or Pause() may have been called
  // multiple times before OnStreamCreated() gets called.
  is_started_ = true;
  if (play_on_start_)
    PlayOnIOThread();
}

void AudioOutputDevice::OnIPCClosed() {
  ipc_ = NULL;
}

void AudioOutputDevice::WillDestroyCurrentMessageLoop() {
  LOG(ERROR) << "IO loop going away before the audio device has been stopped";
  ShutDownOnIOThread();
}

// AudioOutputDevice::AudioThreadCallback

AudioOutputDevice::AudioThreadCallback::AudioThreadCallback(
    const AudioParameters& audio_parameters,
    base::SharedMemoryHandle memory,
    int memory_length,
    AudioRendererSink::RenderCallback* render_callback)
    : AudioDeviceThread::Callback(audio_parameters, memory, memory_length),
      render_callback_(render_callback) {
}

AudioOutputDevice::AudioThreadCallback::~AudioThreadCallback() {
}

void AudioOutputDevice::AudioThreadCallback::MapSharedMemory() {
  shared_memory_.Map(TotalSharedMemorySizeInBytes(memory_length_));
}

// Called whenever we receive notifications about pending data.
void AudioOutputDevice::AudioThreadCallback::Process(int pending_data) {
  if (pending_data == kPauseMark) {
    memset(shared_memory_.memory(), 0, memory_length_);
    SetActualDataSizeInBytes(&shared_memory_, memory_length_, 0);
    return;
  }

  // Convert the number of pending bytes in the render buffer
  // into milliseconds.
  int audio_delay_milliseconds = pending_data / bytes_per_ms_;

  TRACE_EVENT0("audio", "AudioOutputDevice::FireRenderCallback");

  // Update the audio-delay measurement then ask client to render audio.
  size_t num_frames = render_callback_->Render(
      audio_bus_.get(), audio_delay_milliseconds);

  // Interleave, scale, and clip to int.
  // TODO(dalecurtis): Remove this when we have float everywhere:
  // http://crbug.com/114700
  audio_bus_->ToInterleaved(num_frames, audio_parameters_.bits_per_sample() / 8,
                            shared_memory_.memory());

  // Let the host know we are done.
  SetActualDataSizeInBytes(
      &shared_memory_, memory_length_,
      num_frames * audio_parameters_.GetBytesPerFrame());
}

}  // namespace media.
