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

#ifndef STARBOARD_SHARED_WIN32_WCHAR_UTILS_H_
#define STARBOARD_SHARED_WIN32_WCHAR_UTILS_H_

#include <codecvt>
#include <cwchar>
#include <locale>
#include <string>

namespace starboard {
namespace shared {
namespace win32 {

inline std::string wchar_tToUTF8(const wchar_t* const str) {
  std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
  return converter.to_bytes(str);
}

inline std::string wchar_tToUTF8(const wchar_t* const str,
                                 const std::size_t size) {
  std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
  return converter.to_bytes(str, str + size);
}

inline std::wstring CStringToWString(const char* str) {
  std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
  return converter.from_bytes(str);
}

#if defined(__cplusplus_winrt)
inline std::string platformStringToString(Platform::String^ to_convert) {
  std::wstring ws(to_convert->Begin(), to_convert->End());
  return wchar_tToUTF8(ws.data(), ws.size());
}

inline Platform::String^ stringToPlatformString(const std::string& to_convert) {
  std::wstring ws(to_convert.begin(), to_convert.end());
  return ref new Platform::String(ws.c_str());
}
#endif

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_WCHAR_UTILS_H_
