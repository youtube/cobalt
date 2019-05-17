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

#include "starboard/shared/libvpx/vpx_library_loader.h"

#include <dlfcn.h>

#include "starboard/common/log.h"

namespace starboard {
namespace shared {
namespace vpx {

namespace {

const char kVpxLibraryName[] = "libvpx.so";

class LibvpxHandle {
 public:
  LibvpxHandle() { LoadLibrary(); }
  ~LibvpxHandle() {
    if (handle_) {
      dlclose(handle_);
    }
  }

  bool IsLoaded() const { return handle_; }

 private:
  void ReportSymbolError() {
    SB_LOG(ERROR) << "Vpx load error: " << dlerror();
    dlclose(handle_);
    handle_ = NULL;
  }

  void LoadLibrary() {
    SB_DCHECK(!handle_);

    handle_ = dlopen(kVpxLibraryName, RTLD_LAZY);
    if (!handle_) {
      return;
    }

#define INITSYMBOL(symbol)                                              \
  symbol = reinterpret_cast<decltype(symbol)>(dlsym(handle_, #symbol)); \
  if (!symbol) {                                                        \
    ReportSymbolError();                                                \
    return;                                                             \
  }

    INITSYMBOL(vpx_codec_dec_init_ver);
    INITSYMBOL(vpx_codec_vp9_dx);
    INITSYMBOL(vpx_codec_destroy);
    INITSYMBOL(vpx_codec_decode);
    INITSYMBOL(vpx_codec_get_frame);

    // Ensure that the abi version used to build the Cobalt binary matches the
    // abi version of the "libvpx.so".
    vpx_codec_ctx vpx_codec_ctx;
    vpx_codec_dec_cfg_t vpx_config = {0};
    vpx_config.w = 640;
    vpx_config.h = 480;
    vpx_config.threads = 8;

    vpx_codec_err_t status =
        vpx_codec_dec_init(&vpx_codec_ctx, vpx_codec_vp9_dx(), &vpx_config, 0);
    if (status != VPX_CODEC_OK) {
      ReportSymbolError();
      return;
    }
    vpx_codec_destroy(&vpx_codec_ctx);
  }

  void* handle_ = NULL;
};

LibvpxHandle* GetHandle() {
  static LibvpxHandle instance;
  return &instance;
}
}  // namespace

vpx_codec_err_t (*vpx_codec_dec_init_ver)(vpx_codec_ctx_t*,
                                          vpx_codec_iface_t*,
                                          const vpx_codec_dec_cfg_t*,
                                          vpx_codec_flags_t,
                                          int) = NULL;

vpx_codec_iface_t* (*vpx_codec_vp9_dx)(void) = NULL;

vpx_codec_err_t (*vpx_codec_destroy)(vpx_codec_ctx_t*) = NULL;

vpx_codec_err_t (*vpx_codec_decode)(vpx_codec_ctx_t*,
                                    const uint8_t*,
                                    unsigned int,
                                    void*,
                                    long) = NULL;

vpx_image_t* (*vpx_codec_get_frame)(vpx_codec_ctx_t*, vpx_codec_iter_t*) = NULL;

bool is_vpx_supported() {
  return GetHandle()->IsLoaded();
}

}  // namespace vpx
}  // namespace shared
}  // namespace starboard
