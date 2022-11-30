// Copyright 2022 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/media/file_data_source.h"

#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"

namespace cobalt {
namespace media {
namespace {

using base::File;
using base::FilePath;
using base::PathService;

File OpenFile(const FilePath& file_path) {
  int path_keys[] = {paths::DIR_COBALT_WEB_ROOT, base::DIR_TEST_DATA};

  for (auto path_key : path_keys) {
    FilePath root_path;
    bool result = PathService::Get(path_key, &root_path);
    SB_DCHECK(result);

    File file(root_path.Append(file_path), File::FLAG_OPEN | File::FLAG_READ);
    if (file.IsValid()) {
      return std::move(file);
    }
  }

  return File();
}

}  // namespace

FileDataSource::FileDataSource(const GURL& file_url) {
  DCHECK(file_url.is_valid());
  DCHECK(file_url.SchemeIsFile());

  std::string path = file_url.path();
  if (path.empty() || path[0] != '/') {
    return;
  }

  file_ = std::move(OpenFile(base::FilePath(path.substr(1))));
}

void FileDataSource::Read(int64 position, int size, uint8* data,
                          const ReadCB& read_cb) {
  DCHECK_GE(position, 0);
  DCHECK_GE(size, 0);

  if (!file_.IsValid()) {
    read_cb.Run(kReadError);
    return;
  }

  auto bytes_read = file_.Read(position, reinterpret_cast<char*>(data), size);
  if (bytes_read == size) {
    read_cb.Run(bytes_read);
  } else {
    read_cb.Run(kReadError);
  }
}

bool FileDataSource::GetSize(int64* size_out) {
  *size_out = file_.IsValid() ? file_.GetLength() : kInvalidSize;

  return *size_out >= 0;
}

}  // namespace media
}  // namespace cobalt
