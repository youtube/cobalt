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

#include "starboard/shared/libde265/de265_library_loader.h"

#include <dlfcn.h>

#include "starboard/common/log.h"

namespace starboard {
namespace shared {
namespace de265 {

namespace {

const char kDe265LibraryName[] = "libde265.so";

class Libde265Handle {
 public:
  Libde265Handle() { LoadLibrary(); }
  ~Libde265Handle() {
    if (handle_) {
      dlclose(handle_);
    }
  }

  bool IsLoaded() const { return handle_; }

 private:
  void ReportSymbolError() {
    SB_LOG(ERROR) << "De265 load error: " << dlerror();
    dlclose(handle_);
    handle_ = NULL;
  }

  void LoadLibrary() {
    SB_DCHECK(!handle_);

    handle_ = dlopen(kDe265LibraryName, RTLD_LAZY);
    if (!handle_) {
      return;
    }

#define INITSYMBOL(symbol)                                              \
  symbol = reinterpret_cast<decltype(symbol)>(dlsym(handle_, #symbol)); \
  if (!symbol) {                                                        \
    ReportSymbolError();                                                \
    return;                                                             \
  }

    INITSYMBOL(de265_new_decoder);
    INITSYMBOL(de265_start_worker_threads);
    INITSYMBOL(de265_free_decoder);

    INITSYMBOL(de265_push_data);
    INITSYMBOL(de265_decode);
    INITSYMBOL(de265_flush_data);
    INITSYMBOL(de265_reset);

    INITSYMBOL(de265_get_next_picture);
    INITSYMBOL(de265_get_image_width);
    INITSYMBOL(de265_get_image_height);
    INITSYMBOL(de265_get_chroma_format);
    INITSYMBOL(de265_get_bits_per_pixel);
    INITSYMBOL(de265_get_image_plane);
    INITSYMBOL(de265_get_image_PTS);
  }

  void* handle_ = NULL;
};

Libde265Handle* GetHandle() {
  static Libde265Handle instance;
  return &instance;
}
}  // namespace

de265_decoder_context* (*de265_new_decoder)();
de265_error (*de265_start_worker_threads)(de265_decoder_context*,
                                          int number_of_threads);
de265_error (*de265_free_decoder)(de265_decoder_context*);

de265_error (*de265_push_data)(de265_decoder_context*,
                               const void* data,
                               int length,
                               de265_PTS pts,
                               void* user_data);
de265_error (*de265_decode)(de265_decoder_context*, int* more);
de265_error (*de265_flush_data)(de265_decoder_context*);
void (*de265_reset)(de265_decoder_context*);

const de265_image* (*de265_get_next_picture)(de265_decoder_context*);
int (*de265_get_image_width)(const struct de265_image*, int channel);
int (*de265_get_image_height)(const struct de265_image*, int channel);
de265_chroma (*de265_get_chroma_format)(const de265_image*);
int (*de265_get_bits_per_pixel)(const de265_image*, int channel);
const uint8_t* (*de265_get_image_plane)(const de265_image*,
                                        int channel,
                                        int* out_stride);
de265_PTS (*de265_get_image_PTS)(const de265_image*);

bool is_de265_supported() {
  return GetHandle()->IsLoaded();
}

}  // namespace de265
}  // namespace shared
}  // namespace starboard
