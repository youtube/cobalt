// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#import "starboard/tvos/shared/media/vp9_sw_av_video_sample_buffer_builder.h"

#include <algorithm>
#include <limits>

#include "starboard/common/string.h"
#include "starboard/common/time.h"
#include "starboard/system.h"
#include "starboard/tvos/shared/media/vp9_av_sample_buffer_helper.h"

namespace starboard {

namespace {

const int kMaxNumberOfHoldingDecodedImages = 4;
const size_t kMaxMemoryBudget = 1.4 * 1024 * 1024 * 1024;
const size_t kDefaultPrerollFrameCount = 8;
const size_t k4KHdrPrerollFrameCount = 48;
const size_t kDefaultMaxNumberOfCachedFrames = 48;
const size_t k4KHdrMaxNumberOfCachedFrames = 56;
// The initial max number of cached frames should be the maximum number of
// possible cached frames.
const size_t kInitialMaxNumberOfCachedFrames = k4KHdrMaxNumberOfCachedFrames;

// Bounds for the number of threads used for software video decoding.
// https://source.chromium.org/chromium/chromium/src/+/main:media/base/limits.h;l=86
constexpr int kMinVideoDecodeThreads = 2;
constexpr int kMaxVideoDecodeThreads = 16;

// CopyYAndMergeUVRowAligned() copies one row of y data and interleaves one row
// of planar uv data. Note that it processes 64 bytes at a time. The lengths of
// |source_y|, |destination_y| and |destination_uv| should be greater or equal
// to ceil(width/64) * 64. The lengths of |source_u| and |source_v| should be
// greater or equal to ceil(width/64) * 32.
inline void CopyYAndMergeUVRowAligned(const uint8_t* source_y,
                                      const uint8_t* source_u,
                                      const uint8_t* source_v,
                                      uint8_t* destination_y,
                                      uint8_t* destination_uv,
                                      size_t width) {
  asm volatile("dmb nshld                   \n"
               "1:                          \n"
               "ldnp q0, q1, [%0]           \n"
               "add %0, %0, #0x20           \n"
               "stnp q0, q1, [%3]           \n"
               "add %3, %3, #0x20           \n"

               "ldnp q2, q3, [%0]           \n"
               "add %0, %0, #0x20           \n"
               "stnp q2, q3, [%3]           \n"
               "add %3, %3, #0x20           \n"

               "ldnp q0, q2, [%1]           \n"
               "add %1, %1, #0x20           \n"
               "ldnp q1, q3, [%2]           \n"
               "add %2, %2, #0x20           \n"
               "zip1.16b v4, v0, v1          \n"
               "zip2.16b v5, v0, v1          \n"
               "stnp q4, q5, [%4]           \n"
               "zip1.16b v4, v2, v3          \n"
               "zip2.16b v5, v2, v3          \n"
               "stnp q4, q5, [%4, 0x20]     \n"
               "add %4, %4, #0x40           \n"

               "subs %5, %5, 0x40           \n"
               "b.gt       1b               \n"
               :                      // Output registers:
               "+r"(source_y),        // %0
               "+r"(source_u),        // %1
               "+r"(source_v),        // %2
               "+r"(destination_y),   // %3
               "+r"(destination_uv),  // %4
               "+r"(width)            // %5
               :                      // Input registers:
               :                      // Clobber registers:
               "memory", "v0", "v1", "v2", "v3", "v4", "v5");
}

inline void CopyAndMergeUVRow(const uint8_t* source_u,
                              const uint8_t* source_v,
                              uint8_t* destination_uv,
                              size_t width) {
  do {
    *destination_uv++ = *source_u++;
    *destination_uv++ = *source_v++;
    width--;
  } while (width > 0);
}

void CopyVpxImage8IntoPixelBuffer(const vpx_image_t& vpx_image,
                                  CVPixelBufferRef pixel_buffer) {
  SB_DCHECK(vpx_image.fmt == VPX_IMG_FMT_I420);

  CVPixelBufferLockBaseAddress(pixel_buffer, 0);

  size_t width = vpx_image.d_w;
  size_t height = vpx_image.d_h;

  if (width == 0 || height == 0) {
    return;
  }

  uint8_t* source_y = reinterpret_cast<uint8_t*>(vpx_image.planes[0]);
  uint8_t* source_u = reinterpret_cast<uint8_t*>(vpx_image.planes[1]);
  uint8_t* source_v = reinterpret_cast<uint8_t*>(vpx_image.planes[2]);
  uint8_t* destination_y = static_cast<uint8_t*>(
      CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 0));
  uint8_t* destination_uv = static_cast<uint8_t*>(
      CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 1));
  size_t source_y_stride = vpx_image.stride[0];
  size_t source_u_stride = vpx_image.stride[1];
  size_t source_v_stride = vpx_image.stride[2];
  size_t destination_y_stride =
      CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 0);
  size_t destination_uv_stride =
      CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 1);

  SB_DCHECK(source_y_stride >= ceil(width / 64) * 64);
  SB_DCHECK(source_u_stride >= ceil(width / 64) * 32);
  SB_DCHECK(source_v_stride >= ceil(width / 64) * 32);
  SB_DCHECK(destination_y_stride % 64 == 0);
  SB_DCHECK(destination_uv_stride % 64 == 0);

  while (height > 1) {
    memcpy(destination_y, source_y, width);
    source_y += source_y_stride;
    destination_y += destination_y_stride;

    CopyYAndMergeUVRowAligned(source_y, source_u, source_v, destination_y,
                              destination_uv, width);
    source_y += source_y_stride;
    destination_y += destination_y_stride;
    source_u += source_u_stride;
    source_v += source_v_stride;
    destination_uv += destination_uv_stride;

    height -= 2;
  }

  if (height > 0) {
    memcpy(destination_y, source_y, width);
    CopyAndMergeUVRow(source_u, source_v, destination_uv, width / 2);
  }

  CVPixelBufferUnlockBaseAddress(pixel_buffer, 0);
}

