// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/android/shared/audio_decoder.h"

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/audio_sink.h"
#include "starboard/log.h"
#include "starboard/memory.h"

// Can be locally set to |1| for verbose audio decoding.  Verbose audio
// decoding will log the following transitions that take place for each audio
// unit:
//   T1: Our client passes an |InputBuffer| of audio data into us.
//   T2: We pass the |InputBuffer| into our |MediaCodecBridge|.
//   T3: We receive a corresponding media codec output buffer back from our
//       |MediaCodecBridge|.
//   T4: Our client reads a corresponding |DecodedAudio| out of us.
//
// Example usage for debugging audio playback:
//   $ adb logcat -c
//   $ adb logcat | tee log.txt
//   # Play video and get to frozen point.
//   $ CTRL-C
//   $ cat log.txt | grep -P 'T3: pts \d+' | wc -l
//   523
//   $ cat log.txt | grep -P 'T4: pts \d+' | wc -l
//   522
//   # Oh no, why isn't our client reading the audio we have ready to go?
//   # Time to go find out...
#define STARBOARD_ANDROID_SHARED_AUDIO_DECODER_VERBOSE 0
#if STARBOARD_ANDROID_SHARED_AUDIO_DECODER_VERBOSE
#define VERBOSE_MEDIA_LOG() SB_LOG(INFO)
#else
#define VERBOSE_MEDIA_LOG() SB_EAT_STREAM_PARAMETERS
#endif

namespace starboard {
namespace android {
namespace shared {

namespace {

const jlong kDequeueTimeout = 0;
const SbTimeMonotonic kUpdateInterval = 10 * kSbTimeMillisecond;

const jint kNoOffset = 0;
const jlong kNoPts = 0;
const jint kNoSize = 0;
const jint kNoBufferFlags = 0;

// TODO: Merge this once Android decoders are unified
const char* GetNameForMediaCodecStatus(jint status) {
  switch (status) {
    case MEDIA_CODEC_OK:
      return "MEDIA_CODEC_OK";
    case MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER:
      return "MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER";
    case MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER:
      return "MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER";
    case MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED:
      return "MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED";
    case MEDIA_CODEC_OUTPUT_FORMAT_CHANGED:
      return "MEDIA_CODEC_OUTPUT_FORMAT_CHANGED";
    case MEDIA_CODEC_INPUT_END_OF_STREAM:
      return "MEDIA_CODEC_INPUT_END_OF_STREAM";
    case MEDIA_CODEC_OUTPUT_END_OF_STREAM:
      return "MEDIA_CODEC_OUTPUT_END_OF_STREAM";
    case MEDIA_CODEC_NO_KEY:
      return "MEDIA_CODEC_NO_KEY";
    case MEDIA_CODEC_INSUFFICIENT_OUTPUT_PROTECTION:
      return "MEDIA_CODEC_INSUFFICIENT_OUTPUT_PROTECTION";
    case MEDIA_CODEC_ABORT:
      return "MEDIA_CODEC_ABORT";
    case MEDIA_CODEC_ERROR:
      return "MEDIA_CODEC_ERROR";
    default:
      SB_NOTREACHED();
      return "MEDIA_CODEC_ERROR_UNKNOWN";
  }
}

SbMediaAudioSampleType GetSupportedSampleType() {
  SB_DCHECK(
      SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeInt16));
  return kSbMediaAudioSampleTypeInt16;
}

void* IncrementPointerByBytes(void* pointer, int offset) {
  return static_cast<uint8_t*>(pointer) + offset;
}

}  // namespace

// TODO: Re-order ctor initialization list according to the declaration order.
AudioDecoder::AudioDecoder(SbMediaAudioCodec audio_codec,
                           const SbMediaAudioHeader& audio_header,
                           SbDrmSystem drm_system)
    : stream_ended_(false),
      audio_codec_(audio_codec),
      audio_header_(audio_header),
      drm_system_(static_cast<DrmSystem*>(drm_system)),
      decoder_thread_(kSbThreadInvalid),
      media_codec_bridge_(NULL),
      sample_type_(GetSupportedSampleType()),
      pending_work_size_(0),
      output_sample_rate_(audio_header.samples_per_second),
      output_channel_count_(audio_header.number_of_channels) {
  if (!InitializeCodec()) {
    SB_LOG(ERROR) << "Failed to initialize audio decoder.";
    TeardownCodec();
    return;
  }
}

AudioDecoder::~AudioDecoder() {
  JoinOnDecoderThread();
  TeardownCodec();
}

void AudioDecoder::Initialize(const Closure& output_cb,
                              const Closure& error_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb.is_valid());
  SB_DCHECK(!output_cb_.is_valid());
  SB_DCHECK(error_cb.is_valid());
  SB_DCHECK(!error_cb_.is_valid());

