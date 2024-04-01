// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_IPC_COMMON_MEDIA_PARAM_TRAITS_H_
#define MEDIA_GPU_IPC_COMMON_MEDIA_PARAM_TRAITS_H_

#include "media/base/bitstream_buffer.h"
#include "media/gpu/ipc/common/media_param_traits_macros.h"

namespace IPC {

template <>
struct ParamTraits<media::BitstreamBuffer> {
  using param_type = media::BitstreamBuffer;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // MEDIA_GPU_IPC_COMMON_MEDIA_PARAM_TRAITS_H_
