// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class VideoUtilTest : public testing::Test {
 public:
  VideoUtilTest()
      : height_(0),
        y_stride_(0),
        u_stride_(0),
        v_stride_(0) {
  }

  virtual ~VideoUtilTest() {}

  void CreateSourceFrame(int width, int height,
                         int y_stride, int u_stride, int v_stride) {
    EXPECT_GE(y_stride, width);
    EXPECT_GE(u_stride, width / 2);
    EXPECT_GE(v_stride, width / 2);

    height_ = height;
    y_stride_ = y_stride;
    u_stride_ = u_stride;
    v_stride_ = v_stride;

    y_plane_.reset(new uint8[y_stride * height]);
    u_plane_.reset(new uint8[u_stride * height / 2]);
    v_plane_.reset(new uint8[v_stride * height / 2]);
  }

  void CreateDestinationFrame(int width, int height) {
    destination_frame_ =
        VideoFrame::CreateFrame(VideoFrame::YV12, width, height,
                                base::TimeDelta(), base::TimeDelta());
  }

  void CopyPlanes() {
    CopyYPlane(y_plane_.get(), y_stride_, height_, destination_frame_);
    CopyUPlane(u_plane_.get(), u_stride_, height_ / 2, destination_frame_);
    CopyVPlane(v_plane_.get(), v_stride_, height_ / 2, destination_frame_);
  }

 private:
  scoped_array<uint8> y_plane_;
  scoped_array<uint8> u_plane_;
  scoped_array<uint8> v_plane_;

  int height_;
  int y_stride_;
  int u_stride_;
  int v_stride_;

  scoped_refptr<VideoFrame> destination_frame_;

  DISALLOW_COPY_AND_ASSIGN(VideoUtilTest);
};

TEST_F(VideoUtilTest, CopyPlane_Exact) {
  CreateSourceFrame(16, 16, 16, 8, 8);
  CreateDestinationFrame(16, 16);
  CopyPlanes();
}

TEST_F(VideoUtilTest, CopyPlane_SmallerSource) {
  CreateSourceFrame(8, 8, 8, 4, 4);
  CreateDestinationFrame(16, 16);
  CopyPlanes();
}

TEST_F(VideoUtilTest, CopyPlane_SmallerDestination) {
  CreateSourceFrame(16, 16, 16, 8, 8);
  CreateDestinationFrame(8, 8);
  CopyPlanes();
}

}  // namespace media
