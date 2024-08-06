// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_VIDEO_FRAME_MOJOM_TRAITS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_VIDEO_FRAME_MOJOM_TRAITS_H_

#include "base/memory/ref_counted.h"
#include "gpu/ipc/common/mailbox_holder_mojom_traits.h"
#include "gpu/ipc/common/vulkan_ycbcr_info_mojom_traits.h"
#include "third_party/chromium/media/base/ipc/media_param_traits_macros.h"
#include "third_party/chromium/media/base/video_frame.h"
#include "third_party/chromium/media/mojo/mojom/media_types.mojom.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"

namespace mojo {

template <>
struct StructTraits<media_m96::mojom::VideoFrameDataView,
                    scoped_refptr<media_m96::VideoFrame>> {
  static bool IsNull(const scoped_refptr<media_m96::VideoFrame>& input) {
    return !input;
  }

  static void SetToNull(scoped_refptr<media_m96::VideoFrame>* input) {
    *input = nullptr;
  }

  static media_m96::VideoPixelFormat format(
      const scoped_refptr<media_m96::VideoFrame>& input) {
    return input->format();
  }

  static const gfx::Size& coded_size(
      const scoped_refptr<media_m96::VideoFrame>& input) {
    return input->coded_size();
  }

  static const gfx::Rect& visible_rect(
      const scoped_refptr<media_m96::VideoFrame>& input) {
    return input->visible_rect();
  }

  static const gfx::Size& natural_size(
      const scoped_refptr<media_m96::VideoFrame>& input) {
    return input->natural_size();
  }

  static base::TimeDelta timestamp(
      const scoped_refptr<media_m96::VideoFrame>& input) {
    return input->timestamp();
  }

  // TODO(hubbe): Return const ref when VideoFrame::ColorSpace()
  // returns const ref.
  static gfx::ColorSpace color_space(
      const scoped_refptr<media_m96::VideoFrame>& input) {
    return input->ColorSpace();
  }

  static const absl::optional<gfx::HDRMetadata>& hdr_metadata(
      const scoped_refptr<media_m96::VideoFrame>& input) {
    return input->hdr_metadata();
  }

  static const absl::optional<gpu::VulkanYCbCrInfo>& ycbcr_info(
      const scoped_refptr<media_m96::VideoFrame>& input) {
    return input->ycbcr_info();
  }

  static media_m96::mojom::VideoFrameDataPtr data(
      const scoped_refptr<media_m96::VideoFrame>& input);

  // TODO(https://crbug.com/1096727): Change VideoFrame::Metadata() to return a
  // const &.
  static const media_m96::VideoFrameMetadata& metadata(
      const scoped_refptr<media_m96::VideoFrame>& input) {
    return input->metadata();
  }

  static bool Read(media_m96::mojom::VideoFrameDataView input,
                   scoped_refptr<media_m96::VideoFrame>* output);
};

}  // namespace mojo

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_VIDEO_FRAME_MOJOM_TRAITS_H_
