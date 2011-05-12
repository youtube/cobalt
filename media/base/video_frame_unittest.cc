// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"

#include "base/format_macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "media/base/buffers.h"
#include "media/base/mock_filters.h"
#include "media/base/yuv_convert.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Helper function that initializes a YV12 frame with white and black scan
// lines based on the |white_to_black| parameter.  If 0, then the entire
// frame will be black, if 1 then the entire frame will be white.
void InitializeYV12Frame(VideoFrame* frame, double white_to_black) {
  EXPECT_EQ(VideoFrame::YV12, frame->format());
  size_t first_black_row = static_cast<size_t>(frame->height() *
                                               white_to_black);
  uint8* y_plane = frame->data(VideoFrame::kYPlane);
  for (size_t row = 0; row < frame->height(); ++row) {
    int color = (row < first_black_row) ? 0xFF : 0x00;
    memset(y_plane, color, frame->width());
    y_plane += frame->stride(VideoFrame::kYPlane);
  }
  uint8* u_plane = frame->data(VideoFrame::kUPlane);
  uint8* v_plane = frame->data(VideoFrame::kVPlane);
  for (size_t row = 0; row < frame->height(); row += 2) {
    memset(u_plane, 0x80, frame->width() / 2);
    memset(v_plane, 0x80, frame->width() / 2);
    u_plane += frame->stride(VideoFrame::kUPlane);
    v_plane += frame->stride(VideoFrame::kVPlane);
  }
}

// Given a |yv12_frame| this method converts the YV12 frame to RGBA and
// makes sure that all the pixels of the RBG frame equal |expect_rgb_color|.
void ExpectFrameColor(media::VideoFrame* yv12_frame, uint32 expect_rgb_color) {
  // On linux and mac builds if you directly compare using EXPECT_EQ and use
  // the VideoFrame::kNumxxxPlanes constants, it generates an error when
  // linking.  These are declared so that we can compare against locals.
  const size_t expect_yuv_planes = VideoFrame::kNumYUVPlanes;
  const size_t expect_rgb_planes = VideoFrame::kNumRGBPlanes;

  ASSERT_EQ(VideoFrame::YV12, yv12_frame->format());
  ASSERT_EQ(expect_yuv_planes, yv12_frame->planes());
  ASSERT_EQ(yv12_frame->stride(VideoFrame::kUPlane),
            yv12_frame->stride(VideoFrame::kVPlane));

  scoped_refptr<media::VideoFrame> rgb_frame;
  media::VideoFrame::CreateFrame(VideoFrame::RGBA,
                                 yv12_frame->width(),
                                 yv12_frame->height(),
                                 yv12_frame->GetTimestamp(),
                                 yv12_frame->GetDuration(),
                                 &rgb_frame);

  ASSERT_EQ(yv12_frame->width(), rgb_frame->width());
  ASSERT_EQ(yv12_frame->height(), rgb_frame->height());
  ASSERT_EQ(expect_rgb_planes, rgb_frame->planes());

  media::ConvertYUVToRGB32(yv12_frame->data(VideoFrame::kYPlane),
                           yv12_frame->data(VideoFrame::kUPlane),
                           yv12_frame->data(VideoFrame::kVPlane),
                           rgb_frame->data(VideoFrame::kRGBPlane),
                           rgb_frame->width(),
                           rgb_frame->height(),
                           yv12_frame->stride(VideoFrame::kYPlane),
                           yv12_frame->stride(VideoFrame::kUPlane),
                           rgb_frame->stride(VideoFrame::kRGBPlane),
                           media::YV12);

  for (size_t row = 0; row < rgb_frame->height(); ++row) {
    uint32* rgb_row_data = reinterpret_cast<uint32*>(
        rgb_frame->data(VideoFrame::kRGBPlane) +
        (rgb_frame->stride(VideoFrame::kRGBPlane) * row));
    for (size_t col = 0; col < rgb_frame->width(); ++col) {
      SCOPED_TRACE(
          base::StringPrintf("Checking (%" PRIuS ", %" PRIuS ")", row, col));
      EXPECT_EQ(expect_rgb_color, rgb_row_data[col]);
    }
  }
}

