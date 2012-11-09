// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"

#include "base/format_macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "media/base/buffers.h"
#include "media/base/yuv_convert.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using base::MD5DigestToBase16;

// Helper function that initializes a YV12 frame with white and black scan
// lines based on the |white_to_black| parameter.  If 0, then the entire
// frame will be black, if 1 then the entire frame will be white.
void InitializeYV12Frame(VideoFrame* frame, double white_to_black) {
  EXPECT_EQ(VideoFrame::YV12, frame->format());
  int first_black_row = static_cast<int>(frame->coded_size().height() *
                                         white_to_black);
  uint8* y_plane = frame->data(VideoFrame::kYPlane);
  for (int row = 0; row < frame->coded_size().height(); ++row) {
    int color = (row < first_black_row) ? 0xFF : 0x00;
    memset(y_plane, color, frame->stride(VideoFrame::kYPlane));
    y_plane += frame->stride(VideoFrame::kYPlane);
  }
  uint8* u_plane = frame->data(VideoFrame::kUPlane);
  uint8* v_plane = frame->data(VideoFrame::kVPlane);
  for (int row = 0; row < frame->coded_size().height(); row += 2) {
    memset(u_plane, 0x80, frame->stride(VideoFrame::kUPlane));
    memset(v_plane, 0x80, frame->stride(VideoFrame::kVPlane));
    u_plane += frame->stride(VideoFrame::kUPlane);
    v_plane += frame->stride(VideoFrame::kVPlane);
  }
}

// Given a |yv12_frame| this method converts the YV12 frame to RGBA and
// makes sure that all the pixels of the RBG frame equal |expect_rgb_color|.
void ExpectFrameColor(media::VideoFrame* yv12_frame, uint32 expect_rgb_color) {
  ASSERT_EQ(VideoFrame::YV12, yv12_frame->format());
  ASSERT_EQ(yv12_frame->stride(VideoFrame::kUPlane),
            yv12_frame->stride(VideoFrame::kVPlane));

  scoped_refptr<media::VideoFrame> rgb_frame;
  rgb_frame = media::VideoFrame::CreateFrame(VideoFrame::RGB32,
                                             yv12_frame->coded_size(),
                                             yv12_frame->visible_rect(),
                                             yv12_frame->natural_size(),
                                             yv12_frame->GetTimestamp());

  ASSERT_EQ(yv12_frame->coded_size().width(),
      rgb_frame->coded_size().width());
  ASSERT_EQ(yv12_frame->coded_size().height(),
      rgb_frame->coded_size().height());

  media::ConvertYUVToRGB32(yv12_frame->data(VideoFrame::kYPlane),
                           yv12_frame->data(VideoFrame::kUPlane),
                           yv12_frame->data(VideoFrame::kVPlane),
                           rgb_frame->data(VideoFrame::kRGBPlane),
                           rgb_frame->coded_size().width(),
                           rgb_frame->coded_size().height(),
                           yv12_frame->stride(VideoFrame::kYPlane),
                           yv12_frame->stride(VideoFrame::kUPlane),
                           rgb_frame->stride(VideoFrame::kRGBPlane),
                           media::YV12);

  for (int row = 0; row < rgb_frame->coded_size().height(); ++row) {
    uint32* rgb_row_data = reinterpret_cast<uint32*>(
        rgb_frame->data(VideoFrame::kRGBPlane) +
        (rgb_frame->stride(VideoFrame::kRGBPlane) * row));
    for (int col = 0; col < rgb_frame->coded_size().width(); ++col) {
      SCOPED_TRACE(
          base::StringPrintf("Checking (%d, %d)", row, col));
      EXPECT_EQ(expect_rgb_color, rgb_row_data[col]);
    }
  }
}

// Fill each plane to its reported extents and verify accessors report non
// zero values.  Additionally, for the first plane verify the rows and
// row_bytes values are correct.
void ExpectFrameExtents(VideoFrame::Format format, int planes,
                        int bytes_per_pixel, const char* expected_hash) {
  const unsigned char kFillByte = 0x80;
  const int kWidth = 61;
  const int kHeight = 31;
  const base::TimeDelta kTimestamp = base::TimeDelta::FromMicroseconds(1337);

  gfx::Size size(kWidth, kHeight);
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateFrame(
      format, size, gfx::Rect(size), size, kTimestamp);
  ASSERT_TRUE(frame);

  for(int plane = 0; plane < planes; plane++) {
    SCOPED_TRACE(base::StringPrintf("Checking plane %d", plane));
    EXPECT_TRUE(frame->data(plane));
    EXPECT_TRUE(frame->stride(plane));
    EXPECT_TRUE(frame->rows(plane));
    EXPECT_TRUE(frame->row_bytes(plane));

    if (plane == 0) {
      EXPECT_EQ(frame->rows(plane), kHeight);
      EXPECT_EQ(frame->row_bytes(plane), kWidth * bytes_per_pixel);
    }

    memset(frame->data(plane), kFillByte,
           frame->stride(plane) * frame->rows(plane));
  }

  base::MD5Context context;
  base::MD5Init(&context);
  frame->HashFrameForTesting(&context);
  base::MD5Digest digest;
  base::MD5Final(&digest, &context);
  EXPECT_EQ(MD5DigestToBase16(digest), expected_hash);
}

