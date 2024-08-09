// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_CHROMEOS_SCOPED_VIDEO_CAPTURE_JPEG_DECODER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_CHROMEOS_SCOPED_VIDEO_CAPTURE_JPEG_DECODER_H_

#include <memory>

#include "base/sequenced_task_runner.h"
#include "third_party/chromium/media/capture/capture_export.h"
#include "third_party/chromium/media/capture/video/chromeos/video_capture_jpeg_decoder.h"

namespace media_m96 {

// Decorator for media_m96::VideoCaptureJpegDecoder that destroys the decorated
// instance on a given task runner.
class CAPTURE_EXPORT ScopedVideoCaptureJpegDecoder
    : public VideoCaptureJpegDecoder {
 public:
  ScopedVideoCaptureJpegDecoder(
      std::unique_ptr<VideoCaptureJpegDecoder> decoder,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  ~ScopedVideoCaptureJpegDecoder() override;

  // Implementation of VideoCaptureJpegDecoder:
  void Initialize() override;
  STATUS GetStatus() const override;
  void DecodeCapturedData(
      const uint8_t* data,
      size_t in_buffer_size,
      const media_m96::VideoCaptureFormat& frame_format,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      media_m96::VideoCaptureDevice::Client::Buffer out_buffer) override;

 private:
  std::unique_ptr<VideoCaptureJpegDecoder> decoder_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAPTURE_VIDEO_CHROMEOS_SCOPED_VIDEO_CAPTURE_JPEG_DECODER_H_
