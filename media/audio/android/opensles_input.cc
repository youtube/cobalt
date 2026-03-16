// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/opensles_input.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/android/audio_manager_android.h"
#include "media/base/audio_bus.h"
#include "base/task/thread_pool.h"
#include "base/task/single_thread_task_runner.h"

#define LOG_ON_FAILURE_AND_RETURN(op, ...)      \
  do {                                          \
    SLresult err = (op);                        \
    if (err != SL_RESULT_SUCCESS) {             \
      DLOG(ERROR) << #op << " failed: " << err; \
      return __VA_ARGS__;                       \
    }                                           \
  } while (0)

namespace media {

OpenSLESInputStream::OpenSLESInputStream(AudioManagerAndroid* audio_manager,
                                         const AudioParameters& params)
    : audio_manager_(audio_manager),
      callback_(nullptr),
      recorder_(nullptr),
      simple_buffer_queue_(nullptr),
      active_buffer_index_(0),
      buffer_size_bytes_(0),
      started_(false),
      audio_bus_(media::AudioBus::Create(params)),
      no_effects_(params.effects() == AudioParameters::NO_EFFECTS) {
  DVLOG(2) << __PRETTY_FUNCTION__;
  DVLOG(1) << "Audio effects enabled: " << !no_effects_;

  const SampleFormat kSampleFormat = kSampleFormatS16;

  format_.formatType = SL_DATAFORMAT_PCM;
  format_.numChannels = static_cast<SLuint32>(params.channels());
  // Provides sampling rate in milliHertz to OpenSLES.
  format_.samplesPerSec = static_cast<SLuint32>(params.sample_rate() * 1000);
  format_.bitsPerSample = format_.containerSize =
      SampleFormatToBitsPerChannel(kSampleFormat);
  format_.endianness = SL_BYTEORDER_LITTLEENDIAN;
  format_.channelMask = ChannelCountToSLESChannelMask(params.channels());

  buffer_size_bytes_ = params.GetBytesPerBuffer(kSampleFormat);
  hardware_delay_ = base::Seconds(params.frames_per_buffer() /
                                  static_cast<double>(params.sample_rate()));

  memset(&audio_data_, 0, sizeof(audio_data_));
}

OpenSLESInputStream::~OpenSLESInputStream() {
  DVLOG(2) << __PRETTY_FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!recorder_object_.Get());
  DCHECK(!engine_object_.Get());
  DCHECK(!recorder_);
  DCHECK(!simple_buffer_queue_);
  DCHECK(!audio_data_[0]);
}

void OpenSLESInputStream::RealizeRecorderOnBackgroundThread(
    scoped_refptr<base::SingleThreadTaskRunner> audio_thread_runner) {
  SLresult err = engine_object_->Realize(engine_object_.Get(), SL_BOOLEAN_FALSE);

  SLEngineItf engine = nullptr;
  if (err == SL_RESULT_SUCCESS) {
    err = engine_object_->GetInterface(engine_object_.Get(), SL_IID_ENGINE, &engine);
  }

  SLDataLocator_IODevice mic_locator = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT, SL_DEFAULTDEVICEID_AUDIOINPUT, nullptr};
  SLDataSource audio_source = {&mic_locator, nullptr};
  SLDataLocator_AndroidSimpleBufferQueue buffer_queue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, static_cast<SLuint32>(kMaxNumOfBuffersInQueue)};

  // Back to standard:
  SLDataSink audio_sink = {&buffer_queue, &format_};

  const SLInterfaceID interface_id[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_ANDROIDCONFIGURATION};
  const SLboolean interface_required[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

  if (err == SL_RESULT_SUCCESS) {
    err = (*engine)->CreateAudioRecorder(engine, recorder_object_.Receive(), &audio_source, &audio_sink,
                                         2, interface_id, interface_required);
  }

  SLAndroidConfigurationItf recorder_config;
  if (err == SL_RESULT_SUCCESS &&
      recorder_object_->GetInterface(recorder_object_.Get(), SL_IID_ANDROIDCONFIGURATION, &recorder_config) == SL_RESULT_SUCCESS) {
    SLint32 stream_type = SL_ANDROID_RECORDING_PRESET_VOICE_RECOGNITION;
    (*recorder_config)->SetConfiguration(recorder_config, SL_ANDROID_KEY_RECORDING_PRESET, &stream_type, sizeof(SLint32));
  }

  if (err == SL_RESULT_SUCCESS) {
    err = recorder_object_->Realize(recorder_object_.Get(), SL_BOOLEAN_FALSE);
  }

  if (err == SL_RESULT_SUCCESS) {
    recorder_object_->GetInterface(recorder_object_.Get(), SL_IID_RECORD, &recorder_);
    recorder_object_->GetInterface(recorder_object_.Get(), SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &simple_buffer_queue_);
    (*simple_buffer_queue_)->RegisterCallback(simple_buffer_queue_, SimpleBufferQueueCallback, this);
  }

  bool success = (err == SL_RESULT_SUCCESS);

  audio_thread_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&OpenSLESInputStream::OnRealizeComplete,
                     weak_ptr_factory_.GetWeakPtr(), success));
}