  output_cb_ = output_cb;
  error_cb_ = error_cb;
}

void AudioDecoder::Decode(const scoped_refptr<InputBuffer>& input_buffer,
                          const Closure& consumed_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffer);
  SB_DCHECK(output_cb_.is_valid());

  if (stream_ended_) {
    SB_LOG(ERROR) << "Decode() is called after WriteEndOfStream() is called.";
    return;
  }

  if (!SbThreadIsValid(decoder_thread_)) {
    decoder_thread_ =
        SbThreadCreate(0, kSbThreadPriorityNormal, kSbThreadNoAffinity, true,
                       "audio_decoder", &AudioDecoder::ThreadEntryPoint, this);
    SB_DCHECK(SbThreadIsValid(decoder_thread_));
  }

  VERBOSE_MEDIA_LOG() << "T1: pts " << input_buffer->pts();
  event_queue_.PushBack(Event(input_buffer));

  ScopedLock lock(decoded_audios_mutex_);
  if (event_queue_.size() + SbAtomicNoBarrier_Load(&pending_work_size_) +
          decoded_audios_.size() <=
      kMaxPendingWorkSize) {
    Schedule(consumed_cb);
  } else {
    consumed_cb_ = consumed_cb;
  }
}

void AudioDecoder::WriteEndOfStream() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_.is_valid());
  stream_ended_ = true;
  event_queue_.PushBack(Event(Event::kWriteEndOfStream));
}

scoped_refptr<AudioDecoder::DecodedAudio> AudioDecoder::Read() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_.is_valid());

  scoped_refptr<DecodedAudio> result;
  {
    starboard::ScopedLock lock(decoded_audios_mutex_);
    SB_DCHECK(!decoded_audios_.empty());
    if (!decoded_audios_.empty()) {
      result = decoded_audios_.front();
      VERBOSE_MEDIA_LOG() << "T4: pts " << result->pts();
      decoded_audios_.pop();
    }
  }

  if (consumed_cb_.is_valid()) {
    Schedule(consumed_cb_);
    consumed_cb_.reset();
  }
  return result;
}

void AudioDecoder::Reset() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_.is_valid());

  JoinOnDecoderThread();
  TeardownCodec();
  if (!InitializeCodec()) {
    // TODO: Communicate this failure to our clients somehow.
    SB_LOG(ERROR) << "Failed to initialize codec after reset.";
  }

  consumed_cb_.reset();
  stream_ended_ = false;
  pending_work_size_ = 0;

  while (!decoded_audios_.empty()) {
    decoded_audios_.pop();
  }

  CancelPendingJobs();
}

// static
void* AudioDecoder::ThreadEntryPoint(void* context) {
  SB_DCHECK(context);
  AudioDecoder* decoder = static_cast<AudioDecoder*>(context);
  decoder->DecoderThreadFunc();
  return NULL;
}