// CopyAndShiftYRow() copies one row of y data from |source| to |destination|
// and converts right aligned 10-bit data to left aligned. Note that it
// processes 32 bytes in a time. The lengths of |source_y| and |destination|
// should be greater or equal to ceil(width/32) * 32.
inline void CopyAndShiftRowAligned(const uint16_t* source,
                                   uint16_t* destination,
                                   size_t width) {
  asm volatile("dmb nshld                 \n"
               "1:                        \n"
               "ldnp q0, q1, [%0]         \n"
               "add %0, %0, #0x20         \n"
               "shl.8h v0, v0, #0x6       \n"
               "shl.8h v1, v1, #0x6       \n"
               "stnp q0, q1, [%1]         \n"
               "add %1, %1, #0x20         \n"
               "subs %2, %2, 0x10         \n"
               "b.gt       1b             \n"
               :                   // Output registers:
               "+r"(source),       // %0
               "+r"(destination),  // %1
               "+r"(width)         // %2
               :                   // Input registers:
               :                   // Clobber registers:
               "memory", "v0", "v1");
}

// CopyYAndMergeUVRow() copies one row of y data and interleaves one row of
// planar uv data. It also converts right aligned 10-bit data to left aligned.
// Note that it processes 64 bytes in a time. The lengths of |source_y|,
// |destination_y| and |destination_uv| should be greater or equal to
// ceil(width/32) * 32. The lengths of |source_u| and |source_v| should be
// greater or equal to ceil(width/32) * 16.
inline void CopyYAndMergeUVRowAligned(const uint16_t* source_y,
                                      const uint16_t* source_u,
                                      const uint16_t* source_v,
                                      uint16_t* destination_y,
                                      uint16_t* destination_uv,
                                      size_t width) {
  asm volatile("dmb nshld                   \n"
               "1:                          \n"
               "ldnp q0, q1, [%0]           \n"
               "add %0, %0, #0x20           \n"
               "shl.8h v2, v0, #0x6         \n"
               "shl.8h v3, v1, #0x6         \n"
               "stnp q2, q3, [%3]           \n"
               "add %3, %3, #0x20           \n"

               "ldnp q0, q1, [%0]           \n"
               "add %0, %0, #0x20           \n"
               "shl.8h v2, v0, #0x6         \n"
               "shl.8h v3, v1, #0x6         \n"
               "stnp q2, q3, [%3]           \n"
               "add %3, %3, #0x20           \n"

               "ldnp q0, q2, [%1]           \n"
               "add %1, %1, #0x20           \n"
               "shl.8h v0, v0, #0x6         \n"
               "shl.8h v2, v2, #0x6         \n"
               "ldnp q1, q3, [%2]           \n"
               "add %2, %2, #0x20           \n"
               "shl.8h v1, v1, #0x6         \n"
               "zip1.8h v4, v0, v1          \n"
               "zip2.8h v5, v0, v1          \n"
               "stnp q4, q5, [%4]           \n"
               "shl.8h v3, v3, #0x6         \n"
               "zip1.8h v4, v2, v3          \n"
               "zip2.8h v5, v2, v3          \n"
               "stnp q4, q5, [%4, 0x20]     \n"
               "add %4, %4, #0x40           \n"

               "subs %5, %5, 0x20           \n"
               "b.gt       1b               \n"
               :                      // Output registers:
               "+r"(source_y),        // %0
               "+r"(source_u),        // %1
               "+r"(source_v),        // %2
               "+r"(destination_y),   // %3
               "+r"(destination_uv),  // %4
               "+r"(width)            // %5
               :                      // Input registers:
               :                      // Clobber registers:
               "memory", "v0", "v1", "v2", "v3", "v4", "v5");
}

inline void CopyAndShiftYRow(const uint16_t* source,
                             uint16_t* destination,
                             size_t width) {
  while (width > 0) {
    *destination++ = *source++ << 6;
    width--;
  };
}

inline void CopyAndMergeUVRow(const uint16_t* source_u,
                              const uint16_t* source_v,
                              uint16_t* destination_uv,
                              size_t width) {
  while (width > 0) {
    *destination_uv++ = *source_u++ << 6;
    *destination_uv++ = *source_v++ << 6;
    width--;
  };
}

void CopyVpxImage16IntoPixelBuffer(const vpx_image_t& vpx_image,
                                   CVPixelBufferRef pixel_buffer) {
  SB_DCHECK(vpx_image.fmt == VPX_IMG_FMT_I42016);

  CVPixelBufferLockBaseAddress(pixel_buffer, 0);

  size_t width = vpx_image.d_w;
  size_t height = vpx_image.d_h;

  if (width == 0 || height == 0) {
    return;
  }

  SB_DCHECK(width == CVPixelBufferGetWidthOfPlane(pixel_buffer, 0));
  SB_DCHECK(height == CVPixelBufferGetHeightOfPlane(pixel_buffer, 0));
  SB_DCHECK(width / 2 == CVPixelBufferGetWidthOfPlane(pixel_buffer, 1));
  SB_DCHECK(height / 2 == CVPixelBufferGetHeightOfPlane(pixel_buffer, 1));
  SB_DCHECK(height > 0);

  uint16_t* source_y = reinterpret_cast<uint16_t*>(vpx_image.planes[0]);
  uint16_t* source_u = reinterpret_cast<uint16_t*>(vpx_image.planes[1]);
  uint16_t* source_v = reinterpret_cast<uint16_t*>(vpx_image.planes[2]);
  uint16_t* destination_y = static_cast<uint16_t*>(
      CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 0));
  uint16_t* destination_uv = static_cast<uint16_t*>(
      CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 1));
  size_t source_y_stride = vpx_image.stride[0] / 2;
  size_t destination_y_stride =
      CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 0) / 2;
  size_t source_u_stride = vpx_image.stride[1] / 2;
  size_t source_v_stride = vpx_image.stride[2] / 2;
  size_t destination_uv_stride =
      CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 1) / 2;

  SB_DCHECK(source_y_stride >= ceil(width / 32) * 32);
  SB_DCHECK(source_u_stride >= ceil(width / 32) * 16);
  SB_DCHECK(source_v_stride >= ceil(width / 32) * 16);
  SB_DCHECK(destination_y_stride % 32 == 0);
  SB_DCHECK(destination_uv_stride % 32 == 0);

  while (height > 1) {
    CopyYAndMergeUVRowAligned(source_y, source_u, source_v, destination_y,
                              destination_uv, width);
    source_y += source_y_stride;
    destination_y += destination_y_stride;
    source_u += source_u_stride;
    source_v += source_v_stride;
    destination_uv += destination_uv_stride;

    CopyAndShiftRowAligned(source_y, destination_y, width);
    source_y += source_y_stride;
    destination_y += destination_y_stride;

    height -= 2;
  }
  if (height > 0) {
    CopyAndShiftYRow(source_y, destination_y, width);
    CopyAndMergeUVRow(source_u, source_v, destination_uv, width / 2);
  }

  CVPixelBufferUnlockBaseAddress(pixel_buffer, 0);
}

