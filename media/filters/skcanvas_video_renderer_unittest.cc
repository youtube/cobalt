// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "media/filters/skcanvas_video_renderer.h"

using media::VideoFrame;

namespace media {

static const int kWidth = 320;
static const int kHeight = 240;
static const gfx::Rect kNaturalRect(0, 0, kWidth, kHeight);

// Helper for filling a |canvas| with a solid |color|.
void FillCanvas(SkCanvas* canvas, SkColor color) {
  const SkBitmap& bitmap = canvas->getDevice()->accessBitmap(true);
  bitmap.lockPixels();
  bitmap.eraseColor(color);
  bitmap.unlockPixels();
}

// Helper for returning the color of a solid |canvas|.
SkColor GetColor(SkCanvas* canvas) {
  const SkBitmap& bitmap = canvas->getDevice()->accessBitmap(false);
  bitmap.lockPixels();
  SkColor c = bitmap.getColor(0, 0);
  bitmap.unlockPixels();
  return c;
}

class SkCanvasVideoRendererTest : public testing::Test {
 public:
  enum Color {
    kRed,
    kBlue,
  };

  SkCanvasVideoRendererTest();
  virtual ~SkCanvasVideoRendererTest();

  // Paints to |canvas| using |renderer_| without any frame data.
  void PaintWithoutFrame(SkCanvas* canvas);

  // Paints the |video_frame| to the |canvas| using |renderer_|, setting the
  // color of |video_frame| to |color| first.
  void Paint(VideoFrame* video_frame, SkCanvas* canvas, Color color);

  // Getters for various frame sizes.
  VideoFrame* natural_frame() { return natural_frame_; }
  VideoFrame* larger_frame() { return larger_frame_; }
  VideoFrame* smaller_frame() { return smaller_frame_; }

  // Getters for canvases that trigger the various painting paths.
  SkCanvas* fast_path_canvas() { return &fast_path_canvas_; }
  SkCanvas* slow_path_canvas() { return &slow_path_canvas_; }

 private:
  SkCanvasVideoRenderer renderer_;

  scoped_refptr<VideoFrame> natural_frame_;
  scoped_refptr<VideoFrame> larger_frame_;
  scoped_refptr<VideoFrame> smaller_frame_;

  SkDevice fast_path_device_;
  SkCanvas fast_path_canvas_;
  SkDevice slow_path_device_;
  SkCanvas slow_path_canvas_;

