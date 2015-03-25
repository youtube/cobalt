/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MEDIA_PLAYER_BUFFERED_DATA_SOURCE_H_
#define MEDIA_PLAYER_BUFFERED_DATA_SOURCE_H_

#include <stdio.h>

#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "media/base/data_source.h"

namespace media {

enum Preload {
  kPreloadNone,
  kPreloadMetaData,
  kPreloadAuto,
};

// TODO(***REMOVED***) : The temporary data source can only load local files and
//                   ignore all I/O errors except file open error.
class BufferedDataSource : public DataSource {
 public:
  // All callbacks will be posted to the message_loop passed in.
  explicit BufferedDataSource(MessageLoop* message_loop)
      : size_(kInvalidSize), message_loop_(message_loop) {}

  void Initialize(const GURL& url, const base::Callback<void(bool)>& init_cb);
  virtual void SetPreload(Preload preload) {}
  virtual bool HasSingleOrigin() { return true; }
  virtual bool DidPassCORSAccessCheck() const { return true; }
  virtual void Abort() {}

  // DataSource methods
  void Read(int64 position,
            int size,
            uint8* data,
            const DataSource::ReadCB& read_cb) OVERRIDE;
  void Stop(const base::Closure& callback) OVERRIDE {}
  bool GetSize(int64* size_out) OVERRIDE {
    *size_out = size_;
    return size_ != kInvalidSize;
  }
  bool IsStreaming() OVERRIDE { return false; }
  void SetBitrate(int bitrate) OVERRIDE {}

 private:
  static const int64 kInvalidSize = -1;

  std::string file_name_;
  int64 size_;
  MessageLoop* message_loop_;
};

}  // namespace media

#endif  // MEDIA_PLAYER_BUFFERED_DATA_SOURCE_H_
