// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_CODEC_UTILS_H_
#define SKIA_EXT_CODEC_UTILS_H_

#include <string>

#include "third_party/skia/include/core/SkRefCnt.h"

class GrDirectContext;
class SkData;
class SkImage;
class SkPixmap;

namespace skia {

SK_API sk_sp<SkData> EncodePngAsSkData(const SkPixmap& src);
SK_API sk_sp<SkData> EncodePngAsSkData(GrDirectContext* context,
                                       const SkImage* src);
SK_API sk_sp<SkData> EncodePngAsSkData(GrDirectContext* context,
                                       const SkImage* src,
                                       int zlib_compression_level);
SK_API std::string EncodePngAsDataUri(const SkPixmap& src);

}  // namespace skia

#endif  // SKIA_EXT_CODEC_UTILS_H_
