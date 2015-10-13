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

#ifndef MEDIA_FETCHER_BUFFERED_DATA_SOURCE_H_
#define MEDIA_FETCHER_BUFFERED_DATA_SOURCE_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/synchronization/lock.h"
#include "cobalt/loader/fetcher_factory.h"
#include "googleurl/src/gurl.h"
#include "media/player/buffered_data_source.h"

namespace cobalt {
namespace media {

// A BufferedDataSource based on loader::Fetcher that can be used to retrieve
// progressive videos from both local and network sources.
class FetcherBufferedDataSource : public ::media::BufferedDataSource,
                                  private loader::Fetcher::Handler {
 public:
  FetcherBufferedDataSource(const GURL& url,
                            loader::FetcherFactory* fetcher_factory);
  ~FetcherBufferedDataSource() OVERRIDE;

  // DataSource methods.
  void Read(int64 position, int size, uint8* data,
            const ReadCB& read_cb) OVERRIDE;
  void Stop(const base::Closure& callback) OVERRIDE;
  bool GetSize(int64* size_out) OVERRIDE;
  bool IsStreaming() OVERRIDE { return false; }
  void SetBitrate(int bitrate) OVERRIDE { UNREFERENCED_PARAMETER(bitrate); }

 private:
  enum State { kReading, kFinishedReading, kError };

  // loader::Fetcher::Handler methods.
  void OnReceived(loader::Fetcher* fetcher, const char* data,
                  size_t size) OVERRIDE;
  void OnDone(loader::Fetcher* fetcher) OVERRIDE;
  void OnError(loader::Fetcher* fetcher, const std::string& error) OVERRIDE;
  void Read_Locked(int64 position, int size, uint8* data,
                   const ReadCB& read_cb);
  void ProcessPendingRead_Locked();

  base::Lock lock_;
  std::vector<uint8> buffer_;
  scoped_ptr<loader::Fetcher> fetcher_;
  volatile State state_;
  ReadCB pending_read_cb_;
  int64 pending_read_position_;
  int pending_read_size_;
  uint8* pending_read_data_;
};

}  // namespace media
}  // namespace cobalt

#endif  // MEDIA_FETCHER_BUFFERED_DATA_SOURCE_H_
