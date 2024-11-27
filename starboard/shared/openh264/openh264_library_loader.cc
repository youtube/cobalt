// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include <dlfcn.h>

#include "starboard/common/log.h"
#include "starboard/shared/openh264/openh264_library_loader.h"

namespace starboard {
namespace shared {
namespace openh264 {

namespace {

const char kOpenh264LibraryName[] = "libopenh264.so";

class LibOpenh264Handle {
 public:
  LibOpenh264Handle() { LoadLibrary(); }
  ~LibOpenh264Handle() {
    if (handle_) {
      dlclose(handle_);
    }
  }

  bool IsLoaded() const { return handle_; }

 private:
  void ReportSymbolError() {
    SB_LOG(ERROR) << "Openh264 load error: " << dlerror();
    dlclose(handle_);
    handle_ = NULL;
  }

  void LoadLibrary() {
    SB_DCHECK(!handle_);

    handle_ = dlopen(kOpenh264LibraryName, RTLD_LAZY);
    if (!handle_) {
      return;
    }
#define INITSYMBOL(symbol)                                              \
  symbol = reinterpret_cast<decltype(symbol)>(dlsym(handle_, #symbol)); \
  if (!symbol) {                                                        \
    ReportSymbolError();                                                \
    return;                                                             \
  }
    INITSYMBOL(WelsCreateDecoder);
    INITSYMBOL(WelsDestroyDecoder);
  }
  void* handle_ = NULL;
};

LibOpenh264Handle* GetHandle() {
  static LibOpenh264Handle instance;
  return &instance;
}
}  // namespace

int (*WelsCreateDecoder)(ISVCDecoder** ppDecoder);

void (*WelsDestroyDecoder)(ISVCDecoder* pDecoder);

bool is_openh264_supported() {
  return GetHandle()->IsLoaded();
}

}  // namespace openh264
}  // namespace shared
}  // namespace starboard
