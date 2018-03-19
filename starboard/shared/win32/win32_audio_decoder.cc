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

namespace starboard {
namespace shared {
namespace win32 {

using Microsoft::WRL::ComPtr;
using ::starboard::shared::starboard::ThreadChecker;
using ::starboard::shared::win32::CreateAudioTransform;

const size_t kAacSamplesPerFrame = 1024;
// We are using float samples on Xb1.
const size_t kBytesPerSample = sizeof(float);

namespace {

class AbstractWin32AudioDecoderImpl : public AbstractWin32AudioDecoder {
 public:
  AbstractWin32AudioDecoderImpl(SbMediaAudioCodec codec,
                                SbMediaAudioFrameStorageType audio_frame_fmt,
                                SbMediaAudioSampleType sample_type,
                                const SbMediaAudioHeader& audio_header,
                                SbDrmSystem drm_system)
      : thread_checker_(ThreadChecker::kSetThreadIdOnFirstCheck),
        codec_(codec),
        audio_frame_fmt_(audio_frame_fmt),
        sample_type_(sample_type),
        number_of_channels_(audio_header.number_of_channels),
        heaac_detected_(false),
        samples_per_second_(static_cast<int>(audio_header.samples_per_second)) {
    scoped_ptr<MediaTransform> audio_decoder =
        CreateAudioTransform(audio_header, codec_);
    impl_.reset(
        new DecryptingDecoder("audio", audio_decoder.Pass(), drm_system));
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

    const size_t expect_size_in_bytes =
        number_of_channels_ * kAacSamplesPerFrame * kBytesPerSample;
    if (data_size / expect_size_in_bytes == 2) {
      heaac_detected_.store(true);
    }

    DecodedAudioPtr data_ptr(
        new DecodedAudio(number_of_channels_, sample_type_, audio_frame_fmt_,
                         ConvertToSbTime(win32_timestamp), data_size));

    std::copy(data, data + data_size, data_ptr->buffer());

    output_queue_.push(data_ptr);
    media_buffer->Unlock();
  }

  bool TryWrite(const scoped_refptr<InputBuffer>& buff) override {
    SB_DCHECK(thread_checker_.CalledOnValidThread());

    // The incoming audio is in ADTS format which has a 7 bytes header.  But
    // the audio decoder is configured to accept raw AAC.  So we have to adjust
    // the data, size, and subsample mapping to skip the ADTS header.
    const int kADTSHeaderSize = 7;
    const bool write_ok = impl_->TryWriteInputBuffer(buff, kADTSHeaderSize);
    return write_ok;
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
};

}  // anonymous namespace.

scoped_ptr<AbstractWin32AudioDecoder> AbstractWin32AudioDecoder::Create(
    SbMediaAudioCodec code,
    SbMediaAudioFrameStorageType audio_frame_fmt,
    SbMediaAudioSampleType sample_type,
    const SbMediaAudioHeader& audio_header,
    SbDrmSystem drm_system) {
  return scoped_ptr<AbstractWin32AudioDecoder>(
      new AbstractWin32AudioDecoderImpl(code, audio_frame_fmt, sample_type,
                                        audio_header, drm_system));
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
