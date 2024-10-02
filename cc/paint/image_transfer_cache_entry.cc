// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/image_transfer_cache_entry.h"

#include <algorithm>
#include <array>
#include <type_traits>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/GrYUVABackendTextures.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "ui/gfx/color_conversion_sk_filter_cache.h"
#include "ui/gfx/hdr_metadata.h"

namespace cc {
namespace {

struct Context {
  const std::vector<sk_sp<SkImage>> sk_planes_;
};

void ReleaseContext(SkImage::ReleaseContext context) {
  auto* texture_context = static_cast<Context*>(context);
  delete texture_context;
}

bool IsYUVAInfoValid(SkYUVAInfo::PlaneConfig plane_config,
                     SkYUVAInfo::Subsampling subsampling,
                     SkYUVColorSpace yuv_color_space) {
  if (plane_config == SkYUVAInfo::PlaneConfig::kUnknown) {
    return subsampling == SkYUVAInfo::Subsampling::kUnknown &&
           yuv_color_space == kIdentity_SkYUVColorSpace;
  }
  return subsampling != SkYUVAInfo::Subsampling::kUnknown;
}

int NumPixmapsForYUVConfig(SkYUVAInfo::PlaneConfig plane_config) {
  return std::max(SkYUVAInfo::NumPlanes(plane_config), 1);
}

// Creates a SkImage backed by the YUV textures corresponding to |plane_images|.
// The layout is specified by |plane_images_format|). The backend textures are
// first extracted out of the |plane_images| (and work is flushed on each one).
// Note that we assume that the image is opaque (no alpha plane). Then, a
// SkImage is created out of those textures using the
// SkImages::TextureFromYUVATextures() API. Finally, |image_color_space| is the
// color space of the resulting image after applying |yuv_color_space|
// (converting from YUV to RGB). This is assumed to be sRGB if nullptr.
//
// On success, the resulting SkImage is
// returned. On failure, nullptr is returned (e.g., if one of the backend
// textures is invalid or a Skia error occurs).
sk_sp<SkImage> MakeYUVImageFromUploadedPlanes(
    GrDirectContext* context,
    const std::vector<sk_sp<SkImage>>& plane_images,
    const SkYUVAInfo& yuva_info,
    sk_sp<SkColorSpace> image_color_space) {
  // 1) Extract the textures.
  DCHECK_NE(SkYUVAInfo::PlaneConfig::kUnknown, yuva_info.planeConfig());
  DCHECK_NE(SkYUVAInfo::Subsampling::kUnknown, yuva_info.subsampling());
  DCHECK_EQ(static_cast<size_t>(SkYUVAInfo::NumPlanes(yuva_info.planeConfig())),
            plane_images.size());
  DCHECK_LE(plane_images.size(),
            base::checked_cast<size_t>(SkYUVAInfo::kMaxPlanes));
  std::array<GrBackendTexture, SkYUVAInfo::kMaxPlanes> plane_backend_textures;
  for (size_t plane = 0u; plane < plane_images.size(); plane++) {
    if (!SkImages::GetBackendTextureFromImage(
            plane_images[plane], &plane_backend_textures[plane],
            true /* flushPendingGrContextIO */)) {
      DLOG(ERROR) << "Invalid backend texture found";
      return nullptr;
    }
  }

  // 2) Create the YUV image.
  GrYUVABackendTextures yuva_backend_textures(
      yuva_info, plane_backend_textures.data(), kTopLeft_GrSurfaceOrigin);
  Context* ctx = new Context{plane_images};
  sk_sp<SkImage> image = SkImages::TextureFromYUVATextures(
      context, yuva_backend_textures, std::move(image_color_space),
      ReleaseContext, ctx);
  if (!image) {
    DLOG(ERROR) << "Could not create YUV image";
    return nullptr;
  }

  return image;
}

base::CheckedNumeric<uint32_t> SafeSizeForPixmap(const SkPixmap& pixmap) {
  base::CheckedNumeric<uint32_t> safe_size;
  safe_size += PaintOpWriter::SerializedSize(pixmap.colorType());
  safe_size += PaintOpWriter::SerializedSize(pixmap.width());
  safe_size += PaintOpWriter::SerializedSize(pixmap.height());
  safe_size += PaintOpWriter::SerializedSize(pixmap.rowBytes());
  safe_size += 16u;  // The max of GetAlignmentForColorType().
  safe_size += PaintOpWriter::SerializedSizeOfBytes(pixmap.computeByteSize());
  return safe_size;
}

base::CheckedNumeric<uint32_t> SafeSizeForImage(
    const ClientImageTransferCacheEntry::Image& image) {
  base::CheckedNumeric<uint32_t> safe_size;
  safe_size += PaintOpWriter::SerializedSize(image.yuv_plane_config);
  safe_size += PaintOpWriter::SerializedSize(image.yuv_subsampling);
  safe_size += PaintOpWriter::SerializedSize(image.yuv_color_space);
  safe_size += PaintOpWriter::SerializedSize(image.color_space.get());
  const int num_pixmaps = NumPixmapsForYUVConfig(image.yuv_plane_config);
  for (int i = 0; i < num_pixmaps; ++i) {
    safe_size += SafeSizeForPixmap(*image.pixmaps.at(i));
  }
  return safe_size;
}

size_t GetAlignmentForColorType(SkColorType color_type) {
  size_t bpp = SkColorTypeBytesPerPixel(color_type);
  if (bpp <= 4)
    return 4;
  if (bpp <= 16)
    return 16;
  NOTREACHED();
  return 0;
}

bool WritePixmap(PaintOpWriter& writer, const SkPixmap& pixmap) {
  if (pixmap.width() == 0 || pixmap.height() == 0) {
    DLOG(ERROR) << "Cannot write empty pixmap";
    return false;
  }
  DCHECK_GT(pixmap.width(), 0);
  DCHECK_GT(pixmap.height(), 0);
  DCHECK_GT(pixmap.rowBytes(), 0u);
  writer.Write(pixmap.colorType());
  writer.Write(pixmap.width());
  writer.Write(pixmap.height());
  size_t data_size = pixmap.computeByteSize();
  if (data_size == SIZE_MAX) {
    DLOG(ERROR) << "Size overflow writing pixmap";
    return false;
  }
  writer.WriteSize(pixmap.rowBytes());
  writer.WriteSize(data_size);
  // The memory for the pixmap must be aligned to a byte boundary, or mipmap
  // generation can fail.
  // https://crbug.com/863659, https://crbug.com/1300188
  writer.AlignMemory(GetAlignmentForColorType(pixmap.colorType()));
  writer.WriteData(data_size, pixmap.addr());
  return true;
}

bool ReadPixmap(PaintOpReader& reader, SkPixmap& pixmap) {
  if (!reader.valid())
    return false;

  SkColorType color_type = kUnknown_SkColorType;
  reader.Read(&color_type);
  const size_t alignment = GetAlignmentForColorType(color_type);
  if (color_type == kUnknown_SkColorType ||
      color_type == kRGB_101010x_SkColorType ||
      color_type > kLastEnum_SkColorType) {
    DLOG(ERROR) << "Invalid color type";
    return false;
  }
  int width = 0;
  reader.Read(&width);
  int height = 0;
  reader.Read(&height);
  if (width == 0 || height == 0) {
    DLOG(ERROR) << "Empty width or height";
    return false;
  }

  auto image_info =
      SkImageInfo::Make(width, height, color_type, kPremul_SkAlphaType);
  size_t row_bytes = 0;
  reader.ReadSize(&row_bytes);
  if (row_bytes < image_info.minRowBytes()) {
    DLOG(ERROR) << "Row bytes " << row_bytes << " less than minimum "
                << image_info.minRowBytes();
    return false;
  }
  size_t data_size = 0;
  reader.ReadSize(&data_size);
  if (image_info.computeByteSize(row_bytes) > data_size) {
    DLOG(ERROR) << "Data size too small";
    return false;
  }

  reader.AlignMemory(alignment);
  const volatile void* data = reader.ExtractReadableMemory(data_size);
  if (!reader.valid()) {
    DLOG(ERROR) << "Failed to read pixels";
    return false;
  }
  if (reinterpret_cast<uintptr_t>(data) % alignment) {
    DLOG(ERROR) << "Pixel pointer not aligned";
    return false;
  }

  // Const-cast away the "volatile" on |pixel_data|. We specifically understand
  // that a malicious caller may change our pixels under us, and are OK with
  // this as the worst case scenario is visual corruption.
  pixmap = SkPixmap(image_info, const_cast<const void*>(data), row_bytes);
  return true;
}

bool WriteImage(PaintOpWriter& writer,
                const ClientImageTransferCacheEntry::Image& image) {
  DCHECK(IsYUVAInfoValid(image.yuv_plane_config, image.yuv_subsampling,
                         image.yuv_color_space));
  writer.Write(image.color_space);
  writer.Write(image.yuv_plane_config);
  writer.Write(image.yuv_subsampling);
  writer.Write(image.yuv_color_space);
  const int num_pixmaps = NumPixmapsForYUVConfig(image.yuv_plane_config);
  for (int i = 0; i < num_pixmaps; ++i) {
    if (!WritePixmap(writer, *image.pixmaps.at(i))) {
      return false;
    }
  }
  return true;
}

size_t SafeSizeForTargetColorParams(
    const absl::optional<TargetColorParams>& target_color_params) {
  // bool for whether or not there are going to be parameters.
  size_t target_color_params_size = PaintOpWriter::SerializedSize<bool>();
  if (target_color_params) {
    // The target color space.
    target_color_params_size += PaintOpWriter::SerializedSize(
        target_color_params->color_space.ToSkColorSpace().get());
    target_color_params_size += PaintOpWriter::SerializedSize(
        target_color_params->sdr_max_luminance_nits);
    target_color_params_size += PaintOpWriter::SerializedSize(
        target_color_params->hdr_max_luminance_relative);
    target_color_params_size +=
        PaintOpWriter::SerializedSize(target_color_params->enable_tone_mapping);
    // bool for whether or not there is HDR metadata.
    target_color_params_size += PaintOpWriter::SerializedSize<bool>();
    if (auto& hdr_metadata = target_color_params->hdr_metadata) {
      // The minimum and maximum luminance.
      target_color_params_size +=
          PaintOpWriter::SerializedSize(hdr_metadata->max_content_light_level);
      target_color_params_size += PaintOpWriter::SerializedSize(
          hdr_metadata->max_frame_average_light_level);
      // The x and y coordinates for primaries and white point.
      target_color_params_size += PaintOpWriter::SerializedSizeOfElements(
          &hdr_metadata->color_volume_metadata.primaries.fRX, 4 * 2);
      // The CLL and FALL
      target_color_params_size += PaintOpWriter::SerializedSize(
          hdr_metadata->color_volume_metadata.luminance_max);
      target_color_params_size += PaintOpWriter::SerializedSize(
          hdr_metadata->color_volume_metadata.luminance_min);
    }
  }
  return target_color_params_size;
}

void WriteTargetColorParams(
    PaintOpWriter& writer,
    const absl::optional<TargetColorParams>& target_color_params) {
  const bool has_target_color_params = !!target_color_params;
  writer.Write(has_target_color_params);
  if (target_color_params) {
    writer.Write(target_color_params->color_space.ToSkColorSpace().get());
    writer.Write(target_color_params->sdr_max_luminance_nits);
    writer.Write(target_color_params->hdr_max_luminance_relative);
    writer.Write(target_color_params->enable_tone_mapping);

    const bool has_hdr_metadata = !!target_color_params->hdr_metadata;
    writer.Write(has_hdr_metadata);
    if (target_color_params->hdr_metadata) {
      const auto& hdr_metadata = target_color_params->hdr_metadata;
      writer.Write(hdr_metadata->max_content_light_level);
      writer.Write(hdr_metadata->max_frame_average_light_level);

      const auto& color_volume = hdr_metadata->color_volume_metadata;
      writer.Write(color_volume.primaries.fRX);
      writer.Write(color_volume.primaries.fRY);
      writer.Write(color_volume.primaries.fGX);
      writer.Write(color_volume.primaries.fGY);
      writer.Write(color_volume.primaries.fBX);
      writer.Write(color_volume.primaries.fBY);
      writer.Write(color_volume.primaries.fWX);
      writer.Write(color_volume.primaries.fWY);
      writer.Write(color_volume.luminance_max);
      writer.Write(color_volume.luminance_min);
    }
  }
}

bool ReadTargetColorParams(
    PaintOpReader& reader,
    absl::optional<TargetColorParams>& target_color_params) {
  bool has_target_color_params = false;
  reader.Read(&has_target_color_params);
  if (!has_target_color_params) {
    target_color_params = absl::nullopt;
    return true;
  }

  target_color_params = TargetColorParams();
  sk_sp<SkColorSpace> target_color_space;
  reader.Read(&target_color_space);
  if (!target_color_space)
    return false;

  target_color_params->color_space = gfx::ColorSpace(*target_color_space);
  reader.Read(&target_color_params->sdr_max_luminance_nits);
  reader.Read(&target_color_params->hdr_max_luminance_relative);
  reader.Read(&target_color_params->enable_tone_mapping);

  bool has_hdr_metadata = false;
  reader.Read(&has_hdr_metadata);
  if (has_hdr_metadata) {
    gfx::HDRMetadata hdr_metadata;
    unsigned max_content_light_level = 0;
    unsigned max_frame_average_light_level = 0;
    reader.Read(&max_content_light_level);
    reader.Read(&max_frame_average_light_level);

    SkColorSpacePrimaries primaries = SkNamedPrimariesExt::kInvalid;
    float luminance_max = 0;
    float luminance_min = 0;
    reader.Read(&primaries.fRX);
    reader.Read(&primaries.fRY);
    reader.Read(&primaries.fGX);
    reader.Read(&primaries.fGY);
    reader.Read(&primaries.fBX);
    reader.Read(&primaries.fBY);
    reader.Read(&primaries.fWX);
    reader.Read(&primaries.fWY);
    reader.Read(&luminance_max);
    reader.Read(&luminance_min);

    target_color_params->hdr_metadata = gfx::HDRMetadata(
        gfx::ColorVolumeMetadata(primaries, luminance_max, luminance_min),
        max_content_light_level, max_frame_average_light_level);
  }
  return true;
}

sk_sp<SkImage> ReadImage(
    PaintOpReader& reader,
    GrDirectContext* context,
    GrMipMapped mip_mapped_for_upload,
    absl::optional<SkYUVAInfo>* out_yuva_info = nullptr,
    std::vector<sk_sp<SkImage>>* out_yuva_plane_images = nullptr) {
  // Allow a nullptr context for testing using the software renderer.
  const int32_t max_size = context ? context->maxTextureSize() : 0;

  sk_sp<SkColorSpace> color_space;
  reader.Read(&color_space);

  SkYUVAInfo::PlaneConfig plane_config = SkYUVAInfo::PlaneConfig::kUnknown;
  reader.Read(&plane_config);
  if (plane_config < SkYUVAInfo::PlaneConfig::kUnknown ||
      plane_config > SkYUVAInfo::PlaneConfig::kLast) {
    DLOG(ERROR) << "Invalid plane config";
    return nullptr;
  }

  SkYUVAInfo::Subsampling subsampling = SkYUVAInfo::Subsampling::kUnknown;
  reader.Read(&subsampling);
  if (subsampling < SkYUVAInfo::Subsampling::kUnknown ||
      subsampling > SkYUVAInfo::Subsampling::kLast) {
    DLOG(ERROR) << "Invalid subsampling";
    return nullptr;
  }

  SkYUVColorSpace yuv_color_space = kIdentity_SkYUVColorSpace;
  reader.Read(&yuv_color_space);
  if (yuv_color_space < kJPEG_Full_SkYUVColorSpace ||
      yuv_color_space > kLastEnum_SkYUVColorSpace) {
    DLOG(ERROR) << "Invalid YUV color space";
    return nullptr;
  }

  if (!IsYUVAInfoValid(plane_config, subsampling, yuv_color_space)) {
    DLOG(ERROR) << "Invalid YUV configuration";
    return nullptr;
  }

  SkPixmap pixmaps[SkYUVAInfo::kMaxPlanes];
  bool fits_on_gpu = true;
  const int num_pixmaps = NumPixmapsForYUVConfig(plane_config);
  for (int i = 0; i < num_pixmaps; ++i) {
    if (!ReadPixmap(reader, pixmaps[i])) {
      DLOG(ERROR) << "Failed to read pixmap";
      return nullptr;
    }
    fits_on_gpu &=
        pixmaps[i].width() <= max_size && pixmaps[i].height() <= max_size;

    // This is likely unnecessary for YUVA images (the pixmaps of the individual
    // planes should be ignored), but is left here to avoid behavior changes
    // during refactoring.
    pixmaps[i].setColorSpace(color_space);
  }

  if (plane_config == SkYUVAInfo::PlaneConfig::kUnknown) {
    // Point `image_` directly to the data in the transfer cache.
    auto image = SkImages::RasterFromPixmap(pixmaps[0], nullptr, nullptr);
    if (!image) {
      DLOG(ERROR) << "Failed to create image from pixmap";
      return nullptr;
    }

    // Upload to the GPU if the image will fit.
    if (fits_on_gpu) {
      image = SkImages::TextureFromImage(context, image, mip_mapped_for_upload,
                                         skgpu::Budgeted::kNo);
      if (!image) {
        DLOG(ERROR) << "Failed to upload pixmap to texture image.";
        return nullptr;
      }
      DCHECK(image->isTextureBacked());
    }

    if (out_yuva_info) {
      *out_yuva_info = absl::nullopt;
    }
    if (out_yuva_plane_images) {
      out_yuva_plane_images->clear();
    }
    return image;
  } else {
    if (!fits_on_gpu) {
      DLOG(ERROR) << "YUVA images must fit in the GPU texture size limit";
      return nullptr;
    }

    // Upload the planes to the GPU.
    std::vector<sk_sp<SkImage>> plane_images;
    for (int i = 0; i < num_pixmaps; i++) {
      sk_sp<SkImage> plane =
          SkImages::RasterFromPixmap(pixmaps[i], nullptr, nullptr);
      if (!plane) {
        DLOG(ERROR) << "Failed to create image from plane pixmap";
        return nullptr;
      }
      plane = SkImages::TextureFromImage(context, plane, mip_mapped_for_upload,
                                         skgpu::Budgeted::kNo);
      if (!plane) {
        DLOG(ERROR) << "Failed to upload plane pixmap to texture image";
        return nullptr;
      }
      DCHECK(plane->isTextureBacked());
      SkImages::GetBackendTextureFromImage(plane, nullptr,
                                           /*flushPendingGrContextIO=*/true);
      plane_images.push_back(std::move(plane));
    }
    SkYUVAInfo yuva_info(plane_images[0]->dimensions(), plane_config,
                         subsampling, yuv_color_space);

    // Build the YUV image from its planes.
    auto image = MakeYUVImageFromUploadedPlanes(context, plane_images,
                                                yuva_info, color_space);
    if (!image) {
      DLOG(ERROR) << "Failed to make YUV image from planes.";
      return nullptr;
    }

    if (out_yuva_info) {
      *out_yuva_info = yuva_info;
    }
    if (out_yuva_plane_images) {
      *out_yuva_plane_images = std::move(plane_images);
    }
    return image;
  }
}

}  // namespace

size_t NumberOfPlanesForYUVDecodeFormat(YUVDecodeFormat format) {
  switch (format) {
    case YUVDecodeFormat::kYUVA4:
      return 4u;
    case YUVDecodeFormat::kYUV3:
    case YUVDecodeFormat::kYVU3:
      return 3u;
    case YUVDecodeFormat::kYUV2:
      return 2u;
    case YUVDecodeFormat::kUnknown:
      return 0u;
  }
}

////////////////////////////////////////////////////////////////////////////////
// ClientImageTransferCacheEntry::Image

ClientImageTransferCacheEntry::Image::Image() {}
ClientImageTransferCacheEntry::Image::Image(const Image&) = default;
ClientImageTransferCacheEntry::Image&
ClientImageTransferCacheEntry::Image::operator=(const Image&) = default;

ClientImageTransferCacheEntry::Image::Image(const SkPixmap* pixmap)
    : color_space(pixmap->colorSpace()) {
  DCHECK(pixmap);
  pixmaps[0] = pixmap;
}

ClientImageTransferCacheEntry::Image::Image(const SkPixmap yuva_pixmaps[],
                                            const SkYUVAInfo& yuva_info,
                                            const SkColorSpace* color_space)
    : yuv_plane_config(yuva_info.planeConfig()),
      yuv_subsampling(yuva_info.subsampling()),
      yuv_color_space(yuva_info.yuvColorSpace()),
      color_space(color_space) {
  // The size of the first plane must equal the size specified in the
  // SkYUVAInfo.
  DCHECK(yuva_info.dimensions() == yuva_pixmaps[0].dimensions());
  // We fail to serialize some parameters.
  DCHECK_EQ(yuva_info.origin(), kTopLeft_SkEncodedOrigin);
  DCHECK_EQ(yuva_info.sitingX(), SkYUVAInfo::Siting::kCentered);
  DCHECK_EQ(yuva_info.sitingY(), SkYUVAInfo::Siting::kCentered);
  DCHECK(IsYUVAInfoValid(yuv_plane_config, yuv_subsampling, yuv_color_space));
  for (int i = 0; i < SkYUVAInfo::NumPlanes(yuv_plane_config); ++i) {
    pixmaps[i] = &yuva_pixmaps[i];
  }
}

////////////////////////////////////////////////////////////////////////////////
// ClientImageTransferCacheEntry

ClientImageTransferCacheEntry::ClientImageTransferCacheEntry(
    const Image& image,
    bool needs_mips,
    absl::optional<TargetColorParams> target_color_params)
    : needs_mips_(needs_mips),
      target_color_params_(target_color_params),
      id_(GetNextId()),
      image_(image) {
  ComputeSize();
}

ClientImageTransferCacheEntry::ClientImageTransferCacheEntry(
    const Image& image,
    const Image& gainmap_image,
    const SkGainmapInfo& gainmap_info,
    bool needs_mips,
    absl::optional<TargetColorParams> target_color_params)
    : needs_mips_(needs_mips),
      target_color_params_(target_color_params),
      id_(GetNextId()),
      image_(image),
      gainmap_image_(gainmap_image),
      gainmap_info_(gainmap_info) {
  ComputeSize();
}

ClientImageTransferCacheEntry::~ClientImageTransferCacheEntry() = default;

// static
base::AtomicSequenceNumber ClientImageTransferCacheEntry::s_next_id_;

uint32_t ClientImageTransferCacheEntry::SerializedSize() const {
  return size_;
}

uint32_t ClientImageTransferCacheEntry::Id() const {
  return id_;
}

bool ClientImageTransferCacheEntry::Serialize(base::span<uint8_t> data) const {
  DCHECK_GE(data.size(), SerializedSize());
  // We don't need to populate the SerializeOptions here since the writer is
  // only used for serializing primitives.
  PaintOp::SerializeOptions options;
  PaintOpWriter writer(data.data(), data.size(), options);

  DCHECK_EQ(gainmap_image_.has_value(), gainmap_info_.has_value());
  bool has_gainmap = gainmap_image_.has_value();
  writer.Write(has_gainmap);
  writer.Write(needs_mips_);
  WriteTargetColorParams(writer, target_color_params_);
  WriteImage(writer, image_);

  if (has_gainmap) {
    WriteImage(writer, gainmap_image_.value());
    writer.Write(gainmap_info_.value());
  }

  // Size can't be 0 after serialization unless the writer has become invalid.
  if (writer.size() == 0u)
    return false;
  return true;
}

void ClientImageTransferCacheEntry::ComputeSize() {
  base::CheckedNumeric<uint32_t> safe_size;
  safe_size += PaintOpWriter::SerializedSize<bool>();  // has_gainmap
  safe_size += PaintOpWriter::SerializedSize<bool>();  // needs_mips
  safe_size += SafeSizeForTargetColorParams(target_color_params_);
  safe_size += SafeSizeForImage(image_);
  if (gainmap_image_) {
    DCHECK(gainmap_info_);
    safe_size += SafeSizeForImage(gainmap_image_.value());
    safe_size += PaintOpWriter::SerializedSize<SkGainmapInfo>();
  }
  size_ = safe_size.ValueOrDefault(0);
}

////////////////////////////////////////////////////////////////////////////////
// ServiceImageTransferCacheEntry

ServiceImageTransferCacheEntry::ServiceImageTransferCacheEntry() = default;
ServiceImageTransferCacheEntry::~ServiceImageTransferCacheEntry() = default;

ServiceImageTransferCacheEntry::ServiceImageTransferCacheEntry(
    ServiceImageTransferCacheEntry&& other) = default;
ServiceImageTransferCacheEntry& ServiceImageTransferCacheEntry::operator=(
    ServiceImageTransferCacheEntry&& other) = default;

bool ServiceImageTransferCacheEntry::BuildFromHardwareDecodedImage(
    GrDirectContext* context,
    std::vector<sk_sp<SkImage>> plane_images,
    SkYUVAInfo::PlaneConfig plane_config,
    SkYUVAInfo::Subsampling subsampling,
    SkYUVColorSpace yuv_color_space,
    size_t buffer_byte_size,
    bool needs_mips) {
  context_ = context;
  size_ = buffer_byte_size;

  // 1) Generate mipmap chains if requested.
  if (needs_mips) {
    DCHECK(plane_sizes_.empty());
    base::CheckedNumeric<size_t> safe_total_size(0u);
    for (size_t plane = 0; plane < plane_images.size(); plane++) {
      plane_images[plane] =
          SkImages::TextureFromImage(context_, plane_images[plane],
                                     GrMipMapped::kYes, skgpu::Budgeted::kNo);
      if (!plane_images[plane]) {
        DLOG(ERROR) << "Could not generate mipmap chain for plane " << plane;
        return false;
      }
      plane_sizes_.push_back(plane_images[plane]->textureSize());
      safe_total_size += plane_sizes_.back();
    }
    if (!safe_total_size.AssignIfValid(&size_)) {
      DLOG(ERROR) << "Could not calculate the total image size";
      return false;
    }
  }
  plane_images_ = std::move(plane_images);
  if (static_cast<size_t>(SkYUVAInfo::NumPlanes(plane_config)) !=
      plane_images_.size()) {
    DLOG(ERROR) << "Expected " << SkYUVAInfo::NumPlanes(plane_config)
                << " planes, got " << plane_images_.size();
    return false;
  }
  yuva_info_ = SkYUVAInfo(plane_images_[0]->dimensions(), plane_config,
                          subsampling, yuv_color_space);

  // 2) Create a SkImage backed by |plane_images|.
  // TODO(andrescj): support embedded color profiles for hardware decodes and
  // pass the color space to MakeYUVImageFromUploadedPlanes.
  image_ = MakeYUVImageFromUploadedPlanes(
      context_, plane_images_, yuva_info_.value(), SkColorSpace::MakeSRGB());
  if (!image_)
    return false;
  DCHECK(image_->isTextureBacked());
  return true;
}

size_t ServiceImageTransferCacheEntry::CachedSize() const {
  return size_;
}

bool ServiceImageTransferCacheEntry::Deserialize(
    GrDirectContext* context,
    base::span<const uint8_t> data) {
  context_ = context;

  // We don't need to populate the DeSerializeOptions here since the reader is
  // only used for de-serializing primitives.
  std::vector<uint8_t> scratch_buffer;
  PaintOp::DeserializeOptions options(nullptr, nullptr, nullptr,
                                      &scratch_buffer, false, nullptr);
  PaintOpReader reader(data.data(), data.size(), options);

  // Parameters common to RGBA and YUVA images.
  bool has_gainmap = false;
  reader.Read(&has_gainmap);
  bool needs_mips = false;
  reader.Read(&needs_mips);
  absl::optional<TargetColorParams> target_color_params;
  ReadTargetColorParams(reader, target_color_params);
  const GrMipMapped mip_mapped_for_upload =
      needs_mips && !target_color_params ? GrMipMapped::kYes : GrMipMapped::kNo;

  // Deserialize the image.
  image_ = ReadImage(reader, context, mip_mapped_for_upload, &yuva_info_,
                     &plane_images_);
  if (!image_) {
    DLOG(ERROR) << "Failed to deserialize image.";
    return false;
  }

  // If the image doesn't fit in the GPU texture limits, then it is still on
  // the CPU, pointing at memory in the transfer buffer.
  sk_sp<SkImage> image_referencing_transfer_buffer;
  if (!image_->isTextureBacked()) {
    image_referencing_transfer_buffer = image_;
  }
  for (const auto& plane_image : plane_images_) {
    plane_sizes_.push_back(plane_image->textureSize());
  }

  // Read the gainmap image, if one was specified to exist.
  sk_sp<SkImage> gainmap_image;
  SkGainmapInfo gainmap_info;
  if (has_gainmap) {
    if (!target_color_params) {
      DLOG(ERROR) << "Gainmap images need target parameters to render.";
      return false;
    }
    gainmap_image = ReadImage(reader, context, mip_mapped_for_upload);
    if (!gainmap_image) {
      DLOG(ERROR) << "Failed to deserialize gainmap image.";
      return false;
    }
    reader.Read(&gainmap_info);
  }

  // Perform color conversion and tone mapping.
  if (target_color_params) {
    auto target_color_space = target_color_params->color_space.ToSkColorSpace();
    if (!target_color_space) {
      DLOG(ERROR) << "Invalid target color space.";
      return false;
    }

    // TODO(https://crbug.com/1286088): Pass a shared cache as a parameter.
    gfx::ColorConversionSkFilterCache cache;
    if (has_gainmap) {
      image_ =
          cache.ApplyGainmap(image_, gainmap_image, gainmap_info,
                             target_color_params->hdr_max_luminance_relative,
                             image_->isTextureBacked() ? context_ : nullptr);
    } else {
      image_ = cache.ConvertImage(
          image_, target_color_space, target_color_params->hdr_metadata,
          target_color_params->sdr_max_luminance_nits,
          target_color_params->hdr_max_luminance_relative,
          target_color_params->enable_tone_mapping,
          image_->isTextureBacked() ? context_ : nullptr);
    }
    if (!image_) {
      DLOG(ERROR) << "Failed image color conversion";
      return false;
    }

    // Color conversion converts to RGBA. Remove all YUV state.
    yuva_info_ = absl::nullopt;
    plane_images_.clear();
    plane_sizes_.clear();

    // If mipmaps were requested, create them after color conversion.
    if (needs_mips && image_->isTextureBacked()) {
      image_ = SkImages::TextureFromImage(context, image_, GrMipMapped::kYes,
                                          skgpu::Budgeted::kNo);
      if (!image_) {
        DLOG(ERROR) << "Failed to generate mipmaps after color conversion";
        return false;
      }
    }
  }

  // If `image_` is still directly referencing the transfer buffer's memory,
  // make a copy of it (because the memory will go away after this this call).
  if (image_ == image_referencing_transfer_buffer) {
    SkPixmap pixmap;
    if (!image_->peekPixels(&pixmap)) {
      NOTREACHED() << "Image should be referencing transfer buffer SkPixmap";
    }
    image_ = SkImages::RasterFromPixmapCopy(pixmap);
    if (!image_) {
      DLOG(ERROR) << "Failed to create raster copy";
      return false;
    }
  }

  size_ = image_->textureSize();
  return true;
}

const sk_sp<SkImage>& ServiceImageTransferCacheEntry::GetPlaneImage(
    size_t index) const {
  DCHECK_GE(index, 0u);
  DCHECK_LT(index, plane_images_.size());
  DCHECK(plane_images_.at(index));
  return plane_images_.at(index);
}

void ServiceImageTransferCacheEntry::EnsureMips() {
  if (!image_ || !image_->isTextureBacked()) {
    return;
  }
  if (image_->hasMipmaps()) {
    return;
  }

  if (is_yuv()) {
    DCHECK(image_);
    DCHECK(yuva_info_.has_value());
    DCHECK_NE(SkYUVAInfo::PlaneConfig::kUnknown, yuva_info_->planeConfig());
    DCHECK_EQ(
        static_cast<size_t>(SkYUVAInfo::NumPlanes(yuva_info_->planeConfig())),
        plane_images_.size());

    // We first do all the work with local variables. Then, if everything
    // succeeds, we update the object's state. That way, we don't leave it in an
    // inconsistent state if one step of mip generation fails.
    std::vector<sk_sp<SkImage>> mipped_planes;
    std::vector<size_t> mipped_plane_sizes;
    for (size_t plane = 0; plane < plane_images_.size(); plane++) {
      DCHECK(plane_images_.at(plane));
      sk_sp<SkImage> mipped_plane =
          SkImages::TextureFromImage(context_, plane_images_.at(plane),
                                     GrMipMapped::kYes, skgpu::Budgeted::kNo);
      if (!mipped_plane)
        return;
      mipped_planes.push_back(std::move(mipped_plane));
      mipped_plane_sizes.push_back(mipped_planes.back()->textureSize());
    }
    sk_sp<SkImage> mipped_image = MakeYUVImageFromUploadedPlanes(
        context_, mipped_planes, yuva_info_.value(),
        image_->refColorSpace() /* image_color_space */);
    if (!mipped_image) {
      DLOG(ERROR) << "Failed to create YUV image from mipmapped planes";
      return;
    }
    // Note that we cannot update |size_| because the transfer cache keeps track
    // of a total size that is not updated after EnsureMips(). The original size
    // is used when the image is deleted from the cache.
    plane_images_ = std::move(mipped_planes);
    plane_sizes_ = std::move(mipped_plane_sizes);
    image_ = std::move(mipped_image);
  } else {
    sk_sp<SkImage> mipped_image = SkImages::TextureFromImage(
        context_, image_, GrMipMapped::kYes, skgpu::Budgeted::kNo);
    if (!mipped_image) {
      DLOG(ERROR) << "Failed to mipmapped image";
      return;
    }
    image_ = std::move(mipped_image);
  }
}

bool ServiceImageTransferCacheEntry::has_mips() const {
  return (image_ && image_->isTextureBacked()) ? image_->hasMipmaps() : true;
}

bool ServiceImageTransferCacheEntry::fits_on_gpu() const {
  return image_ && image_->isTextureBacked();
}

}  // namespace cc
