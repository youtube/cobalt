// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/win32/win32_audio_decoder.h"

#include <algorithm>
#include <queue>

#include "starboard/atomic.h"
#include "starboard/shared/starboard/thread_checker.h"
#include "starboard/shared/win32/atomic_queue.h"
#include "starboard/shared/win32/audio_decoder.h"
#include "starboard/shared/win32/audio_transform.h"
#include "starboard/shared/win32/decrypting_decoder.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/media_foundation_utils.h"
#include "starboard/shared/win32/wasapi_include.h"

namespace starboard {
namespace shared {
namespace win32 {

using Microsoft::WRL::ComPtr;
using ::starboard::shared::starboard::ThreadChecker;
using ::starboard::shared::win32::CreateAudioTransform;

const size_t kAacSamplesPerFrame = 1024;
// We are using float samples for AAC on Xb1.
const size_t kAacBytesPerSample = sizeof(float);

namespace {
size_t GetExpectedBufferSize(SbMediaAudioCodec codec, int num_channels) {
  switch (codec) {
    case kSbMediaAudioCodecAac:
      return num_channels * kAacSamplesPerFrame * kAacBytesPerSample;
    case kSbMediaAudioCodecAc3:
      return kAc3BufferSize;
    case kSbMediaAudioCodecEac3:
      return kEac3BufferSize;
    default:
      SB_NOTREACHED();
      return size_t(0);
  }
}

class AbstractWin32AudioDecoderImpl : public AbstractWin32AudioDecoder {
 public:
  AbstractWin32AudioDecoderImpl(SbMediaAudioCodec codec,
                                SbMediaAudioFrameStorageType audio_frame_fmt,
                                SbMediaAudioSampleType sample_type,
                                const SbMediaAudioSampleInfo& audio_sample_info,
                                SbDrmSystem drm_system)
      : thread_checker_(ThreadChecker::kSetThreadIdOnFirstCheck),
        codec_(codec),
        audio_frame_fmt_(audio_frame_fmt),
        sample_type_(sample_type),
        number_of_channels_(audio_sample_info.number_of_channels),
        heaac_detected_(false),
        expected_buffer_size_(
            GetExpectedBufferSize(codec,
                                  audio_sample_info.number_of_channels)) {
    scoped_ptr<MediaTransform> audio_decoder =
        CreateAudioTransform(audio_sample_info, codec_);
    impl_.reset(
        new DecryptingDecoder("audio", audio_decoder.Pass(), drm_system));
    switch (codec) {
      case kSbMediaAudioCodecAc3:
        samples_per_second_ = kAc3SamplesPerSecond;
        number_of_channels_ = kIec60958Channels;
        break;
      case kSbMediaAudioCodecEac3:
        samples_per_second_ = kEac3SamplesPerSecond;
        number_of_channels_ = kIec60958Channels;
        break;
      default:
        samples_per_second_ =
            static_cast<int>(audio_sample_info.samples_per_second);
        number_of_channels_ = audio_sample_info.number_of_channels;
    }
  }

  void Consume(ComPtr<IMFSample> sample) {
    DWORD buff_count = 0;
    HRESULT hr = sample->GetBufferCount(&buff_count);
    CheckResult(hr);
    SB_DCHECK(buff_count == 1);

    ComPtr<IMFMediaBuffer> media_buffer;
    hr = sample->GetBufferByIndex(0, &media_buffer);
    if (FAILED(hr)) {
      return;
    }

    LONGLONG win32_timestamp = 0;
    hr = sample->GetSampleTime(&win32_timestamp);
    CheckResult(hr);

    BYTE* buffer;
    DWORD length;
    hr = media_buffer->Lock(&buffer, NULL, &length);
    CheckResult(hr);
    SB_DCHECK(length);

    const uint8_t* data = reinterpret_cast<uint8_t*>(buffer);
    const size_t data_size = static_cast<size_t>(length);

    if (codec_ == kSbMediaAudioCodecAac &&
        (data_size / expected_buffer_size_ == 2)) {
      heaac_detected_.store(true);
    }

    const size_t decoded_data_size = std::max(expected_buffer_size_, data_size);

    if (codec_ == kSbMediaAudioCodecAc3) {
      SB_DCHECK(decoded_data_size == kAc3BufferSize);
    } else if (codec_ == kSbMediaAudioCodecEac3) {
      SB_DCHECK(decoded_data_size == kEac3BufferSize);
    }

    DecodedAudioPtr data_ptr(
        new DecodedAudio(number_of_channels_, sample_type_, audio_frame_fmt_,
                         ConvertToSbTime(win32_timestamp), decoded_data_size));

    std::copy(data, data + data_size, data_ptr->buffer());
    std::memset(data_ptr->buffer() + data_size, 0,
                decoded_data_size - data_size);

    output_queue_.push(data_ptr);
    media_buffer->Unlock();
  }

