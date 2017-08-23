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

#include "starboard/shared/win32/win32_decoder_impl.h"

#include <D3D11.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <numeric>

#include "starboard/byte_swap.h"
#include "starboard/common/ref_counted.h"
#include "starboard/log.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/media_common.h"
#include "starboard/shared/win32/media_foundation_utils.h"

namespace starboard {
namespace shared {
namespace win32 {

using ::starboard::shared::win32::CheckResult;

namespace {

// TODO: merge all kStreamId's.
const int kStreamId = 0;

ComPtr<IMFSample> CreateSample(const void* data,
                               int size,
                               int64_t win32_timestamp) {
  ComPtr<IMFMediaBuffer> buffer;
  HRESULT hr = MFCreateMemoryBuffer(size, &buffer);
  CheckResult(hr);

  BYTE* buffer_ptr;
  hr = buffer->Lock(&buffer_ptr, 0, 0);
  CheckResult(hr);

  SbMemoryCopy(buffer_ptr, data, size);

  hr = buffer->Unlock();
  CheckResult(hr);

  hr = buffer->SetCurrentLength(size);
  CheckResult(hr);

  ComPtr<IMFSample> input;
  hr = MFCreateSample(&input);
  CheckResult(hr);

  hr = input->AddBuffer(buffer.Get());
  CheckResult(hr);

  // sample time is in 100 nanoseconds.
  input->SetSampleTime(win32_timestamp);
  return input;
}

bool TryWriteToMediaProcessor(ComPtr<IMFTransform> media_processor,
                              ComPtr<IMFSample> input,
                              int stream_id) {
  DWORD flag;
  HRESULT hr = media_processor->GetInputStatus(stream_id, &flag);
  CheckResult(hr);

  SB_DCHECK(MFT_INPUT_STATUS_ACCEPT_DATA == flag);
  hr = media_processor->ProcessInput(kStreamId, input.Get(), 0);

  switch (hr) {
    case S_OK: {  // Data was sent to the media processor.
      return true;
    }
    case MF_E_NOTACCEPTING: {
      return false;
    }
    default: {
      SB_NOTREACHED() << "Unexpected error.";
    }
  }
  return false;
}

bool StreamAllocatesMemory(DWORD out_stream_flags) {
  static const DWORD kFlagAutoAllocateMemory =
      MFT_OUTPUT_STREAM_PROVIDES_SAMPLES |
      MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES;

  const bool output_stream_allocates_memory =
      (out_stream_flags & kFlagAutoAllocateMemory) != 0;
  return output_stream_allocates_memory;
}

template <typename T>
void ReleaseIfNotNull(T** ptr) {
  if (*ptr) {
    (*ptr)->Release();
    *ptr = NULL;
  }
}

}  // namespace

DecoderImpl::DecoderImpl(const std::string& type,
                         MediaBufferConsumerInterface* media_buffer_consumer,
                         SbDrmSystem drm_system)
    : type_(type),
      media_buffer_consumer_(media_buffer_consumer),
      discontinuity_(false) {
  SB_DCHECK(media_buffer_consumer_);
  drm_system_ =
      static_cast<::starboard::xb1::shared::playready::SbDrmSystemPlayready*>(
          drm_system);
}

DecoderImpl::~DecoderImpl() {
}

// static
ComPtr<IMFTransform> DecoderImpl::CreateDecoder(CLSID clsid) {
  ComPtr<IMFTransform> decoder;
  LPVOID* ptr_address = reinterpret_cast<LPVOID*>(decoder.GetAddressOf());
  HRESULT hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER,
                                IID_IMFTransform, ptr_address);
  CheckResult(hr);
  return decoder;
}

void DecoderImpl::ActivateDecryptor() {
  SB_DCHECK(decoder_);

  if (!decryptor_) {
    return;
  }

  ComPtr<IMFMediaType> decryptor_input_type;

  HRESULT hr = decoder_->GetInputCurrentType(
      kStreamId, decryptor_input_type.GetAddressOf());
  CheckResult(hr);

  GUID original_sub_type;
  {
    ComPtr<IMFMediaType> output_type;
    hr = decoder_->GetOutputCurrentType(kStreamId, output_type.GetAddressOf());
    CheckResult(hr);
    output_type->GetGUID(MF_MT_SUBTYPE, &original_sub_type);
  }

  const DWORD kFlags = 0;
  hr = decryptor_->SetInputType(kStreamId, decryptor_input_type.Get(), kFlags);
  CheckResult(hr);

  // Ensure that the decryptor and the decoder agrees on the protection of
  // samples transferred between them.
  ComPtr<IMFSampleProtection> decryption_sample_protection;
  ComPtr<IMFSampleProtection> decoder_sample_protection;
  DWORD decryption_protection_version;
  DWORD decoder_protection_version;
  DWORD protection_version;
  BYTE* cert_data = NULL;
  DWORD cert_data_size = 0;
  BYTE* crypt_seed = NULL;
  DWORD crypt_seed_size = 0;
  ComPtr<IMFMediaType> decoder_input_type;

  hr = decryptor_.As(&decryption_sample_protection);
  CheckResult(hr);
  hr = decryption_sample_protection->GetOutputProtectionVersion(
      &decryption_protection_version);
  CheckResult(hr);
  hr = decoder_.As(&decoder_sample_protection);
  CheckResult(hr);
  hr = decoder_sample_protection->GetInputProtectionVersion(
      &decoder_protection_version);
  CheckResult(hr);
  protection_version =
      std::min(decoder_protection_version, decryption_protection_version);
  if (protection_version < SAMPLE_PROTECTION_VERSION_RC4) {
    SB_NOTREACHED();
  }

  hr = decoder_sample_protection->GetProtectionCertificate(
      protection_version,
      &cert_data, &cert_data_size);
  CheckResult(hr);

  hr = decryption_sample_protection->InitOutputProtection(
      protection_version, 0, cert_data, cert_data_size,
      &crypt_seed, &crypt_seed_size);
  CheckResult(hr);

  hr = decoder_sample_protection->InitInputProtection(
      protection_version, 0,
      crypt_seed, crypt_seed_size);
  CheckResult(hr);

  CoTaskMemFree(cert_data);
  CoTaskMemFree(crypt_seed);

  // Ensure that the input type of the decoder is the output type of the
  // decryptor.
  hr = decryptor_->GetOutputAvailableType(
      kStreamId,
      0,  // Type Index
      decoder_input_type.ReleaseAndGetAddressOf());
  CheckResult(hr);
  hr = decryptor_->SetOutputType(kStreamId, decoder_input_type.Get(),
                                 0);  //  _MFT_SET_TYPE_FLAGS
  CheckResult(hr);
  hr = decoder_->SetInputType(kStreamId, decoder_input_type.Get(),
                              0);  //  _MFT_SET_TYPE_FLAGS
  CheckResult(hr);

  // Start the decryptor, note that this should be better abstracted.
  SendMFTMessage(decryptor_.Get(), MFT_MESSAGE_NOTIFY_BEGIN_STREAMING);
  SendMFTMessage(decryptor_.Get(), MFT_MESSAGE_NOTIFY_START_OF_STREAM);

  for (int index = 0;; ++index) {
    ComPtr<IMFMediaType> output_type;
    hr = decoder_->GetOutputAvailableType(0, index, &output_type);
    if (SUCCEEDED(hr)) {
      GUID sub_type;
      output_type->GetGUID(MF_MT_SUBTYPE, &sub_type);
      if (IsEqualGUID(sub_type, original_sub_type)) {
        hr = decoder_->SetOutputType(0, output_type.Get(), 0);
        CheckResult(hr);
        break;
      }
    }
    output_type.Reset();
  }
}

bool DecoderImpl::TryWriteInputBuffer(
    const void* data,
    int size,
    std::int64_t win32_timestamp,
    const std::vector<std::uint8_t>& key_id,
    const std::vector<std::uint8_t>& iv,
    const std::vector<Subsample>& subsamples) {
  // MFSampleExtension_CleanPoint is a key-frame for the video + audio. It is
  // not set here because the win32 system is smart enough to figure this out.
  // It will probably be totally ok to not set this at all. Resolution: If
  // there are problems with win32 video decoding, come back to this and see
  // if setting this will fix it. THis will be used if
  // SbMediaVideoSampleInfo::is_key_frame is true inside of the this function
  // (which will receive an InputBuffer).
  // Set MFSampleExtension_CleanPoint attributes.
  SB_DCHECK(decoder_);
  ComPtr<IMFSample> input = CreateSample(data, size, win32_timestamp);
  SB_DCHECK(decoder_);

  // Has to check both as sometimes the sample can contain an invalid key id.
  // Better check the key id size is 16 and iv size is 8 or 16.
  bool encrypted = !key_id.empty() && !iv.empty();
  if (encrypted) {
    if (!decryptor_) {
      scoped_refptr<::starboard::xb1::shared::playready::PlayreadyLicense>
          license = drm_system_->GetLicense(key_id.data(),
                                            static_cast<int>(key_id.size()));
      if (license && license->usable()) {
        decryptor_ = license->decryptor();
        ActivateDecryptor();
      }
    }
    if (!decryptor_) {
      SB_NOTREACHED();
      return false;
    }
    size_t iv_size = iv.size();
    const char kEightZeros[8] = {0};
    if (iv_size == 16 && SbMemoryCompare(iv.data() + 8, kEightZeros, 8) == 0) {
      // For iv that is 16 bytes long but the the last 8 bytes is 0, we treat
      // it as an 8 bytes iv.
      iv_size = 8;
    }
    input->SetBlob(MFSampleExtension_Encryption_SampleID,
                   reinterpret_cast<const UINT8*>(iv.data()),
                   static_cast<UINT32>(iv_size));
    SB_DCHECK(key_id.size() == sizeof(GUID));
    GUID guid = *reinterpret_cast<const GUID*>(key_id.data());

    guid.Data1 = SbByteSwapU32(guid.Data1);
    guid.Data2 = SbByteSwapU16(guid.Data2);
    guid.Data3 = SbByteSwapU16(guid.Data3);

    input->SetGUID(MFSampleExtension_Content_KeyID, guid);

    std::vector<DWORD> subsamples_data;
    if (!subsamples.empty()) {
      for (auto& subsample : subsamples) {
        subsamples_data.push_back(subsample.clear_bytes);
        subsamples_data.push_back(subsample.encrypted_bytes);
      }
      SB_DCHECK(std::accumulate(subsamples_data.begin(), subsamples_data.end(),
                                0) == size);
    } else {
      subsamples_data.push_back(0);
      subsamples_data.push_back(size);
    }
    input->SetBlob(MFSampleExtension_Encryption_SubSampleMappingSplit,
                   reinterpret_cast<UINT8*>(&subsamples_data[0]),
                   static_cast<UINT32>(subsamples_data.size() * sizeof(DWORD)));
  }

  ComPtr<IMFTransform> media_processor = encrypted ? decryptor_ : decoder_;
  const bool write_ok =
      TryWriteToMediaProcessor(media_processor, input, kStreamId);
  return write_ok;
}

void DecoderImpl::DrainDecoder() {
  // This is used during EOS to get the last few frames.
  SB_DCHECK(decoder_);
  SendMFTMessage(decoder_.Get(), MFT_MESSAGE_NOTIFY_END_OF_STREAM);
  SendMFTMessage(decoder_.Get(), MFT_MESSAGE_COMMAND_DRAIN);
}

bool DecoderImpl::DeliverOutputOnAllTransforms() {
  SB_DCHECK(decoder_);
  bool delivered = false;
  if (decryptor_) {
    while (ComPtr<IMFSample> sample = DeliverOutputOnTransform(decryptor_)) {
      HRESULT hr = decoder_->ProcessInput(kStreamId, sample.Get(), 0);
      if (hr == MF_E_NOTACCEPTING) {
        // The protocol says that when ProcessInput() returns MF_E_NOTACCEPTING,
        // there must be some output available. Retrieve the output and the next
        // ProcessInput() should succeed.
        while (ComPtr<IMFSample> sample_inner =
                   DeliverOutputOnTransform(decoder_)) {
          DeliverDecodedSample(sample_inner);
          delivered = true;
        }
        hr = decoder_->ProcessInput(kStreamId, sample.Get(), 0);
      }
      CheckResult(hr);
      delivered = true;
    }
  }
  while (ComPtr<IMFSample> sample = DeliverOutputOnTransform(decoder_)) {
    DeliverDecodedSample(sample);
    delivered = true;
  }
  return delivered;
}

bool DecoderImpl::DeliverOneOutputOnAllTransforms() {
  SB_DCHECK(decoder_);
  if (decryptor_) {
    if (!cached_decryptor_output_) {
      cached_decryptor_output_ = DeliverOutputOnTransform(decryptor_);
    }
    while (cached_decryptor_output_) {
      HRESULT hr =
          decoder_->ProcessInput(kStreamId, cached_decryptor_output_.Get(), 0);
      if (hr == MF_E_NOTACCEPTING) {
        break;
      } else {
        CheckResult(hr);
        cached_decryptor_output_ = DeliverOutputOnTransform(decryptor_);
      }
    }
    if (cached_decryptor_output_) {
      // The protocol says that when ProcessInput() returns MF_E_NOTACCEPTING,
      // there must be some output available. Retrieve the output and the next
      // ProcessInput() should succeed.
      ComPtr<IMFSample> decoder_output = DeliverOutputOnTransform(decoder_);
      if (decoder_output) {
        DeliverDecodedSample(decoder_output);
      }
      return decoder_output != NULL;
    }
  }

  bool delivered = false;
  if (ComPtr<IMFSample> sample = DeliverOutputOnTransform(decoder_)) {
    DeliverDecodedSample(sample);
    delivered = true;
  }
  return delivered;
}

void DecoderImpl::set_decoder(Microsoft::WRL::ComPtr<IMFTransform> decoder) {
  // Either new object is null or the old one is.
  SB_DCHECK(!decoder || !decoder_);
  decoder_ = decoder;
}

void DecoderImpl::SendMFTMessage(IMFTransform* transform,
                                 MFT_MESSAGE_TYPE msg) {
  if (!transform) {
    return;
  }
  ULONG_PTR data = 0;
  HRESULT hr = transform->ProcessMessage(msg, data);
  CheckResult(hr);
}

void DecoderImpl::PrepareOutputDataBuffer(
    ComPtr<IMFTransform> transform,
    MFT_OUTPUT_DATA_BUFFER* output_data_buffer) {
  output_data_buffer->dwStreamID = kStreamId;
  output_data_buffer->pSample = NULL;
  output_data_buffer->dwStatus = 0;
  output_data_buffer->pEvents = NULL;

  MFT_OUTPUT_STREAM_INFO output_stream_info;
  HRESULT hr = transform->GetOutputStreamInfo(kStreamId, &output_stream_info);
  CheckResult(hr);

  if (StreamAllocatesMemory(output_stream_info.dwFlags)) {
    // Try to let the IMFTransform allocate the memory if possible.
    return;
  }

  ComPtr<IMFSample> sample;
  hr = MFCreateSample(&sample);
  CheckResult(hr);

  ComPtr<IMFMediaBuffer> buffer;
  hr = MFCreateAlignedMemoryBuffer(output_stream_info.cbSize,
                                   output_stream_info.cbAlignment, &buffer);

  CheckResult(hr);

  hr = sample->AddBuffer(buffer.Get());
  CheckResult(hr);

  output_data_buffer->pSample = sample.Detach();
}

ComPtr<IMFSample> DecoderImpl::DeliverOutputOnTransform(
    ComPtr<IMFTransform> transform) {
  MFT_OUTPUT_DATA_BUFFER output_data_buffer;
  PrepareOutputDataBuffer(transform, &output_data_buffer);

  const DWORD kFlags = 0;
  const DWORD kNumberOfBuffers = 1;
  DWORD status = 0;
  HRESULT hr = transform->ProcessOutput(kFlags, kNumberOfBuffers,
                                        &output_data_buffer, &status);

  SB_DCHECK(!output_data_buffer.pEvents);

  ComPtr<IMFSample> output = output_data_buffer.pSample;
  ReleaseIfNotNull(&output_data_buffer.pEvents);
  ReleaseIfNotNull(&output_data_buffer.pSample);

  if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
    ComPtr<IMFMediaType> media_type;

    hr = transform->GetOutputAvailableType(kStreamId,
                                           0,  // TypeIndex
                                           &media_type);
    CheckResult(hr);

    hr = transform->SetOutputType(kStreamId, media_type.Get(), 0);
    CheckResult(hr);

    media_buffer_consumer_->OnNewOutputType(media_type);

    hr = transform->ProcessOutput(kFlags, kNumberOfBuffers, &output_data_buffer,
                                  &status);

    SB_DCHECK(!output_data_buffer.pEvents);

    output = output_data_buffer.pSample;
    ReleaseIfNotNull(&output_data_buffer.pEvents);
    ReleaseIfNotNull(&output_data_buffer.pSample);
  }

  if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT ||
      output_data_buffer.dwStatus == MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE) {
    return NULL;
  }

  SB_DCHECK(output);
  if (discontinuity_) {
    output->SetUINT32(MFSampleExtension_Discontinuity, TRUE);
    discontinuity_ = false;
  }

  CheckResult(hr);
  return output;
}

void DecoderImpl::DeliverDecodedSample(ComPtr<IMFSample> sample) {
  DWORD buff_count = 0;
  HRESULT hr = sample->GetBufferCount(&buff_count);
  CheckResult(hr);
  SB_DCHECK(buff_count == 1);

  ComPtr<IMFMediaBuffer> media_buffer;
  hr = sample->GetBufferByIndex(0, &media_buffer);

  if (SUCCEEDED(hr)) {
    LONGLONG win32_timestamp = 0;
    hr = sample->GetSampleTime(&win32_timestamp);
    CheckResult(hr);
    media_buffer_consumer_->Consume(media_buffer, win32_timestamp);
  }
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
