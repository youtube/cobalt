// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/sandbox/format_guesstimator.h"

#include <algorithm>
#include <vector>

#include "base/path_service.h"
#include "net/base/filename_util.h"
#include "net/base/url_util.h"
#include "starboard/common/string.h"
#include "starboard/file.h"
#include "starboard/memory.h"
#include "starboard/types.h"

namespace cobalt {
namespace media {
namespace sandbox {

namespace {

// Can be called as:
//   IsFormat("https://example.com/audio.mp4", ".mp4")
//   IsFormat("cobalt/demos/video.webm", ".webm")
bool IsFormat(const std::string& path_or_url, const std::string& format) {
  DCHECK(!format.empty() && format[0] == '.') << "Invalid format: " << format;
  return path_or_url.size() > format.size() &&
         path_or_url.substr(path_or_url.size() - format.size()) == format;
}

base::FilePath ResolvePath(const std::string& path) {
  base::FilePath result(path);
  if (!result.IsAbsolute()) {
    base::FilePath content_path;
    base::PathService::Get(base::DIR_TEST_DATA, &content_path);
    CHECK(content_path.IsAbsolute());
    result = content_path.Append(result);
  }
  if (SbFileCanOpen(result.value().c_str(), kSbFileOpenOnly | kSbFileRead)) {
    return result;
  }
  return base::FilePath();
}

// Read the first 4096 bytes of the local file specified by |path|.  If the size
// of the size is less than 4096 bytes, the function reads all of its content.
std::vector<uint8_t> ReadHeader(const base::FilePath& path) {
  const int64_t kHeaderSize = 4096;

  starboard::ScopedFile file(path.value().c_str(),
                             kSbFileOpenOnly | kSbFileRead);
  int64_t bytes_to_read = std::min(kHeaderSize, file.GetSize());
  DCHECK_GE(bytes_to_read, 0);
  std::vector<uint8_t> buffer(bytes_to_read);
  auto bytes_read =
      file.Read(reinterpret_cast<char*>(buffer.data()), bytes_to_read);
  DCHECK_EQ(bytes_to_read, bytes_read);

  return buffer;
}

// Find the first occurrence of |str| in |data|.  If there is no |str| contained
// inside |data|, it returns -1.
off_t FindString(const std::vector<uint8_t>& data, const char* str) {
  size_t size_of_str = SbStringGetLength(str);
  for (off_t offset = 0; offset + size_of_str < data.size(); ++offset) {
    if (SbMemoryCompare(data.data() + offset, str, size_of_str) == 0) {
      return offset;
    }
  }
  return -1;
}

}  // namespace

FormatGuesstimator::FormatGuesstimator(const std::string& path_or_url) {
  GURL url(path_or_url);
  if (url.is_valid()) {
    // If it is a url, assume that it is a progressive video.
    InitializeAsProgressive(url);
    return;
  }
  base::FilePath path = ResolvePath(path_or_url);
  if (path.empty() || !SbFileCanOpen(path.value().c_str(), kSbFileRead)) {
    return;
  }
  if (IsFormat(path_or_url, ".mp4")) {
    InitializeAsMp4(path);
  } else if (IsFormat(path_or_url, ".webm")) {
    InitializeAsWebM(path);
  }
}

void FormatGuesstimator::InitializeAsProgressive(const GURL& url) {
  // Mp4 is the only supported progressive format.
  if (!IsFormat(url.spec(), ".mp4")) {
    return;
  }
  progressive_url_ = url;
  mime_ = "video/mp4";
  codecs_ = "avc1.640028, mp4a.40.2";
}

void FormatGuesstimator::InitializeAsMp4(const base::FilePath& path) {
  std::vector<uint8_t> header = ReadHeader(path);
  if (FindString(header, "ftyp") == -1) {
    return;
  }
  if (FindString(header, "dash") == -1) {
    progressive_url_ = net::FilePathToFileURL(path);
    mime_ = "video/mp4";
    codecs_ = "avc1.640028, mp4a.40.2";
    return;
  }
  if (FindString(header, "vide") != -1) {
    if (FindString(header, "avcC") != -1) {
      adaptive_path_ = path;
      mime_ = "video/mp4";
      codecs_ = "avc1.640028";
      return;
    }
    if (FindString(header, "hvcC") != -1) {
      adaptive_path_ = path;
      mime_ = "video/mp4";
      codecs_ = "hvc1.1.6.H150.90";
      return;
    }
    return;
  }
  if (FindString(header, "soun") != -1) {
    adaptive_path_ = path;
    mime_ = "audio/mp4";
    codecs_ = "mp4a.40.2";
    return;
  }
}

void FormatGuesstimator::InitializeAsWebM(const base::FilePath& path) {
  std::vector<uint8_t> header = ReadHeader(path);
  adaptive_path_ = path;
  if (FindString(header, "OpusHead") == -1) {
    mime_ = "video/webm";
    codecs_ = "vp9";
  } else {
    mime_ = "audio/webm";
    codecs_ = "opus";
  }
}

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt
