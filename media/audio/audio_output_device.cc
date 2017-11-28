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
#include "media/base/limits.h"

namespace media {

// Takes care of invoking the render callback on the audio thread.
// An instance of this class is created for each capture stream in
// OnStreamCreated().
class AudioOutputDevice::AudioThreadCallback
    : public AudioDeviceThread::Callback {
 public:
  AudioThreadCallback(const AudioParameters& audio_parameters,
                      int input_channels,
                      base::SharedMemoryHandle memory,
                      int memory_length,
                      AudioRendererSink::RenderCallback* render_callback);
  virtual ~AudioThreadCallback();

  virtual void MapSharedMemory() override;

  // Called whenever we receive notifications about pending data.
  virtual void Process(int pending_data) override;

 private:
  AudioRendererSink::RenderCallback* render_callback_;
  scoped_ptr<AudioBus> input_bus_;
  scoped_ptr<AudioBus> output_bus_;
  DISALLOW_COPY_AND_ASSIGN(AudioThreadCallback);
};

AudioOutputDevice::AudioOutputDevice(
    AudioOutputIPC* ipc,
    const scoped_refptr<base::MessageLoopProxy>& io_loop)
    : ScopedLoopObserver(io_loop),
      input_channels_(0),
      callback_(NULL),
      ipc_(ipc),
      state_(IDLE),
      play_on_start_(true),
      stopping_hack_(false) {
  CHECK(ipc_);
  stream_id_ = ipc_->AddDelegate(this);
}

void AudioOutputDevice::Initialize(const AudioParameters& params,
                                   RenderCallback* callback) {
  DCHECK(!callback_) << "Calling Initialize() twice?";
  audio_parameters_ = params;
  callback_ = callback;
}

void AudioOutputDevice::InitializeIO(const AudioParameters& params,
                                     int input_channels,
                                     RenderCallback* callback) {
  DCHECK_GE(input_channels, 0);
  DCHECK_LT(input_channels, limits::kMaxChannels);
  input_channels_ = input_channels;
  Initialize(params, callback);
}

AudioOutputDevice::~AudioOutputDevice() {
  // The current design requires that the user calls Stop() before deleting
  // this class.
  DCHECK(audio_thread_.IsStopped());

  if (ipc_)
    ipc_->RemoveDelegate(stream_id_);
}

void AudioOutputDevice::Start() {
  DCHECK(callback_) << "Initialize hasn't been called";
  message_loop()->PostTask(FROM_HERE,
      base::Bind(&AudioOutputDevice::CreateStreamOnIOThread, this,
                 audio_parameters_, input_channels_));
}

void AudioOutputDevice::Stop() {
  {
    base::AutoLock auto_lock(audio_thread_lock_);
    audio_thread_.Stop(MessageLoop::current());
    stopping_hack_ = true;
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

void AudioOutputDevice::CreateStreamOnIOThread(const AudioParameters& params,
                                               int input_channels) {
  DCHECK(message_loop()->BelongsToCurrentThread());
  if (state_ == IDLE) {
    state_ = CREATING_STREAM;
    ipc_->CreateStream(stream_id_, params, input_channels);
  }
}

void AudioOutputDevice::PlayOnIOThread() {
  DCHECK(message_loop()->BelongsToCurrentThread());
  if (state_ == PAUSED) {
    ipc_->PlayStream(stream_id_);
    state_ = PLAYING;
    play_on_start_ = false;
  } else {
    play_on_start_ = true;
  }
}

void AudioOutputDevice::PauseOnIOThread(bool flush) {
  DCHECK(message_loop()->BelongsToCurrentThread());
  if (state_ == PLAYING) {
    ipc_->PauseStream(stream_id_);
    if (flush)
      ipc_->FlushStream(stream_id_);
    state_ = PAUSED;
  } else {
    // Note that |flush| isn't relevant here since this is the case where
    // the stream is first starting.
  }
  play_on_start_ = false;
}

void AudioOutputDevice::ShutDownOnIOThread() {
  DCHECK(message_loop()->BelongsToCurrentThread());

  // Make sure we don't call shutdown more than once.
  if (state_ >= CREATING_STREAM) {
    ipc_->CloseStream(stream_id_);
    state_ = IDLE;
  }

  // We can run into an issue where ShutDownOnIOThread is called right after
  // OnStreamCreated is called in cases where Start/Stop are called before we
  // get the OnStreamCreated callback.  To handle that corner case, we call
  // Stop(). In most cases, the thread will already be stopped.
  //
  // Another situation is when the IO thread goes away before Stop() is called
  // in which case, we cannot use the message loop to close the thread handle
  // and can't rely on the main thread existing either.
  base::AutoLock auto_lock_(audio_thread_lock_);
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  audio_thread_.Stop(NULL);
  audio_callback_.reset();
  stopping_hack_ = false;
}

void AudioOutputDevice::SetVolumeOnIOThread(double volume) {
  DCHECK(message_loop()->BelongsToCurrentThread());
  if (state_ >= CREATING_STREAM)
    ipc_->SetVolume(stream_id_, volume);
}

void AudioOutputDevice::OnStateChanged(AudioOutputIPCDelegate::State state) {
  DCHECK(message_loop()->BelongsToCurrentThread());

  // Do nothing if the stream has been closed.
  if (state_ < CREATING_STREAM)
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
#if defined(OS_WIN)
  DCHECK(handle);
  DCHECK(socket_handle);
#elif defined(__LB_SHELL__) || defined(COBALT)
  DCHECK(handle.get());
#else
  DCHECK_GE(handle.fd, 0);
  DCHECK_GE(socket_handle, 0);
#endif

  if (state_ != CREATING_STREAM)
    return;

  // We can receive OnStreamCreated() on the IO thread after the client has
  // called Stop() but before ShutDownOnIOThread() is processed. In such a
  // situation |callback_| might point to freed memory. Instead of starting
  // |audio_thread_| do nothing and wait for ShutDownOnIOThread() to get called.
  //
  // TODO(scherkus): The real fix is to have sane ownership semantics. The fact
  // that |callback_| (which should own and outlive this object!) can point to
  // freed memory is a mess. AudioRendererSink should be non-refcounted so that
  // owners (WebRtcAudioDeviceImpl, AudioRendererImpl, etc...) can Stop() and
  // delete as they see fit. AudioOutputDevice should internally use WeakPtr
  // to handle teardown and thread hopping. See http://crbug.com/151051 for
  // details.
  base::AutoLock auto_lock(audio_thread_lock_);
  if (stopping_hack_)
    return;

  DCHECK(audio_thread_.IsStopped());
  audio_callback_.reset(new AudioOutputDevice::AudioThreadCallback(
      audio_parameters_, input_channels_, handle, length, callback_));
  audio_thread_.Start(audio_callback_.get(), socket_handle,
      "AudioOutputDevice");
  state_ = PAUSED;

  // We handle the case where Play() and/or Pause() may have been called
  // multiple times before OnStreamCreated() gets called.
  if (play_on_start_)
    PlayOnIOThread();
}

void AudioOutputDevice::OnIPCClosed() {
  DCHECK(message_loop()->BelongsToCurrentThread());
  state_ = IPC_CLOSED;
  ipc_ = NULL;
}

void AudioOutputDevice::WillDestroyCurrentMessageLoop() {
  LOG(ERROR) << "IO loop going away before the audio device has been stopped";
  ShutDownOnIOThread();
}

// AudioOutputDevice::AudioThreadCallback

AudioOutputDevice::AudioThreadCallback::AudioThreadCallback(
    const AudioParameters& audio_parameters,
    int input_channels,
    base::SharedMemoryHandle memory,
    int memory_length,
    AudioRendererSink::RenderCallback* render_callback)
    : AudioDeviceThread::Callback(audio_parameters,
                                  input_channels,
                                  memory,
                                  memory_length),
      render_callback_(render_callback) {
}

AudioOutputDevice::AudioThreadCallback::~AudioThreadCallback() {
}

void AudioOutputDevice::AudioThreadCallback::MapSharedMemory() {
  shared_memory_.Map(TotalSharedMemorySizeInBytes(memory_length_));

  // Calculate output and input memory size.
  int output_memory_size = AudioBus::CalculateMemorySize(audio_parameters_);
  int frames = audio_parameters_.frames_per_buffer();
  int input_memory_size =
      AudioBus::CalculateMemorySize(input_channels_, frames);

  int io_size = output_memory_size + input_memory_size;

  DCHECK_EQ(memory_length_, io_size);

  output_bus_ =
      AudioBus::WrapMemory(audio_parameters_, shared_memory_.memory());

  if (input_channels_ > 0) {
    // The input data is after the output data.
    char* input_data =
        static_cast<char*>(shared_memory_.memory()) + output_memory_size;
    input_bus_ =
        AudioBus::WrapMemory(input_channels_, frames, input_data);
  }
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

  // Update the audio-delay measurement then ask client to render audio.  Since
  // |output_bus_| is wrapping the shared memory the Render() call is writing
  // directly into the shared memory.
  size_t num_frames = audio_parameters_.frames_per_buffer();

  if (input_bus_.get() && input_channels_ > 0) {
    render_callback_->RenderIO(input_bus_.get(),
                               output_bus_.get(),
                               audio_delay_milliseconds);
  } else {
    num_frames = render_callback_->Render(output_bus_.get(),
                                          audio_delay_milliseconds);
  }

  // Let the host know we are done.
  // TODO(dalecurtis): Technically this is not always correct.  Due to channel
  // padding for alignment, there may be more data available than this.  We're
  // relying on AudioSyncReader::Read() to parse this with that in mind.  Rename
  // these methods to Set/GetActualFrameCount().
  SetActualDataSizeInBytes(
      &shared_memory_, memory_length_,
      num_frames * sizeof(*output_bus_->channel(0)) * output_bus_->channels());
}

}  // namespace media.
