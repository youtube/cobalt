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

#ifndef STARBOARD_SHARED_LIBVPX_VPX_LIBRARY_LOADER_H_
#define STARBOARD_SHARED_LIBVPX_VPX_LIBRARY_LOADER_H_

#include <vpx/vpx_decoder.h>

#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace vpx {

bool is_vpx_supported();

extern vpx_codec_err_t (*vpx_codec_dec_init_ver)(vpx_codec_ctx_t*,
                                                 vpx_codec_iface_t*,
                                                 const vpx_codec_dec_cfg_t*,
                                                 vpx_codec_flags_t,
                                                 int);

extern vpx_codec_iface_t* (*vpx_codec_vp9_dx)(void);

extern vpx_codec_err_t (*vpx_codec_destroy)(vpx_codec_ctx_t*);

extern vpx_codec_err_t (*vpx_codec_decode)(vpx_codec_ctx_t*,
                                           const uint8_t*,
                                           unsigned int,
                                           void*,
                                           long);

extern vpx_image_t* (*vpx_codec_get_frame)(vpx_codec_ctx_t*, vpx_codec_iter_t*);

}  // namespace vpx
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_LIBVPX_VPX_LIBRARY_LOADER_H_