void CopyVpxImageIntoPixelBuffer(const vpx_image_t& vpx_image,
                                 CVPixelBufferRef pixel_buffer) {
  SB_DCHECK(pixel_buffer);
  if (vpx_image.fmt == VPX_IMG_FMT_I420) {
    CopyVpxImage8IntoPixelBuffer(vpx_image, pixel_buffer);
  } else {
    SB_DCHECK(vpx_image.fmt == VPX_IMG_FMT_I42016);
    CopyVpxImage16IntoPixelBuffer(vpx_image, pixel_buffer);
  }
}

void SetTransferCharacteristics(CFMutableDictionaryRef attachments,
                                const SbMediaColorMetadata& color) {
  SbMediaTransferId transfer_id = color.transfer;
  switch (transfer_id) {
    case kSbMediaTransferIdGamma22:
      CFNumberRef gamma_level;
      gamma_level = CFNumberCreate(NULL, kCFNumberFloat32Type, &kGammaLevel22);
      CFDictionarySetValue(attachments, kCVImageBufferGammaLevelKey,
                           gamma_level);
      CFRelease(gamma_level);
      CFDictionarySetValue(attachments, kCVImageBufferTransferFunctionKey,
                           kCVImageBufferTransferFunction_UseGamma);
      return;
    case kSbMediaTransferIdGamma24:
      gamma_level = CFNumberCreate(NULL, kCFNumberFloat32Type, &kGammaLevel24);
      CFDictionarySetValue(attachments, kCVImageBufferGammaLevelKey,
                           gamma_level);
      CFRelease(gamma_level);
      CFDictionarySetValue(attachments, kCVImageBufferTransferFunctionKey,
                           kCVImageBufferTransferFunction_UseGamma);
      return;
    case kSbMediaTransferIdGamma28:
      gamma_level = CFNumberCreate(NULL, kCFNumberFloat32Type, &kGammaLevel28);
      CFDictionarySetValue(attachments, kCVImageBufferGammaLevelKey,
                           gamma_level);
      CFRelease(gamma_level);
      CFDictionarySetValue(attachments, kCVImageBufferTransferFunctionKey,
                           kCVImageBufferTransferFunction_UseGamma);
      return;
    case kSbMediaTransferIdBt709:
    case kSbMediaTransferIdUnspecified:
    case kSbMediaTransferIdSmpte170M:
      CFDictionarySetValue(attachments, kCVImageBufferTransferFunctionKey,
                           kCVImageBufferTransferFunction_ITU_R_709_2);
      return;
    case kSbMediaTransferId10BitBt2020:
    case kSbMediaTransferId12BitBt2020:
      CFDictionarySetValue(attachments, kCVImageBufferTransferFunctionKey,
                           kCVImageBufferTransferFunction_ITU_R_2020);
      return;
    case kSbMediaTransferIdSmpteSt2084:
      CFDictionarySetValue(attachments, kCVImageBufferTransferFunctionKey,
                           kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ);
      return;
    case kSbMediaTransferIdAribStdB67:
      CFDictionarySetValue(attachments, kCVImageBufferTransferFunctionKey,
                           kCVImageBufferTransferFunction_ITU_R_2100_HLG);
      return;
    case kSbMediaTransferIdSmpte240M:
      CFDictionarySetValue(attachments, kCVImageBufferTransferFunctionKey,
                           kCVImageBufferTransferFunction_SMPTE_240M_1995);
      return;
    case kSbMediaTransferIdSmpteSt4281:
      CFDictionarySetValue(attachments, kCVImageBufferTransferFunctionKey,
                           kCVImageBufferTransferFunction_SMPTE_ST_428_1);
      return;
    case kSbMediaTransferIdReserved0:
    case kSbMediaTransferIdReserved:
    case kSbMediaTransferIdLinear:
    case kSbMediaTransferIdCustom:
    case kSbMediaTransferIdIec6196621:
    case kSbMediaTransferIdUnknown:
    case kSbMediaTransferIdLog:
    case kSbMediaTransferIdLogSqrt:
    case kSbMediaTransferIdIec6196624:
    case kSbMediaTransferIdBt1361Ecg:
    case kSbMediaTransferIdSmpteSt2084NonHdr:
      SB_NOTREACHED();
  }
  SB_NOTREACHED();
  CFDictionarySetValue(attachments, kCVImageBufferTransferFunctionKey,
                       kCVImageBufferTransferFunction_ITU_R_709_2);
}

