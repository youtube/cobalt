// Copyright 2018 Google Inc. All Rights Reserved.
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

// This file implements the FFMPEGDispatch interface for a library linked
// directly, or which the symbols are already available in the global namespace.

#include "starboard/shared/ffmpeg/ffmpeg_dispatch.h"

#include <dlfcn.h>
#include <map>

#include "starboard/common/scoped_ptr.h"
#include "starboard/log.h"
#include "starboard/once.h"
#include "starboard/shared/ffmpeg/ffmpeg_common.h"
#include "starboard/shared/starboard/lazy_initialization_internal.h"
#include "starboard/string.h"

namespace starboard {
namespace shared {
namespace ffmpeg {
namespace {

FFMPEGDispatch* g_ffmpeg_dispatch_impl;
SbOnceControl g_construct_ffmpeg_dispatch_once = SB_ONCE_INITIALIZER;

void construct_ffmpeg_dispatch() {
  g_ffmpeg_dispatch_impl = new FFMPEGDispatch();
}

void LoadSymbols(FFMPEGDispatch* ffmpeg) {
  SB_DCHECK(ffmpeg->is_valid());
// Load the desired symbols from the shared libraries. Note: If a symbol is
// listed as a '.text' entry in the output of 'objdump -T' on the shared
// library file, then it is directly available from it.

#define INITSYMBOL(symbol) \
  ffmpeg->symbol = reinterpret_cast<decltype(FFMPEGDispatch::symbol)>(symbol);

  // Load symbols from the avutil shared library.
  INITSYMBOL(avutil_version);
  SB_DCHECK(ffmpeg->avutil_version);
  INITSYMBOL(av_malloc);
  INITSYMBOL(av_freep);
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 8, 0)
  INITSYMBOL(av_frame_alloc);
  INITSYMBOL(av_frame_unref);
#endif
  INITSYMBOL(av_samples_get_buffer_size);
  INITSYMBOL(av_opt_set_int);
  INITSYMBOL(av_image_check_size);
  INITSYMBOL(av_buffer_create);

  // Load symbols from the avcodec shared library.
  INITSYMBOL(avcodec_version);
  SB_DCHECK(ffmpeg->avcodec_version);
  INITSYMBOL(avcodec_alloc_context3);
  INITSYMBOL(avcodec_find_decoder);
  INITSYMBOL(avcodec_close);
  INITSYMBOL(avcodec_open2);
  INITSYMBOL(av_init_packet);
  INITSYMBOL(avcodec_decode_audio4);
  INITSYMBOL(avcodec_decode_video2);
  INITSYMBOL(avcodec_flush_buffers);
#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(52, 8, 0)
  INITSYMBOL(avcodec_alloc_frame);
  INITSYMBOL(avcodec_get_frame_defaults);
#endif

  // Load symbols from the avformat shared library.
  INITSYMBOL(avformat_version);
  SB_DCHECK(ffmpeg->avformat_version);
  INITSYMBOL(av_register_all);
  SB_DCHECK(ffmpeg->av_register_all);

#undef INITSYMBOL
}
}  // namespace

FFMPEGDispatch::FFMPEGDispatch() {
  LoadSymbols(this);
  av_register_all();
}

FFMPEGDispatch::~FFMPEGDispatch() {}

// static
FFMPEGDispatch* FFMPEGDispatch::GetInstance() {
  bool initialized =
      SbOnce(&g_construct_ffmpeg_dispatch_once, construct_ffmpeg_dispatch);
  SB_DCHECK(initialized);
  return g_ffmpeg_dispatch_impl;
}

// static
bool FFMPEGDispatch::RegisterSpecialization(int specialization,
                                            int avcodec,
                                            int avformat,
                                            int avutil) {
  // There is always only a single version of the library linked to the
  // application, so there is no need to explicitly track multiple versions.
  return true;
}

bool FFMPEGDispatch::is_valid() const {
  // Loading of a linked library is always successful.
  return true;
}

int FFMPEGDispatch::specialization_version() const {
  // There is only one version of ffmpeg linked to the binary.
  return FFMPEG;
}

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard
