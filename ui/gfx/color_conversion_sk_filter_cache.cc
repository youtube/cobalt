// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_conversion_sk_filter_cache.h"

#include <string>

#include "base/feature_list.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/private/SkGainmapInfo.h"
#include "third_party/skia/include/private/SkGainmapShader.h"
#include "ui/gfx/color_transform.h"

namespace gfx {

namespace {

// Allocate an SkSurface to be used to create the tonemapped result.
static sk_sp<SkSurface> MakeSurfaceForResult(SkImageInfo image_info,
                                             GrDirectContext* context) {
  sk_sp<SkSurface> surface;
  if (context) {
    // TODO(https://crbug.com/1286088): Consider adding mipmap support here.
    surface =
        SkSurface::MakeRenderTarget(context, skgpu::Budgeted::kNo, image_info,
                                    /*sampleCount=*/0, kTopLeft_GrSurfaceOrigin,
                                    /*surfaceProps=*/nullptr,
                                    /*shouldCreateWithMips=*/false);
    // It is not guaranteed that kRGBA_F16_SkColorType is renderable. If we fail
    // to create an SkSurface with that color type, fall back to
    // kN32_SkColorType.
    if (!surface) {
      DLOG(ERROR) << "Falling back to tone mapped 8-bit surface.";
      image_info = image_info.makeColorType(kN32_SkColorType);
      surface = SkSurface::MakeRenderTarget(
          context, skgpu::Budgeted::kNo, image_info,
          /*sampleCount=*/0, kTopLeft_GrSurfaceOrigin,
          /*surfaceProps=*/nullptr,
          /*shouldCreateWithMips=*/false);
    }
  } else {
    surface = SkSurface::MakeRaster(image_info, image_info.minRowBytes(),
                                    /*surfaceProps=*/nullptr);
  }
  return surface;
}

}  // namespace

ColorConversionSkFilterCache::ColorConversionSkFilterCache() = default;
ColorConversionSkFilterCache::~ColorConversionSkFilterCache() = default;

bool ColorConversionSkFilterCache::Key::Key::operator==(
    const Key& other) const {
  return src == other.src && src_bit_depth == other.src_bit_depth &&
         dst == other.dst &&
         sdr_max_luminance_nits == other.sdr_max_luminance_nits;
}

bool ColorConversionSkFilterCache::Key::operator!=(const Key& other) const {
  return !(*this == other);
}

bool ColorConversionSkFilterCache::Key::operator<(const Key& other) const {
  return std::tie(src, src_bit_depth, dst, sdr_max_luminance_nits) <
         std::tie(other.src, other.src_bit_depth, other.dst,
                  other.sdr_max_luminance_nits);
}

ColorConversionSkFilterCache::Key::Key(const gfx::ColorSpace& src,
                                       uint32_t src_bit_depth,
                                       const gfx::ColorSpace& dst,
                                       float sdr_max_luminance_nits)
    : src(src),
      src_bit_depth(src_bit_depth),
      dst(dst),
      sdr_max_luminance_nits(sdr_max_luminance_nits) {}

sk_sp<SkColorFilter> ColorConversionSkFilterCache::Get(
    const gfx::ColorSpace& src,
    const gfx::ColorSpace& dst,
    float resource_offset,
    float resource_multiplier,
    absl::optional<uint32_t> src_bit_depth,
    absl::optional<gfx::HDRMetadata> src_hdr_metadata,
    float sdr_max_luminance_nits,
    float dst_max_luminance_relative) {
  // Set unused parameters to bogus values, so that they do not result in
  // different keys for the same conversion.
  if (!src.IsToneMappedByDefault()) {
    // If the source is not going to be tone mapped, then `src_hdr_metadata`
    // and `dst_max_luminance_relative` will not be used, so set them nonsense
    // values.
    src_hdr_metadata = absl::nullopt;
    dst_max_luminance_relative = 0;

    // If neither source nor destination will use `sdr_max_luminance_nits`, then
    // set it to a nonsense value.
    if (!dst.IsAffectedBySDRWhiteLevel() && !src.IsAffectedBySDRWhiteLevel()) {
      sdr_max_luminance_nits = 0;
    }
  }

  const Key key(src, src_bit_depth.value_or(0), dst, sdr_max_luminance_nits);
  sk_sp<SkRuntimeEffect>& effect = cache_[key];

  gfx::ColorTransform::Options options;
  options.tone_map_pq_and_hlg_to_dst = true;
  if (src_bit_depth)
    options.src_bit_depth = src_bit_depth.value();
  options.sdr_max_luminance_nits = sdr_max_luminance_nits;
  options.src_hdr_metadata = src_hdr_metadata;
  options.dst_max_luminance_relative = dst_max_luminance_relative;
  if (!effect) {
    std::unique_ptr<gfx::ColorTransform> transform =
        gfx::ColorTransform::NewColorTransform(src, dst, options);
    effect = transform->GetSkRuntimeEffect();
  }

  return effect->makeColorFilter(gfx::ColorTransform::GetSkShaderUniforms(
      src, dst, resource_offset, resource_multiplier, options));
}

sk_sp<SkImage> ColorConversionSkFilterCache::ApplyGainmap(
    sk_sp<SkImage> base_image,
    sk_sp<SkImage> gainmap_image,
    const SkGainmapInfo& gainmap_info,
    float dst_max_luminance_relative,
    GrDirectContext* context) {
  DCHECK_EQ(base_image->isTextureBacked(), gainmap_image->isTextureBacked());
  DCHECK_EQ(!!context, base_image->isTextureBacked());

  // If `gainmap_image` will not be applied, then return `base_image` directly.
  switch (gainmap_info.fBaseImageType) {
    case SkGainmapInfo::BaseImageType::kSDR:
      if (dst_max_luminance_relative <= gainmap_info.fDisplayRatioSdr) {
        return base_image;
      }
      break;
    case SkGainmapInfo::BaseImageType::kHDR:
      if (dst_max_luminance_relative >= gainmap_info.fDisplayRatioHdr) {
        return base_image;
      }
      break;
  }

  // The output surface will be in a linearized version of the input
  // base_image's color space.
  sk_sp<SkColorSpace> surface_color_space = base_image->refColorSpace();
  if (surface_color_space) {
    surface_color_space = surface_color_space->makeLinearGamma();
  } else {
    surface_color_space = SkColorSpace::MakeSRGBLinear();
  }
  SkImageInfo surface_info =
      SkImageInfo::Make(base_image->dimensions(),
                        SkColorInfo(kRGBA_F16_SkColorType, kPremul_SkAlphaType,
                                    surface_color_space));

  // Create the surface to render the gainmap shader to.
  sk_sp<SkSurface> surface = MakeSurfaceForResult(surface_info, context);
  if (!surface) {
    LOG(ERROR) << "Failed to create SkSurface for applying gainmap.";
    return base_image;
  }

  // Render the gainmap shader to the surface
  SkRect image_rect = SkRect::MakeSize(SkSize::Make(base_image->dimensions()));
  SkRect gainmap_rect =
      SkRect::MakeSize(SkSize::Make(gainmap_image->dimensions()));
  SkRect surface_rect =
      SkRect::MakeSize(SkSize::Make(surface_info.dimensions()));
  sk_sp<SkShader> shader = SkGainmapShader::Make(
      base_image, image_rect, SkSamplingOptions(), gainmap_image, gainmap_rect,
      SkSamplingOptions(), gainmap_info, surface_rect,
      dst_max_luminance_relative, surface_color_space);
  DCHECK(shader);
  SkPaint paint;
  paint.setShader(shader);
  surface->getCanvas()->drawRect(surface_rect, paint);

  // Return the surface's contents as an SkImage.
  return surface->makeImageSnapshot();
}

sk_sp<SkImage> ColorConversionSkFilterCache::ConvertImage(
    sk_sp<SkImage> image,
    sk_sp<SkColorSpace> target_color_space,
    absl::optional<gfx::HDRMetadata> src_hdr_metadata,
    float sdr_max_luminance_nits,
    float dst_max_luminance_relative,
    bool enable_tone_mapping,
    GrDirectContext* context) {
  DCHECK(image);
  DCHECK(target_color_space);
  sk_sp<SkColorSpace> image_sk_color_space = image->refColorSpace();
  if (!image_sk_color_space)
    return image->makeColorSpace(target_color_space, context);

  if (!enable_tone_mapping)
    return image->makeColorSpace(target_color_space, context);

  gfx::ColorSpace image_color_space(*image_sk_color_space);
  switch (image_color_space.GetTransferID()) {
    case ColorSpace::TransferID::PQ:
    case ColorSpace::TransferID::HLG:
      break;
    default:
      return image->makeColorSpace(target_color_space, context);
  }

  SkImageInfo image_info =
      SkImageInfo::Make(image->dimensions(),
                        SkColorInfo(kRGBA_F16_SkColorType, kPremul_SkAlphaType,
                                    image_sk_color_space));
  sk_sp<SkSurface> surface = MakeSurfaceForResult(image_info, context);
  if (!surface) {
    DLOG(ERROR) << "Failed to create SkSurface color conversion.";
    return nullptr;
  }

  sk_sp<SkColorFilter> filter =
      Get(image_color_space, gfx::ColorSpace(*target_color_space),
          /*resource_offset=*/0, /*resource_multiplier=*/1,
          /*src_bit_depth=*/absl::nullopt, src_hdr_metadata,
          sdr_max_luminance_nits, dst_max_luminance_relative);
  SkPaint paint;
  paint.setBlendMode(SkBlendMode::kSrc);
  paint.setColorFilter(filter);
  SkSamplingOptions sampling_options(SkFilterMode::kNearest);
  surface->getCanvas()->drawImage(image,
                                  /*x=*/0, /*y=*/0, sampling_options, &paint);
  return surface->makeImageSnapshot()->reinterpretColorSpace(
      target_color_space);
}

}  // namespace gfx