void SetMatrixCoefficients(CFMutableDictionaryRef attachments,
                           const SbMediaColorMetadata& color) {
  SbMediaMatrixId matrix_id = color.matrix;
  switch (matrix_id) {
    case kSbMediaMatrixIdBt709:
    case kSbMediaMatrixIdUnspecified:
      CFDictionarySetValue(attachments, kCVImageBufferYCbCrMatrixKey,
                           kCVImageBufferYCbCrMatrix_ITU_R_709_2);
      return;
    case kSbMediaMatrixIdBt2020NonconstantLuminance:
      CFDictionarySetValue(attachments, kCVImageBufferYCbCrMatrixKey,
                           kCVImageBufferYCbCrMatrix_ITU_R_2020);
      return;
    case kSbMediaMatrixIdFcc:
    case kSbMediaMatrixIdSmpte170M:
    case kSbMediaMatrixIdBt470Bg:
      CFDictionarySetValue(attachments, kCVImageBufferYCbCrMatrixKey,
                           kCVImageBufferYCbCrMatrix_ITU_R_601_4);
      return;
    case kSbMediaMatrixIdSmpte240M:
      CFDictionarySetValue(attachments, kCVImageBufferYCbCrMatrixKey,
                           kCVImageBufferYCbCrMatrix_SMPTE_240M_1995);
      return;
    case kSbMediaMatrixIdRgb:
    case kSbMediaMatrixIdYCgCo:
    case kSbMediaMatrixIdBt2020ConstantLuminance:
    case kSbMediaMatrixIdYDzDx:
    case kSbMediaMatrixIdReserved:
    case kSbMediaMatrixIdInvalid:
      SB_NOTREACHED();
  }
  SB_NOTREACHED();
  CFDictionarySetValue(attachments, kCVImageBufferYCbCrMatrixKey,
                       kCVImageBufferYCbCrMatrix_ITU_R_709_2);
}

void SetPrimaries(CFMutableDictionaryRef attachments,
                  const SbMediaColorMetadata& color) {
  SbMediaPrimaryId primary_id = color.primaries;
  switch (primary_id) {
    case kSbMediaPrimaryIdBt709:
    case kSbMediaPrimaryIdUnspecified:
      CFDictionarySetValue(attachments, kCVImageBufferColorPrimariesKey,
                           kCVImageBufferColorPrimaries_ITU_R_709_2);
      return;
    case kSbMediaPrimaryIdBt2020:
      CFDictionarySetValue(attachments, kCVImageBufferColorPrimariesKey,
                           kCVImageBufferColorPrimaries_ITU_R_2020);
      return;
    case kSbMediaPrimaryIdSmpte170M:
    case kSbMediaPrimaryIdSmpte240M:
      CFDictionarySetValue(attachments, kCVImageBufferColorPrimariesKey,
                           kCVImageBufferColorPrimaries_SMPTE_C);
      return;
    case kSbMediaPrimaryIdBt470Bg:
      CFDictionarySetValue(attachments, kCVImageBufferColorPrimariesKey,
                           kCVImageBufferColorPrimaries_EBU_3213);
      return;
    case kSbMediaPrimaryIdSmpteSt4312:
      CFDictionarySetValue(attachments, kCVImageBufferColorPrimariesKey,
                           kCVImageBufferColorPrimaries_DCI_P3);
      return;
    case kSbMediaPrimaryIdSmpteSt4321:
      CFDictionarySetValue(attachments, kCVImageBufferColorPrimariesKey,
                           kCVImageBufferColorPrimaries_P3_D65);
      return;
    case kSbMediaPrimaryIdReserved0:
    case kSbMediaPrimaryIdReserved:
    case kSbMediaPrimaryIdFilm:
    case kSbMediaPrimaryIdBt470M:
    case kSbMediaPrimaryIdSmpteSt4281:
    case kSbMediaPrimaryIdUnknown:
    case kSbMediaPrimaryIdXyzD50:
    case kSbMediaPrimaryIdCustom:
      SB_NOTREACHED();
  }
  SB_NOTREACHED();
  CFDictionarySetValue(attachments, kCVImageBufferColorPrimariesKey,
                       kCVImageBufferColorPrimaries_ITU_R_709_2);
}

void SetContentLightLevelInfo(CFMutableDictionaryRef attachments,
                              const SbMediaColorMetadata& color) {
  LightLevelInfoSEI light_info(color);
  CFDataRef data;
  data = CFDataCreate(NULL, reinterpret_cast<UInt8*>(&light_info),
                      sizeof(light_info));
  CFDictionarySetValue(attachments, kCVImageBufferContentLightLevelInfoKey,
                       data);
  CFRelease(data);
}

void SetMasteringMetadata(CFMutableDictionaryRef attachments,
                          const SbMediaColorMetadata& color) {
  MasteringDisplayColorVolumeSEI color_volume(color.mastering_metadata);
  CFDataRef data;
  data = CFDataCreate(NULL, reinterpret_cast<UInt8*>(&color_volume),
                      sizeof(color_volume));
  CFDictionarySetValue(attachments,
                       kCVImageBufferMasteringDisplayColorVolumeKey, data);
  CFRelease(data);
}

bool IsHdrVideo(const VideoStreamInfo& video_stream_info) {
  const auto& mime = video_stream_info.mime;
  const auto& primaries = video_stream_info.color_metadata.primaries;
  return mime.find("codecs=\"vp9.2") != mime.npos ||
         mime.find("codecs=\"vp09.02") != mime.npos ||
         primaries == kSbMediaPrimaryIdBt2020;
}

// Returns the number of threads to use for VP9 software decoding based on the
// frame width. For higher resolutions, more threads are used to match the
// maximum number of tiles possible. The number of threads is also clamped to
// the number of hardware threads available on the system. Refer similar
// chromium implementation
// https://source.chromium.org/chromium/chromium/src/+/main:media/filters/vpx_video_decoder.cc;l=39
int GetVpxVideoDecoderThreadCount(int width) {
  int desired_threads = kMinVideoDecodeThreads;
  // For VP9 decoding increase the number of decode threads to equal the
  // maximum number of tiles possible for higher resolution streams.
  if (width >= 3840) {
    desired_threads = 16;
  } else if (width >= 2560) {
    desired_threads = 8;
  } else if (width >= 1280) {
    desired_threads = 4;
  }

  const int hardware_threads = SbSystemGetNumberOfProcessors();
  desired_threads = std::min(desired_threads, hardware_threads);
  return std::clamp(desired_threads, kMinVideoDecodeThreads,
                    kMaxVideoDecodeThreads);
}

}  // namespace

