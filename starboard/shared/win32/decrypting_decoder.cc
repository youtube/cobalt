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

#include "starboard/shared/win32/decrypting_decoder.h"

#include <algorithm>
#include <numeric>

#include "starboard/byte_swap.h"
#include "starboard/common/ref_counted.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/media_foundation_utils.h"

namespace starboard {
namespace shared {
namespace win32 {

namespace {

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

void AttachDrmDataToSample(ComPtr<IMFSample> sample,
                           int sample_size,
                           const uint8_t* key_id,
                           int key_id_size,
                           const uint8_t* iv,
                           int iv_size,
                           const SbDrmSubSampleMapping* subsample_mapping,
                           int subsample_count) {
  if (iv_size == 16 && SbMemoryIsZero(iv + 8, 8)) {
    // For iv that is 16 bytes long but the the last 8 bytes is 0, we treat
    // it as an 8 bytes iv.
    iv_size = 8;
  }
  sample->SetBlob(MFSampleExtension_Encryption_SampleID,
                  reinterpret_cast<const UINT8*>(iv),
                  static_cast<UINT32>(iv_size));
  SB_DCHECK(key_id_size == sizeof(GUID));
  GUID guid = *reinterpret_cast<const GUID*>(key_id);

  guid.Data1 = SbByteSwapU32(guid.Data1);
  guid.Data2 = SbByteSwapU16(guid.Data2);
  guid.Data3 = SbByteSwapU16(guid.Data3);

  sample->SetGUID(MFSampleExtension_Content_KeyID, guid);

  SB_DCHECK(sizeof(DWORD) * 2 == sizeof(SbDrmSubSampleMapping));

  SbDrmSubSampleMapping default_subsample = {0, sample_size};
  if (subsample_count == 0) {
    subsample_mapping = &default_subsample;
    subsample_count = 1;
  }
  sample->SetBlob(
      MFSampleExtension_Encryption_SubSampleMappingSplit,
      reinterpret_cast<const UINT8*>(subsample_mapping),
      static_cast<UINT32>(subsample_count * sizeof(SbDrmSubSampleMapping)));
}

}  // namespace

DecryptingDecoder::DecryptingDecoder(const std::string& type,
                                     scoped_ptr<MediaTransform> decoder,
                                     SbDrmSystem drm_system)
    : type_(type), decoder_(decoder.Pass()) {
  SB_DCHECK(decoder_.get());
  drm_system_ = static_cast<SbDrmSystemPlayready*>(drm_system);
}

DecryptingDecoder::~DecryptingDecoder() {
  Reset();
}

bool DecryptingDecoder::TryWriteInputBuffer(
    const scoped_refptr<InputBuffer>& input_buffer,
    int bytes_to_skip_in_sample) {
  SB_DCHECK(input_buffer);
  SB_DCHECK(bytes_to_skip_in_sample >= 0);

  ComPtr<IMFSample> input_sample;

  const SbDrmSampleInfo* drm_info = input_buffer->drm_info();
  const uint8_t* key_id = NULL;
  int key_id_size = 0;
  bool encrypted = false;

  if (drm_info != NULL && drm_info->identifier_size == 16 &&
      (drm_info->initialization_vector_size == 8 ||
       drm_info->initialization_vector_size == 16)) {
    key_id = drm_info->identifier;
    key_id_size = drm_info->identifier_size;
    encrypted = true;
  }

  if (input_buffer == last_input_buffer_) {
    SB_DCHECK(last_input_sample_);
    input_sample = last_input_sample_;
  } else {
    if (input_buffer->size() < bytes_to_skip_in_sample) {
      SB_NOTREACHED();
      return false;
    }

    const void* data = input_buffer->data() + bytes_to_skip_in_sample;
    int size = input_buffer->size() - bytes_to_skip_in_sample;

    std::int64_t win32_timestamp = ConvertToWin32Time(input_buffer->pts());
    const uint8_t* iv = NULL;
    int iv_size = 0;
    const SbDrmSubSampleMapping* subsample_mapping = NULL;
    int subsample_count = 0;

    if (drm_info != NULL && drm_info->initialization_vector_size != 0) {
      if (bytes_to_skip_in_sample != 0) {
        if (drm_info->subsample_count != 0 && drm_info->subsample_count != 1) {
          return false;
        }
        if (drm_info->subsample_count == 1) {
          if (drm_info->subsample_mapping[0].clear_byte_count !=
              bytes_to_skip_in_sample) {
            return false;
          }
        }
      } else {
        subsample_mapping = drm_info->subsample_mapping;
        subsample_count = drm_info->subsample_count;
      }

      iv = drm_info->initialization_vector;
      iv_size = drm_info->initialization_vector_size;
    }

    // MFSampleExtension_CleanPoint is a key-frame for the video + audio. It is
    // not set here because the win32 system is smart enough to figure this out.
    // It will probably be totally ok to not set this at all. Resolution: If
    // there are problems with win32 video decoding, come back to this and see
    // if setting this will fix it. THis will be used if
    // SbMediaVideoSampleInfo::is_key_frame is true inside of the this function
    // (which will receive an InputBuffer).
    input_sample = CreateSample(data, size, win32_timestamp);

    if (encrypted) {
      AttachDrmDataToSample(input_sample, size, key_id, key_id_size, iv,
                            iv_size, subsample_mapping, subsample_count);
    }
    last_input_buffer_ = input_buffer;
    last_input_sample_ = input_sample;
  }

  if (encrypted) {
    if (!decryptor_) {
      if (decoder_->draining()) {
        return false;
      }
      if (!decoder_->drained()) {
        decoder_->Drain();
        return false;
      }
      decoder_->ResetFromDrained();
      scoped_refptr<SbDrmSystemPlayready::License> license =
          drm_system_->GetLicense(key_id, key_id_size);
      if (license && license->usable()) {
        decryptor_.reset(new MediaTransform(license->decryptor()));
        bool success = ActivateDecryptor();
        if (!success) {
          decryptor_.reset();
          return false;
        }
      }
    }
    if (!decryptor_) {
      SB_NOTREACHED();
      return false;
    }
  }

  if (encrypted) {
    return decryptor_->TryWrite(input_sample);
  }
  return decoder_->TryWrite(input_sample);
}

bool DecryptingDecoder::ProcessAndRead(ComPtr<IMFSample>* output,
                                       ComPtr<IMFMediaType>* new_type) {
  bool did_something = false;

  *output = decoder_->TryRead(new_type);
  did_something |= *output != NULL;

  if (decryptor_) {
    if (!pending_decryptor_output_) {
      ComPtr<IMFMediaType> ignored_type;
      pending_decryptor_output_ = decryptor_->TryRead(&ignored_type);
      did_something |= pending_decryptor_output_ != NULL;
    }

    if (pending_decryptor_output_) {
      if (decoder_->TryWrite(pending_decryptor_output_)) {
        pending_decryptor_output_.Reset();
        did_something = true;
      }
    }

    if (decryptor_->drained() && !decoder_->draining() &&
        !decoder_->drained()) {
      decoder_->Drain();
      did_something = true;
    }
  }

  return did_something;
}

void DecryptingDecoder::Drain() {
  if (decryptor_) {
    decryptor_->Drain();
  } else {
    decoder_->Drain();
  }
}

bool DecryptingDecoder::ActivateDecryptor() {
  SB_DCHECK(decryptor_);

  ComPtr<IMFMediaType> decoder_output_type = decoder_->GetCurrentOutputType();
  decryptor_->SetInputType(decoder_->GetCurrentInputType());

  GUID original_sub_type;
  decoder_output_type->GetGUID(MF_MT_SUBTYPE, &original_sub_type);

  // Ensure that the decryptor and the decoder agrees on the protection of
  // samples transferred between them.
  ComPtr<IMFSampleProtection> decryption_sample_protection =
      decryptor_->GetSampleProtection();
  SB_DCHECK(decryption_sample_protection);

  DWORD decryption_protection_version;
  HRESULT hr = decryption_sample_protection->GetOutputProtectionVersion(
      &decryption_protection_version);
  CheckResult(hr);

  ComPtr<IMFSampleProtection> decoder_sample_protection =
      decoder_->GetSampleProtection();
  SB_DCHECK(decoder_sample_protection);

  DWORD decoder_protection_version;
  hr = decoder_sample_protection->GetInputProtectionVersion(
      &decoder_protection_version);
  CheckResult(hr);

  DWORD protection_version =
      std::min(decoder_protection_version, decryption_protection_version);
  if (protection_version < SAMPLE_PROTECTION_VERSION_RC4) {
    SB_NOTREACHED();
    return true;
  }

  BYTE* cert_data = NULL;
  DWORD cert_data_size = 0;

  hr = decoder_sample_protection->GetProtectionCertificate(
      protection_version, &cert_data, &cert_data_size);
  CheckResult(hr);

  BYTE* crypt_seed = NULL;
  DWORD crypt_seed_size = 0;
  hr = decryption_sample_protection->InitOutputProtection(
      protection_version, 0, cert_data, cert_data_size, &crypt_seed,
      &crypt_seed_size);
  if (FAILED(hr)) {
    // This can happen if we call InitOutputProtection while procesing
    // a UWP resume event or shortly after.
    return false;
  }
  CheckResult(hr);

  hr = decoder_sample_protection->InitInputProtection(
      protection_version, 0, crypt_seed, crypt_seed_size);
  CheckResult(hr);

  CoTaskMemFree(cert_data);
  CoTaskMemFree(crypt_seed);

  // Ensure that the input type of the decoder is the output type of the
  // decryptor.
  ComPtr<IMFMediaType> decoder_input_type;
  std::vector<ComPtr<IMFMediaType>> decryptor_output_types =
      decryptor_->GetAvailableOutputTypes();
  SB_DCHECK(!decryptor_output_types.empty());

  decryptor_->SetOutputType(decryptor_output_types[0]);
  decoder_->SetInputType(decryptor_output_types[0]);

  std::vector<ComPtr<IMFMediaType>> decoder_output_types =
      decoder_->GetAvailableOutputTypes();
  for (auto output_type : decoder_output_types) {
    GUID sub_type;
    output_type->GetGUID(MF_MT_SUBTYPE, &sub_type);
    if (IsEqualGUID(sub_type, original_sub_type)) {
      decoder_->SetOutputType(output_type);
      return true;
    }
  }
  return true;
}

void DecryptingDecoder::Reset() {
  if (decryptor_) {
    decryptor_->Reset();
  }
  decoder_->Reset();

  last_input_buffer_ = nullptr;
  last_input_sample_ = nullptr;
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
