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

#include "base/circular_buffer_shell.h"
#include "base/compiler_specific.h"
#include "base/message_loop_proxy.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "cobalt/loader/fetcher_factory.h"
#include "googleurl/src/gurl.h"
#include "media/player/buffered_data_source.h"

namespace cobalt {
namespace media {

// TODO(***REMOVED***): This class requires a large block of memory.  Consider to
// use ShellBufferFactory for its memory if possible to avoid possible OOM.

// A BufferedDataSource based on loader::Fetcher that can be used to retrieve
// progressive videos from both local and network sources.
// It uses a fixed size circular buffer so we may not be able to store all data
// into this buffer.  It is based on the following assumptions/strategies:
// 1. It assumes that the buffer is large enough to fulfill one Read() request.
//    So any outstanding request only requires at most one request.
// 2. It will do one initial request to retrieve the target resource.  If the
//    whole resource can be fit into the buffer, no further request will be
//    fired.
// 3. If the resource doesn't fit into the buffer.  The class will store
//    kBackwardBytes bytes before the last read offset(LRO) and kForwardBytes
//    after LRO.  Note that if LRO is less than kBackwardBytes, then data starts
//    from offset 0 will be cached.
// 4. It assumes that the server supports range request.
// 5. All data stored are continuous.
class FetcherBufferedDataSource : public ::media::BufferedDataSource,
                                  private loader::Fetcher::Handler {
 public:
  static const uint32 kBackwardBytes = 1024 * 1024;
  static const uint32 kForwardBytes = 3 * 1024 * 1024;
  static const uint32 kBufferCapacity = kBackwardBytes + kForwardBytes;
  static const int64 kInvalidSize = -1;

  // Because the Fetchers have to be created and destroyed on the same thread,
  // we use the message_loop passed in to create and destroy Fetchers.
  FetcherBufferedDataSource(
      const scoped_refptr<base::MessageLoopProxy>& message_loop,
      const GURL& url, loader::FetcherFactory* fetcher_factory);
  ~FetcherBufferedDataSource() OVERRIDE;

  // DataSource methods.
  void Read(int64 position, int size, uint8* data,
            const ReadCB& read_cb) OVERRIDE;
  void Stop(const base::Closure& callback) OVERRIDE;
  bool GetSize(int64* size_out) OVERRIDE;
  bool IsStreaming() OVERRIDE { return false; }
  void SetBitrate(int bitrate) OVERRIDE { UNREFERENCED_PARAMETER(bitrate); }

 private:
  typedef loader::Fetcher Fetcher;

  // Fetcher::Handler methods.
  void OnResponseStarted(
      Fetcher* fetcher,
      const scoped_refptr<net::HttpResponseHeaders>& headers) OVERRIDE;
  void OnReceived(Fetcher* fetcher, const char* data, size_t size) OVERRIDE;
  void OnDone(Fetcher* fetcher) OVERRIDE;
  void OnError(Fetcher* fetcher, const std::string& error) OVERRIDE;
  void CreateNewFetcher();
  void Read_Locked(uint64 position, uint64 size, uint8* data,
                   const ReadCB& read_cb);
  void ProcessPendingRead_Locked();
  void TryToSendRequest_Locked();

  base::Lock lock_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;
  GURL url_;
  loader::FetcherFactory* fetcher_factory_;
  scoped_ptr<Fetcher> fetcher_;

  // |buffer_| stores a continuous block of data of target resource starts from
  // |buffer_offset_|.  When the target resource can be fit into |buffer_|,
  // |buffer_offset_| will always be 0.
  base::CircularBufferShell buffer_;
  uint64 buffer_offset_;

  base::optional<uint64> total_size_of_resource_;
  bool error_occured_;

  uint64 last_request_offset_;
  uint64 last_request_size_;

  // This is usually the same as pending_read_position_.  Represent it
  // explicitly using a separate variable.
  uint64 last_read_position_;

  ReadCB pending_read_cb_;
  uint64 pending_read_position_;
  uint64 pending_read_size_;
  uint8* pending_read_data_;
};

}  // namespace media
}  // namespace cobalt

#endif  // MEDIA_FETCHER_BUFFERED_DATA_SOURCE_H_
