// Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/shared/opensles/opensles_audio_sink_type.h"

#include <algorithm>
#include <vector>
#include <queue> // @todo: use starboard queue ? see player_worker.cc for e.g.

#include "starboard/condition_variable.h"
#include "starboard/memory.h"
#include "starboard/mutex.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "starboard/audio_sink.h"
#include "starboard/log.h"
#include <android/log.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))

#define LOG_ON_FAILURE(op, ...)              \
  do {                                                  \
      SLresult err = (op);                              \
      if (err != SL_RESULT_SUCCESS) {                   \
          SB_DLOG(ERROR) << #op << " GB: failed: " << err;  \
          LOGI("GB: %s %d: error = %d", __FUNCTION__, __LINE__, err); \
      }                                                 \
  } while(0)


static const int kMaxNumOfBuffersInQueue = 2;
static const int AUDIO_DATA_STORAGE_SIZE = 4096 * 100;

// in number of samples, native audio buffer. Also SLES queue buffer size //
// 1024 bytes here ; including channels
static const int AUDIO_DATA_SAMPLES_PER_BUFFER = 512;

// total frames per request (all channels included) . 2 below is channels_
static const int kMaxFramesToWritePerChannel = AUDIO_DATA_SAMPLES_PER_BUFFER/2;

typedef struct EnqueuedBufferData_ {
    uint8_t buffer[AUDIO_DATA_SAMPLES_PER_BUFFER * 2];
    uint32_t framesEnqueued;
} EnqueuedBufferData;

namespace starboard {
namespace shared {
namespace opensles {
namespace {

// Helper function to compute the size of the two valid starboard audio sample
// types.
size_t GetSampleSize(SbMediaAudioSampleType sample_type) {
  switch (sample_type) {
    case kSbMediaAudioSampleTypeFloat32:
      return sizeof(float);
    case kSbMediaAudioSampleTypeInt16:
      return sizeof(int16_t);
  }
  SB_NOTREACHED();
  return 0u;
}

void* IncrementPointerByBytes(void* pointer, size_t offset) {
  return static_cast<void*>(static_cast<uint8_t*>(pointer) + offset);
}

class OpenSLESAudioSink : public SbAudioSinkPrivate {
 public:
  OpenSLESAudioSink(Type* type,
                    int channels,
                    int sampling_frequency_hz,
                    SbMediaAudioSampleType sample_type,
                    SbAudioSinkFrameBuffers frame_buffers,
                    int frames_per_channel,
                    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
                    SbAudioSinkConsumeFramesFunc consume_frame_func,
                    void* context, SLObjectItf engineObj,
                    SLEngineItf engineItf, SLObjectItf outputMix);
  ~OpenSLESAudioSink() SB_OVERRIDE;

  bool IsType(Type* type) SB_OVERRIDE { return type_ == type; }

  bool is_valid() { return playItf_ != NULL; }

  void FillBufferQueue();

  SLint16 pcmData[AUDIO_DATA_STORAGE_SIZE];

  std::queue<EnqueuedBufferData*> bufferQ_;

 private:
  void WriteFrames(int, int);
  int WriteFrames1(int, int);
  void WriteSilenceBuffers(int num_buffers);

  static void* ThreadEntryPoint(void* context);
  void AudioThreadFunc();
  bool destroying_;

  int sinkTid_;

  SbThread audio_out_thread_;
  starboard::Mutex mutex_;
  starboard::ConditionVariable creation_signal_;
  starboard::ConditionVariable cv_switch_playstate_;


  Type* type_;
  SbAudioSinkUpdateSourceStatusFunc update_source_status_func_;
  SbAudioSinkConsumeFramesFunc consume_frame_func_;
  void* context_;

  int channels_;
  int sampling_frequency_hz_;
  SbMediaAudioSampleType sample_type_;
  void* frame_buffer_;
  int frames_per_channel_;


  SbTime time_to_wait_;

  SLObjectItf engineObj_;
  SLEngineItf engineItf_;

  SLObjectItf outputMix_;

  SLObjectItf audioPlayer_;
  SLAndroidSimpleBufferQueueItf simpleBufferQueueItf_;
  SLPlayItf playItf_;

