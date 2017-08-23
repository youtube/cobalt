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

#include "starboard/shared/win32/atomic_queue.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/media_foundation_utils.h"
#include "starboard/shared/win32/win32_decoder_impl.h"

namespace starboard {
namespace shared {
namespace win32 {

namespace {
using Microsoft::WRL::ComPtr;
using ::starboard::shared::win32::CheckResult;

const int kStreamId = 0;

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

std::vector<ComPtr<IMFMediaType>> GetAvailableTypes(IMFTransform* decoder) {
  std::vector<ComPtr<IMFMediaType>> output;
  for (DWORD i = 0; ; ++i) {
    ComPtr<IMFMediaType> curr_type;
    HRESULT input_hr_success = decoder->GetInputAvailableType(
        kStreamId,
        i,
        curr_type.GetAddressOf());
    if (!SUCCEEDED(input_hr_success)) {
      break;
    }
    output.push_back(curr_type);
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

class AbstractWin32AudioDecoderImpl : public AbstractWin32AudioDecoder,
                                      public MediaBufferConsumerInterface {
 public:
  AbstractWin32AudioDecoderImpl(SbMediaAudioCodec codec,
                                SbMediaAudioFrameStorageType audio_frame_fmt,
                                SbMediaAudioSampleType sample_type,
                                const SbMediaAudioHeader& audio_header,
                                SbDrmSystem drm_system)
      : codec_(codec),
        audio_frame_fmt_(audio_frame_fmt),
        sample_type_(sample_type),
        audio_header_(audio_header) {
    MediaBufferConsumerInterface* media_cb = this;
    impl_.reset(new DecoderImpl("audio", media_cb, drm_system));
    EnsureAudioDecoderCreated();
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

  virtual void Consume(ComPtr<IMFMediaBuffer> media_buffer,
                       int64_t win32_timestamp) {
    BYTE* buffer;
    DWORD length;
    HRESULT hr = media_buffer->Lock(&buffer, NULL, &length);
    CheckResult(hr);
    SB_DCHECK(length);

    const uint8_t* data = reinterpret_cast<uint8_t*>(buffer);
    const size_t data_size = static_cast<size_t>(length);

    DecodedAudioPtr data_ptr(new DecodedAudio(
        audio_header_.number_of_channels, sample_type_, audio_frame_fmt_,
        ConvertToMediaTime(win32_timestamp), data_size));

    std::copy(data, data + data_size, data_ptr->buffer());

    output_queue_.PushBack(data_ptr);
    media_buffer->Unlock();
  }

  void OnNewOutputType(const ComPtr<IMFMediaType>& /*type*/) override {}

  ComPtr<IMFMediaType> Configure(IMFTransform* decoder) {
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

    std::vector<ComPtr<IMFMediaType>> available_types =
        GetAvailableTypes(decoder);

    GUID audio_fmt_guid = ConvertToWin32AudioCodec(codec_);
    available_types = Filter(audio_fmt_guid, available_types);
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

    hr = decoder->SetInputType(0, selected.Get(), 0);

    CheckResult(hr);
    return selected;
  }

  void EnsureAudioDecoderCreated() SB_OVERRIDE {
    if (impl_->has_decoder()) {
      return;
    }

    ComPtr<IMFTransform> decoder =
        DecoderImpl::CreateDecoder(CLSID_MSAACDecMFT);

    ComPtr<IMFMediaType> media_type = Configure(decoder.Get());

    SB_DCHECK(decoder);

    impl_->set_decoder(decoder);

    // TODO: MFWinAudioFormat_PCM?
    ComPtr<IMFMediaType> output_type =
        FindMediaType(MFAudioFormat_Float, decoder.Get());

    SB_DCHECK(output_type);

    HRESULT hr = decoder->SetOutputType(0, output_type.Get(), 0);
    CheckResult(hr);

    decoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    decoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
  }

  bool TryWrite(const InputBuffer& buff) SB_OVERRIDE {
    EnsureAudioDecoderCreated();
    if (!impl_->has_decoder()) {
      return false;  // TODO: Signal an error.
    }

    const void* data = buff.data();
    const int size = buff.size();
    const int64_t media_timestamp = buff.pts();

    std::vector<uint8_t> key_id;
    std::vector<uint8_t> iv;
    std::vector<Subsample> subsamples;

    const SbDrmSampleInfo* drm_info = buff.drm_info();

    if (drm_info != NULL && drm_info->initialization_vector_size != 0) {
      key_id.assign(drm_info->identifier,
                    drm_info->identifier + drm_info->identifier_size);
      iv.assign(drm_info->initialization_vector,
                drm_info->initialization_vector +
                    drm_info->initialization_vector_size);
      subsamples.reserve(drm_info->subsample_count);
      for (int32_t i = 0; i < drm_info->subsample_count; ++i) {
        Subsample subsample = {
            static_cast<uint32_t>(
                drm_info->subsample_mapping[i].clear_byte_count),
            static_cast<uint32_t>(
                drm_info->subsample_mapping[i].encrypted_byte_count)};
        subsamples.push_back(subsample);
      }
    }

    const std::int64_t win32_time_stamp = ConvertToWin32Time(media_timestamp);

    // Adjust the offset for 7 bytes to remove the ADTS header.
    const int kADTSHeaderSize = 7;
    const uint8_t* audio_start =
        static_cast<const uint8_t*>(data) + kADTSHeaderSize;
    const int audio_size = size - kADTSHeaderSize;
    if (!subsamples.empty()) {
      SB_DCHECK(subsamples[0].clear_bytes == kADTSHeaderSize);
      subsamples[0].clear_bytes = 0;
    }

    const bool write_ok = impl_->TryWriteInputBuffer(
        audio_start, audio_size, win32_time_stamp, key_id, iv, subsamples);
    impl_->DeliverOutputOnAllTransforms();
    return write_ok;
  }

  void WriteEndOfStream() SB_OVERRIDE {
    if (impl_->has_decoder()) {
      impl_->DrainDecoder();
      impl_->DeliverOutputOnAllTransforms();
      output_queue_.PushBack(new DecodedAudio);
    } else {
      // Don't call DrainDecoder() if input data is never queued.
      // TODO: Send EOS.
    }
  }

  scoped_refptr<DecodedAudio> ProcessAndRead() SB_OVERRIDE {
    impl_->DeliverOutputOnAllTransforms();
    scoped_refptr<DecodedAudio> output = output_queue_.PopFront();
    return output;
  }

  SbMediaAudioCodec codec_;
  SbMediaAudioHeader audio_header_;
  SbMediaAudioSampleType sample_type_;
  SbMediaAudioFrameStorageType audio_frame_fmt_;
  scoped_ptr<DecoderImpl> impl_;
  AtomicQueue<DecodedAudioPtr> output_queue_;
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