AudioInputStream::OpenOutcome OpenSLESInputStream::Open() {
  DVLOG(2) << __PRETTY_FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != State::kCreated) return AudioInputStream::OpenOutcome::kFailed;
  // if (engine_object_.Get())
  //   return AudioInputStream::OpenOutcome::kFailed;

  if (!CreateRecorderObject())
    return AudioInputStream::OpenOutcome::kFailed;

  state_ = State::kRealizing;
  LOG(INFO) << "YO THOR: Offloading Realize to background thread";

  // Capture the current thread's task runner (The Audio Thread)
  auto audio_thread_runner = base::SingleThreadTaskRunner::GetCurrentDefault();

  //SetupAudioBuffer();
  // Post the Realize call to the ThreadPool instead of doing it here
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&OpenSLESInputStream::RealizeRecorderOnBackgroundThread,
                     base::Unretained(this),
                     audio_thread_runner));
  return AudioInputStream::OpenOutcome::kSuccess;
}

void OpenSLESInputStream::OnRealizeComplete(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!success) {
    state_ = State::kError;
    LOG(ERROR) << "ASYNCHRONOUS REALIZE FAILED: Audio server is likely dead.";
    return;
  }

  State old_state = state_;
  state_ = State::kReady;

  // If Start() was already called while we were realizing,
  // we need to trigger the actual recording now.
  if (old_state == State::kRecording) {
    Record();
  }
}

void OpenSLESInputStream::Start(AudioInputCallback* callback) {

  DCHECK(thread_checker_.CalledOnValidThread());
  callback_ = callback;

  if (state_ == State::kReady) {
    state_ = State::kRecording;
    Record(); // Fast path: Realize already finished
  } else if (state_ == State::kRealizing) {
    state_ = State::kRecording;
    LOG(INFO) << "YO THOR: Start() called while realizing. Queuing recording.";
    // We don't call Record() yet; OnRealizeComplete will trigger it.
  }

  // LOG(INFO) << "YO THOR " << __PRETTY_FUNCTION__;
  // DCHECK(thread_checker_.CalledOnValidThread());
  // DCHECK(callback);
  // DCHECK(recorder_);
  // DCHECK(simple_buffer_queue_);
  // if (started_)
  //   return;

  // base::AutoLock lock(lock_);
  // DCHECK(!callback_ || callback_ == callback);
  // callback_ = callback;
  // active_buffer_index_ = 0;

  // LOG(INFO) << "YO THOR OpenSLESInputStream::Start - Enqueuing initial buffers";
  // // Enqueues kMaxNumOfBuffersInQueue zero buffers to get the ball rolling.
  // // TODO(henrika): add support for Start/Stop/Start sequences when we are
  // // able to clear the buffer queue. There is currently a bug in the OpenSLES
  // // implementation which forces us to always call Stop() and Close() before
  // // calling Start() again.
  // SLresult err = SL_RESULT_UNKNOWN_ERROR;
  // for (int i = 0; i < kMaxNumOfBuffersInQueue; ++i) {
  //   err = (*simple_buffer_queue_)->Enqueue(
  //       simple_buffer_queue_, audio_data_[i], buffer_size_bytes_);
  //   if (SL_RESULT_SUCCESS != err) {
  //     HandleError(err);
  //     started_ = false;
  //     return;
  //   }
  // }

  // LOG(INFO) << "YO THOR OpenSLESInputStream::Start - Setting record state to RECORDING";
  // // Start the recording by setting the state to SL_RECORDSTATE_RECORDING.
  // // When the object is in the SL_RECORDSTATE_RECORDING state, adding buffers
  // // will implicitly start the filling process.
  // err = (*recorder_)->SetRecordState(recorder_, SL_RECORDSTATE_RECORDING);
  // if (SL_RESULT_SUCCESS != err) {
  //   HandleError(err);
  //   started_ = false;
  //   return;
  // }

  // started_ = true;
  // LOG(INFO) << "YO THOR OpenSLESInputStream::Start - Started";
}

void OpenSLESInputStream::Stop() {
  DVLOG(2) << __PRETTY_FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // ADD THIS: If the background task hasn't finished,
  // just update the state so Record() never runs.
  if (state_ == State::kRealizing || state_ == State::kRecording) {
      state_ = State::kCreated; // Or a dedicated kCancelled state
  }

  if (!started_ || !recorder_) // Ensure recorder exists before calling it
    return;

  base::AutoLock lock(lock_);

  // Stop recording by setting the record state to SL_RECORDSTATE_STOPPED.
  LOG_ON_FAILURE_AND_RETURN(
      (*recorder_)->SetRecordState(recorder_, SL_RECORDSTATE_STOPPED));

  // Clear the buffer queue to get rid of old data when resuming recording.
  LOG_ON_FAILURE_AND_RETURN(
      (*simple_buffer_queue_)->Clear(simple_buffer_queue_));

  started_ = false;
  callback_ = nullptr;
}

