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

#ifndef STARBOARD_SHARED_LIBAOM_AOM_LIBRARY_LOADER_H_
#define STARBOARD_SHARED_LIBAOM_AOM_LIBRARY_LOADER_H_

#include <aom/aom_decoder.h>

#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace aom {

bool is_aom_supported();

extern aom_codec_err_t (*aom_codec_dec_init_ver)(aom_codec_ctx_t*,
                                                 aom_codec_iface_t*,
                                                 const aom_codec_dec_cfg_t*,
                                                 aom_codec_flags_t,
                                                 int);

extern aom_codec_iface_t* (*aom_codec_av1_dx)(void);

extern aom_codec_err_t (*aom_codec_destroy)(aom_codec_ctx_t*);

extern aom_codec_err_t (*aom_codec_decode)(aom_codec_ctx_t*,
                                           const uint8_t*,
                                           size_t,
                                           void*);

extern aom_image_t* (*aom_codec_get_frame)(aom_codec_ctx_t*, aom_codec_iter_t*);

}  // namespace aom
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_LIBAOM_AOM_LIBRARY_LOADER_H_
