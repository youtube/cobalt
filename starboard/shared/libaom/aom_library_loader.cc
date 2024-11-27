// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/libaom/aom_library_loader.h"

#include <dlfcn.h>

#include "starboard/common/log.h"

namespace starboard {
namespace shared {
namespace aom {

namespace {

const char kAomLibraryName[] = "libaom.so";

class LibaomHandle {
 public:
  LibaomHandle() { LoadLibrary(); }
  ~LibaomHandle() {
    if (handle_) {
      dlclose(handle_);
    }
  }

  bool IsLoaded() const { return handle_; }

 private:
  void ReportSymbolError() {
    SB_LOG(ERROR) << "Aom load error: " << dlerror();
    dlclose(handle_);
    handle_ = NULL;
  }

  void LoadLibrary() {
    SB_DCHECK(!handle_);

    handle_ = dlopen(kAomLibraryName, RTLD_LAZY);
    if (!handle_) {
      return;
    }

#define INITSYMBOL(symbol)                                              \
  symbol = reinterpret_cast<decltype(symbol)>(dlsym(handle_, #symbol)); \
  if (!symbol) {                                                        \
    ReportSymbolError();                                                \
    return;                                                             \
  }

    INITSYMBOL(aom_codec_dec_init_ver);
    INITSYMBOL(aom_codec_av1_dx);
    INITSYMBOL(aom_codec_destroy);
    INITSYMBOL(aom_codec_decode);
    INITSYMBOL(aom_codec_get_frame);
  }

  void* handle_ = NULL;
};

LibaomHandle* GetHandle() {
  static LibaomHandle instance;
  return &instance;
}

}  // namespace

aom_codec_err_t (*aom_codec_dec_init_ver)(aom_codec_ctx_t*,
                                          aom_codec_iface_t*,
                                          const aom_codec_dec_cfg_t*,
                                          aom_codec_flags_t,
                                          int) = NULL;

aom_codec_iface_t* (*aom_codec_av1_dx)(void) = NULL;

aom_codec_err_t (*aom_codec_destroy)(aom_codec_ctx_t*) = NULL;

aom_codec_err_t (*aom_codec_decode)(aom_codec_ctx_t*,
                                    const uint8_t*,
                                    size_t,
                                    void*) = NULL;

aom_image_t* (*aom_codec_get_frame)(aom_codec_ctx_t*, aom_codec_iter_t*) = NULL;

bool is_aom_supported() {
  return GetHandle()->IsLoaded();
}

}  // namespace aom
}  // namespace shared
}  // namespace starboard
