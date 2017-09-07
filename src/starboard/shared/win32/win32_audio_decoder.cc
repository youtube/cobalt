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

#include "starboard/shared/starboard/thread_checker.h"
#include "starboard/shared/win32/atomic_queue.h"
#include "starboard/shared/win32/decrypting_decoder.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/media_foundation_utils.h"

namespace starboard {
namespace shared {
namespace win32 {

namespace {

using Microsoft::WRL::ComPtr;
using ::starboard::shared::starboard::ThreadChecker;

std::vector<ComPtr<IMFMediaType>> Filter(
    GUID subtype_guid, const std::vector<ComPtr<IMFMediaType>>& input) {
  std::vector<ComPtr<IMFMediaType>> output;
  for (size_t i = 0; i < input.size(); ++i) {
    ComPtr<IMFMediaType> curr = input[i];
    GUID guid_value;
    HRESULT hr = curr->GetGUID(MF_MT_SUBTYPE, &guid_value);
    CheckResult(hr);
    if (subtype_guid == guid_value) {
      output.push_back(curr);
    }
  }

  return output;
}

class WinAudioFormat {
 public:
  explicit WinAudioFormat(const SbMediaAudioHeader& audio_header) {
    WAVEFORMATEX* wave_format = WaveFormatTexPtr();
    wave_format->nAvgBytesPerSec = audio_header.average_bytes_per_second;
    wave_format->nBlockAlign = audio_header.block_alignment;
    wave_format->nChannels = audio_header.number_of_channels;
    wave_format->nSamplesPerSec = audio_header.samples_per_second;
    wave_format->wBitsPerSample = audio_header.bits_per_sample;
    wave_format->wFormatTag = audio_header.format_tag;

    // TODO: Investigate this more.
    wave_format->cbSize = kAudioExtraFormatBytes;
    std::uint8_t* audio_specific_config = AudioSpecificConfigPtr();

    // These are hard-coded audio specif audio configuration.
    // Use |SbMediaAudioHeader::audio_specific_config| instead.
    SB_DCHECK(kAudioExtraFormatBytes == 2);
    // TODO: What do these values do?
    audio_specific_config[0] = 0x12;
    audio_specific_config[1] = 0x10;
  }
  WAVEFORMATEX* WaveFormatTexPtr() {
    return reinterpret_cast<WAVEFORMATEX*>(full_structure);
  }
  uint8_t* AudioSpecificConfigPtr() {
    return full_structure + sizeof(WAVEFORMATEX);
  }

  UINT32 Size() const {
    return sizeof(full_structure);
  }

 private:
  static const UINT32 kAudioExtraFormatBytes = 2;
  uint8_t full_structure[sizeof(WAVEFORMATEX) + kAudioExtraFormatBytes];
};

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
        audio_header_(audio_header),
        impl_("audio", CLSID_MSAACDecMFT, drm_system) {
    Configure();
  }

  static GUID ConvertToWin32AudioCodec(SbMediaAudioCodec codec) {
    switch (codec) {
      case kSbMediaAudioCodecNone: { return MFAudioFormat_PCM; }
      case kSbMediaAudioCodecAac: { return MFAudioFormat_AAC; }
      case kSbMediaAudioCodecOpus: { return MFAudioFormat_Opus; }
      case kSbMediaAudioCodecVorbis: {
        SB_NOTIMPLEMENTED();
      }
    }
    return MFAudioFormat_PCM;
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

    DecodedAudioPtr data_ptr(new DecodedAudio(
        audio_header_.number_of_channels, sample_type_, audio_frame_fmt_,
        ConvertToMediaTime(win32_timestamp), data_size));

    std::copy(data, data + data_size, data_ptr->buffer());

    output_queue_.push(data_ptr);
    media_buffer->Unlock();
  }

  void Configure() {
    ComPtr<IMFMediaType> input_type;
    HRESULT hr = MFCreateMediaType(&input_type);
    CheckResult(hr);

    WinAudioFormat audio_fmt(audio_header_);
    hr = MFInitMediaTypeFromWaveFormatEx(
        input_type.Get(),
        audio_fmt.WaveFormatTexPtr(),
        audio_fmt.Size());

    CheckResult(hr);

    hr = input_type->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0);  // raw aac
    CheckResult(hr);

    GUID subtype = ConvertToWin32AudioCodec(codec_);
    hr = input_type->SetGUID(MF_MT_SUBTYPE, subtype);
    CheckResult(hr);

    MediaTransform& decoder = impl_.GetDecoder();
    std::vector<ComPtr<IMFMediaType>> available_types =
        decoder.GetAvailableInputTypes();

    available_types = Filter(subtype, available_types);
    SB_DCHECK(available_types.size());
    ComPtr<IMFMediaType> selected = available_types[0];

    std::vector<GUID> attribs = {
      MF_MT_AUDIO_BLOCK_ALIGNMENT,
      MF_MT_AUDIO_SAMPLES_PER_SECOND,
      MF_MT_AUDIO_AVG_BYTES_PER_SECOND,
      MF_MT_AUDIO_NUM_CHANNELS,
    };

    for (auto it = attribs.begin(); it != attribs.end(); ++it) {
      CopyUint32Property(*it, input_type.Get(), selected.Get());
    }

    decoder.SetInputType(selected);
    decoder.SetOutputTypeBySubType(MFAudioFormat_Float);
  }

  bool TryWrite(const scoped_refptr<InputBuffer>& buff) SB_OVERRIDE {
    SB_DCHECK(thread_checker_.CalledOnValidThread());

    // The incoming audio is in ADTS format which has a 7 bytes header.  But
    // the audio decoder is configured to accept raw AAC.  So we have to adjust
    // the data, size, and subsample mapping to skip the ADTS header.
    const int kADTSHeaderSize = 7;
    const bool write_ok = impl_.TryWriteInputBuffer(buff, kADTSHeaderSize);
    return write_ok;
  }

  void WriteEndOfStream() SB_OVERRIDE {
    SB_DCHECK(thread_checker_.CalledOnValidThread());

    impl_.Drain();
    ComPtr<IMFSample> sample;
    ComPtr<IMFMediaType> media_type;
    while (impl_.ProcessAndRead(&sample, &media_type)) {
      if (sample) {
        Consume(sample);
      }
    }
    output_queue_.push(new DecodedAudio);
  }

  scoped_refptr<DecodedAudio> ProcessAndRead() SB_OVERRIDE {
    SB_DCHECK(thread_checker_.CalledOnValidThread());

    ComPtr<IMFSample> sample;
    ComPtr<IMFMediaType> media_type;
    while (impl_.ProcessAndRead(&sample, &media_type)) {
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

  void Reset() SB_OVERRIDE {
    impl_.Reset();
    std::queue<DecodedAudioPtr> empty;
    output_queue_.swap(empty);
    thread_checker_.Detach();
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
  const SbMediaAudioHeader audio_header_;
  const SbMediaAudioSampleType sample_type_;
  const SbMediaAudioFrameStorageType audio_frame_fmt_;
  DecryptingDecoder impl_;
  std::queue<DecodedAudioPtr> output_queue_;
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
