// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_TRANSFORM_H_
#define UI_GFX_COLOR_TRANSFORM_H_

#include <memory>
#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/effects/SkRuntimeEffect.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_space_export.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/hdr_metadata.h"

namespace gfx {

class COLOR_SPACE_EXPORT ColorTransform {
 public:
  struct Options {
    // Used in testing to verify that optimizations have no effect.
    bool disable_optimizations = false;

    // Used to adjust the transfer and range adjust matrices.
    uint32_t src_bit_depth = kDefaultBitDepth;
    uint32_t dst_bit_depth = kDefaultBitDepth;

    // If set to true, then map PQ and HLG imputs such that their maximum
    // luminance will be `dst_max_luminance_relative`.
    bool tone_map_pq_and_hlg_to_dst = false;

    // Used for tone mapping and for interpreting color spaces whose
    // definition depends on an SDR white point.
    // TODO(https://crbug.com/1286082): Use this value in the transform.
    float sdr_max_luminance_nits = ColorSpace::kDefaultSDRWhiteLevel;

    // Used for tone mapping PQ sources.
    absl::optional<gfx::HDRMetadata> src_hdr_metadata;

    // The maximum luminance value for the destination, as a multiple of
    // `sdr_max_luminance_nits` (so this is 1 for SDR displays).
    // TODO(https://crbug.com/1286076): Use this value for transforming
    // PQ and HLG content.
    float dst_max_luminance_relative = 1.f;
  };

  // TriStimulus is a color coordinate in any color space.
  // Channel order is XYZ, RGB or YUV.
  typedef Point3F TriStim;

  ColorTransform();

  ColorTransform(const ColorTransform&) = delete;
  ColorTransform& operator=(const ColorTransform&) = delete;

  virtual ~ColorTransform();
  virtual gfx::ColorSpace GetSrcColorSpace() const = 0;
  virtual gfx::ColorSpace GetDstColorSpace() const = 0;

  // Perform transformation of colors, |colors| is both input and output.
  virtual void Transform(TriStim* colors, size_t num) const = 0;

  // Return an SkRuntimeEffect to perform this transform.
  virtual sk_sp<SkRuntimeEffect> GetSkRuntimeEffect() const = 0;

  // Return the uniforms used by the above SkRuntimeEffect.
  static sk_sp<SkData> GetSkShaderUniforms(const ColorSpace& src,
                                           const ColorSpace& dst,
                                           float offset,
                                           float multiplier,
                                           const Options& options);

  // Returns true if this transform is the identity.
  virtual bool IsIdentity() const = 0;

  virtual size_t NumberOfStepsForTesting() const = 0;

  // Two special cases:
  // 1. If no source color space is specified (i.e., src.IsValid() is false), do
  // no transformation.
  // 2. If the target color space is not defined (i.e., dst.IsValid() is false),
  // just apply the range adjust and inverse transfer matrices. This can be used
  // for YUV to RGB color conversion.
  static std::unique_ptr<ColorTransform> NewColorTransform(
      const ColorSpace& src,
      const ColorSpace& dst);

  static std::unique_ptr<ColorTransform> NewColorTransform(
      const ColorSpace& src,
      const ColorSpace& dst,
      const Options& options);

 private:
  // The default bit depth assumed by NewColorTransform().
  static constexpr int kDefaultBitDepth = 8;
};

}  // namespace gfx

#endif  // UI_GFX_COLOR_TRANSFORM_H_