TEST(VideoFrame, CreateFrame) {
  const int kWidth = 64;
  const int kHeight = 48;
  const base::TimeDelta kTimestamp = base::TimeDelta::FromMicroseconds(1337);

  // Create a YV12 Video Frame.
  gfx::Size size(kWidth, kHeight);
  scoped_refptr<media::VideoFrame> frame =
      VideoFrame::CreateFrame(media::VideoFrame::YV12, size, gfx::Rect(size),
                              size, kTimestamp);
  ASSERT_TRUE(frame);

  // Test VideoFrame implementation.
  EXPECT_EQ(media::VideoFrame::YV12, frame->format());
  {
    SCOPED_TRACE("");
    InitializeYV12Frame(frame, 0.0f);
    ExpectFrameColor(frame, 0xFF000000);
  }
  base::MD5Digest digest;
  base::MD5Context context;
  base::MD5Init(&context);
  frame->HashFrameForTesting(&context);
  base::MD5Final(&digest, &context);
  EXPECT_EQ(MD5DigestToBase16(digest), "9065c841d9fca49186ef8b4ef547e79b");
  {
    SCOPED_TRACE("");
    InitializeYV12Frame(frame, 1.0f);
    ExpectFrameColor(frame, 0xFFFFFFFF);
  }
  base::MD5Init(&context);
  frame->HashFrameForTesting(&context);
  base::MD5Final(&digest, &context);
  EXPECT_EQ(MD5DigestToBase16(digest), "911991d51438ad2e1a40ed5f6fc7c796");

  // Test an empty frame.
  frame = VideoFrame::CreateEmptyFrame();
  EXPECT_TRUE(frame->IsEndOfStream());
}

TEST(VideoFrame, CreateBlackFrame) {
  const int kWidth = 2;
  const int kHeight = 2;
  const uint8 kExpectedYRow[] = { 0, 0 };
  const uint8 kExpectedUVRow[] = { 128 };

  scoped_refptr<media::VideoFrame> frame =
      VideoFrame::CreateBlackFrame(gfx::Size(kWidth, kHeight));
  ASSERT_TRUE(frame);

  // Test basic properties.
  EXPECT_EQ(0, frame->GetTimestamp().InMicroseconds());
  EXPECT_FALSE(frame->IsEndOfStream());

  // Test |frame| properties.
  EXPECT_EQ(VideoFrame::YV12, frame->format());
  EXPECT_EQ(kWidth, frame->coded_size().width());
  EXPECT_EQ(kHeight, frame->coded_size().height());

  // Test frames themselves.
  uint8* y_plane = frame->data(VideoFrame::kYPlane);
  for (int y = 0; y < frame->coded_size().height(); ++y) {
    EXPECT_EQ(0, memcmp(kExpectedYRow, y_plane, arraysize(kExpectedYRow)));
    y_plane += frame->stride(VideoFrame::kYPlane);
  }

  uint8* u_plane = frame->data(VideoFrame::kUPlane);
  uint8* v_plane = frame->data(VideoFrame::kVPlane);
  for (int y = 0; y < frame->coded_size().height() / 2; ++y) {
    EXPECT_EQ(0, memcmp(kExpectedUVRow, u_plane, arraysize(kExpectedUVRow)));
    EXPECT_EQ(0, memcmp(kExpectedUVRow, v_plane, arraysize(kExpectedUVRow)));
    u_plane += frame->stride(VideoFrame::kUPlane);
    v_plane += frame->stride(VideoFrame::kVPlane);
  }
}

// Ensure each frame is properly sized and allocated.  Will trigger OOB reads
// and writes as well as incorrect frame hashes otherwise.
TEST(VideoFrame, CheckFrameExtents) {
  // Each call consists of a VideoFrame::Format, # of planes, bytes per pixel,
  // and the expected hash of all planes if filled with kFillByte (defined in
  // ExpectFrameExtents).
  ExpectFrameExtents(
      VideoFrame::RGB32,  1, 4, "de6d3d567e282f6a38d478f04fc81fb0");
  ExpectFrameExtents(
      VideoFrame::YV12,   3, 1, "71113bdfd4c0de6cf62f48fb74f7a0b1");
  ExpectFrameExtents(
      VideoFrame::YV16,   3, 1, "9bb99ac3ff350644ebff4d28dc01b461");
}

}  // namespace media
