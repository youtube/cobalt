// Copyright 2026 The Cobalt Authors. All Rights Reserved.

#include "media/audio/android/starboard_audio_input_stream.h"

#include "base/logging.h"
#include "base/time/time.h"

#include "media/audio/android/audio_manager_android.h"
#include "media/base/audio_bus.h"

#define LOG_ON_FAILURE_AND_RETURN(op, ...)      \
  do {                                          \
    SLresult err = (op);                        \
    if (err != SL_RESULT_SUCCESS) {             \
      DLOG(ERROR) << #op << " failed: " << err; \
      return __VA_ARGS__;                       \
    }                                           \
  } while (0)

namespace media {

StarboardAudioInputStream::StarboardAudioInputStream(AudioManagerAndroid* audio_manager,
                                                     const AudioParameters& params)
    : audio_manager_(audio_manager),
      callback_(nullptr),
      recorder_(nullptr),
      simple_buffer_queue_(nullptr),
      active_buffer_index_(0),
      buffer_size_bytes_(0),
      started_(false) {
  DVLOG(2) << __PRETTY_FUNCTION__;

  // Hardcode to 16kHz Mono 16-bit PCM (Starboard style)
  format_.formatType = SL_ANDROID_DATAFORMAT_PCM_EX;
  format_.numChannels = 1;
  format_.sampleRate = kSampleRateHz * 1000; // milliHertz
  format_.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
  format_.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
  format_.channelMask = SL_SPEAKER_FRONT_CENTER;
  format_.endianness = SL_BYTEORDER_LITTLEENDIAN;
  format_.representation = SL_ANDROID_PCM_REPRESENTATION_SIGNED_INT;

  // Use the params to determine the buffer size, but we'll be 16kHz
  // If the passed params are already 16kHz, this is easy.
  // C25 used 128 samples per buffer. 
  // Let's use what's passed but ensure it's calculated for 16-bit Mono.
  buffer_size_bytes_ = (kSampleRateHz * params.frames_per_buffer() / params.sample_rate()) * sizeof(int16_t);
  
  // If we couldn't scale it properly, just use a reasonable default.
  if (buffer_size_bytes_ == 0) {
      buffer_size_bytes_ = kDefaultBufferSizeInBytes; // Reasonable fallback
  }

  audio_bus_ = media::AudioBus::Create(1, buffer_size_bytes_ / sizeof(int16_t));
  hardware_delay_ = base::Seconds(audio_bus_->frames() / static_cast<double>(kSampleRateHz));

  memset(&audio_data_, 0, sizeof(audio_data_));
}

StarboardAudioInputStream::~StarboardAudioInputStream() {
  DVLOG(2) << __PRETTY_FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!recorder_object_.Get());
  DCHECK(!engine_object_.Get());
  DCHECK(!recorder_);
  DCHECK(!simple_buffer_queue_);
  DCHECK(!audio_data_[0]);
}

AudioInputStream::OpenOutcome StarboardAudioInputStream::Open() {
  LOG(INFO) << "StarboardAudioInputStream::Open";
  DCHECK(thread_checker_.CalledOnValidThread());
  
  if (engine_object_.Get()) {
    LOG(INFO) << "StarboardAudioInputStream already Open (Pre-started)";
    return AudioInputStream::OpenOutcome::kSuccess;
  }

  if (!CreateRecorder())
    return AudioInputStream::OpenOutcome::kFailed;

  SetupAudioBuffer();

  // PRE-START hardware immediately to bypass the Mojo Start handshake.
  // This allows the hardware to warm up while the Renderer thread is busy.
  SLresult err = SL_RESULT_UNKNOWN_ERROR;
  for (int i = 0; i < kMaxNumOfBuffersInQueue; ++i) {
    err = (*simple_buffer_queue_)->Enqueue(
        simple_buffer_queue_, audio_data_[i], buffer_size_bytes_);
    if (SL_RESULT_SUCCESS != err) {
      HandleError(err);
      return AudioInputStream::OpenOutcome::kFailed;
    }
  }

  err = (*recorder_)->SetRecordState(recorder_, SL_RECORDSTATE_RECORDING);
  if (SL_RESULT_SUCCESS != err) {
    HandleError(err);
    return AudioInputStream::OpenOutcome::kFailed;
  }

  LOG(INFO) << "Cobalt: Hardware PRE-STARTED in Open()";
  return AudioInputStream::OpenOutcome::kSuccess;
}

void StarboardAudioInputStream::Start(AudioInputCallback* callback) {

  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);
  DCHECK(recorder_);
  DCHECK(simple_buffer_queue_);
  
  base::AutoLock lock(lock_);
  callback_ = callback;
  started_ = true;
}

void StarboardAudioInputStream::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!started_)
    return;

  base::AutoLock lock(lock_);
  (*recorder_)->SetRecordState(recorder_, SL_RECORDSTATE_STOPPED);
  (*simple_buffer_queue_)->Clear(simple_buffer_queue_);
  started_ = false;
  callback_ = nullptr;
}

void StarboardAudioInputStream::Close() {
  Stop();
  {
    base::AutoLock lock(lock_);
    recorder_object_.Reset();
    simple_buffer_queue_ = nullptr;
    recorder_ = nullptr;
    engine_object_.Reset();
    ReleaseAudioBuffer();
  }
  audio_manager_->ReleaseInputStream(this);
}

