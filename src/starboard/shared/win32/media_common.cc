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

#include "starboard/shared/win32/media_common.h"

#include <Mfapi.h>
#include <Mferror.h>
#include <Mfidl.h>
#include <Mfobjects.h>
#include <Rpc.h>
#include <comutil.h>
#include <wrl\client.h>  // For ComPtr.

#include "starboard/common/ref_counted.h"
#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/mutex.h"
#include "starboard/shared/starboard/player/closure.h"
#include "starboard/shared/starboard/player/filter/player_components.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/video_frame_internal.h"
#include "starboard/string.h"

namespace starboard {
namespace shared {
namespace win32 {

// Converts 90khz to 10Mhz (100ns time).
int64_t ConvertToWin32Time(SbMediaTime input) {
  int64_t out = input;
  out *= 1000;
  out /= 9;
  return out;
}

// Convert the other way around.
SbMediaTime ConvertToMediaTime(int64_t input) {
  SbMediaTime out = input;
  out *= 9;
  out /= 1000;
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

HRESULT CreateDecoderTransform(const GUID& decoder_guid,
                               ComPtr<IMFTransform>* transform) {
  return CoCreateInstance(decoder_guid, NULL, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(transform->GetAddressOf()));
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