void AudioDecoder::DecoderThreadFunc() {
  // A second staging queue that holds events of type |kWriteInputBuffer| and
  // |kWriteEndOfStream| only, and is only accessed from the decoder thread,
  // so it does not need to be thread-safe.
  std::deque<Event> pending_work;

  // TODO: Revisit |stream_ended_| logic.  It should be guarded by a lock and
  // a flag should be also set inside WriteEndOfStream().
  for (;;) {
    // TODO: Replace |event_queue_| with a plain locked std::queue and only call
    // swap() when |pending_work| is empty to avoid unnecessary locks.
    Event event = event_queue_.PollFront();

    if (event.type == Event::kWriteInputBuffer ||
        event.type == Event::kWriteEndOfStream ||
        event.type == Event::kWriteCodecConfig) {
      pending_work.push_back(event);
      SbAtomicNoBarrier_Store(&pending_work_size_, pending_work.size());
    }

    if (event.type == Event::kReset) {
      SB_LOG(INFO) << "Reset event occurred.";
      jint status = media_codec_bridge_->Flush();
      if (status != MEDIA_CODEC_OK) {
        SB_LOG(ERROR) << "Failed to flush video media codec.";
      }
      return;
    }

    bool did_work = false;
    did_work |= ProcessOneInputBuffer(&pending_work);
    SbAtomicNoBarrier_Store(&pending_work_size_, pending_work.size());
    did_work |= DequeueAndProcessOutputBuffer();

    if (event.type == Event::kInvalid && !did_work) {
      SbThreadSleep(kSbTimeMillisecond);
      continue;
    }
  }
}

void AudioDecoder::JoinOnDecoderThread() {
  if (SbThreadIsValid(decoder_thread_)) {
    event_queue_.Clear();
    event_queue_.PushBack(Event(Event::kReset));
    SbThreadJoin(decoder_thread_, NULL);
    event_queue_.Clear();
  }
  decoder_thread_ = kSbThreadInvalid;
}

bool AudioDecoder::InitializeCodec() {
  jobject j_media_crypto = drm_system_ ? drm_system_->GetMediaCrypto() : NULL;
  SB_DCHECK(!drm_system_ || j_media_crypto);
  media_codec_bridge_ = MediaCodecBridge::CreateAudioMediaCodecBridge(
      audio_codec_, audio_header_, j_media_crypto);
  if (!media_codec_bridge_) {
    return false;
  }
  if (audio_header_.audio_specific_config_size > 0) {
    event_queue_.PushBack(Event(Event::kWriteCodecConfig));
  }
  return true;
}

void AudioDecoder::TeardownCodec() {
  media_codec_bridge_.reset();
}

