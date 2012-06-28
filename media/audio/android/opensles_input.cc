// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/opensles_input.h"

#include "base/logging.h"
#include "media/audio/android/audio_manager_android.h"

namespace media {

OpenSLESInputStream::OpenSLESInputStream(AudioManagerAndroid* audio_manager,
                                         const AudioParameters& params)
    : audio_manager_(audio_manager),
      callback_(NULL),
      recorder_(NULL),
      simple_buffer_queue_(NULL),
      active_queue_(0),
      buffer_size_bytes_(0),
      started_(false) {
  format_.formatType = SL_DATAFORMAT_PCM;
  format_.numChannels = static_cast<SLuint32>(params.channels());
  // Provides sampling rate in milliHertz to OpenSLES.
  format_.samplesPerSec = static_cast<SLuint32>(params.sample_rate() * 1000);
  format_.bitsPerSample = params.bits_per_sample();
  format_.containerSize = params.bits_per_sample();
  format_.channelMask = SL_SPEAKER_FRONT_CENTER;
  format_.endianness = SL_BYTEORDER_LITTLEENDIAN;

  buffer_size_bytes_ = params.GetBytesPerBuffer();

  memset(&audio_data_, 0, sizeof(audio_data_));
}

OpenSLESInputStream::~OpenSLESInputStream() {
  DCHECK(!recorder_object_.Get());
  DCHECK(!engine_object_.Get());
  DCHECK(!recorder_);
  DCHECK(!simple_buffer_queue_);
  DCHECK(!audio_data_[0]);
}

bool OpenSLESInputStream::Open() {
  if (engine_object_.Get())
    return false;

  if (!CreateRecorder())
    return false;

  SetupAudioBuffer();

  return true;
}

void OpenSLESInputStream::Start(AudioInputCallback* callback) {
  DCHECK(callback);
  DCHECK(recorder_);
  DCHECK(simple_buffer_queue_);
  if (started_)
    return;

  // Enable the flags before streaming.
  callback_ = callback;
  active_queue_ = 0;
  started_ = true;

  SLresult err = SL_RESULT_UNKNOWN_ERROR;
  // Enqueues |kNumOfQueuesInBuffer| zero buffers to get the ball rolling.
  for (int i = 0; i < kNumOfQueuesInBuffer - 1; ++i) {
    err = (*simple_buffer_queue_)->Enqueue(
        simple_buffer_queue_,
        audio_data_[i],
        buffer_size_bytes_);
    if (SL_RESULT_SUCCESS != err) {
      HandleError(err);
      return;
    }
  }

  // Start the recording by setting the state to |SL_RECORDSTATE_RECORDING|.
  err = (*recorder_)->SetRecordState(recorder_, SL_RECORDSTATE_RECORDING);
  DCHECK_EQ(SL_RESULT_SUCCESS, err);
  if (SL_RESULT_SUCCESS != err)
    HandleError(err);
}

void OpenSLESInputStream::Stop() {
  if (!started_)
    return;

  // Stop recording by setting the record state to |SL_RECORDSTATE_STOPPED|.
  SLresult err = (*recorder_)->SetRecordState(recorder_,
                                              SL_RECORDSTATE_STOPPED);
  DLOG_IF(WARNING, SL_RESULT_SUCCESS != err) << "SetRecordState() failed to "
                                             << "set the state to stop";

  // Clear the buffer queue to get rid of old data when resuming recording.
  err = (*simple_buffer_queue_)->Clear(simple_buffer_queue_);
  DLOG_IF(WARNING, SL_RESULT_SUCCESS != err) << "Clear() failed to clear "
                                             << "the buffer queue";

  started_ = false;
}

void OpenSLESInputStream::Close() {
  // Stop the stream if it is still recording.
  Stop();

  // Explicitly free the player objects and invalidate their associated
  // interfaces. They have to be done in the correct order.
  recorder_object_.Reset();
  engine_object_.Reset();
  simple_buffer_queue_ = NULL;
  recorder_ = NULL;

  ReleaseAudioBuffer();

  audio_manager_->ReleaseInputStream(this);
}

double OpenSLESInputStream::GetMaxVolume() {
  NOTIMPLEMENTED();
  return 0.0;
}

void OpenSLESInputStream::SetVolume(double volume) {
  NOTIMPLEMENTED();
}

double OpenSLESInputStream::GetVolume() {
  NOTIMPLEMENTED();
  return 0.0;
}

void OpenSLESInputStream::SetAutomaticGainControl(bool enabled) {
  NOTIMPLEMENTED();
}

bool OpenSLESInputStream::GetAutomaticGainControl() {
  NOTIMPLEMENTED();
  return false;
}

bool OpenSLESInputStream::CreateRecorder() {
  // Initializes the engine object with specific option. After working with the
  // object, we need to free the object and its resources.
  SLEngineOption option[] = {
    { SL_ENGINEOPTION_THREADSAFE, static_cast<SLuint32>(SL_BOOLEAN_TRUE) }
  };
  SLresult err = slCreateEngine(engine_object_.Receive(),
                                1,
                                option,
                                0,
                                NULL,
                                NULL);
  DCHECK_EQ(SL_RESULT_SUCCESS, err);
  if (SL_RESULT_SUCCESS != err)
    return false;

  // Realize the SL engine object in synchronous mode.
  err = engine_object_->Realize(engine_object_.Get(), SL_BOOLEAN_FALSE);
  DCHECK_EQ(SL_RESULT_SUCCESS, err);
  if (SL_RESULT_SUCCESS != err)
    return false;

  // Get the SL engine interface which is implicit.
  SLEngineItf engine;
  err = engine_object_->GetInterface(
      engine_object_.Get(), SL_IID_ENGINE, &engine);
  DCHECK_EQ(SL_RESULT_SUCCESS, err);
  if (SL_RESULT_SUCCESS != err)
    return false;

  // Audio source configuration.
  SLDataLocator_IODevice mic_locator = {
    SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
    SL_DEFAULTDEVICEID_AUDIOINPUT, NULL
  };
  SLDataSource audio_source = { &mic_locator, NULL };

  // Audio sink configuration.
  SLDataLocator_AndroidSimpleBufferQueue buffer_queue = {
    SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, // Locator type.
    static_cast<SLuint32>(kNumOfQueuesInBuffer)  // Number of buffers.
  };
  SLDataSink audio_sink = { &buffer_queue, &format_ };

  // Create an audio recorder.
  const SLuint32 number_of_interfaces = 1;
  const SLInterfaceID interface_id[number_of_interfaces] = {
    SL_IID_ANDROIDSIMPLEBUFFERQUEUE
  };
  const SLboolean interface_required[number_of_interfaces] = {
    SL_BOOLEAN_TRUE
  };
  err = (*engine)->CreateAudioRecorder(engine,
                                       recorder_object_.Receive(),
                                       &audio_source,
                                       &audio_sink,
                                       number_of_interfaces,
                                       interface_id,
                                       interface_required);
  DCHECK_EQ(SL_RESULT_SUCCESS, err);
  if (SL_RESULT_SUCCESS != err) {
    DLOG(ERROR) << "CreateAudioRecorder failed with error code " << err;
    return false;
  }

  // Realize the recorder object in synchronous mode.
  err = recorder_object_->Realize(recorder_object_.Get(), SL_BOOLEAN_FALSE);
  DCHECK_EQ(SL_RESULT_SUCCESS, err);
  if (SL_RESULT_SUCCESS != err) {
    DLOG(ERROR) << "Recprder Realize() failed with error code " << err;
    return false;
  }

  // Get an implicit recorder interface.
  err = recorder_object_->GetInterface(recorder_object_.Get(),
                                       SL_IID_RECORD,
                                       &recorder_);
  DCHECK_EQ(SL_RESULT_SUCCESS, err);
  if (SL_RESULT_SUCCESS != err)
    return false;

  // Get the simple buffer queue interface.
  err = recorder_object_->GetInterface(recorder_object_.Get(),
                                       SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                       &simple_buffer_queue_);
  DCHECK_EQ(SL_RESULT_SUCCESS, err);
  if (SL_RESULT_SUCCESS != err)
    return false;

  // Register the input callback for the simple buffer queue.
  // This callback will be called when receiving new data from the device.
  err = (*simple_buffer_queue_)->RegisterCallback(simple_buffer_queue_,
                                                  SimpleBufferQueueCallback,
                                                  this);
  DCHECK_EQ(SL_RESULT_SUCCESS, err);

  return (SL_RESULT_SUCCESS == err);
}

void OpenSLESInputStream::SimpleBufferQueueCallback(
    SLAndroidSimpleBufferQueueItf buffer_queue, void* instance) {
  OpenSLESInputStream* stream =
      reinterpret_cast<OpenSLESInputStream*>(instance);
  stream->ReadBufferQueue();
}

void OpenSLESInputStream::ReadBufferQueue() {
  if (!started_)
    return;

  // Get the enqueued buffer from the soundcard.
  SLresult err = (*simple_buffer_queue_)->Enqueue(
      simple_buffer_queue_,
      audio_data_[active_queue_],
      buffer_size_bytes_);
  if (SL_RESULT_SUCCESS != err)
    HandleError(err);

  // TODO(xians): Get an accurate delay estimation.
  callback_->OnData(this,
                    audio_data_[active_queue_],
                    buffer_size_bytes_,
                    buffer_size_bytes_,
                    0.0);

  active_queue_ = (active_queue_ + 1) % kNumOfQueuesInBuffer;
}

void OpenSLESInputStream::SetupAudioBuffer() {
  DCHECK(!audio_data_[0]);
  for (int i = 0; i < kNumOfQueuesInBuffer; ++i) {
    audio_data_[i] = new uint8[buffer_size_bytes_];
  }
}

void OpenSLESInputStream::ReleaseAudioBuffer() {
  if (audio_data_[0]) {
    for (int i = 0; i < kNumOfQueuesInBuffer; ++i) {
      delete [] audio_data_[i];
      audio_data_[i] = NULL;
    }
  }
}

void OpenSLESInputStream::HandleError(SLresult error) {
  DLOG(FATAL) << "OpenSLES error " << error;
  if (callback_)
    callback_->OnError(this, error);
}

}  // namespace media
