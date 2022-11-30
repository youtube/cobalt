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

#ifndef COBALT_MEDIA_FILE_DATA_SOURCE_H_
#define COBALT_MEDIA_FILE_DATA_SOURCE_H_

#include "base/files/file.h"
#include "cobalt/media/base/data_source.h"
#include "url/gurl.h"

namespace cobalt {
namespace media {

// A file based DataSource used to retrieve progressive videos from local files.
// The class is for testing purposes only, and shouldn't be used in production
// environment.
// Its member functions can be called from multiple threads.  However, this
// class doesn't synchronize the calls.  It's the responsibility of its user to
// ensure that such calls are synchronized.
class FileDataSource : public DataSource {
 public:
  static constexpr int64 kInvalidSize = -1;

  explicit FileDataSource(const GURL& file_url);

  // DataSource methods.
  void Read(int64 position, int size, uint8* data,
            const ReadCB& read_cb) override;
  void Stop() override {}
  void Abort() override {}
  bool GetSize(int64* size_out) override;
  void SetDownloadingStatusCB(
      const DownloadingStatusCB& downloading_status_cb) override {}

 private:
  base::File file_;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_FILE_DATA_SOURCE_H_