void OpenSLESInputStream::Close() {
  DVLOG(2) << __PRETTY_FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // Stop the stream if it is still recording.
  Stop();
  {
    // TODO(henrika): Do we need to hold the lock here?
    base::AutoLock lock(lock_);

    // Destroy the buffer queue recorder object and invalidate all associated
    // interfaces.
    recorder_object_.Reset();
    simple_buffer_queue_ = nullptr;
    recorder_ = nullptr;

    // Destroy the engine object. We don't store any associated interface for
    // this object.
    engine_object_.Reset();
    ReleaseAudioBuffer();
  }

  audio_manager_->ReleaseInputStream(this);
}

double OpenSLESInputStream::GetMaxVolume() {
  return 0.0;
}

void OpenSLESInputStream::SetVolume(double volume) {
}

double OpenSLESInputStream::GetVolume() {
  return 0.0;
}

bool OpenSLESInputStream::SetAutomaticGainControl(bool enabled) {
  return false;
}

bool OpenSLESInputStream::GetAutomaticGainControl() {
  return false;
}

bool OpenSLESInputStream::IsMuted() {
  return false;
}

void OpenSLESInputStream::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  // Not supported. Do nothing.
}

bool OpenSLESInputStream::CreateRecorderObject() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!engine_object_.Get());
  DCHECK(!recorder_object_.Get());
  DCHECK(!recorder_);
  DCHECK(!simple_buffer_queue_);

  // Initializes the engine object with specific option. After working with the
  // object, we need to free the object and its resources.
  SLEngineOption option[] = {
      {SL_ENGINEOPTION_THREADSAFE, static_cast<SLuint32>(SL_BOOLEAN_TRUE)}};
  LOG_ON_FAILURE_AND_RETURN(
      slCreateEngine(engine_object_.Receive(), 1, option, 0, nullptr, nullptr),
      false);

  return true;
}

void OpenSLESInputStream::SimpleBufferQueueCallback(
    SLAndroidSimpleBufferQueueItf buffer_queue,
    void* instance) {
  OpenSLESInputStream* stream =
      reinterpret_cast<OpenSLESInputStream*>(instance);
  stream->ReadBufferQueue();
}

void OpenSLESInputStream::ReadBufferQueue() {
  base::AutoLock lock(lock_);
  if (!started_)
    return;

  TRACE_EVENT0("audio", "OpenSLESOutputStream::ReadBufferQueue");

  // Convert from interleaved format to deinterleaved audio bus format.
  audio_bus_->FromInterleaved<SignedInt16SampleTypeTraits>(
      reinterpret_cast<int16_t*>(audio_data_[active_buffer_index_]),
      audio_bus_->frames());

  // TODO(henrika): Investigate if it is possible to get an accurate
  // delay estimation.
  callback_->OnData(audio_bus_.get(), base::TimeTicks::Now() - hardware_delay_,
                    0.0, {});

  // Done with this buffer. Send it to device for recording.
  SLresult err =
      (*simple_buffer_queue_)->Enqueue(simple_buffer_queue_,
                                       audio_data_[active_buffer_index_],
                                       buffer_size_bytes_);
  if (SL_RESULT_SUCCESS != err)
    HandleError(err);

  active_buffer_index_ = (active_buffer_index_ + 1) % kMaxNumOfBuffersInQueue;
}

void OpenSLESInputStream::SetupAudioBuffer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!audio_data_[0]);
  for (int i = 0; i < kMaxNumOfBuffersInQueue; ++i) {
    audio_data_[i] = new uint8_t[buffer_size_bytes_];
  }
}

void OpenSLESInputStream::ReleaseAudioBuffer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (audio_data_[0]) {
    for (int i = 0; i < kMaxNumOfBuffersInQueue; ++i) {
      delete[] audio_data_[i];
      audio_data_[i] = nullptr;
    }
  }
}

void OpenSLESInputStream::Record() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);
  // Guard: Ensure we are in the correct state and have hardware handles
  if (state_ != State::kRecording || !recorder_object_.Get() || !simple_buffer_queue_) {
    LOG(WARNING) << "YO THOR: Record() called but not ready. State: " << static_cast<int>(state_);
    return;
  }

  // 1. Safe Buffer Allocation
  if (!audio_data_[0]) {
    SetupAudioBuffer();
  }

  // 2. Initial Buffer Enqueue (Prime the pump)
  SLresult err = SL_RESULT_UNKNOWN_ERROR;
  for (int i = 0; i < kMaxNumOfBuffersInQueue; ++i) {
    err = (*simple_buffer_queue_)->Enqueue(
        simple_buffer_queue_, audio_data_[i], buffer_size_bytes_);
    if (err != SL_RESULT_SUCCESS) {
      HandleError(err);
      return;
    }
  }

  // 3. Start Recording
  err = (*recorder_)->SetRecordState(recorder_, SL_RECORDSTATE_RECORDING);
  if (err != SL_RESULT_SUCCESS) {
    HandleError(err);
    return;
  }

  started_ = true;
  LOG(INFO) << "YO THOR: OpenSLES Recording started successfully on Audio Thread.";
}

void OpenSLESInputStream::HandleError(SLresult error) {
  DLOG(ERROR) << "OpenSLES Input error " << error;
  if (callback_)
    callback_->OnError();
}

}  // namespace media