  DISALLOW_COPY_AND_ASSIGN(SkCanvasVideoRendererTest);
};

SkCanvasVideoRendererTest::SkCanvasVideoRendererTest()
    : natural_frame_(VideoFrame::CreateBlackFrame(gfx::Size(kWidth, kHeight))),
      larger_frame_(VideoFrame::CreateBlackFrame(
          gfx::Size(kWidth * 2, kHeight * 2))),
      smaller_frame_(VideoFrame::CreateBlackFrame(
          gfx::Size(kWidth / 2, kHeight / 2))),
      fast_path_device_(SkBitmap::kARGB_8888_Config, kWidth, kHeight, true),
      fast_path_canvas_(&fast_path_device_),
      slow_path_device_(SkBitmap::kARGB_8888_Config, kWidth, kHeight, false),
      slow_path_canvas_(&slow_path_device_) {
  // Give each frame a unique timestamp.
  natural_frame_->SetTimestamp(base::TimeDelta::FromMilliseconds(1));
  larger_frame_->SetTimestamp(base::TimeDelta::FromMilliseconds(2));
  smaller_frame_->SetTimestamp(base::TimeDelta::FromMilliseconds(3));
}

SkCanvasVideoRendererTest::~SkCanvasVideoRendererTest() {}

void SkCanvasVideoRendererTest::PaintWithoutFrame(SkCanvas* canvas) {
  renderer_.Paint(NULL, canvas, kNaturalRect, 0xFF);
}

void SkCanvasVideoRendererTest::Paint(VideoFrame* video_frame,
                                      SkCanvas* canvas,
                                      Color color) {
  switch (color) {
    case kRed:
      media::FillYUV(video_frame, 76, 84, 255);
      break;
    case kBlue:
      media::FillYUV(video_frame, 29, 255, 107);
      break;
  }
  renderer_.Paint(video_frame, canvas, kNaturalRect, 0xFF);
}

TEST_F(SkCanvasVideoRendererTest, FastPaint_NoFrame) {
  // Test that black gets painted over canvas.
  FillCanvas(fast_path_canvas(), SK_ColorRED);
  PaintWithoutFrame(fast_path_canvas());
  EXPECT_EQ(SK_ColorBLACK, GetColor(fast_path_canvas()));
}

TEST_F(SkCanvasVideoRendererTest, SlowPaint_NoFrame) {
  // Test that black gets painted over canvas.
  FillCanvas(slow_path_canvas(), SK_ColorRED);
  PaintWithoutFrame(slow_path_canvas());
  EXPECT_EQ(SK_ColorBLACK, GetColor(slow_path_canvas()));
}

TEST_F(SkCanvasVideoRendererTest, FastPaint_Natural) {
  Paint(natural_frame(), fast_path_canvas(), kRed);
  EXPECT_EQ(SK_ColorRED, GetColor(fast_path_canvas()));
}

TEST_F(SkCanvasVideoRendererTest, SlowPaint_Natural) {
  Paint(natural_frame(), slow_path_canvas(), kRed);
  EXPECT_EQ(SK_ColorRED, GetColor(slow_path_canvas()));
}

TEST_F(SkCanvasVideoRendererTest, FastPaint_Larger) {
  Paint(natural_frame(), fast_path_canvas(), kRed);
  EXPECT_EQ(SK_ColorRED, GetColor(fast_path_canvas()));

  Paint(larger_frame(), fast_path_canvas(), kBlue);
  EXPECT_EQ(SK_ColorBLUE, GetColor(fast_path_canvas()));
}

TEST_F(SkCanvasVideoRendererTest, SlowPaint_Larger) {
  Paint(natural_frame(), slow_path_canvas(), kRed);
  EXPECT_EQ(SK_ColorRED, GetColor(slow_path_canvas()));

  Paint(larger_frame(), slow_path_canvas(), kBlue);
  EXPECT_EQ(SK_ColorBLUE, GetColor(slow_path_canvas()));
}

TEST_F(SkCanvasVideoRendererTest, FastPaint_Smaller) {
  Paint(natural_frame(), fast_path_canvas(), kRed);
  EXPECT_EQ(SK_ColorRED, GetColor(fast_path_canvas()));

  Paint(smaller_frame(), fast_path_canvas(), kBlue);
  EXPECT_EQ(SK_ColorBLUE, GetColor(fast_path_canvas()));
}

TEST_F(SkCanvasVideoRendererTest, SlowPaint_Smaller) {
  Paint(natural_frame(), slow_path_canvas(), kRed);
  EXPECT_EQ(SK_ColorRED, GetColor(slow_path_canvas()));

  Paint(smaller_frame(), slow_path_canvas(), kBlue);
  EXPECT_EQ(SK_ColorBLUE, GetColor(slow_path_canvas()));
}

TEST_F(SkCanvasVideoRendererTest, FastPaint_NoTimestamp) {
  VideoFrame* video_frame = natural_frame();
  video_frame->SetTimestamp(media::kNoTimestamp());
  Paint(video_frame, fast_path_canvas(), kRed);
  EXPECT_EQ(SK_ColorRED, GetColor(fast_path_canvas()));
}

TEST_F(SkCanvasVideoRendererTest, SlowPaint_NoTimestamp) {
  VideoFrame* video_frame = natural_frame();
  video_frame->SetTimestamp(media::kNoTimestamp());
  Paint(video_frame, slow_path_canvas(), kRed);
  EXPECT_EQ(SK_ColorRED, GetColor(slow_path_canvas()));
}

TEST_F(SkCanvasVideoRendererTest, FastPaint_SameVideoFrame) {
  Paint(natural_frame(), fast_path_canvas(), kRed);
  EXPECT_EQ(SK_ColorRED, GetColor(fast_path_canvas()));

  // Fast paints always get painted to the canvas.
  Paint(natural_frame(), fast_path_canvas(), kBlue);
  EXPECT_EQ(SK_ColorBLUE, GetColor(fast_path_canvas()));
}

TEST_F(SkCanvasVideoRendererTest, SlowPaint_SameVideoFrame) {
  Paint(natural_frame(), slow_path_canvas(), kRed);
  EXPECT_EQ(SK_ColorRED, GetColor(slow_path_canvas()));

  // Slow paints can get cached, expect the old color value.
  Paint(natural_frame(), slow_path_canvas(), kBlue);
  EXPECT_EQ(SK_ColorRED, GetColor(slow_path_canvas()));
}

}  // namespace media
