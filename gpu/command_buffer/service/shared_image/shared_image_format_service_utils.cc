// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "build/buildflag.h"
#include "components/viz/common/resources/shared_image_format_utils.h"

namespace gpu {

namespace {

#if BUILDFLAG(ENABLE_VULKAN)
VkFormat ToVkFormatSinglePlanarInternal(viz::SharedImageFormat format) {
  CHECK(format.is_single_plane());
  if (format == viz::SinglePlaneFormat::kRGBA_8888) {
    return VK_FORMAT_R8G8B8A8_UNORM;  // or VK_FORMAT_R8G8B8A8_SRGB
  } else if (format == viz::SinglePlaneFormat::kRGBA_4444) {
    return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
  } else if (format == viz::SinglePlaneFormat::kBGRA_8888) {
    return VK_FORMAT_B8G8R8A8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kR_8) {
    return VK_FORMAT_R8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kRGB_565) {
    return VK_FORMAT_R5G6B5_UNORM_PACK16;
  } else if (format == viz::SinglePlaneFormat::kBGR_565) {
    return VK_FORMAT_B5G6R5_UNORM_PACK16;
  } else if (format == viz::SinglePlaneFormat::kRG_88) {
    return VK_FORMAT_R8G8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kRGBA_F16) {
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  } else if (format == viz::SinglePlaneFormat::kR_16) {
    return VK_FORMAT_R16_UNORM;
  } else if (format == viz::SinglePlaneFormat::kRG_1616) {
    return VK_FORMAT_R16G16_UNORM;
  } else if (format == viz::SinglePlaneFormat::kRGBX_8888) {
    return VK_FORMAT_R8G8B8A8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kBGRX_8888) {
    return VK_FORMAT_B8G8R8A8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kRGBA_1010102) {
    return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
  } else if (format == viz::SinglePlaneFormat::kBGRA_1010102) {
    return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
  } else if (format == viz::SinglePlaneFormat::kALPHA_8) {
    return VK_FORMAT_R8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kLUMINANCE_8) {
    return VK_FORMAT_R8_UNORM;
  } else if (format == viz::LegacyMultiPlaneFormat::kYV12) {
    return VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;
  } else if (format == viz::LegacyMultiPlaneFormat::kNV12) {
    return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
  } else if (format == viz::SinglePlaneFormat::kETC1) {
    return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
  } else if (format == viz::SinglePlaneFormat::kLUMINANCE_F16) {
    return VK_FORMAT_R16_SFLOAT;
  } else if (format == viz::LegacyMultiPlaneFormat::kP010) {
    return VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
  }
  return VK_FORMAT_UNDEFINED;
}
#endif

}  // namespace

// Wraps functions from shared_image_format_utils.h that are made private with
// friending to prevent their existing client-side usage (which is an
// anti-pattern) from growing within a class that
// SharedImageFormatRestrictedUtils can friend. (Note that if
// SharedImageFormatRestrictedUtils instead directly friended the
// service-side calling functions, any client-side code could then also
// directly call those service-side calling functions as well, defeating the
// purpose).
class SharedImageFormatRestrictedUtilsAccessor {
 public:
  // Returns GL data format for given `format`.
  static GLenum GLDataFormat(viz::SharedImageFormat format, int plane_index) {
    DCHECK(format.IsValidPlaneIndex(plane_index));
    if (format.is_single_plane()) {
      return viz::SharedImageFormatRestrictedSinglePlaneUtils::ToGLDataFormat(
          format);
    }

    // For multiplanar formats without external sampler, GL formats are per
    // plane. For single channel planes Y, U, V, A return GL_RED_EXT. For 2
    // channel plane UV return GL_RG_EXT.
    int num_channels = format.NumChannelsInPlane(plane_index);
    DCHECK_LE(num_channels, 2);
    return num_channels == 2 ? GL_RG_EXT : GL_RED_EXT;
  }

