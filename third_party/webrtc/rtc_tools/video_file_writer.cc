/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/video_file_writer.h"

#include <stdint.h>

#include <cstdio>
#include <string>

#include "absl/strings/match.h"
#include "api/video/video_frame_buffer.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace test {
namespace {

void WriteVideoToFile(const scoped_refptr<Video>& video,
                      const std::string& file_name,
                      int fps,
                      bool isY4m) {
  RTC_CHECK(video);
  FILE* output_file = fopen(file_name.c_str(), "wb");
  if (output_file == nullptr) {
    RTC_LOG(LS_ERROR) << "Could not open file for writing: " << file_name;
    return;
  }

  if (isY4m) {
    fprintf(output_file, "YUV4MPEG2 W%d H%d F%d:1 C420\n", video->width(),
            video->height(), fps);
  }
  for (size_t i = 0; i < video->number_of_frames(); ++i) {
    if (isY4m) {
      std::string frame = "FRAME\n";
      fwrite(frame.c_str(), 1, 6, output_file);
    }
    scoped_refptr<I420BufferInterface> buffer = video->GetFrame(i);
    RTC_CHECK(buffer) << "Frame: " << i
                      << "\nWhile trying to create: " << file_name;
    const uint8_t* data_y = buffer->DataY();
    int stride = buffer->StrideY();
    for (int j = 0; j < video->height(); ++j) {
      fwrite(data_y + j * stride, /*size=*/1, stride, output_file);
    }
    const uint8_t* data_u = buffer->DataU();
    stride = buffer->StrideU();
    for (int j = 0; j < buffer->ChromaHeight(); ++j) {
      fwrite(data_u + j * stride, /*size=*/1, stride, output_file);
    }
    const uint8_t* data_v = buffer->DataV();
    stride = buffer->StrideV();
    for (int j = 0; j < buffer->ChromaHeight(); ++j) {
      fwrite(data_v + j * stride, /*size=*/1, stride, output_file);
    }
  }
  if (ferror(output_file) != 0) {
    RTC_LOG(LS_ERROR) << "Error writing to file " << file_name;
  }
  fclose(output_file);
}

}  // Anonymous namespace

void WriteVideoToFile(const scoped_refptr<Video>& video,
                      const std::string& file_name,
                      int fps) {
  WriteVideoToFile(video, file_name, fps,
                   /*isY4m=*/absl::EndsWith(file_name, ".y4m"));
}

void WriteY4mVideoToFile(const scoped_refptr<Video>& video,
                         const std::string& file_name,
                         int fps) {
  WriteVideoToFile(video, file_name, fps, /*isY4m=*/true);
}

void WriteYuvVideoToFile(const scoped_refptr<Video>& video,
                         const std::string& file_name,
                         int fps) {
  WriteVideoToFile(video, file_name, fps, /*isY4m=*/false);
}

}  // namespace test
}  // namespace webrtc