Vp9SwAVVideoSampleBufferBuilder::Vp9SwAVVideoSampleBufferBuilder(
    const VideoStreamInfo& video_stream_info)
    : is_hdr_(IsHdrVideo(video_stream_info)),
      preroll_frame_count_(kDefaultPrerollFrameCount),
      max_number_of_cached_buffers_(kInitialMaxNumberOfCachedFrames) {
  if (is_hdr_) {
    const SbMediaColorMetadata& color_metadata =
        video_stream_info.color_metadata;
    CFMutableDictionaryRef attachments =
        CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks);
    SetTransferCharacteristics(attachments, color_metadata);
    SetMatrixCoefficients(attachments, color_metadata);
    SetPrimaries(attachments, color_metadata);
    SetContentLightLevelInfo(attachments, color_metadata);
    SetMasteringMetadata(attachments, color_metadata);
    pixel_buffer_attachments_ = attachments;
  }
}

Vp9SwAVVideoSampleBufferBuilder::~Vp9SwAVVideoSampleBufferBuilder() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  Reset();
}

void Vp9SwAVVideoSampleBufferBuilder::Reset() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  AVVideoSampleBufferBuilder::Reset();
  // Clear pending buffers to avoid unnecessary decoding.
  {
    std::lock_guard lock(pending_input_buffers_mutex_);
    while (!pending_input_buffers_.empty()) {
      pending_input_buffers_.pop();
    }
  }
  // Clear holding decoded images. As we don't need it anymore, no need to build
  // sample buffers for pending decoded images.
  {
    std::lock_guard lock(decoded_images_mutex_);
    while (!decoded_images_.empty()) {
      decoded_images_.pop();
    }
  }
  // Wait until all jobs on |decoder_thread_| are done.
  if (decoder_thread_) {
    decoder_thread_->ScheduleAndWait(
        std::bind(&Vp9SwAVVideoSampleBufferBuilder::TeardownCodec, this));
    decoder_thread_.reset();
  }
  current_frame_width_ = 0;
  current_frame_height_ = 0;
  total_input_frames_ = 0;
  total_output_frames_ = 0;
  preroll_frame_count_ = kDefaultPrerollFrameCount;
  max_number_of_cached_buffers_ = kInitialMaxNumberOfCachedFrames;
  stream_ended_ = false;
  skip_loop_filter_ = false;
  frames_with_skip_loop_filter_ = 0;
  skip_loop_filter_on_decoder_thread_ = false;

#ifdef VP9_SW_DECODER_PRINT_OUT_DECODING_TIME
  total_decoding_time_in_last_second_ = 0;
  total_bits_in_last_second_ = 0;
  while (!decoding_times_.empty()) {
    decoding_times_.pop();
  }
#endif  // VP9_SW_DECODER_PRINT_OUT_DECODING_TIME

  SB_DCHECK(decoded_images_.empty());
  SB_DCHECK(pending_input_buffers_.empty());
  SB_DCHECK(decoding_input_buffers_.empty());
}

void Vp9SwAVVideoSampleBufferBuilder::WriteInputBuffer(
    const scoped_refptr<InputBuffer>& input_buffer,
    int64_t media_time_offset) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(input_buffer);
  SB_DCHECK(output_cb_);
  SB_DCHECK(input_buffer->video_stream_info().codec == kSbMediaVideoCodecVp9);

  if (error_occurred_) {
    SB_LOG(ERROR) << "WriteInputFrame() was called after an error.";
    return;
  }
  if (stream_ended_) {
    ReportError("WriteInputFrame() was called after WriteEndOfStream().");
    return;
  }
  if (!decoder_thread_) {
    // |media_time_offset| only changes after Reset(), it's safe to set it here
    // only once.
    media_time_offset_ = media_time_offset;
    decoder_thread_ = JobThread::Create("vpx_video_decoder");
  }
  SB_DCHECK(media_time_offset_ == media_time_offset);

  {
    std::lock_guard lock(pending_input_buffers_mutex_);
    pending_input_buffers_.push(input_buffer);
  }
  decoder_thread_->Schedule(
      std::bind(&Vp9SwAVVideoSampleBufferBuilder::DecodeOneBuffer, this));
}

void Vp9SwAVVideoSampleBufferBuilder::WriteEndOfStream() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  if (error_occurred_) {
    SB_LOG(ERROR) << "WriteEndOfStream() was called after an error.";
    return;
  }
  if (decoder_thread_) {
    decoder_thread_->Schedule(
        std::bind(&Vp9SwAVVideoSampleBufferBuilder::DecodeEndOfStream, this));
  }
  stream_ended_ = true;
}

size_t Vp9SwAVVideoSampleBufferBuilder::GetPrerollFrameCount() const {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  return std::min<size_t>(preroll_frame_count_, GetMaxNumberOfCachedFrames());
}

size_t Vp9SwAVVideoSampleBufferBuilder::GetMaxNumberOfCachedFrames() const {
#if defined(INTERNAL_BUILD)
  return max_number_of_cached_buffers_ -
         decoder_get_number_of_allocated_frame_buffers();
#else
  // TODO: b/448550237 - Add implementation to get number of allocated frame.
  return max_number_of_cached_buffers_;
#endif
}

void Vp9SwAVVideoSampleBufferBuilder::OnCachedFramesWatermarkLow() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  skip_loop_filter_ = true;
}

void Vp9SwAVVideoSampleBufferBuilder::OnCachedFramesWatermarkHigh() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  skip_loop_filter_ = false;
}