  // Returns GL data type for given `format`.
  static GLenum GLDataType(viz::SharedImageFormat format) {
    if (format.is_single_plane()) {
      return viz::SharedImageFormatRestrictedSinglePlaneUtils::ToGLDataType(
          format);
    }

    switch (format.channel_format()) {
      case viz::SharedImageFormat::ChannelFormat::k8:
        return GL_UNSIGNED_BYTE;
      case viz::SharedImageFormat::ChannelFormat::k10:
        return GL_UNSIGNED_SHORT;
      case viz::SharedImageFormat::ChannelFormat::k16:
        return GL_UNSIGNED_SHORT;
      case viz::SharedImageFormat::ChannelFormat::k16F:
        return GL_HALF_FLOAT_OES;
    }
  }

  // Returns the GL format used internally for matching with the texture format
  // for a given `format`.
  static GLenum GLInternalFormat(viz::SharedImageFormat format,
                                 int plane_index) {
    DCHECK(format.IsValidPlaneIndex(plane_index));
    if (format.is_single_plane()) {
      // In GLES2, the internal format must match the texture format. (It no
      // longer is true in GLES3, however it still holds for the BGRA
      // extension.) GL_EXT_texture_norm16 follows GLES3 semantics and only
      // exposes a sized internal format (GL_R16_EXT).
      if (format == viz::SinglePlaneFormat::kR_16) {
        return GL_R16_EXT;
      } else if (format == viz::SinglePlaneFormat::kRG_1616) {
        return GL_RG16_EXT;
      } else if (format == viz::SinglePlaneFormat::kETC1) {
        return GL_ETC1_RGB8_OES;
      } else if (format == viz::SinglePlaneFormat::kRGBA_1010102 ||
                 format == viz::SinglePlaneFormat::kBGRA_1010102) {
        return GL_RGB10_A2_EXT;
      }
      return GLDataFormat(format, /*plane_index=*/0);
    }

    // For multiplanar formats without external sampler, GL formats are per
    // plane. For single channel 8-bit planes Y, U, V, A return GL_RED_EXT. For
    // single channel 10/16-bit planes Y,  U, V, A return GL_R16_EXT. For 2
    // channel plane 8-bit UV return GL_RG_EXT. For 2 channel plane 10/16-bit UV
    // return GL_RG16_EXT.
    int num_channels = format.NumChannelsInPlane(plane_index);
    DCHECK_LE(num_channels, 2);
    switch (format.channel_format()) {
      case viz::SharedImageFormat::ChannelFormat::k8:
        return num_channels == 2 ? GL_RG_EXT : GL_RED_EXT;
      case viz::SharedImageFormat::ChannelFormat::k10:
      case viz::SharedImageFormat::ChannelFormat::k16:
        return num_channels == 2 ? GL_RG16_EXT : GL_R16_EXT;
      case viz::SharedImageFormat::ChannelFormat::k16F:
        return num_channels == 2 ? GL_RG16F_EXT : GL_R16F_EXT;
    }
  }

