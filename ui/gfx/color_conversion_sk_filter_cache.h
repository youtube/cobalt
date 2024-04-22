// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_CONVERSION_SK_FILTER_CACHE_H_
#define UI_GFX_COLOR_CONVERSION_SK_FILTER_CACHE_H_

#include "base/containers/flat_map.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_space_export.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/hdr_metadata.h"

class GrDirectContext;
class SkImage;
class SkColorFilter;
class SkRuntimeEffect;
struct SkGainmapInfo;

namespace gfx {

class COLOR_SPACE_EXPORT ColorConversionSkFilterCache {
 public:
  ColorConversionSkFilterCache();
  ColorConversionSkFilterCache(const ColorConversionSkFilterCache&) = delete;
  ColorConversionSkFilterCache& operator=(const ColorConversionSkFilterCache&) =
      delete;
  ~ColorConversionSkFilterCache();

  // Retrieve an SkColorFilter to transform `src` to `dst`. The bit depth of
  // `src` maybe specified in `src_bit_depth` (relevant only for YUV to RGB
  // conversion). The filter also applies the offset `src_resource_offset` and
  // then scales by `src_resource_multiplier`. Apply tone mapping of `src` is
  // HLG or PQ, using `sdr_max_luminance_nits`, `src_hdr_metadata`, and
  // `dst_max_luminance_relative` as parameters.
  sk_sp<SkColorFilter> Get(const gfx::ColorSpace& src,
                           const gfx::ColorSpace& dst,
                           float resource_offset,
                           float resource_multiplier,
                           absl::optional<uint32_t> src_bit_depth,
                           absl::optional<gfx::HDRMetadata> src_hdr_metadata,
                           float sdr_max_luminance_nits,
                           float dst_max_luminance_relative);

  // Convert `image` to be in `target_color_space`, performing tone mapping as
  // needed (using `sdr_max_luminance_nits` and `dst_max_luminance_relative`).
  // If `image` is GPU backed then `context` should be its GrDirectContext,
  // otherwise, `context` should be nullptr. The resulting image will not have
  // mipmaps.
  // If the feature ImageToneMapping is disabled, then this function is
  // equivalent to calling `image->makeColorSpace(target_color_space, context)`,
  // and no tone mapping is performed.
  sk_sp<SkImage> ConvertImage(sk_sp<SkImage> image,
                              sk_sp<SkColorSpace> target_color_space,
                              absl::optional<gfx::HDRMetadata> src_hdr_metadata,
                              float sdr_max_luminance_nits,
                              float dst_max_luminance_relative,
                              bool enable_tone_mapping,
                              GrDirectContext* context);

  // Apply the gainmap in `gainmap_image` to `base_image`, using the parameters
  // in `gainmap_info` and `dst_max_luminance_relative`, and return the
  // resulting image.
  // * If `context` is non-nullptr, then `base_image` and `gainmap_image` must
  //   be texture-backed and on `context`, and the result will be texture backed
  //   and on `context`.
  // * If `context` is nullptr, then the arguments should be bitmaps, and the
  //   result will be a bitmap.
  sk_sp<SkImage> ApplyGainmap(sk_sp<SkImage> base_image,
                              sk_sp<SkImage> gainmap_image,
                              const SkGainmapInfo& gainmap_info,
                              float dst_max_luminance_relative,
                              GrDirectContext* context);

 public:
  struct Key {
    Key(const gfx::ColorSpace& src,
        uint32_t src_bit_depth,
        const gfx::ColorSpace& dst,
        float sdr_max_luminance_nits);

    gfx::ColorSpace src;
    uint32_t src_bit_depth = 0;
    gfx::ColorSpace dst;
    float sdr_max_luminance_nits = 0.f;

    bool operator==(const Key& other) const;
    bool operator!=(const Key& other) const;
    bool operator<(const Key& other) const;
  };

  base::flat_map<Key, sk_sp<SkRuntimeEffect>> cache_;
};

}  // namespace gfx

#endif  // UI_GFX_COLOR_CONVERSION_SK_FILTER_CACHE_H_
