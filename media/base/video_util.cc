// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_util.h"

#include "base/logging.h"
#include "media/base/video_frame.h"

namespace media {

static void CopyPlane(size_t plane, const uint8* source, int stride, int rows,
                      VideoFrame* frame) {
  uint8* dest = frame->data(plane);
  int dest_stride = frame->stride(plane);

  // Clamp in case source frame has smaller stride.
  int bytes_to_copy_per_row = std::min(frame->row_bytes(plane), stride);

  // Clamp in case source frame has smaller height.
  int rows_to_copy = std::min(frame->rows(plane), rows);

  // Copy!
  for (int row = 0; row < rows_to_copy; ++row) {
    memcpy(dest, source, bytes_to_copy_per_row);
    source += stride;
    dest += dest_stride;
  }
}

void CopyYPlane(const uint8* source, int stride, int rows, VideoFrame* frame) {
  CopyPlane(VideoFrame::kYPlane, source, stride, rows, frame);
}

void CopyUPlane(const uint8* source, int stride, int rows, VideoFrame* frame) {
  CopyPlane(VideoFrame::kUPlane, source, stride, rows, frame);
}

void CopyVPlane(const uint8* source, int stride, int rows, VideoFrame* frame) {
  CopyPlane(VideoFrame::kVPlane, source, stride, rows, frame);
}

void FillYUV(VideoFrame* frame, uint8 y, uint8 u, uint8 v) {
  // Fill the Y plane.
  uint8* y_plane = frame->data(VideoFrame::kYPlane);
  int y_rows = frame->rows(VideoFrame::kYPlane);
  int y_row_bytes = frame->row_bytes(VideoFrame::kYPlane);
  for (int i = 0; i < y_rows; ++i) {
    memset(y_plane, y, y_row_bytes);
    y_plane += frame->stride(VideoFrame::kYPlane);
  }

  // Fill the U and V planes.
  uint8* u_plane = frame->data(VideoFrame::kUPlane);
  uint8* v_plane = frame->data(VideoFrame::kVPlane);
  int uv_rows = frame->rows(VideoFrame::kUPlane);
  int u_row_bytes = frame->row_bytes(VideoFrame::kUPlane);
  int v_row_bytes = frame->row_bytes(VideoFrame::kVPlane);
  for (int i = 0; i < uv_rows; ++i) {
    memset(u_plane, u, u_row_bytes);
    memset(v_plane, v, v_row_bytes);
    u_plane += frame->stride(VideoFrame::kUPlane);
    v_plane += frame->stride(VideoFrame::kVPlane);
  }
}

}  // namespace media