  bool TryWrite(const scoped_refptr<InputBuffer>& buff) override {
    SB_DCHECK(thread_checker_.CalledOnValidThread());

    if (codec_ == kSbMediaAudioCodecAac) {
      // The incoming audio is in ADTS format which has a 7 bytes header.  But
      // the audio decoder is configured to accept raw AAC.  So we have to
      // adjust the data, size, and subsample mapping to skip the ADTS header.
      const int kADTSHeaderSize = 7;
      return impl_->TryWriteInputBuffer(buff, kADTSHeaderSize);
    }
    return impl_->TryWriteInputBuffer(buff, 0);
  }

  void WriteEndOfStream() override {
    SB_DCHECK(thread_checker_.CalledOnValidThread());

    impl_->Drain();
    ComPtr<IMFSample> sample;
    ComPtr<IMFMediaType> media_type;
    while (impl_->ProcessAndRead(&sample, &media_type)) {
      if (sample) {
        Consume(sample);
      }
    }
    output_queue_.push(new DecodedAudio);
  }

  scoped_refptr<DecodedAudio> ProcessAndRead() override {
    SB_DCHECK(thread_checker_.CalledOnValidThread());

    ComPtr<IMFSample> sample;
    ComPtr<IMFMediaType> media_type;
    while (impl_->ProcessAndRead(&sample, &media_type)) {
      if (sample) {
        Consume(sample);
      }
    }
    if (output_queue_.empty()) {
      return NULL;
    }
    scoped_refptr<DecodedAudio> output = output_queue_.front();
    output_queue_.pop();
    return output;
  }

  void Reset() override {
    impl_->Reset();
    std::queue<DecodedAudioPtr> empty;
    output_queue_.swap(empty);
    thread_checker_.Detach();
  }

  int GetSamplesPerSecond() const override {
    if (heaac_detected_.load()) {
      return samples_per_second_ * 2;
    }
    return samples_per_second_;
  }

  // The object is single-threaded and is driven by a dedicated thread.
  // However the thread may gets destroyed and re-created over the life time of
  // this object.  We enforce that certain member functions can only called
  // from one thread while still allows this object to be driven by different
  // threads by:
  // 1. The |thread_checker_| is initially created without attaching to any
  //    thread.
  // 2. When a thread is destroyed, Reset() will be called which in turn calls
  //    Detach() on the |thread_checker_| to allow the object to attach to a
  //    new thread.
  ::starboard::shared::starboard::ThreadChecker thread_checker_;
  const SbMediaAudioCodec codec_;
  const SbMediaAudioSampleType sample_type_;
  const SbMediaAudioFrameStorageType audio_frame_fmt_;
  scoped_ptr<DecryptingDecoder> impl_;
  std::queue<DecodedAudioPtr> output_queue_;
  uint16_t number_of_channels_;
  atomic_bool heaac_detected_;
  int samples_per_second_;
  const size_t expected_buffer_size_;
};

}  // anonymous namespace.

scoped_ptr<AbstractWin32AudioDecoder> AbstractWin32AudioDecoder::Create(
    SbMediaAudioCodec code,
    SbMediaAudioFrameStorageType audio_frame_fmt,
    SbMediaAudioSampleType sample_type,
    const SbMediaAudioSampleInfo& audio_sample_info,
    SbDrmSystem drm_system) {
  return scoped_ptr<AbstractWin32AudioDecoder>(
      new AbstractWin32AudioDecoderImpl(code, audio_frame_fmt, sample_type,
                                        audio_sample_info, drm_system));
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
