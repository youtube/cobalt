// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/test_data_util.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "media/ffmpeg/ffmpeg_common.h"

namespace media {

void ReadTestDataFile(const std::string& name, scoped_array<uint8>* buffer,
                      int* size) {
  FilePath file_path;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &file_path));

  file_path = file_path.Append(FILE_PATH_LITERAL("media"))
      .Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("data"))
      .AppendASCII(name);

  int64 tmp = 0;
  CHECK(file_util::GetFileSize(file_path, &tmp))
      << "Failed to get file size for '" << name << "'";

  // Why FF_INPUT_BUFFER_PADDING_SIZE? FFmpeg assumes all input buffers are
  // padded. Since most of our test data is passed to FFmpeg, it makes sense
  // to do the padding here instead of scattering it around test code.
  int file_size = static_cast<int>(tmp);
  buffer->reset(new uint8[file_size + FF_INPUT_BUFFER_PADDING_SIZE]);

  CHECK(file_size == file_util::ReadFile(file_path,
                                         reinterpret_cast<char*>(buffer->get()),
                                         file_size))
    << "Failed to read '" << name << "'";
  *size = file_size;
}

void ReadTestDataFile(const std::string& name, scoped_refptr<Buffer>* buffer) {
  scoped_array<uint8> buf;
  int buf_size;
  ReadTestDataFile(name, &buf, &buf_size);
  *buffer = new DataBuffer(buf.release(), buf_size);
}

}  // namespace media