void Vp9SwAVVideoSampleBufferBuilder::InitializeCodec() {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());
  SB_DCHECK(!context_);
  SB_DCHECK(!builder_thread_);

  context_.reset(new vpx_codec_ctx);
  vpx_codec_dec_cfg_t vpx_config = {0};
  vpx_config.w = current_frame_width_;
  vpx_config.h = current_frame_height_;
  vpx_config.threads = GetVpxVideoDecoderThreadCount(current_frame_width_);

  vpx_codec_err_t status =
      vpx_codec_dec_init(context_.get(), &vpx_codec_vp9_dx_algo, &vpx_config,
                         VPX_CODEC_USE_FRAME_THREADING);
  if (status != VPX_CODEC_OK) {
    ReportError(
        FormatString("vpx_codec_dec_init() failed with status %d.", status));
    context_.reset();
    return;
  }

  if (is_hdr_ &&
      (current_frame_width_ > 2560 || current_frame_height_ > 1440)) {
    // Loop filter optimization improves HDR performance, but also increases the
    // overal CPU load of the decoder and make the app less "friendly" to the
    // system.  As a result, we only enable it for 4k HDR for now.
    status = vpx_codec_control(context_.get(), VP9D_SET_LOOP_FILTER_OPT, 1);
    SB_DCHECK(status == VPX_CODEC_OK);
  }

  builder_thread_ = JobThread::Create("vp9_buffer_builder");
}

void Vp9SwAVVideoSampleBufferBuilder::TeardownCodec() {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());
  // Wait until all jobs on |builder_thread_| are done. It may take some time if
  // |builder_thread_| is waiting for available memory.
  builder_thread_.reset();
  // |decoding_input_buffers_| and |decoded_images_| may be not empty when
  // resetting.
  decoding_input_buffers_.clear();
  while (!decoded_images_.empty()) {
    decoded_images_.pop();
  }
  // CVPixelBufferPoolRelease is NULL safe.
  CVPixelBufferPoolRelease(pixel_buffer_pool_);
  pixel_buffer_pool_ = nullptr;
  if (context_) {
    vpx_codec_destroy(context_.get());
    context_.reset();
  }
}

void Vp9SwAVVideoSampleBufferBuilder::DecodeOneBuffer() {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());
  if (error_occurred_) {
    return;
  }
  {
    std::lock_guard lock(decoded_images_mutex_);
    // We don't want to hold too many decoded images. It would increase the
    // memory usage of vpx decoder and would be released only after vpx decoder
    // is destroyed.
    if (decoded_images_.size() > kMaxNumberOfHoldingDecodedImages) {
      decoder_thread_->Schedule(
          std::bind(&Vp9SwAVVideoSampleBufferBuilder::DecodeOneBuffer, this),
          5000);
      return;
    }
  }

  scoped_refptr<InputBuffer> input_buffer;
  {
    std::lock_guard lock(pending_input_buffers_mutex_);
    if (pending_input_buffers_.empty()) {
      // |pending_input_buffers_| may be empty when and only when the decoder is
      // resetting.
      return;
    }
    input_buffer = pending_input_buffers_.front();
    pending_input_buffers_.pop();
  }

  const auto& stream_info = input_buffer->video_stream_info();
  const unsigned int stream_info_width =
      std::max(0, stream_info.frame_size.width);
  const unsigned int stream_info_height =
      std::max(0, stream_info.frame_size.height);
  if (!context_ || stream_info_width != current_frame_width_ ||
      stream_info_height != current_frame_height_) {
    if (context_) {
      FlushPendingBuffers();
      TeardownCodec();
    }
    current_frame_width_ = stream_info_width;
    current_frame_height_ = stream_info_height;
    // Update |max_number_of_cached_buffers_| and |preroll_frame_count_|. The
    // following simple calculation doesn't consider the frame size of cached
    // frames, which may be larger. So, we make |max_number_of_cached_buffers_|
    // monotonic decreasing to avoid problems.
    size_t frames_size_in_bytes =
        current_frame_width_ * current_frame_height_ * (is_hdr_ ? 3 : 1.5);
    size_t max_number_of_pixel_buffers =
        kMaxMemoryBudget / frames_size_in_bytes;
    if (is_hdr_ &&
        (current_frame_width_ > 2560 || current_frame_height_ > 1440)) {
      preroll_frame_count_ = k4KHdrPrerollFrameCount;
      max_number_of_pixel_buffers =
          std::min(max_number_of_pixel_buffers, k4KHdrMaxNumberOfCachedFrames);
    } else {
      preroll_frame_count_ = kDefaultPrerollFrameCount;
      max_number_of_pixel_buffers = std::min(max_number_of_pixel_buffers,
                                             kDefaultMaxNumberOfCachedFrames);
    }
    if (max_number_of_cached_buffers_ > max_number_of_pixel_buffers) {
      max_number_of_cached_buffers_ = max_number_of_pixel_buffers;
    }
    InitializeCodec();
  }

  SB_DCHECK(context_);
  SB_DCHECK_EQ(current_frame_width_, stream_info_width);
  SB_DCHECK_EQ(current_frame_height_, stream_info_height);

  bool skip_loop_filter = skip_loop_filter_;
  if (skip_loop_filter != skip_loop_filter_on_decoder_thread_) {
    skip_loop_filter_on_decoder_thread_ = skip_loop_filter;
    vpx_codec_control_(context_.get(), VP9_SET_SKIP_LOOP_FILTER,
                       skip_loop_filter_on_decoder_thread_);
    vpx_codec_control_(context_.get(), VP9D_SET_ROW_MT,
                       skip_loop_filter_on_decoder_thread_);
    if (skip_loop_filter_on_decoder_thread_) {
      SB_LOG(INFO) << "Start to skip loop filter for frame "
                   << input_buffer->timestamp()
                   << " total frames with loop filter skipped: "
                   << frames_with_skip_loop_filter_;
    } else {
      SB_LOG(INFO) << "Stopped skipping loop filter for frame "
                   << input_buffer->timestamp()
                   << " total frames with loop filter skipped: "
                   << frames_with_skip_loop_filter_;
    }
  }
  if (skip_loop_filter_on_decoder_thread_) {
    frames_with_skip_loop_filter_++;
  }

#ifdef VP9_SW_DECODER_PRINT_OUT_DECODING_TIME
  int64_t start = CurrentMonotonicTime();
