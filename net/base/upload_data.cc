// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_data.h"

#include "base/file_util.h"
#include "base/platform_file.h"
#include "base/logging.h"

namespace net {

uint64 UploadData::GetContentLength() const {
  uint64 len = 0;
  std::vector<Element>::const_iterator it = elements_.begin();
  for (; it != elements_.end(); ++it)
    len += (*it).GetContentLength();
  return len;
}

void UploadData::CloseFiles() {
  std::vector<Element>::iterator it = elements_.begin();
  for (; it != elements_.end(); ++it) {
    if (it->type() == TYPE_FILE)
      it->Close();
  }
}

base::PlatformFile UploadData::Element::platform_file() const {
  DCHECK(type_ == TYPE_FILE) << "platform_file on non-file Element";

  return file_;
}

void UploadData::Element::Close() {
  DCHECK(type_ == TYPE_FILE) << "Close on non-file Element";

  if (file_ != base::kInvalidPlatformFileValue)
    base::ClosePlatformFile(file_);
  file_ = base::kInvalidPlatformFileValue;
}

void UploadData::Element::SetToFilePathRange(const FilePath& path,
                                             uint64 offset,
                                             uint64 length) {
  type_ = TYPE_FILE;
  file_range_offset_ = 0;
  file_range_length_ = 0;

  Close();

  if (offset + length < offset) {
    LOG(ERROR) << "Upload file offset and length overflow 64-bits. Ignoring.";
    return;
  }

  base::PlatformFile file = base::CreatePlatformFile(
      path, base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ, NULL);
  if (file == base::kInvalidPlatformFileValue) {
    // This case occurs when the user selects a file that isn't readable.
    file_path_= path;
    return;
  }

  uint64 file_size;
  if (!base::GetPlatformFileSize(file, &file_size)) {
    base::ClosePlatformFile(file);
    return;
  }

  if (offset > file_size) {
    base::ClosePlatformFile(file);
    return;
  }
  if (offset + length > file_size)
    length = file_size - offset;

  file_ = file;
  file_path_ = path;
  file_range_offset_ = offset;
  file_range_length_ = length;
}

}  // namespace net