TEST(VideoFrame, CreateFrame) {
  const size_t kWidth = 64;
  const size_t kHeight = 48;
  const base::TimeDelta kTimestampA = base::TimeDelta::FromMicroseconds(1337);
  const base::TimeDelta kDurationA = base::TimeDelta::FromMicroseconds(1667);
  const base::TimeDelta kTimestampB = base::TimeDelta::FromMicroseconds(1234);
  const base::TimeDelta kDurationB = base::TimeDelta::FromMicroseconds(5678);

  // Create a YV12 Video Frame.
  scoped_refptr<media::VideoFrame> frame;
  VideoFrame::CreateFrame(media::VideoFrame::YV12, kWidth, kHeight,
                          kTimestampA, kDurationA, &frame);
  ASSERT_TRUE(frame);

  // Test StreamSample implementation.
  EXPECT_EQ(kTimestampA.InMicroseconds(),
            frame->GetTimestamp().InMicroseconds());
  EXPECT_EQ(kDurationA.InMicroseconds(),
            frame->GetDuration().InMicroseconds());
  EXPECT_FALSE(frame->IsEndOfStream());
  frame->SetTimestamp(kTimestampB);
  frame->SetDuration(kDurationB);
  EXPECT_EQ(kTimestampB.InMicroseconds(),
            frame->GetTimestamp().InMicroseconds());
  EXPECT_EQ(kDurationB.InMicroseconds(),
            frame->GetDuration().InMicroseconds());
  EXPECT_FALSE(frame->IsEndOfStream());

  // Test VideoFrame implementation.
  EXPECT_EQ(media::VideoFrame::TYPE_SYSTEM_MEMORY, frame->type());
  EXPECT_EQ(media::VideoFrame::YV12, frame->format());
  {
    SCOPED_TRACE("");
    InitializeYV12Frame(frame, 0.0f);
    ExpectFrameColor(frame, 0xFF000000);
  }
  {
    SCOPED_TRACE("");
    InitializeYV12Frame(frame, 1.0f);
    ExpectFrameColor(frame, 0xFFFFFFFF);
  }

  // Test an empty frame.
  VideoFrame::CreateEmptyFrame(&frame);
  EXPECT_TRUE(frame->IsEndOfStream());
}

TEST(VideoFrame, CreateBlackFrame) {
  const size_t kWidth = 2;
  const size_t kHeight = 2;
  const uint8 kExpectedYRow[] = { 0, 0 };
  const uint8 kExpectedUVRow[] = { 128 };

  scoped_refptr<media::VideoFrame> frame;
  VideoFrame::CreateBlackFrame(kWidth, kHeight, &frame);
  ASSERT_TRUE(frame);

  // Test basic properties.
  EXPECT_EQ(0, frame->GetTimestamp().InMicroseconds());
  EXPECT_EQ(0, frame->GetDuration().InMicroseconds());
  EXPECT_FALSE(frame->IsEndOfStream());

  // Test |frame| properties.
  EXPECT_EQ(VideoFrame::YV12, frame->format());
  EXPECT_EQ(kWidth, frame->width());
  EXPECT_EQ(kHeight, frame->height());
  EXPECT_EQ(3u, frame->planes());

  // Test frames themselves.
  uint8* y_plane = frame->data(VideoFrame::kYPlane);
  for (size_t y = 0; y < frame->height(); ++y) {
    EXPECT_EQ(0, memcmp(kExpectedYRow, y_plane, arraysize(kExpectedYRow)));
    y_plane += frame->stride(VideoFrame::kYPlane);
  }

  uint8* u_plane = frame->data(VideoFrame::kUPlane);
  uint8* v_plane = frame->data(VideoFrame::kVPlane);
  for (size_t y = 0; y < frame->height() / 2; ++y) {
    EXPECT_EQ(0, memcmp(kExpectedUVRow, u_plane, arraysize(kExpectedUVRow)));
    EXPECT_EQ(0, memcmp(kExpectedUVRow, v_plane, arraysize(kExpectedUVRow)));
    u_plane += frame->stride(VideoFrame::kUPlane);
    v_plane += frame->stride(VideoFrame::kVPlane);
  }
}

TEST(VideoFram, CreateExternalFrame) {
  scoped_array<uint8> memory(new uint8[1]);

  scoped_refptr<media::VideoFrame> frame;
  uint8* data[3] = {memory.get(), NULL, NULL};
  int strides[3] = {1, 0, 0};
  VideoFrame::CreateFrameExternal(media::VideoFrame::TYPE_SYSTEM_MEMORY,
                                  media::VideoFrame::RGB32, 0, 0, 3,
                                  data, strides,
                                  base::TimeDelta(), base::TimeDelta(),
                                  NULL, &frame);
  ASSERT_TRUE(frame);

  // Test frame properties.
  EXPECT_EQ(1, frame->stride(VideoFrame::kRGBPlane));
  EXPECT_EQ(memory.get(), frame->data(VideoFrame::kRGBPlane));

  // Delete |memory| and then |frame|.
  memory.reset();
  frame = NULL;
}

}  // namespace media