  // Returns texture storage format for given `format`.
  static GLenum TextureStorageFormat(viz::SharedImageFormat format,
                                     bool use_angle_rgbx_format,
                                     int plane_index) {
    DCHECK(format.IsValidPlaneIndex(plane_index));
    if (format.is_single_plane()) {
      return viz::SharedImageFormatRestrictedSinglePlaneUtils::
          ToGLTextureStorageFormat(format, use_angle_rgbx_format);
    }

    // For multiplanar formats without external sampler, GL formats are per
    // plane. For single channel 8-bit planes Y, U, V, A return GL_R8_EXT. For
    // single channel 10/16-bit planes Y,  U, V, A return GL_R16_EXT. For 2
    // channel plane 8-bit UV return GL_RG8_EXT. For 2 channel plane 10/16-bit
    // UV return GL_RG16_EXT.
    int num_channels = format.NumChannelsInPlane(plane_index);
    DCHECK_LE(num_channels, 2);
    switch (format.channel_format()) {
      case viz::SharedImageFormat::ChannelFormat::k8:
        return num_channels == 2 ? GL_RG8_EXT : GL_R8_EXT;
      case viz::SharedImageFormat::ChannelFormat::k10:
      case viz::SharedImageFormat::ChannelFormat::k16:
        return num_channels == 2 ? GL_RG16_EXT : GL_R16_EXT;
      case viz::SharedImageFormat::ChannelFormat::k16F:
        return num_channels == 2 ? GL_RG16F_EXT : GL_R16F_EXT;
    }
  }
};

gfx::BufferFormat ToBufferFormat(viz::SharedImageFormat format) {
  if (format.is_single_plane()) {
    return viz::SinglePlaneSharedImageFormatToBufferFormat(format);
  }

  if (format == viz::MultiPlaneFormat::kYV12) {
    return gfx::BufferFormat::YVU_420;
  } else if (format == viz::MultiPlaneFormat::kNV12) {
    return gfx::BufferFormat::YUV_420_BIPLANAR;
  } else if (format == viz::MultiPlaneFormat::kNV12A) {
    return gfx::BufferFormat::YUVA_420_TRIPLANAR;
  } else if (format == viz::MultiPlaneFormat::kP010) {
    return gfx::BufferFormat::P010;
  }
  NOTREACHED() << "format=" << format.ToString();
  return gfx::BufferFormat::RGBA_8888;
}

SkYUVAInfo::PlaneConfig ToSkYUVAPlaneConfig(viz::SharedImageFormat format) {
  switch (format.plane_config()) {
    case viz::SharedImageFormat::PlaneConfig::kY_U_V:
      return SkYUVAInfo::PlaneConfig::kY_U_V;
    case viz::SharedImageFormat::PlaneConfig::kY_V_U:
      return SkYUVAInfo::PlaneConfig::kY_V_U;
    case viz::SharedImageFormat::PlaneConfig::kY_UV:
      return SkYUVAInfo::PlaneConfig::kY_UV;
    case viz::SharedImageFormat::PlaneConfig::kY_UV_A:
      return SkYUVAInfo::PlaneConfig::kY_UV_A;
  }
}

SkYUVAInfo::Subsampling ToSkYUVASubsampling(viz::SharedImageFormat format) {
  switch (format.subsampling()) {
    case viz::SharedImageFormat::Subsampling::k420:
      return SkYUVAInfo::Subsampling::k420;
  }
}

SkColorType ToClosestSkColorTypeExternalSampler(viz::SharedImageFormat format) {
  CHECK(format.PrefersExternalSampler());
  auto channel_format = format.channel_format();
  switch (channel_format) {
    case viz::SharedImageFormat::ChannelFormat::k8:
      return format.HasAlpha() ? kRGBA_8888_SkColorType : kRGB_888x_SkColorType;
    case viz::SharedImageFormat::ChannelFormat::k10:
      return kRGBA_1010102_SkColorType;
    case viz::SharedImageFormat::ChannelFormat::k16:
      return kR16G16B16A16_unorm_SkColorType;
    case viz::SharedImageFormat::ChannelFormat::k16F:
      return kRGBA_F16_SkColorType;
  }
}

GLFormatDesc ToGLFormatDescExternalSampler(viz::SharedImageFormat format) {
  CHECK(format.PrefersExternalSampler());
  GLenum ext_format = format.HasAlpha() ? GL_RGBA : GL_RGB;
  GLFormatDesc gl_format;
  gl_format.data_type = GL_NONE;
  gl_format.data_format = ext_format;
  gl_format.image_internal_format = ext_format;
  switch (format.channel_format()) {
    case viz::SharedImageFormat::ChannelFormat::k8:
      gl_format.storage_internal_format =
          format.HasAlpha() ? GL_RGBA8_OES : GL_RGB8_OES;
      break;
    case viz::SharedImageFormat::ChannelFormat::k10:
      gl_format.storage_internal_format = GL_RGB10_A2_EXT;
      break;
    case viz::SharedImageFormat::ChannelFormat::k16:
      gl_format.storage_internal_format = GL_RGBA16_EXT;
      break;
    case viz::SharedImageFormat::ChannelFormat::k16F:
      gl_format.storage_internal_format = GL_RGBA16F_EXT;
      break;
  }
  gl_format.target = GL_TEXTURE_EXTERNAL_OES;
  return gl_format;
}

GLFormatDesc ToGLFormatDesc(viz::SharedImageFormat format,
                            int plane_index,
                            bool use_angle_rgbx_format) {
  GLFormatDesc gl_format;
  gl_format.data_type =
      SharedImageFormatRestrictedUtilsAccessor::GLDataType(format);
  gl_format.data_format =
      SharedImageFormatRestrictedUtilsAccessor::GLDataFormat(format,
                                                             plane_index);
  gl_format.image_internal_format =
      SharedImageFormatRestrictedUtilsAccessor::GLInternalFormat(format,
                                                                 plane_index);
  gl_format.storage_internal_format =
      SharedImageFormatRestrictedUtilsAccessor::TextureStorageFormat(
          format, use_angle_rgbx_format, plane_index);
  gl_format.target = GL_TEXTURE_2D;
  return gl_format;
}

GLFormatDesc ToGLFormatDescOverrideHalfFloatType(viz::SharedImageFormat format,
                                                 int plane_index,
                                                 bool use_angle_rgbx_format,
                                                 bool use_half_float_oes) {
  GLFormatDesc format_desc =
      ToGLFormatDesc(format, plane_index, use_angle_rgbx_format);
  // GL_HALF_FLOAT and GL_HALF_FLOAT_OES have different values so cannot be used
  // interchangeably.
  if (format_desc.data_type == GL_HALF_FLOAT_OES && !use_half_float_oes) {
    format_desc.data_type = GL_HALF_FLOAT;
  }
  // ES3 requires using sized internal format for GL_HALF_FLOAT.
  if (format_desc.image_internal_format == GL_RGBA &&
      format_desc.data_format == GL_RGBA &&
      format_desc.data_type == GL_HALF_FLOAT) {
    format_desc.image_internal_format = GL_RGBA16F;
  }
  return format_desc;
}

#if BUILDFLAG(ENABLE_VULKAN)
bool HasVkFormat(viz::SharedImageFormat format) {
  if (format.is_single_plane()) {
    return ToVkFormatSinglePlanarInternal(format) != VK_FORMAT_UNDEFINED;
  } else if (format == viz::MultiPlaneFormat::kYV12 ||
             format == viz::MultiPlaneFormat::kNV12 ||
             format == viz::MultiPlaneFormat::kP010 ||
             format == viz::MultiPlaneFormat::kI420) {
    return true;
  }

  return false;
}

VkFormat ToVkFormatExternalSampler(viz::SharedImageFormat format) {
  CHECK(format.PrefersExternalSampler());
  if (format == viz::MultiPlaneFormat::kYV12 ||
      format == viz::MultiPlaneFormat::kI420) {
    return VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;
  } else if (format == viz::MultiPlaneFormat::kNV12) {
    return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
  } else if (format == viz::MultiPlaneFormat::kP010) {
    return VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
  }

  NOTREACHED() << "Unsupported format: " << format.ToString();
  return VK_FORMAT_UNDEFINED;
}

VkFormat ToVkFormatSinglePlanar(viz::SharedImageFormat format) {
  CHECK(format.is_single_plane());
  auto result = ToVkFormatSinglePlanarInternal(format);
  DCHECK_NE(result, VK_FORMAT_UNDEFINED)
      << "Unsupported format " << format.ToString();
  return result;
}

VkFormat ToVkFormat(viz::SharedImageFormat format, int plane_index) {
  DCHECK(format.IsValidPlaneIndex(plane_index));

  if (format.is_single_plane()) {
    return ToVkFormatSinglePlanar(format);
  }

  // The following SharedImageFormat constants have PrefersExternalSampler()
  // false so they create a separate VkImage per plane and return the single
  // planar equivalents. NOTE: Callsites that handle formats with external
  // sampling need to call ToVkFormatExternalSampler() if external sampling is
  // being used.
  CHECK(!format.PrefersExternalSampler());
  if (format == viz::MultiPlaneFormat::kYV12 ||
      format == viz::MultiPlaneFormat::kI420) {
    // Based on VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM.
    return VK_FORMAT_R8_UNORM;
  } else if (format == viz::MultiPlaneFormat::kNV12) {
    // Based on VK_FORMAT_G8_B8R8_2PLANE_420_UNORM.
    return plane_index == 0 ? VK_FORMAT_R8_UNORM : VK_FORMAT_R8G8_UNORM;
  } else if (format == viz::MultiPlaneFormat::kP010) {
    // Based on VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 but using
    // 16bit unorm plane formats as they are class compatible and more widely
    // supported.
    return plane_index == 0 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R16G16_UNORM;
  }

  NOTREACHED() << "Unsupported format: " << format.ToString();
  return VK_FORMAT_UNDEFINED;
}
#endif

wgpu::TextureFormat ToDawnFormat(viz::SharedImageFormat format) {
  if (format == viz::SinglePlaneFormat::kRGBA_8888 ||
      format == viz::SinglePlaneFormat::kRGBX_8888) {
    return wgpu::TextureFormat::RGBA8Unorm;
  } else if (format == viz::SinglePlaneFormat::kBGRA_8888 ||
             format == viz::SinglePlaneFormat::kBGRX_8888) {
    return wgpu::TextureFormat::BGRA8Unorm;
  } else if (format == viz::SinglePlaneFormat::kR_8 ||
             format == viz::SinglePlaneFormat::kALPHA_8 ||
             format == viz::SinglePlaneFormat::kLUMINANCE_8) {
    return wgpu::TextureFormat::R8Unorm;
  } else if (format == viz::SinglePlaneFormat::kRG_88) {
    return wgpu::TextureFormat::RG8Unorm;
  } else if (format == viz::SinglePlaneFormat::kR_16) {
    return wgpu::TextureFormat::R16Unorm;
  } else if (format == viz::SinglePlaneFormat::kLUMINANCE_F16) {
    return wgpu::TextureFormat::R16Float;
  } else if (format == viz::SinglePlaneFormat::kRG_1616) {
    return wgpu::TextureFormat::RG16Unorm;
  } else if (format == viz::SinglePlaneFormat::kRGBA_F16) {
    return wgpu::TextureFormat::RGBA16Float;
  } else if (format == viz::SinglePlaneFormat::kRGBA_1010102) {
    return wgpu::TextureFormat::RGB10A2Unorm;
  } else if (format == viz::LegacyMultiPlaneFormat::kNV12 ||
             format == viz::MultiPlaneFormat::kNV12) {
    return wgpu::TextureFormat::R8BG8Biplanar420Unorm;
  } else if (format == viz::LegacyMultiPlaneFormat::kP010 ||
             format == viz::MultiPlaneFormat::kP010) {
    return wgpu::TextureFormat::R10X6BG10X6Biplanar420Unorm;
  }
  // TODO(crbug.com/1175525): Add R8BG8A8Triplanar420Unorm format for dawn.
  NOTREACHED() << "Unsupported format: " << format.ToString();
  return wgpu::TextureFormat::Undefined;
}

wgpu::TextureFormat ToDawnFormat(viz::SharedImageFormat format,
                                 int plane_index) {
  CHECK(format.is_multi_plane() || format.IsLegacyMultiplanar() ||
        (plane_index == 0));
  // The multi plane formats create a separate image per plane and return the
  // single planar equivalents.
  // TODO(crbug.com/1449108): The above reasoning does not hold unilaterally
  // on Android, and this function will need more information to determine the
  // correct operation to take on that platform.
#if BUILDFLAG(IS_ANDROID)
  CHECK(format.is_single_plane() && !format.IsLegacyMultiplanar());
#endif
  if (format == viz::LegacyMultiPlaneFormat::kNV12 ||
      format == viz::MultiPlaneFormat::kNV12 ||
      format == viz::LegacyMultiPlaneFormat::kNV12A ||
      format == viz::MultiPlaneFormat::kNV12A) {
    // Y and A planes are R8, UV is RG8.
    return plane_index == 1 ? wgpu::TextureFormat::RG8Unorm
                            : wgpu::TextureFormat::R8Unorm;
  } else if (format == viz::LegacyMultiPlaneFormat::kP010 ||
             format == viz::MultiPlaneFormat::kP010) {
    // Y plane is R16, UV is RG16.
    return plane_index == 0 ? wgpu::TextureFormat::R16Unorm
                            : wgpu::TextureFormat::RG16Unorm;
  } else if (format == viz::LegacyMultiPlaneFormat::kYV12 ||
             format == viz::MultiPlaneFormat::kYV12 ||
             format == viz::MultiPlaneFormat::kI420) {
    // All planes are R8.
    return wgpu::TextureFormat::R8Unorm;
  } else {
    // Fallback to return single-plane format.
    return ToDawnFormat(format);
  }
}

WGPUTextureFormat ToWGPUFormat(viz::SharedImageFormat format) {
  return static_cast<WGPUTextureFormat>(ToDawnFormat(format));
}

WGPUTextureFormat ToWGPUFormat(viz::SharedImageFormat format, int plane_index) {
  return static_cast<WGPUTextureFormat>(ToDawnFormat(format, plane_index));
}

wgpu::TextureUsage GetSupportedDawnTextureUsage(
    bool is_yuv_plane,
    bool is_dcomp_surface,
    bool supports_multiplanar_rendering) {
  if (is_dcomp_surface) {
    // Textures from DComp surfaces cannot be used as TextureBinding, however
    // DCompSurfaceImageBacking creates a textureable intermediate texture.
    // TODO(crbug.com/1468844): Remove TextureBinding usage when the
    // intermediate workaround is remove.
    return wgpu::TextureUsage::RenderAttachment |
           wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc |
           wgpu::TextureUsage::CopyDst;
  }

  // The below usages are not supported for multiplanar formats in Dawn.
  // TODO(crbug.com/1451784): Use read/write intent instead of format to get
  // correct usages.
  if (!is_yuv_plane) {
    return wgpu::TextureUsage::RenderAttachment |
           wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc |
           wgpu::TextureUsage::CopyDst;
  }

  // This indirectly checks for MultiPlanarRenderTargets feature being supported
  // by the dawn backend device.
  if (supports_multiplanar_rendering) {
    return wgpu::TextureUsage::RenderAttachment |
           wgpu::TextureUsage::TextureBinding;
  }

  return wgpu::TextureUsage::TextureBinding;
}

wgpu::TextureAspect GetDawnTextureAspect(viz::SharedImageFormat format,
                                         int plane_index) {
  if (format.is_single_plane() && !format.IsLegacyMultiplanar()) {
    DCHECK_EQ(plane_index, 0);
    return wgpu::TextureAspect::All;
  }

  if (plane_index == 0) {
    return wgpu::TextureAspect::Plane0Only;
  }

  DCHECK_EQ(plane_index, 1);
  return wgpu::TextureAspect::Plane1Only;
}

skgpu::graphite::TextureInfo GetGraphiteTextureInfo(
    GrContextType gr_context_type,
    viz::SharedImageFormat format,
    int plane_index,
    bool is_yuv_plane,
    bool mipmapped,
    bool scanout_dcomp_surface,
    bool supports_multiplanar_rendering) {
  if (gr_context_type == GrContextType::kGraphiteMetal) {
#if BUILDFLAG(SKIA_USE_METAL)
    return GetGraphiteMetalTextureInfo(format, plane_index, is_yuv_plane,
                                       mipmapped);
#endif
  } else {
    CHECK_EQ(gr_context_type, GrContextType::kGraphiteDawn);
#if BUILDFLAG(SKIA_USE_DAWN)
    return GetGraphiteDawnTextureInfo(format, plane_index, is_yuv_plane,
                                      mipmapped, scanout_dcomp_surface,
                                      supports_multiplanar_rendering);
#endif
  }
  NOTREACHED_NORETURN();
}

#if BUILDFLAG(SKIA_USE_DAWN)
skgpu::graphite::DawnTextureInfo GetGraphiteDawnTextureInfo(
    viz::SharedImageFormat format,
    int plane_index,
    bool is_yuv_plane,
    bool mipmapped,
    bool scanout_dcomp_surface,
    bool supports_multiplanar_rendering) {
  skgpu::graphite::DawnTextureInfo dawn_texture_info;
  wgpu::TextureFormat wgpu_format = ToDawnFormat(format, plane_index);
  if (wgpu_format != wgpu::TextureFormat::Undefined) {
    dawn_texture_info.fSampleCount = 1;
    dawn_texture_info.fFormat = wgpu_format;
    dawn_texture_info.fUsage = GetSupportedDawnTextureUsage(
        is_yuv_plane, scanout_dcomp_surface, supports_multiplanar_rendering);
    dawn_texture_info.fMipmapped =
        mipmapped ? skgpu::Mipmapped::kYes : skgpu::Mipmapped::kNo;
  }
  return dawn_texture_info;
}
#endif

}  // namespace gpu
