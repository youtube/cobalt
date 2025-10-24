/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/evaluation/utils.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/system/file_wrapper.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace {

constexpr char kFrameHeader[] = "FRAME\n";

// Reading 30 bytes from the Y4M header should be enough to get the resolution
// and framerate. The header starts with: `YUV4MPEG2 W<WIDTH> H<HEIGTH>
// Fn<NUMERATOR>:Fd<DENOMINATOR>`.
constexpr int kHeaderBytesToRead = 30;

}  // namespace

TempY4mFileCreator::TempY4mFileCreator(int width, int height, int framerate)
    : width_(width),
      height_(height),
      framerate_(framerate),
      frame_size_(width * height * 3 / 2),
      y4m_filepath_(test::TempFilename(test::OutputPath(), "temp_video")) {
  // A file with the given name path should just have been created.
  RTC_CHECK_EQ(test::GetFileSize(y4m_filepath_), 0);
}

TempY4mFileCreator::~TempY4mFileCreator() {
  RTC_CHECK(test::RemoveFile(y4m_filepath_.c_str()));
}

void TempY4mFileCreator::CreateTempY4mFile(
    ArrayView<const uint8_t> file_content) {
  RTC_CHECK_EQ(file_content.size() % frame_size_, 0)
      << "Content size is not a multiple of frame size. Probably some data is "
         "missing.";
  FileWrapper video_file = FileWrapper::OpenWriteOnly(y4m_filepath_);
  RTC_CHECK(video_file.is_open());

  WriteFileHeader(video_file);

  // Write frame content.
  int frame_number = file_content.size() / frame_size_;
  for (int frame_index = 0; frame_index < frame_number; ++frame_index) {
    RTC_CHECK(video_file.Write(kFrameHeader, sizeof(kFrameHeader) - 1));
    RTC_CHECK_LT(frame_size_ * frame_index, file_content.size());
    RTC_CHECK(video_file.Write(file_content.data() + frame_size_ * frame_index,
                               frame_size_));
  }

  RTC_CHECK(video_file.Flush());
}

void TempY4mFileCreator::WriteFileHeader(FileWrapper& video_file) const {
  StringBuilder frame_header;
  frame_header << "YUV4MPEG2 W" << width_ << " H" << height_ << " F"
               << framerate_ << ":1 C420\n";
  RTC_CHECK(video_file.Write(frame_header.str().c_str(), frame_header.size()));
}

Y4mMetadata ReadMetadataFromY4mHeader(absl::string_view clip_path) {
  FILE* file = fopen(std::string(clip_path).c_str(), "r");
  RTC_CHECK(file) << "Could not open " << clip_path;

  char header[kHeaderBytesToRead];
  RTC_CHECK(fgets(header, sizeof(header), file) != nullptr)
      << "File " << clip_path << " is too small";
  fclose(file);

  int fps_numerator;
  int fps_denominator;
  int width;
  int height;
  RTC_CHECK_EQ(sscanf(header, "YUV4MPEG2 W%u H%u F%i:%i", &width, &height,
                      &fps_numerator, &fps_denominator),
               4);
  RTC_CHECK_NE(fps_denominator, 0);
  return {.width = width,
          .height = height,
          .framerate = fps_numerator / fps_denominator};
}

}  // namespace webrtc
