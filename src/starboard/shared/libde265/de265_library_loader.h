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

#ifndef STARBOARD_SHARED_LIBDE265_DE265_LIBRARY_LOADER_H_
#define STARBOARD_SHARED_LIBDE265_DE265_LIBRARY_LOADER_H_

#include <libde265/de265.h>

#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace de265 {

bool is_de265_supported();

extern de265_decoder_context* (*de265_new_decoder)();
extern de265_error (*de265_start_worker_threads)(de265_decoder_context*,
                                                 int number_of_threads);
extern de265_error (*de265_free_decoder)(de265_decoder_context*);

extern de265_error (*de265_push_data)(de265_decoder_context*,
                                      const void* data,
                                      int length,
                                      de265_PTS pts,
                                      void* user_data);
extern de265_error (*de265_decode)(de265_decoder_context*, int* more);
extern de265_error (*de265_flush_data)(de265_decoder_context*);
extern void (*de265_reset)(de265_decoder_context*);

extern const de265_image* (*de265_get_next_picture)(de265_decoder_context*);
extern int (*de265_get_image_width)(const de265_image*, int channel);
extern int (*de265_get_image_height)(const de265_image*, int channel);
extern de265_chroma (*de265_get_chroma_format)(const de265_image*);
extern int (*de265_get_bits_per_pixel)(const de265_image*, int channel);
extern const uint8_t* (*de265_get_image_plane)(const de265_image*,
                                               int channel,
                                               int* out_stride);
extern de265_PTS (*de265_get_image_PTS)(const de265_image*);

}  // namespace de265
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_LIBDE265_DE265_LIBRARY_LOADER_H_