double StarboardAudioInputStream::GetMaxVolume() { return 0.0; }
void StarboardAudioInputStream::SetVolume(double volume) {}
double StarboardAudioInputStream::GetVolume() { return 0.0; }
bool StarboardAudioInputStream::SetAutomaticGainControl(bool enabled) { return false; }
bool StarboardAudioInputStream::GetAutomaticGainControl() { return false; }
bool StarboardAudioInputStream::IsMuted() { return false; }
void StarboardAudioInputStream::SetOutputDeviceForAec(const std::string& output_device_id) {}

bool StarboardAudioInputStream::CreateRecorder() {
  SLEngineOption option[] = {
      {SL_ENGINEOPTION_THREADSAFE, static_cast<SLuint32>(SL_BOOLEAN_TRUE)}};
  LOG_ON_FAILURE_AND_RETURN(
      slCreateEngine(engine_object_.Receive(), 1, option, 0, nullptr, nullptr),
      false);

  LOG_ON_FAILURE_AND_RETURN(
      engine_object_->Realize(engine_object_.Get(), SL_BOOLEAN_FALSE), false);

  SLEngineItf engine;
  LOG_ON_FAILURE_AND_RETURN(engine_object_->GetInterface(
                                engine_object_.Get(), SL_IID_ENGINE, &engine),
                            false);

  SLDataLocator_IODevice mic_locator = {SL_DATALOCATOR_IODEVICE,
                                        SL_IODEVICE_AUDIOINPUT,
                                        SL_DEFAULTDEVICEID_AUDIOINPUT, nullptr};
  SLDataSource audio_source = {&mic_locator, nullptr};

  SLDataLocator_AndroidSimpleBufferQueue buffer_queue = {
      SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
      static_cast<SLuint32>(kMaxNumOfBuffersInQueue)};
  SLDataSink audio_sink = {&buffer_queue, &format_};

  const SLInterfaceID interface_id[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                        SL_IID_ANDROIDCONFIGURATION};
  const SLboolean interface_required[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

  LOG_ON_FAILURE_AND_RETURN(
      (*engine)->CreateAudioRecorder(
          engine, recorder_object_.Receive(), &audio_source, &audio_sink,
          std::size(interface_id), interface_id, interface_required),
      false);

  SLAndroidConfigurationItf recorder_config;
  LOG_ON_FAILURE_AND_RETURN(
      recorder_object_->GetInterface(recorder_object_.Get(),
                                     SL_IID_ANDROIDCONFIGURATION,
                                     &recorder_config),
      false);

  // Use VOICE_RECOGNITION preset for low-latency and minimal DSP (Starboard style)
  SLint32 stream_type = SL_ANDROID_RECORDING_PRESET_VOICE_RECOGNITION;
  LOG_ON_FAILURE_AND_RETURN(
      (*recorder_config)->SetConfiguration(recorder_config,
                                           SL_ANDROID_KEY_RECORDING_PRESET,
                                           &stream_type,
                                           sizeof(SLint32)),
      false);

  LOG_ON_FAILURE_AND_RETURN(
      recorder_object_->Realize(recorder_object_.Get(), SL_BOOLEAN_FALSE),
      false);

  LOG_ON_FAILURE_AND_RETURN(
      recorder_object_->GetInterface(
          recorder_object_.Get(), SL_IID_RECORD, &recorder_),
      false);

  LOG_ON_FAILURE_AND_RETURN(
      recorder_object_->GetInterface(recorder_object_.Get(),
                                     SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                     &simple_buffer_queue_),
      false);

  LOG_ON_FAILURE_AND_RETURN(
      (*simple_buffer_queue_)->RegisterCallback(
          simple_buffer_queue_, SimpleBufferQueueCallback, this),
      false);

  return true;
}

void StarboardAudioInputStream::SimpleBufferQueueCallback(
    SLAndroidSimpleBufferQueueItf buffer_queue,
    void* instance) {
  StarboardAudioInputStream* stream =
      reinterpret_cast<StarboardAudioInputStream*>(instance);
  stream->ReadBufferQueue();
}

void StarboardAudioInputStream::ReadBufferQueue() {
  base::AutoLock lock(lock_);

  // If the Renderer isn't ready, we just drop the pre-warm data but keep the loop running.
  if (callback_) {
    // Convert from interleaved format to deinterleaved audio bus format.
    audio_bus_->FromInterleaved<SignedInt16SampleTypeTraits>(
        reinterpret_cast<int16_t*>(audio_data_[active_buffer_index_]),
        audio_bus_->frames());

    callback_->OnData(audio_bus_.get(), base::TimeTicks::Now() - hardware_delay_,
                      0.0, {});
  }

  (*simple_buffer_queue_)->Enqueue(simple_buffer_queue_,
                                   audio_data_[active_buffer_index_],
                                   buffer_size_bytes_);

  active_buffer_index_ = (active_buffer_index_ + 1) % kMaxNumOfBuffersInQueue;
}

void StarboardAudioInputStream::SetupAudioBuffer() {
  DCHECK(!audio_data_[0]);
  for (int i = 0; i < kMaxNumOfBuffersInQueue; ++i) {
    audio_data_[i] = new uint8_t[buffer_size_bytes_];
  }
}

void StarboardAudioInputStream::ReleaseAudioBuffer() {
  if (audio_data_[0]) {
    for (int i = 0; i < kMaxNumOfBuffersInQueue; ++i) {
      delete[] audio_data_[i];
      audio_data_[i] = nullptr;
    }
  }
}

void StarboardAudioInputStream::HandleError(SLresult error) {
  DLOG(ERROR) << "Starboard Input error " << error;
  if (callback_)
    callback_->OnError();
}

}  // namespace media
