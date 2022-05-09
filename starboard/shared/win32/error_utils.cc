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

// Not breaking these functions up because however one is implemented, the
// others should be implemented similarly.

#include "starboard/shared/win32/error_utils.h"

#include <Mfapi.h>
#include <Mferror.h>
#include <propvarutil.h>

#include <utility>

#include "starboard/common/log.h"
#include "starboard/shared/win32/wchar_utils.h"

namespace starboard {
namespace shared {
namespace win32 {
namespace {

std::string GetFormatHresultMessage(HRESULT hr) {
  std::stringstream ss;
  LPWSTR error_message;
  int message_size = FormatMessage(
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr,  // Unused with FORMAT_MESSAGE_FROM_SYSTEM.
      hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPTSTR)&error_message,
      0,  // Minimum size for output buffer.
      nullptr);
  SB_DCHECK(message_size);
  ss << wchar_tToUTF8(error_message);
  LocalFree(error_message);
  return ss.str();
}

#define MAKE_HR_PAIR(X) std::pair<HRESULT, std::string>(X, #X)
const std::pair<HRESULT, std::string> kHresultValueStrings[] = {
  MAKE_HR_PAIR(S_OK),
  MAKE_HR_PAIR(MF_E_PLATFORM_NOT_INITIALIZED),
  MAKE_HR_PAIR(MF_E_BUFFERTOOSMALL),
  MAKE_HR_PAIR(MF_E_INVALIDREQUEST),
  MAKE_HR_PAIR(MF_E_INVALIDSTREAMNUMBER),
  MAKE_HR_PAIR(MF_E_INVALIDMEDIATYPE),
  MAKE_HR_PAIR(MF_E_NOTACCEPTING),
  MAKE_HR_PAIR(MF_E_NOT_INITIALIZED),
  MAKE_HR_PAIR(MF_E_UNSUPPORTED_REPRESENTATION),
  MAKE_HR_PAIR(MF_E_NO_MORE_TYPES),
  MAKE_HR_PAIR(MF_E_UNSUPPORTED_SERVICE),
  MAKE_HR_PAIR(MF_E_UNEXPECTED),
  MAKE_HR_PAIR(MF_E_INVALIDNAME),
  MAKE_HR_PAIR(MF_E_INVALIDTYPE),
  MAKE_HR_PAIR(MF_E_INVALID_FILE_FORMAT),
  MAKE_HR_PAIR(MF_E_INVALIDINDEX),
  MAKE_HR_PAIR(MF_E_INVALID_TIMESTAMP),
  MAKE_HR_PAIR(MF_E_UNSUPPORTED_SCHEME),
  MAKE_HR_PAIR(MF_E_UNSUPPORTED_BYTESTREAM_TYPE),
  MAKE_HR_PAIR(MF_E_UNSUPPORTED_TIME_FORMAT),
  MAKE_HR_PAIR(MF_E_NO_SAMPLE_TIMESTAMP),
  MAKE_HR_PAIR(MF_E_NO_SAMPLE_DURATION),
  MAKE_HR_PAIR(MF_E_INVALID_STREAM_DATA),
  MAKE_HR_PAIR(MF_E_RT_UNAVAILABLE),
  MAKE_HR_PAIR(MF_E_UNSUPPORTED_RATE),
  MAKE_HR_PAIR(MF_E_THINNING_UNSUPPORTED),
  MAKE_HR_PAIR(MF_E_REVERSE_UNSUPPORTED),
  MAKE_HR_PAIR(MF_E_UNSUPPORTED_RATE_TRANSITION),
  MAKE_HR_PAIR(MF_E_RATE_CHANGE_PREEMPTED),
  MAKE_HR_PAIR(MF_E_NOT_FOUND),
  MAKE_HR_PAIR(MF_E_NOT_AVAILABLE),
  MAKE_HR_PAIR(MF_E_NO_CLOCK),
  MAKE_HR_PAIR(MF_S_MULTIPLE_BEGIN),
  MAKE_HR_PAIR(MF_E_MULTIPLE_BEGIN),
  MAKE_HR_PAIR(MF_E_MULTIPLE_SUBSCRIBERS),
  MAKE_HR_PAIR(MF_E_TIMER_ORPHANED),
  MAKE_HR_PAIR(MF_E_STATE_TRANSITION_PENDING),
  MAKE_HR_PAIR(MF_E_UNSUPPORTED_STATE_TRANSITION),
  MAKE_HR_PAIR(MF_E_UNRECOVERABLE_ERROR_OCCURRED),
  MAKE_HR_PAIR(MF_E_SAMPLE_HAS_TOO_MANY_BUFFERS),
  MAKE_HR_PAIR(MF_E_SAMPLE_NOT_WRITABLE),
  MAKE_HR_PAIR(MF_E_INVALID_KEY),
  MAKE_HR_PAIR(MF_E_BAD_STARTUP_VERSION),
  MAKE_HR_PAIR(MF_E_UNSUPPORTED_CAPTION),
  MAKE_HR_PAIR(MF_E_INVALID_POSITION),
  MAKE_HR_PAIR(MF_E_ATTRIBUTENOTFOUND),
  MAKE_HR_PAIR(MF_E_PROPERTY_TYPE_NOT_ALLOWED),
  MAKE_HR_PAIR(MF_E_TOPO_INVALID_OPTIONAL_NODE),
  MAKE_HR_PAIR(MF_E_TOPO_CANNOT_FIND_DECRYPTOR),
  MAKE_HR_PAIR(MF_E_TOPO_CODEC_NOT_FOUND),
  MAKE_HR_PAIR(MF_E_TOPO_CANNOT_CONNECT),
  MAKE_HR_PAIR(MF_E_TOPO_UNSUPPORTED),
  MAKE_HR_PAIR(MF_E_TOPO_INVALID_TIME_ATTRIBUTES),
  MAKE_HR_PAIR(MF_E_TOPO_LOOPS_IN_TOPOLOGY),
  MAKE_HR_PAIR(MF_E_TOPO_MISSING_PRESENTATION_DESCRIPTOR),
  MAKE_HR_PAIR(MF_E_TOPO_MISSING_STREAM_DESCRIPTOR),
  MAKE_HR_PAIR(MF_E_TOPO_STREAM_DESCRIPTOR_NOT_SELECTED),
  MAKE_HR_PAIR(MF_E_TOPO_MISSING_SOURCE),
  MAKE_HR_PAIR(MF_E_TOPO_SINK_ACTIVATES_UNSUPPORTED),
  MAKE_HR_PAIR(MF_E_TRANSFORM_TYPE_NOT_SET),
  MAKE_HR_PAIR(MF_E_TRANSFORM_STREAM_CHANGE),
  MAKE_HR_PAIR(MF_E_TRANSFORM_INPUT_REMAINING),
  MAKE_HR_PAIR(MF_E_TRANSFORM_PROFILE_MISSING),
  MAKE_HR_PAIR(MF_E_TRANSFORM_PROFILE_INVALID_OR_CORRUPT),
  MAKE_HR_PAIR(MF_E_TRANSFORM_PROFILE_TRUNCATED),
  MAKE_HR_PAIR(MF_E_TRANSFORM_PROPERTY_PID_NOT_RECOGNIZED),
  MAKE_HR_PAIR(MF_E_TRANSFORM_PROPERTY_VARIANT_TYPE_WRONG),
  MAKE_HR_PAIR(MF_E_TRANSFORM_PROPERTY_NOT_WRITEABLE),
  MAKE_HR_PAIR(MF_E_TRANSFORM_PROPERTY_ARRAY_VALUE_WRONG_NUM_DIM),
  MAKE_HR_PAIR(MF_E_TRANSFORM_PROPERTY_VALUE_SIZE_WRONG),
  MAKE_HR_PAIR(MF_E_TRANSFORM_PROPERTY_VALUE_OUT_OF_RANGE),
  MAKE_HR_PAIR(MF_E_TRANSFORM_PROPERTY_VALUE_INCOMPATIBLE),
  MAKE_HR_PAIR(MF_E_TRANSFORM_NOT_POSSIBLE_FOR_CURRENT_OUTPUT_MEDIATYPE),
  MAKE_HR_PAIR(MF_E_TRANSFORM_NOT_POSSIBLE_FOR_CURRENT_INPUT_MEDIATYPE),
  MAKE_HR_PAIR(MF_E_TRANSFORM_NOT_POSSIBLE_FOR_CURRENT_MEDIATYPE_COMBINATION),
  MAKE_HR_PAIR(MF_E_TRANSFORM_CONFLICTS_WITH_OTHER_CURRENTLY_ENABLED_FEATURES),
  MAKE_HR_PAIR(MF_E_TRANSFORM_NEED_MORE_INPUT),
  MAKE_HR_PAIR(MF_E_TRANSFORM_NOT_POSSIBLE_FOR_CURRENT_SPKR_CONFIG),
  MAKE_HR_PAIR(MF_E_TRANSFORM_CANNOT_CHANGE_MEDIATYPE_WHILE_PROCESSING),
  MAKE_HR_PAIR(MF_S_TRANSFORM_DO_NOT_PROPAGATE_EVENT),
  MAKE_HR_PAIR(MF_E_UNSUPPORTED_D3D_TYPE),
  MAKE_HR_PAIR(MF_E_TRANSFORM_ASYNC_LOCKED),
  MAKE_HR_PAIR(MF_E_TRANSFORM_CANNOT_INITIALIZE_ACM_DRIVER),
};
#undef MAKE_HR_PAIR

bool FindHResultEnumString(HRESULT hr, std::string* output) {
  const size_t n = sizeof(kHresultValueStrings) /
                   sizeof(*kHresultValueStrings);

  for (auto i = 0; i < n; ++i) {
    const auto& elems = kHresultValueStrings[i];
    if (hr == elems.first) {
      *output = elems.second;
      return true;
    }
  }
  return false;
}

}  // namespace

std::ostream& operator<<(std::ostream& os, const Win32ErrorCode& error_code) {
  LPWSTR error_message;
  int message_size = FormatMessage(
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr,  // Unused with FORMAT_MESSAGE_FROM_SYSTEM.
      error_code.GetHRESULT(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPTSTR)&error_message,
      0,  // Minimum size for output buffer.
      nullptr);
  SB_DCHECK(message_size);
  os << wchar_tToUTF8(error_message);
  LocalFree(error_message);

  return os;
}

void DebugLogWinError() {
#if defined(_DEBUG)
  DWORD error_code = GetLastError();
  if (!error_code)
    return;

  SB_LOG(ERROR) << Win32ErrorCode(error_code);
#endif  // defined(_DEBUG)
}

std::string HResultToString(HRESULT hr) {
  std::string enum_str;
  bool has_enum_str = FindHResultEnumString(hr, &enum_str);
  std::string error_message = GetFormatHresultMessage(hr);

  std::stringstream ss;
  if (has_enum_str) {
    ss << enum_str << ": ";
  }

  ss << "\"" << error_message << "\"";
  return ss.str();
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