bool AudioDecoder::ProcessOneInputBuffer(std::deque<Event>* pending_work) {
  SB_DCHECK(pending_work);
  if (pending_work->empty()) {
    return false;
  }

  SB_CHECK(media_codec_bridge_);

  // During secure playback, and only secure playback, is is possible that our
  // attempt to enqueue an input buffer will be rejected by MediaCodec because
  // we do not have a key yet.  In this case, we hold on to the input buffer
  // that we have already set up, and repeatedly attempt to enqueue it until
  // it works.  Ideally, we would just wait until MediaDrm was ready, however
  // the shared starboard player framework assumes that it is possible to
  // perform decryption and decoding as separate steps, so from its
  // perspective, having made it to this point implies that we ready to
  // decode.  It is not possible to do them as separate steps on Android. From
  // the perspective of user application, decryption and decoding are one
  // atomic step.
  DequeueInputResult dequeue_input_result;
  Event event;
  bool input_buffer_already_written = false;
  if (pending_queue_input_buffer_task_) {
    dequeue_input_result =
        pending_queue_input_buffer_task_->dequeue_input_result;
    SB_DCHECK(dequeue_input_result.index >= 0);
    event = pending_queue_input_buffer_task_->event;
    pending_queue_input_buffer_task_ = nullopt_t();
    input_buffer_already_written = true;
  } else {
    dequeue_input_result =
        media_codec_bridge_->DequeueInputBuffer(kDequeueTimeout);
    event = pending_work->front();
    if (dequeue_input_result.index < 0) {
      HandleError("dequeueInputBuffer", dequeue_input_result.status);
      return false;
    }
    pending_work->pop_front();
  }

  SB_DCHECK(event.type == Event::kWriteCodecConfig ||
            event.type == Event::kWriteInputBuffer ||
            event.type == Event::kWriteEndOfStream);
  const scoped_refptr<InputBuffer>& input_buffer = event.input_buffer;
  if (event.type == Event::kWriteEndOfStream) {
    SB_DCHECK(pending_work->empty());
  }
  const void* data = NULL;
  int size = 0;
  if (event.type == Event::kWriteCodecConfig) {
    data = audio_header_.audio_specific_config;
    size = audio_header_.audio_specific_config_size;
  } else if (event.type == Event::kWriteInputBuffer) {
    data = input_buffer->data();
    size = input_buffer->size();
    VERBOSE_MEDIA_LOG() << "T2: pts " << input_buffer->pts();
  } else if (event.type == Event::kWriteEndOfStream) {
    data = NULL;
    size = 0;
  }

  // Don't bother rewriting the same data if we already did it last time we
  // were called and had it stored in |pending_queue_input_buffer_task_|.
  if (!input_buffer_already_written && event.type != Event::kWriteEndOfStream) {
    ScopedJavaByteBuffer byte_buffer(
        media_codec_bridge_->GetInputBuffer(dequeue_input_result.index));
    if (byte_buffer.IsNull() || byte_buffer.capacity() < size) {
      SB_LOG(ERROR) << "Unable to write to MediaCodec input buffer.";
      return false;
    }
    byte_buffer.CopyInto(data, size);
  }

  jint status;
  if (event.type == Event::kWriteCodecConfig) {
    status = media_codec_bridge_->QueueInputBuffer(dequeue_input_result.index,
                                                   kNoOffset, size, kNoPts,
                                                   BUFFER_FLAG_CODEC_CONFIG);
  } else if (event.type == Event::kWriteInputBuffer) {
    jlong pts_us = ConvertSbMediaTimeToMicroseconds(input_buffer->pts());
    if (drm_system_ && input_buffer->drm_info()) {
      status = media_codec_bridge_->QueueSecureInputBuffer(
          dequeue_input_result.index, kNoOffset, *input_buffer->drm_info(),
          pts_us);
    } else {
      status = media_codec_bridge_->QueueInputBuffer(
          dequeue_input_result.index, kNoOffset, size, pts_us, kNoBufferFlags);
    }
  } else {
    status = media_codec_bridge_->QueueInputBuffer(dequeue_input_result.index,
                                                   kNoOffset, size, kNoPts,
                                                   BUFFER_FLAG_END_OF_STREAM);
  }

  if (status != MEDIA_CODEC_OK) {
    HandleError("queue(Secure)?InputBuffer", status);
    // TODO: Stop the decoding loop on fatal error.
    SB_DCHECK(!pending_queue_input_buffer_task_);
    pending_queue_input_buffer_task_ = {dequeue_input_result, event};
    return false;
  }

  return true;
}

bool AudioDecoder::DequeueAndProcessOutputBuffer() {
  SB_CHECK(media_codec_bridge_);

  DequeueOutputResult dequeue_output_result =
      media_codec_bridge_->DequeueOutputBuffer(kDequeueTimeout);

  // Note that if the |index| field of |DequeueOutputResult| is negative, then
  // all fields other than |status| and |index| are invalid.  This is
  // especially important, as the Java side of |MediaCodecBridge| will reuse
  // objects for returned results behind the scenes.
  if (dequeue_output_result.index < 0) {
    if (dequeue_output_result.status == MEDIA_CODEC_OUTPUT_FORMAT_CHANGED) {
      RefreshOutputFormat();
      return true;
    }

    if (dequeue_output_result.status == MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED) {
      SB_DLOG(INFO) << "Output buffers changed, trying to dequeue again.";
      return true;
    }

    HandleError("dequeueOutputBuffer", dequeue_output_result.status);
    return false;
  }

  ProcessOutputBuffer(dequeue_output_result);
  return true;
}