#endif  // VP9_SW_DECODER_PRINT_OUT_DECODING_TIME

  int64_t timestamp = input_buffer->timestamp();
  static_assert(sizeof(timestamp) == sizeof(void*),
                "sizeof(timestamp) must equal to sizeof(void*).");
  vpx_codec_err_t status = vpx_codec_decode(
      context_.get(), input_buffer->data(), input_buffer->size(),
      reinterpret_cast<void*>(timestamp), 0);
  total_input_frames_++;
  SB_DCHECK(decoding_input_buffers_.find(timestamp) ==
            decoding_input_buffers_.end());
  decoding_input_buffers_.emplace(timestamp, input_buffer);

  if (status != VPX_CODEC_OK) {
    ReportError(
        FormatString("vpx_codec_decode() failed with status %d.", status));
    return;
  }
  TryGetOneFrame();

#ifdef VP9_SW_DECODER_PRINT_OUT_DECODING_TIME
  int64_t now = CurrentMonotonicTime();
  int64_t decoding_time = now - start;
  decoding_times_.push(
      std::make_tuple(now, decoding_time, input_buffer->size() * 8));
  while (now - std::get<0>(decoding_times_.front()) > 1000000) {
    total_decoding_time_in_last_second_ -= std::get<1>(decoding_times_.front());
    total_bits_in_last_second_ -= std::get<2>(decoding_times_.front());
    decoding_times_.pop();
  }
  total_decoding_time_in_last_second_ += decoding_time;
  total_bits_in_last_second_ += input_buffer->size() * 8;
  SB_LOG(INFO) << "Decoding time of frame (" << total_output_frames_ << " / "
               << total_input_frames_ << "): " << decoding_time
               << ", bitrate: " << input_buffer->size() * 8 << ", 1-sec avg: "
               << total_decoding_time_in_last_second_ / decoding_times_.size()
               << ", 1-sec bits: " << total_bits_in_last_second_;
#endif  // VP9_SW_DECODER_PRINT_OUT_DECODING_TIME
}

void Vp9SwAVVideoSampleBufferBuilder::DecodeEndOfStream() {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());
  if (error_occurred_) {
    return;
  }
  {
    std::lock_guard lock(pending_input_buffers_mutex_);
    if (!pending_input_buffers_.empty()) {
      decoder_thread_->Schedule(
          std::bind(&Vp9SwAVVideoSampleBufferBuilder::DecodeEndOfStream, this),
          pending_input_buffers_.size() * 16000);
      return;
    }
  }
  FlushPendingBuffers();
}

void Vp9SwAVVideoSampleBufferBuilder::FlushPendingBuffers() {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());
  if (error_occurred_) {
    return;
  }
  // Flush frames in decoder.
  while (total_output_frames_ < total_input_frames_) {
    // Pass |kSbInt64Max| as timestamp for the end of stream frame. It shouldn't
    // be used but just in case.
    const int64_t kEndOfStreamTimestamp = std::numeric_limits<int64_t>::max();
    vpx_codec_err_t status =
        vpx_codec_decode(context_.get(), 0, 0,
                         reinterpret_cast<void*>(kEndOfStreamTimestamp), 0);
    if (status != VPX_CODEC_OK) {
      ReportError(
          FormatString("vpx_codec_decode() failed with status %d.", status));
      return;
    }
    if (!TryGetOneFrame()) {
      ReportError(
          "Failed to flush frames in decoder, wrong number of outputs.");
      return;
    }
  }
}

bool Vp9SwAVVideoSampleBufferBuilder::TryGetOneFrame() {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());

  vpx_codec_iter_t dummy = NULL;
  const vpx_image_t* vpx_image = vpx_codec_get_frame(context_.get(), &dummy);
  if (!vpx_image) {
    // No output yet.
    return false;
  }
  total_output_frames_++;

  if (is_hdr_ && vpx_image->fmt != VPX_IMG_FMT_I42016) {
    ReportError(FormatString("Invalid hdr vpx_image.fmt: %d.", vpx_image->fmt));
    SB_NOTREACHED();
    return false;
  } else if (!is_hdr_ && vpx_image->fmt != VPX_IMG_FMT_I420) {
    ReportError(FormatString("Invalid vpx_image.fmt: %d.", vpx_image->fmt));
    SB_NOTREACHED();
    return false;
  }
  SB_DCHECK(vpx_image->stride[VPX_PLANE_Y] ==
            vpx_image->stride[VPX_PLANE_U] * 2);
  SB_DCHECK(vpx_image->stride[VPX_PLANE_U] == vpx_image->stride[VPX_PLANE_V]);
  SB_DCHECK(vpx_image->planes[VPX_PLANE_Y] < vpx_image->planes[VPX_PLANE_U]);
  SB_DCHECK(vpx_image->planes[VPX_PLANE_U] < vpx_image->planes[VPX_PLANE_V]);
  if (vpx_image->stride[VPX_PLANE_Y] != vpx_image->stride[VPX_PLANE_U] * 2 ||
      vpx_image->stride[VPX_PLANE_U] != vpx_image->stride[VPX_PLANE_V] ||
      vpx_image->planes[VPX_PLANE_Y] >= vpx_image->planes[VPX_PLANE_U] ||
      vpx_image->planes[VPX_PLANE_U] >= vpx_image->planes[VPX_PLANE_V]) {
    ReportError("Invalid yuv plane format.");
    return false;
  }
  SB_DCHECK(vpx_image->d_w == current_frame_width_);
  SB_DCHECK(vpx_image->d_h == current_frame_height_);
  if (vpx_image->d_w != current_frame_width_ ||
      vpx_image->d_h != current_frame_height_) {
    ReportError("Invalid frame size.");
    return false;
  }
  SB_DCHECK(builder_thread_);

  int64_t timestamp = reinterpret_cast<int64_t>(vpx_image->user_priv);
  SB_DCHECK(decoding_input_buffers_.find(timestamp) !=
            decoding_input_buffers_.end());
  {
    std::lock_guard lock(decoded_images_mutex_);
    decoded_images_.emplace(
        new VpxImageWrapper(decoding_input_buffers_[timestamp], *vpx_image));
  }
  decoding_input_buffers_.erase(timestamp);
  builder_thread_->Schedule(
      std::bind(&Vp9SwAVVideoSampleBufferBuilder::BuildSampleBuffer, this));
  return true;
}

