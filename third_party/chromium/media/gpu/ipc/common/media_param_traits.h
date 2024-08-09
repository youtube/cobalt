// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_GPU_IPC_COMMON_MEDIA_PARAM_TRAITS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_IPC_COMMON_MEDIA_PARAM_TRAITS_H_

#include "third_party/chromium/media/base/bitstream_buffer.h"
#include "third_party/chromium/media/gpu/ipc/common/media_param_traits_macros.h"

namespace IPC {

template <>
struct ParamTraits<media_m96::BitstreamBuffer> {
  using param_type = media_m96::BitstreamBuffer;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_GPU_IPC_COMMON_MEDIA_PARAM_TRAITS_H_