void AudioDecoder::ProcessOutputBuffer(
    const DequeueOutputResult& dequeue_output_result) {
  SB_DCHECK(media_codec_bridge_);
  SB_DCHECK(output_cb_.is_valid());
  SB_DCHECK(dequeue_output_result.index >= 0);

  if (dequeue_output_result.flags & BUFFER_FLAG_END_OF_STREAM) {
    media_codec_bridge_->ReleaseOutputBuffer(dequeue_output_result.index,
                                             false);
    {
      starboard::ScopedLock lock(decoded_audios_mutex_);
      decoded_audios_.push(new DecodedAudio());
    }

    Schedule(output_cb_);
    return;
  }

  ScopedJavaByteBuffer byte_buffer(
      media_codec_bridge_->GetOutputBuffer(dequeue_output_result.index));
  SB_DCHECK(!byte_buffer.IsNull());

  if (dequeue_output_result.num_bytes > 0) {
    int16_t* data = static_cast<int16_t*>(IncrementPointerByBytes(
        byte_buffer.address(), dequeue_output_result.offset));
    int size = dequeue_output_result.num_bytes;
    if (2 * audio_header_.samples_per_second == output_sample_rate_) {
      // The audio is encoded using implicit HE-AAC.  As the audio sink has
      // been created already we try to down-mix the decoded data to half of
      // its channels so the audio sink can play it with the correct pitch.
      for (int i = 0; i < size / sizeof(int16_t); i++) {
        data[i / 2] = (static_cast<int32_t>(data[i]) +
                       static_cast<int32_t>(data[i + 1]) / 2);
      }
      size /= 2;
    }

    scoped_refptr<DecodedAudio> decoded_audio = new DecodedAudio(
        audio_header_.number_of_channels, GetSampleType(), GetStorageType(),
        ConvertMicrosecondsToSbMediaTime(
            dequeue_output_result.presentation_time_microseconds),
        size);

    SbMemoryCopy(decoded_audio->buffer(), data, size);
    {
      starboard::ScopedLock lock(decoded_audios_mutex_);
      decoded_audios_.push(decoded_audio);
      VERBOSE_MEDIA_LOG() << "T3: pts " << decoded_audios_.front()->pts();
    }
    Schedule(output_cb_);
  }

  media_codec_bridge_->ReleaseOutputBuffer(dequeue_output_result.index, false);
}

void AudioDecoder::RefreshOutputFormat() {
  AudioOutputFormatResult output_format =
      media_codec_bridge_->GetAudioOutputFormat();
  if (output_format.status == MEDIA_CODEC_ERROR) {
    SB_LOG(ERROR) << "|getOutputFormat| failed";
    return;
  }
  output_sample_rate_ = output_format.sample_rate;
  output_channel_count_ = output_format.channel_count;
}

void AudioDecoder::HandleError(const char* action_name, jint status) {
  SB_DCHECK(status != MEDIA_CODEC_OK);

  bool retry = false;
  if (status == MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER) {
    // Don't bother logging a try again later status, it happens a lot.
    return;
  } else if (status == MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER) {
    // Don't bother logging a try again later status, it will happen a lot.
    return;
  } else if (status == MEDIA_CODEC_NO_KEY) {
    retry = true;
  } else if (status == MEDIA_CODEC_INSUFFICIENT_OUTPUT_PROTECTION) {
    drm_system_->OnInsufficientOutputProtection();
  } else {
    error_cb_.Run();
  }

  if (retry) {
    SB_LOG(INFO) << "|" << action_name << "| failed with status: "
                 << GetNameForMediaCodecStatus(status)
                 << ", will try again after a delay.";
  } else {
    SB_LOG(ERROR) << "|" << action_name << "| failed with status: "
                  << GetNameForMediaCodecStatus(status) << ".";
  }
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
