// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_utils.h"

#include <map>
#include <sstream>

// build_config.h must come before BUILDFLAG()
#include "build/build_config.h"
#if BUILDFLAG(IS_CHROMEOS)
#include <linux/media/av1-ctrls.h>
#endif

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/macros.h"
#include "ui/gfx/geometry/size.h"

// This has not been accepted upstream.
#ifndef V4L2_PIX_FMT_AV1
#define V4L2_PIX_FMT_AV1 v4l2_fourcc('A', 'V', '0', '1') /* AV1 */
#endif
// This has been upstreamed and backported for ChromeOS, but has not been
// picked up by the Chromium sysroots.
#ifndef V4L2_PIX_FMT_AV1_FRAME
#define V4L2_PIX_FMT_AV1_FRAME v4l2_fourcc('A', 'V', '1', 'F')
#endif

#define MAKE_V4L2_CODEC_PAIR(codec, suffix) \
  std::make_pair(codec##_##suffix, codec)

namespace media {

void RecordMediaIoctlUMA(MediaIoctlRequests function) {
  base::UmaHistogramEnumeration("Media.V4l2VideoDecoder.MediaIoctlError",
                                function);
}

void RecordVidiocIoctlErrorUMA(VidiocIoctlRequests function) {
  base::UmaHistogramEnumeration("Media.V4l2VideoDecoder.VidiocIoctlError",
                                function);
}

const char* V4L2MemoryToString(const v4l2_memory memory) {
  switch (memory) {
    case V4L2_MEMORY_MMAP:
      return "V4L2_MEMORY_MMAP";
    case V4L2_MEMORY_USERPTR:
      return "V4L2_MEMORY_USERPTR";
    case V4L2_MEMORY_DMABUF:
      return "V4L2_MEMORY_DMABUF";
    case V4L2_MEMORY_OVERLAY:
      return "V4L2_MEMORY_OVERLAY";
    default:
      return "UNKNOWN";
  }
}

std::string V4L2FormatToString(const struct v4l2_format& format) {
  std::ostringstream s;
  s << "v4l2_format type: " << format.type;
  if (format.type == V4L2_BUF_TYPE_VIDEO_CAPTURE ||
      format.type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
    //  single-planar
    const struct v4l2_pix_format& pix = format.fmt.pix;
    s << ", width_height: " << gfx::Size(pix.width, pix.height).ToString()
      << ", pixelformat: " << FourccToString(pix.pixelformat)
      << ", field: " << pix.field << ", bytesperline: " << pix.bytesperline
      << ", sizeimage: " << pix.sizeimage;
  } else if (V4L2_TYPE_IS_MULTIPLANAR(format.type)) {
    const struct v4l2_pix_format_mplane& pix_mp = format.fmt.pix_mp;
    // As long as num_planes's type is uint8_t, ostringstream treats it as a
    // char instead of an integer, which is not what we want. Casting
    // pix_mp.num_planes unsigned int solves the issue.
    s << ", width_height: " << gfx::Size(pix_mp.width, pix_mp.height).ToString()
      << ", pixelformat: " << FourccToString(pix_mp.pixelformat)
      << ", field: " << pix_mp.field
      << ", num_planes: " << static_cast<unsigned int>(pix_mp.num_planes);
    for (size_t i = 0; i < pix_mp.num_planes; ++i) {
      const struct v4l2_plane_pix_format& plane_fmt = pix_mp.plane_fmt[i];
      s << ", plane_fmt[" << i << "].sizeimage: " << plane_fmt.sizeimage
        << ", plane_fmt[" << i << "].bytesperline: " << plane_fmt.bytesperline;
    }
  } else {
    s << " unsupported yet.";
  }
  return s.str();
}

std::string V4L2BufferToString(const struct v4l2_buffer& buffer) {
  std::ostringstream s;
  s << "v4l2_buffer type: " << buffer.type << ", memory: " << buffer.memory
    << ", index: " << buffer.index << " bytesused: " << buffer.bytesused
    << ", length: " << buffer.length;
  if (buffer.type == V4L2_BUF_TYPE_VIDEO_CAPTURE ||
      buffer.type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
    //  single-planar
    if (buffer.memory == V4L2_MEMORY_MMAP) {
      s << ", m.offset: " << buffer.m.offset;
    } else if (buffer.memory == V4L2_MEMORY_USERPTR) {
      s << ", m.userptr: " << buffer.m.userptr;
    } else if (buffer.memory == V4L2_MEMORY_DMABUF) {
      s << ", m.fd: " << buffer.m.fd;
    }
  } else if (V4L2_TYPE_IS_MULTIPLANAR(buffer.type)) {
    for (size_t i = 0; i < buffer.length; ++i) {
      const struct v4l2_plane& plane = buffer.m.planes[i];
      s << ", m.planes[" << i << "](bytesused: " << plane.bytesused
        << ", length: " << plane.length
        << ", data_offset: " << plane.data_offset;
      if (buffer.memory == V4L2_MEMORY_MMAP) {
        s << ", m.mem_offset: " << plane.m.mem_offset;
      } else if (buffer.memory == V4L2_MEMORY_USERPTR) {
        s << ", m.userptr: " << plane.m.userptr;
      } else if (buffer.memory == V4L2_MEMORY_DMABUF) {
        s << ", m.fd: " << plane.m.fd;
      }
      s << ")";
    }
  } else {
    s << " unsupported yet.";
  }
  return s.str();
}

VideoCodecProfile V4L2ProfileToVideoCodecProfile(uint32_t v4l2_codec,
                                                 uint32_t v4l2_profile) {
  switch (v4l2_codec) {
    case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
      switch (v4l2_profile) {
        // H264 Stereo amd Multiview High are not tested and the use is
        // minuscule, skip.
        case V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE:
        case V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE:
          return H264PROFILE_BASELINE;
        case V4L2_MPEG_VIDEO_H264_PROFILE_MAIN:
          return H264PROFILE_MAIN;
        case V4L2_MPEG_VIDEO_H264_PROFILE_EXTENDED:
          return H264PROFILE_EXTENDED;
        case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH:
          return H264PROFILE_HIGH;
      }
      break;
    case V4L2_CID_MPEG_VIDEO_VP8_PROFILE:
      switch (v4l2_profile) {
        case V4L2_MPEG_VIDEO_VP8_PROFILE_0:
        case V4L2_MPEG_VIDEO_VP8_PROFILE_1:
        case V4L2_MPEG_VIDEO_VP8_PROFILE_2:
        case V4L2_MPEG_VIDEO_VP8_PROFILE_3:
          return VP8PROFILE_ANY;
      }
      break;
    case V4L2_CID_MPEG_VIDEO_VP9_PROFILE:
      switch (v4l2_profile) {
        // VP9 Profile 1 and 3 are not tested and the use is minuscule, skip.
        case V4L2_MPEG_VIDEO_VP9_PROFILE_0:
          return VP9PROFILE_PROFILE0;
        case V4L2_MPEG_VIDEO_VP9_PROFILE_2:
          // TODO(b/250698011): Support Profile 2 when launched
          return VIDEO_CODEC_PROFILE_UNKNOWN;
      }
      break;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
      switch (v4l2_profile) {
        case V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN:
          return HEVCPROFILE_MAIN;
        case V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_STILL_PICTURE:
          return HEVCPROFILE_MAIN_STILL_PICTURE;
        case V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10:
          return HEVCPROFILE_MAIN10;
      }
      break;
#endif
#if BUILDFLAG(IS_CHROMEOS)
    case V4L2_CID_MPEG_VIDEO_AV1_PROFILE:
      switch (v4l2_profile) {
        case V4L2_MPEG_VIDEO_AV1_PROFILE_MAIN:
          return AV1PROFILE_PROFILE_MAIN;
        case V4L2_MPEG_VIDEO_AV1_PROFILE_HIGH:
          return AV1PROFILE_PROFILE_HIGH;
        case V4L2_MPEG_VIDEO_AV1_PROFILE_PROFESSIONAL:
          return AV1PROFILE_PROFILE_PRO;
      }
      break;
#endif
  }
  return VIDEO_CODEC_PROFILE_UNKNOWN;
}

size_t GetNumPlanesOfV4L2PixFmt(uint32_t pix_fmt) {
  absl::optional<Fourcc> fourcc = Fourcc::FromV4L2PixFmt(pix_fmt);
  if (fourcc && fourcc->IsMultiPlanar()) {
    return VideoFrame::NumPlanes(fourcc->ToVideoPixelFormat());
  }
  return 1u;
}

absl::optional<VideoFrameLayout> V4L2FormatToVideoFrameLayout(
    const struct v4l2_format& format) {
  if (!V4L2_TYPE_IS_MULTIPLANAR(format.type)) {
    VLOGF(1) << "v4l2_buf_type is not multiplanar: " << std::hex << "0x"
             << format.type;
    return absl::nullopt;
  }
  const v4l2_pix_format_mplane& pix_mp = format.fmt.pix_mp;
  const uint32_t& pix_fmt = pix_mp.pixelformat;
  const auto video_fourcc = Fourcc::FromV4L2PixFmt(pix_fmt);
  if (!video_fourcc) {
    VLOGF(1) << "Failed to convert pixel format to VideoPixelFormat: "
             << FourccToString(pix_fmt);
    return absl::nullopt;
  }
  const VideoPixelFormat video_format = video_fourcc->ToVideoPixelFormat();
  const size_t num_buffers = pix_mp.num_planes;
  const size_t num_color_planes = VideoFrame::NumPlanes(video_format);
  if (num_color_planes == 0) {
    VLOGF(1) << "Unsupported video format for NumPlanes(): "
             << VideoPixelFormatToString(video_format);
    return absl::nullopt;
  }
  if (num_buffers > num_color_planes) {
    VLOGF(1) << "pix_mp.num_planes: " << num_buffers
             << " should not be larger than NumPlanes("
             << VideoPixelFormatToString(video_format)
             << "): " << num_color_planes;
    return absl::nullopt;
  }
  // Reserve capacity in advance to prevent unnecessary vector reallocation.
  std::vector<ColorPlaneLayout> planes;
  planes.reserve(num_color_planes);
  for (size_t i = 0; i < num_buffers; ++i) {
    const v4l2_plane_pix_format& plane_format = pix_mp.plane_fmt[i];
    planes.emplace_back(static_cast<int32_t>(plane_format.bytesperline), 0u,
                        plane_format.sizeimage);
  }
  // For the case that #color planes > #buffers, it fills stride of color
  // plane which does not map to buffer.
  // Right now only some pixel formats are supported: NV12, YUV420, YVU420.
  if (num_color_planes > num_buffers) {
    const int32_t y_stride = planes[0].stride;
    // Note that y_stride is from v4l2 bytesperline and its type is uint32_t.
    // It is safe to cast to size_t.
    const size_t y_stride_abs = static_cast<size_t>(y_stride);
    switch (pix_fmt) {
      case V4L2_PIX_FMT_NV12:
        // The stride of UV is the same as Y in NV12.
        // The height is half of Y plane.
        planes.emplace_back(y_stride, y_stride_abs * pix_mp.height,
                            y_stride_abs * pix_mp.height / 2);
        DCHECK_EQ(2u, planes.size());
        break;
      case V4L2_PIX_FMT_YUV420:
      case V4L2_PIX_FMT_YVU420: {
        // The spec claims that two Cx rows (including padding) is exactly as
        // long as one Y row (including padding). So stride of Y must be even
        // number.
        if (y_stride % 2 != 0 || pix_mp.height % 2 != 0) {
          VLOGF(1) << "Plane-Y stride and height should be even; stride: "
                   << y_stride << ", height: " << pix_mp.height;
          return absl::nullopt;
        }
        const int32_t half_stride = y_stride / 2;
        const size_t plane_0_area = y_stride_abs * pix_mp.height;
        const size_t plane_1_area = plane_0_area / 4;
        planes.emplace_back(half_stride, plane_0_area, plane_1_area);
        planes.emplace_back(half_stride, plane_0_area + plane_1_area,
                            plane_1_area);
        DCHECK_EQ(3u, planes.size());
        break;
      }
      default:
        VLOGF(1) << "Cannot derive stride for each plane for pixel format "
                 << FourccToString(pix_fmt);
        return absl::nullopt;
    }
  }

  // Some V4L2 devices expect buffers to be page-aligned. We cannot detect
  // such devices individually, so set this as a video frame layout property.
  constexpr size_t buffer_alignment = 0x1000;
  if (num_buffers == 1) {
    return VideoFrameLayout::CreateWithPlanes(
        video_format, gfx::Size(pix_mp.width, pix_mp.height), std::move(planes),
        buffer_alignment);
  } else {
    return VideoFrameLayout::CreateMultiPlanar(
        video_format, gfx::Size(pix_mp.width, pix_mp.height), std::move(planes),
        buffer_alignment);
  }
}

namespace {
using v4l2_enum_type = decltype(V4L2_PIX_FMT_H264);
// Correspondence from V4L2 codec described as a pixel format to a Control ID.
static const std::map<v4l2_enum_type, v4l2_enum_type>
    kV4L2CodecPixFmtToProfileCID = {
        {V4L2_PIX_FMT_H264, V4L2_CID_MPEG_VIDEO_H264_PROFILE},
        {V4L2_PIX_FMT_H264_SLICE, V4L2_CID_MPEG_VIDEO_H264_PROFILE},
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        {V4L2_PIX_FMT_HEVC, V4L2_CID_MPEG_VIDEO_HEVC_PROFILE},
        {V4L2_PIX_FMT_HEVC_SLICE, V4L2_CID_MPEG_VIDEO_HEVC_PROFILE},
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        {V4L2_PIX_FMT_VP8, V4L2_CID_MPEG_VIDEO_VP8_PROFILE},
        {V4L2_PIX_FMT_VP8_FRAME, V4L2_CID_MPEG_VIDEO_VP8_PROFILE},
        {V4L2_PIX_FMT_VP9, V4L2_CID_MPEG_VIDEO_VP9_PROFILE},
        {V4L2_PIX_FMT_VP9_FRAME, V4L2_CID_MPEG_VIDEO_VP9_PROFILE},
#if BUILDFLAG(IS_CHROMEOS)
        {V4L2_PIX_FMT_AV1, V4L2_CID_MPEG_VIDEO_AV1_PROFILE},
        {V4L2_PIX_FMT_AV1_FRAME, V4L2_CID_MPEG_VIDEO_AV1_PROFILE},
#endif
};

// Default VideoCodecProfiles associated to a V4L2 Codec Control ID.
static const std::map<v4l2_enum_type, std::vector<VideoCodecProfile>>
    kDefaultVideoCodecProfilesForProfileCID = {
        {V4L2_CID_MPEG_VIDEO_H264_PROFILE,
         {
             H264PROFILE_BASELINE,
             H264PROFILE_MAIN,
             H264PROFILE_HIGH,
         }},
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        {V4L2_CID_MPEG_VIDEO_HEVC_PROFILE, {HEVCPROFILE_MAIN}},
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        {V4L2_CID_MPEG_VIDEO_VP8_PROFILE, {VP8PROFILE_ANY}},
        {V4L2_CID_MPEG_VIDEO_VP9_PROFILE, {VP9PROFILE_PROFILE0}},
#if BUILDFLAG(IS_CHROMEOS)
        {V4L2_CID_MPEG_VIDEO_AV1_PROFILE, {AV1PROFILE_PROFILE_MAIN}},
#endif
};

// Correspondence from a VideoCodecProfiles to V4L2 codec described
// as a pixel format.
static const std::map<VideoCodecProfile,
                      std::pair<v4l2_enum_type, v4l2_enum_type>>
    kVideoCodecProfileToV4L2CodecPixFmt = {
        {H264PROFILE_BASELINE, MAKE_V4L2_CODEC_PAIR(V4L2_PIX_FMT_H264, SLICE)},
        {H264PROFILE_MAIN, MAKE_V4L2_CODEC_PAIR(V4L2_PIX_FMT_H264, SLICE)},
        {H264PROFILE_HIGH, MAKE_V4L2_CODEC_PAIR(V4L2_PIX_FMT_H264, SLICE)},
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        {HEVCPROFILE_MAIN, MAKE_V4L2_CODEC_PAIR(V4L2_PIX_FMT_HEVC, SLICE)},
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        {VP8PROFILE_ANY, MAKE_V4L2_CODEC_PAIR(V4L2_PIX_FMT_VP8, FRAME)},
        {VP9PROFILE_PROFILE0, MAKE_V4L2_CODEC_PAIR(V4L2_PIX_FMT_VP9, FRAME)},
#if BUILDFLAG(IS_CHROMEOS)
        {AV1PROFILE_PROFILE_MAIN,
         MAKE_V4L2_CODEC_PAIR(V4L2_PIX_FMT_AV1, FRAME)},
#endif
};

}  // namespace

std::vector<VideoCodecProfile> EnumerateSupportedProfilesForV4L2Codec(
    const IoctlAsCallback& ioctl_cb,
    uint32_t codec_as_pix_fmt) {
  if (!base::Contains(kV4L2CodecPixFmtToProfileCID, codec_as_pix_fmt)) {
    // This is OK: there are many codecs that are not supported by Chrome.
    VLOGF(4) << "Unsupported codec: " << FourccToString(codec_as_pix_fmt);
    return {};
  }
  const auto profile_cid = kV4L2CodecPixFmtToProfileCID.at(codec_as_pix_fmt);

  v4l2_queryctrl query_ctrl = {.id = static_cast<__u32>(profile_cid)};
  if (ioctl_cb.Run(VIDIOC_QUERYCTRL, &query_ctrl) != kIoctlOk) {
    // This happens for example for VP8 on Hana MTK8173, or for HEVC on Trogdor
    // QC SC7180) at the time of writing.
    DVLOGF(4) << "Driver doesn't support enumerating "
              << FourccToString(codec_as_pix_fmt)
              << " profiles, using default ones.";
    DCHECK(
        base::Contains(kDefaultVideoCodecProfilesForProfileCID, profile_cid));
    return kDefaultVideoCodecProfilesForProfileCID.at(profile_cid);
  }

  std::vector<VideoCodecProfile> profiles;

  v4l2_querymenu query_menu = {.id = query_ctrl.id,
                               .index = static_cast<__u32>(query_ctrl.minimum)};
  for (; static_cast<int>(query_menu.index) <= query_ctrl.maximum;
       query_menu.index++) {
    if (ioctl_cb.Run(VIDIOC_QUERYMENU, &query_menu) != kIoctlOk) {
      continue;
    }
    const VideoCodecProfile profile =
        V4L2ProfileToVideoCodecProfile(profile_cid, query_menu.index);
    DVLOGF_IF(4, profile == VIDEO_CODEC_PROFILE_UNKNOWN)
        << "Profile: " << query_menu.name
        << " not supported by Chrome, skipping.";

    if (profile != VIDEO_CODEC_PROFILE_UNKNOWN) {
      profiles.push_back(profile);
      DVLOGF(4) << "Found supported profile: " << query_menu.name;
    }
  }

  // Erase duplicated profiles. This is needed because H264PROFILE_BASELINE maps
  // to both V4L2_MPEG_VIDEO_H264_PROFILE__BASELINE/CONSTRAINED_BASELINE
  base::ranges::sort(profiles);
  profiles.erase(base::ranges::unique(profiles), profiles.end());
  return profiles;
}

std::vector<uint32_t> EnumerateSupportedPixFmts(const IoctlAsCallback& ioctl_cb,
                                                v4l2_buf_type buf_type) {
  DCHECK(buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ||
         buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);

  std::vector<v4l2_enum_type> pix_fmts;
  v4l2_fmtdesc fmtdesc = {.type = buf_type};
  for (; ioctl_cb.Run(VIDIOC_ENUM_FMT, &fmtdesc) == kIoctlOk; ++fmtdesc.index) {
    DVLOGF(4) << "Enumerated "
              << (buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE
                      ? "codec: "
                      : "pixel format: ")
              << FourccToString(fmtdesc.pixelformat) << " ("
              << fmtdesc.description << ")";
    pix_fmts.push_back(fmtdesc.pixelformat);
  }

  return pix_fmts;
}

void GetSupportedResolution(const IoctlAsCallback& ioctl_cb,
                            uint32_t pixelformat,
                            gfx::Size* min_resolution,
                            gfx::Size* max_resolution) {
  constexpr gfx::Size kDefaultMaxCodedSize(1920, 1088);
  *max_resolution = kDefaultMaxCodedSize;
  constexpr gfx::Size kDefaultMinCodedSize(16, 16);
  *min_resolution = kDefaultMinCodedSize;

  v4l2_frmsizeenum frame_size;
  memset(&frame_size, 0, sizeof(frame_size));
  frame_size.pixel_format = pixelformat;
  if (ioctl_cb.Run(VIDIOC_ENUM_FRAMESIZES, &frame_size) == kIoctlOk) {
    if (frame_size.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
      max_resolution->SetSize(frame_size.stepwise.max_width,
                              frame_size.stepwise.max_height);
      min_resolution->SetSize(frame_size.stepwise.min_width,
                              frame_size.stepwise.min_height);
    } else {
#if BUILDFLAG(IS_CHROMEOS)
      // All of Chrome-supported implementations support STEPWISE only.
      CHECK_EQ(frame_size.type, V4L2_FRMSIZE_TYPE_STEPWISE);
#endif
    }
  } else {
    DLOGF(INFO) << "VIDIOC_ENUM_FRAMESIZES failed, using default values";
  }
}

uint32_t VideoCodecProfileToV4L2PixFmt(VideoCodecProfile profile,
                                       bool slice_based) {
  CHECK(base::Contains(kVideoCodecProfileToV4L2CodecPixFmt, profile))
      << "Unsupported profile: " << GetProfileName(profile);

  const auto& v4l2_pix_fmt = kVideoCodecProfileToV4L2CodecPixFmt.at(profile);
  return slice_based ? v4l2_pix_fmt.first : v4l2_pix_fmt.second;
}

base::TimeDelta TimeValToTimeDelta(const struct timeval& timeval) {
  struct timespec ts;
  const struct timeval temp_timeval = timeval;
  TIMEVAL_TO_TIMESPEC(&temp_timeval, &ts);
  return base::TimeDelta::FromTimeSpec(ts);
}

struct timeval TimeDeltaToTimeVal(base::TimeDelta time_delta) {
  return base::Time::FromTimeSpec(time_delta.ToTimeSpec()).ToTimeVal();
}

}  // namespace media
