// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_MOJOM_VIDEO_CAPTURE_TYPES_MOJOM_TRAITS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_MOJOM_VIDEO_CAPTURE_TYPES_MOJOM_TRAITS_H_

#include "third_party/chromium/media/base/video_facing.h"
#include "third_party/chromium/media/capture/mojom/video_capture_types.mojom-shared.h"
#include "third_party/chromium/media/capture/video/video_capture_device_descriptor.h"
#include "third_party/chromium/media/capture/video/video_capture_device_info.h"
#include "third_party/chromium/media/capture/video/video_capture_feedback.h"
#include "third_party/chromium/media/capture/video_capture_types.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    EnumTraits<media_m96::mojom::ResolutionChangePolicy,
               media_m96::ResolutionChangePolicy> {
  static media_m96::mojom::ResolutionChangePolicy ToMojom(
      media_m96::ResolutionChangePolicy policy);

  static bool FromMojom(media_m96::mojom::ResolutionChangePolicy input,
                        media_m96::ResolutionChangePolicy* out);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    EnumTraits<media_m96::mojom::PowerLineFrequency, media_m96::PowerLineFrequency> {
  static media_m96::mojom::PowerLineFrequency ToMojom(
      media_m96::PowerLineFrequency frequency);

  static bool FromMojom(media_m96::mojom::PowerLineFrequency input,
                        media_m96::PowerLineFrequency* out);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    EnumTraits<media_m96::mojom::VideoCapturePixelFormat, media_m96::VideoPixelFormat> {
  static media_m96::mojom::VideoCapturePixelFormat ToMojom(
      media_m96::VideoPixelFormat input);
  static bool FromMojom(media_m96::mojom::VideoCapturePixelFormat input,
                        media_m96::VideoPixelFormat* output);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    EnumTraits<media_m96::mojom::VideoCaptureBufferType,
               media_m96::VideoCaptureBufferType> {
  static media_m96::mojom::VideoCaptureBufferType ToMojom(
      media_m96::VideoCaptureBufferType buffer_type);

  static bool FromMojom(media_m96::mojom::VideoCaptureBufferType input,
                        media_m96::VideoCaptureBufferType* out);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    EnumTraits<media_m96::mojom::VideoCaptureError, media_m96::VideoCaptureError> {
  static media_m96::mojom::VideoCaptureError ToMojom(
      media_m96::VideoCaptureError buffer_type);

  static bool FromMojom(media_m96::mojom::VideoCaptureError input,
                        media_m96::VideoCaptureError* out);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    EnumTraits<media_m96::mojom::VideoCaptureFrameDropReason,
               media_m96::VideoCaptureFrameDropReason> {
  static media_m96::mojom::VideoCaptureFrameDropReason ToMojom(
      media_m96::VideoCaptureFrameDropReason buffer_type);

  static bool FromMojom(media_m96::mojom::VideoCaptureFrameDropReason input,
                        media_m96::VideoCaptureFrameDropReason* out);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    EnumTraits<media_m96::mojom::VideoFacingMode, media_m96::VideoFacingMode> {
  static media_m96::mojom::VideoFacingMode ToMojom(media_m96::VideoFacingMode input);
  static bool FromMojom(media_m96::mojom::VideoFacingMode input,
                        media_m96::VideoFacingMode* output);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    EnumTraits<media_m96::mojom::VideoCaptureApi, media_m96::VideoCaptureApi> {
  static media_m96::mojom::VideoCaptureApi ToMojom(media_m96::VideoCaptureApi input);
  static bool FromMojom(media_m96::mojom::VideoCaptureApi input,
                        media_m96::VideoCaptureApi* output);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    EnumTraits<media_m96::mojom::VideoCaptureTransportType,
               media_m96::VideoCaptureTransportType> {
  static media_m96::mojom::VideoCaptureTransportType ToMojom(
      media_m96::VideoCaptureTransportType input);
  static bool FromMojom(media_m96::mojom::VideoCaptureTransportType input,
                        media_m96::VideoCaptureTransportType* output);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    StructTraits<media_m96::mojom::VideoCaptureControlSupportDataView,
                 media_m96::VideoCaptureControlSupport> {
  static bool pan(const media_m96::VideoCaptureControlSupport& input) {
    return input.pan;
  }

  static bool tilt(const media_m96::VideoCaptureControlSupport& input) {
    return input.tilt;
  }

  static bool zoom(const media_m96::VideoCaptureControlSupport& input) {
    return input.zoom;
  }

  static bool Read(media_m96::mojom::VideoCaptureControlSupportDataView data,
                   media_m96::VideoCaptureControlSupport* out);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    StructTraits<media_m96::mojom::VideoCaptureFormatDataView,
                 media_m96::VideoCaptureFormat> {
  static const gfx::Size& frame_size(const media_m96::VideoCaptureFormat& format) {
    return format.frame_size;
  }

  static float frame_rate(const media_m96::VideoCaptureFormat& format) {
    return format.frame_rate;
  }

  static media_m96::VideoPixelFormat pixel_format(
      const media_m96::VideoCaptureFormat& format) {
    return format.pixel_format;
  }

  static bool Read(media_m96::mojom::VideoCaptureFormatDataView data,
                   media_m96::VideoCaptureFormat* out);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    StructTraits<media_m96::mojom::VideoCaptureParamsDataView,
                 media_m96::VideoCaptureParams> {
  static media_m96::VideoCaptureFormat requested_format(
      const media_m96::VideoCaptureParams& params) {
    return params.requested_format;
  }

  static media_m96::VideoCaptureBufferType buffer_type(
      const media_m96::VideoCaptureParams& params) {
    return params.buffer_type;
  }

  static media_m96::ResolutionChangePolicy resolution_change_policy(
      const media_m96::VideoCaptureParams& params) {
    return params.resolution_change_policy;
  }

  static media_m96::PowerLineFrequency power_line_frequency(
      const media_m96::VideoCaptureParams& params) {
    return params.power_line_frequency;
  }

  static bool enable_face_detection(
      const media_m96::VideoCaptureParams& params) {
    return params.enable_face_detection;
  }

  static bool Read(media_m96::mojom::VideoCaptureParamsDataView data,
                   media_m96::VideoCaptureParams* out);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    StructTraits<media_m96::mojom::VideoCaptureDeviceDescriptorDataView,
                 media_m96::VideoCaptureDeviceDescriptor> {
  static const std::string& display_name(
      const media_m96::VideoCaptureDeviceDescriptor& input) {
    return input.display_name();
  }

  static const std::string& device_id(
      const media_m96::VideoCaptureDeviceDescriptor& input) {
    return input.device_id;
  }

  static const std::string& model_id(
      const media_m96::VideoCaptureDeviceDescriptor& input) {
    return input.model_id;
  }

  static media_m96::VideoFacingMode facing_mode(
      const media_m96::VideoCaptureDeviceDescriptor& input) {
    return input.facing;
  }

  static media_m96::VideoCaptureApi capture_api(
      const media_m96::VideoCaptureDeviceDescriptor& input) {
    return input.capture_api;
  }

  static media_m96::VideoCaptureControlSupport control_support(
      const media_m96::VideoCaptureDeviceDescriptor& input) {
    return input.control_support();
  }

  static media_m96::VideoCaptureTransportType transport_type(
      const media_m96::VideoCaptureDeviceDescriptor& input) {
    return input.transport_type;
  }

  static bool Read(media_m96::mojom::VideoCaptureDeviceDescriptorDataView data,
                   media_m96::VideoCaptureDeviceDescriptor* output);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    StructTraits<media_m96::mojom::VideoCaptureDeviceInfoDataView,
                 media_m96::VideoCaptureDeviceInfo> {
  static const media_m96::VideoCaptureDeviceDescriptor& descriptor(
      const media_m96::VideoCaptureDeviceInfo& input) {
    return input.descriptor;
  }

  static const std::vector<media_m96::VideoCaptureFormat>& supported_formats(
      const media_m96::VideoCaptureDeviceInfo& input) {
    return input.supported_formats;
  }

  static bool Read(media_m96::mojom::VideoCaptureDeviceInfoDataView data,
                   media_m96::VideoCaptureDeviceInfo* output);
};

template <>
struct COMPONENT_EXPORT(MEDIA_CAPTURE_MOJOM_TRAITS)
    StructTraits<media_m96::mojom::VideoCaptureFeedbackDataView,
                 media_m96::VideoCaptureFeedback> {
  static double resource_utilization(
      const media_m96::VideoCaptureFeedback& feedback) {
    return feedback.resource_utilization;
  }

  static float max_framerate_fps(const media_m96::VideoCaptureFeedback& feedback) {
    return feedback.max_framerate_fps;
  }

  static int max_pixels(const media_m96::VideoCaptureFeedback& feedback) {
    return feedback.max_pixels;
  }

  static bool require_mapped_frame(
      const media_m96::VideoCaptureFeedback& feedback) {
    return feedback.require_mapped_frame;
  }

  static const std::vector<gfx::Size>& mapped_sizes(
      const media_m96::VideoCaptureFeedback& feedback) {
    return feedback.mapped_sizes;
  }

  static bool Read(media_m96::mojom::VideoCaptureFeedbackDataView data,
                   media_m96::VideoCaptureFeedback* output);
};
}  // namespace mojo

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_MOJOM_VIDEO_CAPTURE_TYPES_MOJOM_TRAITS_H_
