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

#include "starboard/shared/win32/media_foundation_utils.h"

#include <Mfapi.h>
#include <Mferror.h>
#include <propvarutil.h>

#include <ios>
#include <sstream>
#include <utility>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/shared/win32/error_utils.h"

namespace starboard {
namespace shared {
namespace win32 {

namespace {

#define MAKE_GUID_PAIR(X) std::pair<GUID, std::string>(X, #X)

const std::pair<GUID, std::string> kMfMtAudio[] = {
  MAKE_GUID_PAIR(MF_MT_AAC_PAYLOAD_TYPE),
  MAKE_GUID_PAIR(MF_MT_AUDIO_AVG_BYTES_PER_SECOND),
  MAKE_GUID_PAIR(MF_MT_AUDIO_BITS_PER_SAMPLE),
  MAKE_GUID_PAIR(MF_MT_AUDIO_BLOCK_ALIGNMENT),
  MAKE_GUID_PAIR(MF_MT_AUDIO_CHANNEL_MASK),
  MAKE_GUID_PAIR(MF_MT_AUDIO_FLOAT_SAMPLES_PER_SECOND),
  MAKE_GUID_PAIR(MF_MT_AUDIO_NUM_CHANNELS),
  MAKE_GUID_PAIR(MF_MT_AUDIO_SAMPLES_PER_BLOCK),
  MAKE_GUID_PAIR(MF_MT_AUDIO_SAMPLES_PER_SECOND),
  MAKE_GUID_PAIR(MF_MT_AUDIO_NUM_CHANNELS),
  MAKE_GUID_PAIR(MF_MT_MAJOR_TYPE),
  MAKE_GUID_PAIR(MF_MT_AUDIO_PREFER_WAVEFORMATEX),
  MAKE_GUID_PAIR(MF_MT_USER_DATA),
  MAKE_GUID_PAIR(MF_MT_SUBTYPE),
  MAKE_GUID_PAIR(MFAudioFormat_AAC),
  MAKE_GUID_PAIR(MFAudioFormat_ADTS),
  MAKE_GUID_PAIR(MFAudioFormat_ALAC),
  MAKE_GUID_PAIR(MFAudioFormat_AMR_NB),
  MAKE_GUID_PAIR(MFAudioFormat_AMR_WB),
  MAKE_GUID_PAIR(MFAudioFormat_AMR_WP),
  MAKE_GUID_PAIR(MFAudioFormat_Dolby_AC3),
  MAKE_GUID_PAIR(MFAudioFormat_Dolby_AC3_SPDIF),
  MAKE_GUID_PAIR(MFAudioFormat_Dolby_DDPlus),
  MAKE_GUID_PAIR(MFAudioFormat_DRM),
  MAKE_GUID_PAIR(MFAudioFormat_DTS),
  MAKE_GUID_PAIR(MFAudioFormat_FLAC),
  MAKE_GUID_PAIR(MFAudioFormat_Float),
  MAKE_GUID_PAIR(MFAudioFormat_Float_SpatialObjects),
  MAKE_GUID_PAIR(MFAudioFormat_MP3),
  MAKE_GUID_PAIR(MFAudioFormat_MPEG),
  MAKE_GUID_PAIR(MFAudioFormat_MSP1),
  MAKE_GUID_PAIR(MFAudioFormat_Opus),
  MAKE_GUID_PAIR(MFAudioFormat_PCM),
  MAKE_GUID_PAIR(MFAudioFormat_WMASPDIF),
  MAKE_GUID_PAIR(MFAudioFormat_WMAudio_Lossless),
  MAKE_GUID_PAIR(MFAudioFormat_WMAudioV8),
  MAKE_GUID_PAIR(MFAudioFormat_WMAudioV9),
  MAKE_GUID_PAIR(MFAudioFormat_WMAudioV9),
  MAKE_GUID_PAIR(MFMediaType_Audio),
};
#undef MAKE_GUID_PAIR

std::string GuidToFallbackString(GUID guid) {
  std::stringstream ss;
  wchar_t* guid_str = nullptr;
  StringFromCLSID(guid, &guid_str);
  ss << guid_str;
  CoTaskMemFree(guid_str);
  return ss.str();
}

std::string MfGuidToString(GUID guid) {
  const size_t n = sizeof(kMfMtAudio) / sizeof(*kMfMtAudio);
  for (auto i = 0; i < n; ++i) {
    const auto& elems = kMfMtAudio[i];
    if (guid == elems.first) {
      return elems.second;
    }
  }
  return GuidToFallbackString(guid);
}

std::string ImfAttributesToString(IMFAttributes* type) {
  std::stringstream ss;
  UINT32 n = 0;
  HRESULT hr = type->GetCount(&n);
  CheckResult(hr);
  for (UINT32 i = 0; i < n; ++i) {
    GUID key;
    PROPVARIANT val;
    type->GetItemByIndex(i, &key, &val);
    PropVariantClear(&val);

    MF_ATTRIBUTE_TYPE attrib_type;
    hr = type->GetItemType(key, &attrib_type);
    CheckResult(hr);

    std::string key_str = MfGuidToString(key);
    ss << key_str << ": ";

    switch (attrib_type) {
      case MF_ATTRIBUTE_GUID: {
        GUID value_guid;
        hr = type->GetGUID(key, &value_guid);
        ss << MfGuidToString(value_guid) << "\n";
        break;
      }

      case MF_ATTRIBUTE_DOUBLE: {
        double value = 0;
        hr = type->GetDouble(key, &value);
        ss << value << "\n";
        break;
      }

      case MF_ATTRIBUTE_BLOB: {
        // Skip.
        ss << "<BLOB>" << "\n";
        break;
      }

      case MF_ATTRIBUTE_UINT32: {
        UINT32 int_val = 0;
        hr = type->GetUINT32(key, &int_val);
        ss << int_val << "\n";
        break;
      }

      case MF_ATTRIBUTE_UINT64: {
        UINT64 int_val = 0;
        hr = type->GetUINT64(key, &int_val);
        ss << int_val << "\n";
        break;
      }

      case MF_ATTRIBUTE_STRING: {
        UINT32 length = 0;
        hr = type->GetStringLength(key, &length);
        CheckResult(hr);
        ++length;  // For trailing 0.
        std::vector<wchar_t> buffer(length);
        hr = type->GetString(key, buffer.data(), length, NULL);
        CheckResult(hr);
        ss << buffer.data() << "\n";
        break;
      }

      case MF_ATTRIBUTE_IUNKNOWN: {
        SB_NOTIMPLEMENTED();
        break;
      }
    }
  }
  ss << "\n";
  return ss.str();
}

}  // namespace.

void CopyProperties(IMFMediaType* source, IMFMediaType* destination) {
  UINT32 attribute_count = 0;
  HRESULT hr = source->GetCount(&attribute_count);
  CheckResult(hr);
  for (UINT32 i = 0; i < attribute_count; ++i) {
    GUID key;
    PROPVARIANT variant;
    hr = source->GetItemByIndex(i, &key, &variant);
    CheckResult(hr);
    PropVariantClear(&variant);

    MF_ATTRIBUTE_TYPE attrib_type;
    hr = source->GetItemType(key, &attrib_type);
    CheckResult(hr);

    switch (attrib_type) {
      case MF_ATTRIBUTE_GUID: {
        GUID value_guid;
        hr = source->GetGUID(key, &value_guid);
        CheckResult(hr);
        hr = destination->SetGUID(key, value_guid);
        CheckResult(hr);
        break;
      }

      case MF_ATTRIBUTE_DOUBLE: {
        double value = 0;
        hr = source->GetDouble(key, &value);
        CheckResult(hr);
        hr = destination->SetDouble(key, value);
        CheckResult(hr);
        break;
      }

      case MF_ATTRIBUTE_BLOB: {
        UINT32 blob_size = 0;
        hr = source->GetBlobSize(key, &blob_size);
        CheckResult(hr);
        std::vector<UINT8> blob(blob_size);
        hr = source->GetBlob(key, blob.data(), blob_size, &blob_size);
        CheckResult(hr);
        hr = destination->SetBlob(key, blob.data(), blob_size);
        CheckResult(hr);
        break;
      }

      case MF_ATTRIBUTE_UINT32: {
        UINT32 int_val = 0;
        hr = source->GetUINT32(key, &int_val);
        CheckResult(hr);
        hr = destination->SetUINT32(key, int_val);
        CheckResult(hr);
        break;
      }

      case MF_ATTRIBUTE_UINT64: {
        UINT64 int_val = 0;
        hr = source->GetUINT64(key, &int_val);
        CheckResult(hr);
        hr = destination->SetUINT64(key, int_val);
        CheckResult(hr);
        break;
      }

      case MF_ATTRIBUTE_STRING: {
        UINT32 length = 0;
        hr = source->GetStringLength(key, &length);
        CheckResult(hr);
        ++length;  // For trailing 0.
        std::vector<wchar_t> buffer(length);
        hr = source->GetString(key, buffer.data(), length, NULL);
        CheckResult(hr);
        hr = destination->SetString(key, buffer.data());
        CheckResult(hr);
        break;
      }

      case MF_ATTRIBUTE_IUNKNOWN: {
        SB_NOTIMPLEMENTED();
        break;
      }
    }
  }
}

std::ostream& operator<<(std::ostream& os, const IMFMediaType& media_type) {
  const IMFAttributes* attribs = &media_type;  // Upcast.
  std::string output_str =
      ImfAttributesToString(const_cast<IMFAttributes*>(attribs));
  os << output_str;
  return os;
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
