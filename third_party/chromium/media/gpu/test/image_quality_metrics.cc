// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/gpu/test/video_frame_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv/compare.h"
#include "ui/gfx/geometry/point.h"

#define ASSERT_TRUE_OR_RETURN(predicate, return_value) \
  do {                                                 \
    if (!(predicate)) {                                \
      ADD_FAILURE();                                   \
      return (return_value);                           \
    }                                                  \
  } while (0)

namespace media {
namespace test {
namespace {
// The metrics of the similarity of two images.
enum SimilarityMetrics {
  PSNR,  // Peak Signal-to-Noise Ratio. For detail see
         // https://en.wikipedia.org/wiki/Peak_signal-to-noise_ratio
  SSIM,  // Structural Similarity. For detail see
         // https://en.wikipedia.org/wiki/Structural_similarity
};

// Computes the SSIM of a window of 8x8 samples between two planes where each
// sample is 16 bits. This is modeled after libyuv::Ssim8x8_C().
double SSIM16BitPlane8x8(const uint8_t* src_a,
                         int stride_a,
                         const uint8_t* src_b,
                         int stride_b) {
  int64_t sum_a = 0;
  int64_t sum_b = 0;
  int64_t sum_sq_a = 0;
  int64_t sum_sq_b = 0;
  int64_t sum_axb = 0;
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 8; ++j) {
      // Read 16 bits and store it in a 32 bits value to avoid overflow in the
      // following calculations.
      const uint32_t a = static_cast<uint32_t>(
          *reinterpret_cast<const uint16_t*>(src_a + 2 * j));
      const uint32_t b = static_cast<uint32_t>(
          *reinterpret_cast<const uint16_t*>(src_b + 2 * j));
      sum_a += a;
      sum_b += b;
      sum_sq_a += a * a;
      sum_sq_b += b * b;
      sum_axb += a * b;
    }

    src_a += stride_a;
    src_b += stride_b;
  }

  constexpr int64_t count = 64;
  constexpr int64_t cc1 = 1759164917;   // (64^2*(.01*65535)^2
  constexpr int64_t cc2 = 15832484259;  // (64^2*(.03*65535)^2
  constexpr int64_t c1 = (cc1 * count * count) >> 12;
  constexpr int64_t c2 = (cc2 * count * count) >> 12;
  const int64_t sum_a_x_sum_b = sum_a * sum_b;
  const int64_t ssim_n =
      (2 * sum_a_x_sum_b + c1) * (2 * count * sum_axb - 2 * sum_a_x_sum_b + c2);
  const int64_t sum_a_sq = sum_a * sum_a;
  const int64_t sum_b_sq = sum_b * sum_b;
  const int64_t ssim_d =
      (sum_a_sq + sum_b_sq + c1) *
      (count * sum_sq_a - sum_a_sq + count * sum_sq_b - sum_b_sq + c2);

