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

#include "starboard/shared/win32/media_common.h"

#include <Mfapi.h>
#include <Mferror.h>
#include <Mfidl.h>
#include <Mfobjects.h>
#include <Rpc.h>
#include <comutil.h>
#include <wrl\client.h>  // For ComPtr.

#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/ref_counted.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/player/filter/player_components.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"

namespace starboard {
namespace shared {
namespace win32 {

// Converts microseconds to 10Mhz (100ns time).
int64_t ConvertToWin32Time(SbTime input) {
  int64_t out = input;
  out *= 10;
  return out;
}

// Convert the other way around.
SbTime ConvertToSbTime(int64_t input) {
  SbTime out = input;
  out /= 10;
  return out;
}

std::vector<ComPtr<IMFMediaType>> GetAllOutputMediaTypes(
    int stream_id,
    IMFTransform* decoder) {
  std::vector<ComPtr<IMFMediaType>> output;
  for (int index = 0;; ++index) {
    ComPtr<IMFMediaType> media_type;
    HRESULT hr = decoder->GetOutputAvailableType(stream_id, index, &media_type);
    if (SUCCEEDED(hr)) {
      output.push_back(media_type);
    } else {
      SB_DCHECK(hr == MF_E_NO_MORE_TYPES);
      break;
    }
  }
  return output;
}

std::vector<ComPtr<IMFMediaType>> GetAllInputMediaTypes(
    int stream_id,
    IMFTransform* transform) {
  std::vector<ComPtr<IMFMediaType>> input_types;

  for (DWORD i = 0;; ++i) {
    ComPtr<IMFMediaType> curr_type;
    HRESULT hr = transform->GetInputAvailableType(stream_id, i,
                                                  curr_type.GetAddressOf());
    if (FAILED(hr)) {
      break;
    }
    input_types.push_back(curr_type);
  }
  return input_types;
}

std::vector<ComPtr<IMFMediaType>> FilterMediaBySubType(
    const std::vector<ComPtr<IMFMediaType>>& input,
    GUID sub_type_filter) {
  std::vector<ComPtr<IMFMediaType>> output;
  for (auto it = input.begin(); it != input.end(); ++it) {
    ComPtr<IMFMediaType> media_type = *it;
    GUID media_sub_type = {0};
    media_type->GetGUID(MF_MT_SUBTYPE, &media_sub_type);
    if (IsEqualGUID(media_sub_type, sub_type_filter)) {
      output.push_back(media_type);
    }
  }
  return output;
}

HRESULT CreateAV1Decoder(const IID& iid, void** object) {
  MFT_REGISTER_TYPE_INFO type_info = {MFMediaType_Video, MFVideoFormat_AV1};
  MFT_REGISTER_TYPE_INFO output_info = {MFMediaType_Video, MFVideoFormat_NV12};

  IMFActivate** acts;
  UINT32 acts_num = 0;
  HRESULT hr = ::MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER,
                           MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT |
                               MFT_ENUM_FLAG_UNTRUSTED_STOREMFT,
                           &type_info, &output_info, &acts, &acts_num);
  if (FAILED(hr))
    return hr;

  if (acts_num < 1)
    return E_FAIL;

  hr = acts[0]->ActivateObject(iid, object);
  for (UINT32 i = 0; i < acts_num; ++i)
    acts[i]->Release();
  CoTaskMemFree(acts);
  return hr;
}

HRESULT CreateDecoderTransform(const GUID& decoder_guid,
                               ComPtr<IMFTransform>* transform) {
  if (decoder_guid == MFVideoFormat_AV1) {
    return CreateAV1Decoder(IID_PPV_ARGS(transform->GetAddressOf()));
  }
  return CoCreateInstance(decoder_guid, NULL, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(transform->GetAddressOf()));
}

bool IsHardwareAv1DecoderSupported() {
  static bool av1_decoder_supported = false;
  static bool av1_support_checked = false;
  if (!av1_support_checked) {
    MFT_REGISTER_TYPE_INFO type_info = {MFMediaType_Video, MFVideoFormat_AV1};
    MFT_REGISTER_TYPE_INFO output_info = {MFMediaType_Video,
                                          MFVideoFormat_NV12};

    IMFActivate** acts;
    UINT32 acts_num = 0;
    HRESULT hr = ::MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER,
                             MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT |
                                 MFT_ENUM_FLAG_UNTRUSTED_STOREMFT,
                             &type_info, &output_info, &acts, &acts_num);
    for (UINT32 i = 0; i < acts_num; ++i)
      acts[i]->Release();
    av1_decoder_supported = SUCCEEDED(hr) && acts_num >= 1;
    av1_support_checked = true;
  }
  return av1_decoder_supported;
}
}  // namespace win32
}  // namespace shared
}  // namespace starboard