void Vp9SwAVVideoSampleBufferBuilder::BuildSampleBuffer() {
  SB_DCHECK(builder_thread_->BelongsToCurrentThread());
  if (error_occurred_) {
    return;
  }

  if (!pixel_buffer_pool_) {
    CreatePixelBufferPool();
  }

  OSStatus status;
  CVPixelBufferRef pixel_buffer;
  @autoreleasepool {
    NSDictionary* aux_attributes = @{
      (id)
      kCVPixelBufferPoolAllocationThresholdKey : @(GetMaxNumberOfCachedFrames())
    };
    status = CVPixelBufferPoolCreatePixelBufferWithAuxAttributes(
        kCFAllocatorDefault, pixel_buffer_pool_,
        (__bridge CFDictionaryRef)aux_attributes, &pixel_buffer);
  }  // @autoreleasepool
  if (status == kCVReturnWouldExceedAllocationThreshold) {
    // The decoder needs more memory to produce more frame, retry later. Note
    // that the output pixel buffers may be hold by either video renderer or
    // AVSBDL.
    builder_thread_->Schedule(
        std::bind(&Vp9SwAVVideoSampleBufferBuilder::BuildSampleBuffer, this),
        16000);
    return;
  } else if (status != 0) {
    ReportOSError("CreatePixelBuffer", status);
    return;
  }

  std::unique_ptr<VpxImageWrapper> image_wrapper;
  {
    std::lock_guard lock(decoded_images_mutex_);
    // |decoded_images_| may be empty when and only when the decoder is
    // resetting.
    if (decoded_images_.empty()) {
      CFRelease(pixel_buffer);
      return;
    }
    image_wrapper = std::move(decoded_images_.front());
    decoded_images_.pop();
  }

  const vpx_image_t& vpx_image = image_wrapper->vpx_image();
  CopyVpxImageIntoPixelBuffer(vpx_image, pixel_buffer);

  if (pixel_buffer_attachments_) {
    CVBufferSetAttachments(pixel_buffer, pixel_buffer_attachments_,
                           kCVAttachmentMode_ShouldPropagate);
  }

  CMVideoFormatDescriptionRef format_description;
  status = CMVideoFormatDescriptionCreateForImageBuffer(
      kCFAllocatorDefault, pixel_buffer, &format_description);
  if (status != 0) {
    CFRelease(pixel_buffer);
    ReportOSError("FormatDescriptionCreate", status);
    return;
  }

  int64_t timestamp = reinterpret_cast<int64_t>(vpx_image.user_priv);
  CMSampleTimingInfo timing_info = {
      .duration = kCMTimeInvalid,
      .presentationTimeStamp =
          CMTimeMake(timestamp + media_time_offset_, 1000000),
      .decodeTimeStamp = kCMTimeInvalid,
  };
  CMSampleBufferRef cm_sample_buffer;
  status = CMSampleBufferCreateReadyWithImageBuffer(
      kCFAllocatorDefault, pixel_buffer, format_description, &timing_info,
      &cm_sample_buffer);
  CFRelease(pixel_buffer);
  CFRelease(format_description);
  if (status != 0) {
    ReportOSError("SampleBufferCreate", status);
    return;
  }

  scoped_refptr<AVSampleBuffer> sample_buffer(
      new AVSampleBuffer(cm_sample_buffer, image_wrapper->input_buffer()));
  output_cb_(sample_buffer);
}

void Vp9SwAVVideoSampleBufferBuilder::CreatePixelBufferPool() {
  SB_DCHECK(!pixel_buffer_pool_);
  @autoreleasepool {
    const size_t kBytesPerRowAlignment = 64;
    NSDictionary* attributes;
    if (is_hdr_) {
      attributes = @{
        (id)kCVPixelBufferPixelFormatTypeKey :
            @(kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange),
        (id)kCVPixelBufferWidthKey : @(current_frame_width_),
        (id)kCVPixelBufferHeightKey : @(current_frame_height_),
        (id)kCVPixelBufferBytesPerRowAlignmentKey : @(kBytesPerRowAlignment),
        (id)kCVPixelBufferIOSurfacePropertiesKey : @{},
      };
    } else {
      attributes = @{
        (id)kCVPixelBufferPixelFormatTypeKey :
            @(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange),
        (id)kCVPixelBufferWidthKey : @(current_frame_width_),
        (id)kCVPixelBufferHeightKey : @(current_frame_height_),
        (id)kCVPixelBufferBytesPerRowAlignmentKey : @(kBytesPerRowAlignment),
        (id)kCVPixelBufferIOSurfacePropertiesKey : @{},
      };
    }
    OSStatus status = CVPixelBufferPoolCreate(
        kCFAllocatorDefault, NULL, (__bridge CFDictionaryRef)attributes,
        &pixel_buffer_pool_);
    if (status != 0) {
      ReportOSError("PixelBufferPoolCreate", status);
      return;
    }
  }
}

Vp9SwAVVideoSampleBufferBuilder::VpxImageWrapper::VpxImageWrapper(
    const scoped_refptr<InputBuffer>& input_buffer,
    const vpx_image_t& vpx_image)
    : input_buffer_(input_buffer), vpx_image_(vpx_image) {
#if defined(INTERNAL_BUILD)
  // TODO: b/448550237 - Check if we need to acquire the hw image.
  decoder_acquire_hw_image(vpx_image_.fb_priv);
#endif
}

Vp9SwAVVideoSampleBufferBuilder::VpxImageWrapper::~VpxImageWrapper() {
#if defined(INTERNAL_BUILD)
  // TODO: b/448550237 - Check if we need to release the hw image.
  decoder_release_hw_image(vpx_image_.fb_priv);
#endif
}

}  // namespace starboard