  if (ssim_d == 0)
    return std::numeric_limits<double>::max();
  return ssim_n * 1.0 / ssim_d;
}

// Computes the SSIM between two planes where each sample is 16 bits. This is
// modeled after libyuv::CalcFrameSsim().
double Calc16bitPlaneSSIM(const uint8_t* src_a,
                          int stride_a,
                          const uint8_t* src_b,
                          int stride_b,
                          int width,
                          int height) {
  int samples = 0;
  double ssim_total = 0;
  for (int i = 0; i < height - 8; i += 4) {
    for (int j = 0; j < width - 8; j += 4) {
      // Double |j| because the color depth is 16 bits.
      ssim_total +=
          SSIM16BitPlane8x8(src_a + 2 * j, stride_a, src_b + 2 * j, stride_b);
      samples++;
    }
    // |stride_a| and |stride_b| are bytes. No need to double them.
    src_a += stride_a * 4;
    src_b += stride_b * 4;
  }

  ssim_total /= samples;
  return ssim_total;
}

// Computes the SSIM between two YUV420P010 buffers. This is modeled after
// libyuv::I420Ssim().
double ComputeYUV420P10SSIM(const uint8_t* src_y_a,
                            int stride_y_a,
                            const uint8_t* src_u_a,
                            int stride_u_a,
                            const uint8_t* src_v_a,
                            int stride_v_a,
                            const uint8_t* src_y_b,
                            int stride_y_b,
                            const uint8_t* src_u_b,
                            int stride_u_b,
                            const uint8_t* src_v_b,
                            int stride_v_b,
                            int width,
                            int height) {
  const double ssim_y = Calc16bitPlaneSSIM(src_y_a, stride_y_a, src_y_b,
                                           stride_y_b, width, height);
  const int width_uv = (width + 1) >> 1;
  const int height_uv = (height + 1) >> 1;
  const double ssim_u = Calc16bitPlaneSSIM(src_u_a, stride_u_a, src_u_b,
                                           stride_u_b, width_uv, height_uv);
  const double ssim_v = Calc16bitPlaneSSIM(src_v_a, stride_v_a, src_v_b,
                                           stride_v_b, width_uv, height_uv);
  return ssim_y * 0.8 + 0.1 * (ssim_u + ssim_v);
}

double ComputeSimilarity(const VideoFrame* frame1,
                         const VideoFrame* frame2,
                         SimilarityMetrics mode) {
  ASSERT_TRUE_OR_RETURN(
      frame1->IsMappable() && frame2->IsMappable(),
      static_cast<double>(std::numeric_limits<std::size_t>::max()));
  ASSERT_TRUE_OR_RETURN(
      frame1->visible_rect().size() == frame2->visible_rect().size(),
      static_cast<double>(std::numeric_limits<std::size_t>::max()));
  // Ideally, frame1->BitDepth() should be the same as frame2->BitDepth()
  // always. But in the 10 bit case, the 10 bit frame can be carried with P016LE
  // whose bit depth is regarded to be 16. This is due to a lack of NV12 10-bit
  // buffer format in media::VideoPixelFormat. As a workaround for this, we
  // determine the common bit depth as the smaller one.
  ASSERT_TRUE_OR_RETURN(
      (frame1->BitDepth() == 8 && frame1->BitDepth() == frame2->BitDepth()) ||
          std::min(frame1->BitDepth(), frame2->BitDepth()) == 10,
      static_cast<double>(std::numeric_limits<std::size_t>::max()));
  const size_t bit_depth = std::min(frame1->BitDepth(), frame2->BitDepth());
  const VideoPixelFormat common_format =
      bit_depth == 8 ? PIXEL_FORMAT_I420 : PIXEL_FORMAT_YUV420P10;

  // These are used, only if frames are converted to |common_format|, for
  // keeping converted frames alive until the end of function.
  scoped_refptr<VideoFrame> converted_frame1;
  scoped_refptr<VideoFrame> converted_frame2;

  if (frame1->format() != common_format) {
    converted_frame1 = ConvertVideoFrame(frame1, common_format);
    frame1 = converted_frame1.get();
  }

  if (frame2->format() != common_format) {
    converted_frame2 = ConvertVideoFrame(frame2, common_format);
    frame2 = converted_frame2.get();
  }

  decltype(&libyuv::I420Psnr) metric_func = nullptr;
  switch (mode) {
    case SimilarityMetrics::PSNR:
      if (bit_depth == 8)
        metric_func = &libyuv::I420Psnr;
      break;
    case SimilarityMetrics::SSIM:
      if (bit_depth == 8)
        metric_func = &libyuv::I420Ssim;
      else if (bit_depth == 10)
        metric_func = &ComputeYUV420P10SSIM;
      break;
  }
  ASSERT_TRUE_OR_RETURN(metric_func, std::numeric_limits<double>::max());

  return metric_func(
      frame1->visible_data(0), frame1->stride(0), frame1->visible_data(1),
      frame1->stride(1), frame1->visible_data(2), frame1->stride(2),
      frame2->visible_data(0), frame2->stride(0), frame2->visible_data(1),
      frame2->stride(1), frame2->visible_data(2), frame2->stride(2),
      frame1->visible_rect().width(), frame1->visible_rect().height());
}
}  // namespace

size_t CompareFramesWithErrorDiff(const VideoFrame& frame1,
                                  const VideoFrame& frame2,
                                  uint8_t tolerance) {
  ASSERT_TRUE_OR_RETURN(frame1.IsMappable() && frame2.IsMappable(),
                        std::numeric_limits<std::size_t>::max());
  ASSERT_TRUE_OR_RETURN(frame1.format() == frame2.format(),
                        std::numeric_limits<std::size_t>::max());
  ASSERT_TRUE_OR_RETURN(
      frame1.visible_rect().size() == frame2.visible_rect().size(),
      std::numeric_limits<std::size_t>::max());
  size_t diff_cnt = 0;

  const VideoPixelFormat format = frame1.format();
  const size_t num_planes = VideoFrame::NumPlanes(format);
  const gfx::Size& visible_size = frame1.visible_rect().size();
  for (size_t i = 0; i < num_planes; ++i) {
    const uint8_t* data1 = frame1.visible_data(i);
    const int stride1 = frame1.stride(i);
    const uint8_t* data2 = frame2.visible_data(i);
    const int stride2 = frame2.stride(i);
    const size_t rows = VideoFrame::Rows(i, format, visible_size.height());
    const int row_bytes = VideoFrame::RowBytes(i, format, visible_size.width());
    for (size_t r = 0; r < rows; ++r) {
      for (int c = 0; c < row_bytes; c++) {
        uint8_t b1 = data1[(stride1 * r) + c];
        uint8_t b2 = data2[(stride2 * r) + c];
        uint8_t diff = std::max(b1, b2) - std::min(b1, b2);
        diff_cnt += diff > tolerance;
      }
    }
  }
  return diff_cnt;
}

double ComputePSNR(const VideoFrame& frame1, const VideoFrame& frame2) {
  return ComputeSimilarity(&frame1, &frame2, SimilarityMetrics::PSNR);
}

double ComputeSSIM(const VideoFrame& frame1, const VideoFrame& frame2) {
  return ComputeSimilarity(&frame1, &frame2, SimilarityMetrics::SSIM);
}
}  // namespace test
}  // namespace media