  SLint16* startAddress_;


};

OpenSLESAudioSink::OpenSLESAudioSink(Type* type,
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType sample_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkConsumeFramesFunc consume_frame_func,
    void* context, SLObjectItf engineObj, SLEngineItf engineItf,
    SLObjectItf outputMix)
    : type_(type),
      channels_(channels),
      sampling_frequency_hz_(sampling_frequency_hz),
      sample_type_(sample_type),
      frame_buffer_(frame_buffers[0]),
      frames_per_channel_(frames_per_channel),
      time_to_wait_( kMaxFramesToWritePerChannel * kSbTimeSecond / sampling_frequency_hz),
      update_source_status_func_(update_source_status_func),
      consume_frame_func_(consume_frame_func),
      context_(context),
      destroying_(false),
      creation_signal_(mutex_),
      cv_switch_playstate_(mutex_),
      engineObj_(engineObj),
      engineItf_(engineItf),
      outputMix_(outputMix) {
   SB_DCHECK(update_source_status_func_);
   SB_DCHECK(consume_frame_func_);
   SB_DCHECK(frame_buffer_);
   SB_DCHECK(SbAudioSinkIsAudioSampleTypeSupported(sample_type_));

   LOGI("opensles: @ %s %d sample_type = %d", __FUNCTION__, __LINE__, sample_type);

   ScopedLock lock(mutex_);
   audio_out_thread_ =
      SbThreadCreate(0, kSbThreadPriorityRealTime, kSbThreadNoAffinity, true,
                      "opensles_audio_out", &OpenSLESAudioSink::ThreadEntryPoint, this);
   SB_DCHECK(SbThreadIsValid(audio_out_thread_));
   creation_signal_.Wait();

}

// Destructor
OpenSLESAudioSink::~OpenSLESAudioSink() {
  {
    // Wait for play etc to be over
    ScopedLock lock(mutex_);
    LOGI("opensles @@@ %s %d: tid = %d ", __FUNCTION__, __LINE__, sinkTid_);
    destroying_ = true;
  }

  // Join thread(s)
  SbThreadJoin(audio_out_thread_, NULL);
  // Stop player
  LOG_ON_FAILURE((*playItf_)->SetPlayState(playItf_, SL_PLAYSTATE_STOPPED));
  // Clear Buffer Queue
  LOG_ON_FAILURE((*simpleBufferQueueItf_)->Clear(simpleBufferQueueItf_));
  simpleBufferQueueItf_ = NULL;

  // Destroy player
  (*audioPlayer_)->Destroy(audioPlayer_);
  audioPlayer_ = NULL;
  playItf_ = NULL;

  if (bufferQ_.size())
      LOGI("GB: BUG BUG size = %d", bufferQ_.size());

  LOGI("GB @@@ %s: %d:  tid = %d DONE", __FUNCTION__, __LINE__, sinkTid_);
}

// static
void* OpenSLESAudioSink::ThreadEntryPoint(void* context) {
  SB_DCHECK(context);
  OpenSLESAudioSink* sink = reinterpret_cast<OpenSLESAudioSink*>(context);
  sink->AudioThreadFunc();

  return NULL;
}

void SimpleBufferQueueCallback(
 SLAndroidSimpleBufferQueueItf bufferQueueItf,
 void *instance) {

 OpenSLESAudioSink *sink = reinterpret_cast<OpenSLESAudioSink*>(instance);

 sink->FillBufferQueue();
}

void OpenSLESAudioSink::FillBufferQueue() {
  // check if the player is in the playing state (should be)
  SLuint32 state;
  LOG_ON_FAILURE((*playItf_)->GetPlayState(playItf_, &state));
  if (state != SL_PLAYSTATE_PLAYING) {
     if (bufferQ_.size()) {
         delete bufferQ_.front();
         bufferQ_.pop();
     }
     LOGI("opensles: Player not in playing state, returning from Cb");
     return;
  }

  // SLES buffer queue has never more than 2 members, we're about to queue the
  // second running member below, if the size > 1, that means, this call back is
  // from the play back of the front member of the queue
  if (bufferQ_.size()) {
    delete bufferQ_.front();
    bufferQ_.pop();
  }

  int frames_in_buffer = -1, offset_in_frames = -1;
  bool is_playing = false, is_eos_reached = false;

  update_source_status_func_(&frames_in_buffer, &offset_in_frames,
    &is_playing, &is_eos_reached, context_);

  if (!is_playing || !frames_in_buffer) {
    SLAndroidSimpleBufferQueueState state;
    LOG_ON_FAILURE((*simpleBufferQueueItf_)->GetState(simpleBufferQueueItf_, &state));
    LOGI("GB: SLES Queue state = count = %d index = %d", state.count, state.index);
    if (!bufferQ_.size() && !state.count) {
      LOGI("GB: %s: %d: waking up tid = %d", __FUNCTION__, __LINE__, sinkTid_);
      cv_switch_playstate_.Signal();
    }
    return;
  }

  ScopedLock lock(mutex_);
  int frames_consumed = WriteFrames1(frames_in_buffer, offset_in_frames);

  if (frames_consumed > 0) {
     consume_frame_func_(frames_consumed, context_);
  }
}

void OpenSLESAudioSink::AudioThreadFunc() {
 sinkTid_ = SbThreadGetId();

 // Setup the data source structure
 SLDataLocator_AndroidSimpleBufferQueue simple_buffer_queue = {
     SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
     kMaxNumOfBuffersInQueue
 };

 SLDataFormat_PCM format;

 // Setup format of the content
 format.formatType = SL_DATAFORMAT_PCM;
 format.numChannels = channels_;
 LOGI("opensles: sampling freq = %d", sampling_frequency_hz_);
 format.samplesPerSec = sampling_frequency_hz_ * 1000;
 format.bitsPerSample = GetSampleSize(sample_type_) * 8;
 format.containerSize = format.bitsPerSample;
 format.endianness = SL_BYTEORDER_LITTLEENDIAN;
 format.channelMask = (format.numChannels == 2) ?
     (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT) :
     (SL_SPEAKER_FRONT_CENTER);

 SLDataSource audioSource = { &simple_buffer_queue, &format };

 // Setup Data Sink structure
 SLDataLocator_OutputMix locator_output_mix = { SL_DATALOCATOR_OUTPUTMIX, outputMix_ };

 SLDataSink audioSink = { &locator_output_mix, NULL };

 // Create audio player
 SLInterfaceID iidArray[] = { SL_IID_BUFFERQUEUE };
 SLboolean required[] = { SL_BOOLEAN_TRUE };

 LOG_ON_FAILURE((*engineItf_)->CreateAudioPlayer(engineItf_, &audioPlayer_,
             &audioSource, &audioSink, 1 /* size of iidArray */, iidArray, required));

 // Realize player in synchronous mode
 LOG_ON_FAILURE((*audioPlayer_)->Realize(audioPlayer_, SL_BOOLEAN_FALSE));

 // get player Interface
 LOG_ON_FAILURE((*audioPlayer_)->GetInterface(audioPlayer_, SL_IID_PLAY, (void*)&playItf_));

 // get buffer queue Itf
 LOG_ON_FAILURE((*audioPlayer_)->GetInterface(audioPlayer_, SL_IID_BUFFERQUEUE, (void*)&simpleBufferQueueItf_));

 // Register Call back with Buffer Queue
 LOG_ON_FAILURE((*simpleBufferQueueItf_)->RegisterCallback(simpleBufferQueueItf_,
             SimpleBufferQueueCallback, this));

 ////////////// Audio Player created and setup completed ///////////

 startAddress_ = pcmData;

 LOGI("GB: startAddress = %p", startAddress_);

 SLint32 maxDataToWrite = channels_ * kMaxFramesToWritePerChannel * GetSampleSize(sample_type_);

 creation_signal_.Signal();

 while(1) {
  {
    ScopedTryLock lock(mutex_);
    if (lock.is_locked()) {
      if (destroying_) {
         LOG_ON_FAILURE((*playItf_)->SetPlayState(playItf_, SL_PLAYSTATE_STOPPED));
         LOGI("GB: destroying tid = %d", sinkTid_);
         return;
      }
    }
  }
  int frames_in_buffer = -1, offset_in_frames = -1;
  bool is_playing = false, is_eos_reached = false;

  update_source_status_func_(&frames_in_buffer, &offset_in_frames,
                             &is_playing, &is_eos_reached, context_);

  if (is_playing && frames_in_buffer > 0) {
    // enqueue a silence frame to maintain 2 buffers in SLES q
    ScopedLock lock(mutex_);
    WriteSilenceBuffers(1);
    
    int frames_consumed = WriteFrames1(frames_in_buffer, offset_in_frames);

    if (frames_consumed > 0) {
//     LOGI("GB: @@ Cb: %s %d:: Frames consumed = %d", __FUNCTION__, __LINE__, frames_consumed);
     consume_frame_func_(frames_consumed, context_);
    }

    // Set into playing state
    LOG_ON_FAILURE((*playItf_)->SetPlayState(playItf_, SL_PLAYSTATE_PLAYING));

    // go into wait, will be woken up when the player is paused
    cv_switch_playstate_.Wait();

    LOGI("GB %s %d, tid = %d woken up", __FUNCTION__, __LINE__, sinkTid_);
 
    LOG_ON_FAILURE((*playItf_)->SetPlayState(playItf_, SL_PLAYSTATE_STOPPED));

    LOG_ON_FAILURE((*simpleBufferQueueItf_)->Clear(simpleBufferQueueItf_));

  } else {
    LOG_ON_FAILURE((*playItf_)->SetPlayState(playItf_, SL_PLAYSTATE_STOPPED));
    // SetPlayState to PAUSE or STOP?
  }
  // Sleep for the periodic callback to updateSourceStatusFunc
  SbThreadSleep(time_to_wait_);
 }
}

void OpenSLESAudioSink::WriteSilenceBuffers(int num_buffers) {

  SLint32 maxDataToWrite = channels_ * kMaxFramesToWritePerChannel * GetSampleSize(sample_type_);
  SLint16* startAddress = pcmData;
  for (int i = 0; i < num_buffers; i++) {
    EnqueuedBufferData *pBufferData = new EnqueuedBufferData();

    pBufferData->framesEnqueued = kMaxFramesToWritePerChannel;

    LOG_ON_FAILURE((*simpleBufferQueueItf_)->Enqueue(simpleBufferQueueItf_,
      (void *)startAddress, maxDataToWrite));
    startAddress = (SLint16 *)((char*)startAddress_ + maxDataToWrite);
    bufferQ_.push(pBufferData);
  }

}


int OpenSLESAudioSink::WriteFrames1(int frames_in_buffer, int offset_in_frames) {

  // we should not write more than the native audio buffer size
  int frames_to_write = std::min(frames_in_buffer, kMaxFramesToWritePerChannel);

  // contiguous buffers to the end of the circular queuue
  int frames_to_buffer_end = frames_per_channel_ - offset_in_frames;

  // can only write till the buffer end at max as src is a circular queue
  frames_to_write = std::min(frames_to_write, frames_to_buffer_end);

  if (!frames_to_write) {
      LOGI("GB: %s %d: frames_to_buffer_end = %d", __FUNCTION__, __LINE__,
              frames_to_buffer_end);
      return 0;
  }
  
  int data_to_write = frames_to_write * GetSampleSize(sample_type_) * channels_;

  //@todo ASSERT data_to_write <= AUDIO_DATA_SAMPLES_PER_BUFFER * 2

  // Copy internally before enqueuing, as we'll report right after enqueuing
  // that we've consumed these frames, so client might free/overwrite src frames
  EnqueuedBufferData *pBufferData = new EnqueuedBufferData();

  pBufferData->framesEnqueued = frames_to_write;

  SbMemoryCopy(&pBufferData->buffer, IncrementPointerByBytes(frame_buffer_,
    offset_in_frames * channels_ * GetSampleSize(sample_type_)), data_to_write);

  // EnqueuedBufferData node prepared, push to the queue
  bufferQ_.push(pBufferData);

    LOG_ON_FAILURE((*simpleBufferQueueItf_)->Enqueue(simpleBufferQueueItf_,
      &pBufferData->buffer, data_to_write));

  return frames_to_write;
}

} // namespace


SbAudioSink OpenSLESAudioSinkType::Create(
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType audio_sample_type,
    SbMediaAudioFrameStorageType audio_frame_storage_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkConsumeFramesFunc consume_frames_func,
    void* context) {

  SB_DCHECK(update_source_status_func);
  SB_DCHECK(consume_frames_func);
  SB_DCHECK(frame_buffers);
  SB_DCHECK(SbAudioSinkIsAudioSampleTypeSupported(audio_sample_type));

  if (!update_source_status_func || !consume_frames_func || !frame_buffers)
      return NULL;

  if (!SbAudioSinkIsAudioSampleTypeSupported(audio_sample_type))
      return NULL;

  if (!SbAudioSinkGetNearestSupportedSampleFrequency(sampling_frequency_hz))
      return NULL;

  if (!engineObj_ || !engineItf_ || !outputMix_) {
    LOGI("opensles: EngineObj = %p, EngineItf = %p outputMix_ = %p:: returning",
          engineObj_, engineItf_, outputMix_);
    return NULL;
  }

  OpenSLESAudioSink* audio_sink = new OpenSLESAudioSink(
      this, channels, sampling_frequency_hz, audio_sample_type, frame_buffers,
      frames_per_channel, update_source_status_func, consume_frames_func,
      context, engineObj_, engineItf_, outputMix_);

  if (!audio_sink->is_valid()) {
    LOGI("opensles: @@@ Invalid SINK %s %d", __FUNCTION__, __LINE__);
    delete audio_sink;
    return kSbAudioSinkInvalid;
  }
  LOGI("opensles: @@@ %s %d", __FUNCTION__, __LINE__);

  return audio_sink;
}

}  // namespace opensles
}  // namespace shared
}  // namespace starboard

int SbAudioSinkGetMaxChannels() {
    return 2;
}

// @todo: implement properly
int SbAudioSinkGetNearestSupportedSampleFrequency(int sampling_frequency_hz) {
    if (sampling_frequency_hz <= 0) {
        SB_LOG(ERROR) << "Invalid audio sampling frequency "
                      <<sampling_frequency_hz;
        return 1;
    }
    return sampling_frequency_hz;
}

bool SbAudioSinkIsAudioSampleTypeSupported(
    SbMediaAudioSampleType audio_sample_type) {
    return audio_sample_type == kSbMediaAudioSampleTypeInt16;
}

bool SbAudioSinkIsAudioFrameStorageTypeSupported(
    SbMediaAudioFrameStorageType audio_frame_storage_type) {
  return audio_frame_storage_type == kSbMediaAudioFrameStorageTypeInterleaved;
}

namespace {
SbAudioSinkPrivate::Type* opensles_audio_sink_type_;
}  // namespace

starboard::shared::opensles::OpenSLESAudioSinkType::OpenSLESAudioSinkType() {
 // Create SLEngine here
 SLresult res;

 SLEngineOption engineOption[] = {
     (SLuint32) SL_ENGINEOPTION_THREADSAFE,
     (SLuint32) SL_BOOLEAN_TRUE
 };

 LOG_ON_FAILURE(slCreateEngine(&engineObj_, 1, engineOption, 0, NULL, NULL));

 // Realize Engine in Sync mode
 LOG_ON_FAILURE((*engineObj_)->Realize(engineObj_, SL_BOOLEAN_FALSE));

 // Get the SL Engine Interface
 LOG_ON_FAILURE((*engineObj_)->GetInterface(engineObj_, SL_IID_ENGINE, (void*)&engineItf_));

 // Create output mix object
 LOG_ON_FAILURE((*engineItf_)->CreateOutputMix(engineItf_, &outputMix_, 0, NULL, NULL));

 // Realize output mix
 LOG_ON_FAILURE((*outputMix_)->Realize(outputMix_, SL_BOOLEAN_FALSE));

 LOGI("opensles: EngineObj = %p, EngineItf = %p OutputMix = %p", engineObj_, engineItf_, outputMix_);

}

starboard::shared::opensles::OpenSLESAudioSinkType::~OpenSLESAudioSinkType() {
  // Destroy Mix Object
  (*outputMix_)->Destroy(outputMix_);
  outputMix_ = NULL;
  // Destroy Engine
  (*engineObj_)->Destroy(engineObj_);
  engineObj_ = NULL;
  engineItf_ = NULL;
  LOGI("opensles: %s %d :: Destroyed Engine", __FUNCTION__, __LINE__);
}

void SbAudioSinkPrivate::PlatformInitialize() {
  LOGI("opensles: at %s %d", __FUNCTION__, __LINE__);
  SB_DCHECK(!opensles_audio_sink_type_);
  opensles_audio_sink_type_ = new starboard::shared::opensles::OpenSLESAudioSinkType;
  SetPrimaryType(opensles_audio_sink_type_);
  EnableFallbackToStub();
}

// static
void SbAudioSinkPrivate::PlatformTearDown() {
  SB_DCHECK(opensles_audio_sink_type_ == GetPrimaryType());
  SetPrimaryType(NULL);
  delete opensles_audio_sink_type_;
  opensles_audio_sink_type_ = NULL;
}
