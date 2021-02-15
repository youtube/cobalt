// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_FETCHER_BUFFERED_DATA_SOURCE_H_
#define COBALT_MEDIA_FETCHER_BUFFERED_DATA_SOURCE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "cobalt/base/circular_buffer_shell.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/loader/fetcher.h"
#include "cobalt/loader/origin.h"
#include "cobalt/loader/url_fetcher_string_writer.h"
#include "cobalt/media/player/buffered_data_source.h"
#include "cobalt/network/network_module.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace cobalt {
namespace media {

// TODO: This class requires a large block of memory.

// A BufferedDataSource based on net::URLFetcher that can be used to retrieve
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
class FetcherBufferedDataSource : public BufferedDataSource,
                                  private net::URLFetcherDelegate {
 public:
  static const int64 kInvalidSize = -1;

  // Because the Fetchers have to be created and destroyed on the same thread,
  // we use the task_runner passed in to create and destroy Fetchers.
  FetcherBufferedDataSource(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const GURL& url, const csp::SecurityCallback& security_callback,
      network::NetworkModule* network_module, loader::RequestMode request_mode,
      loader::Origin origin);
  ~FetcherBufferedDataSource() override;

  // DataSource methods.
  void Read(int64 position, int size, uint8* data,
            const ReadCB& read_cb) override;
  void Stop() override;
  bool GetSize(int64* size_out) override;
  bool IsStreaming() override { return false; }
  void SetBitrate(int bitrate) override {}

  // BufferedDataSource methods.
  void SetDownloadingStatusCB(
      const DownloadingStatusCB& downloading_status_cb) override;

 private:
  class CancelableClosure
      : public base::RefCountedThreadSafe<CancelableClosure> {
   public:
    explicit CancelableClosure(const base::Closure& closure);

    void Cancel();
    base::Closure AsClosure();

   private:
    void Call();

    base::Lock lock_;
    base::Closure closure_;
  };

  // net::URLFetcherDelegate methods
  void OnURLFetchResponseStarted(const net::URLFetcher* source) override;
  void OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                  int64_t current, int64_t total,
                                  int64_t current_network_bytes) override;
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  void CreateNewFetcher();
  void UpdateDownloadingStatus(bool is_downloading);
  void Read_Locked(uint64 position, size_t size, uint8* data,
                   const ReadCB& read_cb);
  void ProcessPendingRead_Locked();
  void TryToSendRequest_Locked();

  base::Lock lock_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  GURL url_;
  network::NetworkModule* network_module_;
  std::unique_ptr<net::URLFetcher> fetcher_;

  bool is_downloading_;
  DownloadingStatusCB downloading_status_cb_;

  // |fetcher_| has to be destroyed on the thread it's created.  So it cannot be
  // safely destroyed inside Read_Locked().  Save |fetcher_| into
  // |fetcher_to_be_destroyed_| to ensure that it is properly destroyed either
  // inside CreateNewFetcher() or in the dtor while still allow |fetcher_| to be
  // set to NULL to invalidate outstanding read.
  std::unique_ptr<net::URLFetcher> fetcher_to_be_destroyed_;

  // |buffer_| stores a continuous block of data of target resource starts from
  // |buffer_offset_|.  When the target resource can be fit into |buffer_|,
  // |buffer_offset_| will always be 0.
  base::CircularBufferShell buffer_;
  uint64 buffer_offset_;

  base::Optional<uint64> total_size_of_resource_;
  bool error_occured_;

  uint64 last_request_offset_;
  uint64 last_request_size_;

  // This is usually the same as pending_read_position_.  Represent it
  // explicitly using a separate variable.
  uint64 last_read_position_;

  ReadCB pending_read_cb_;
  uint64 pending_read_position_;
  size_t pending_read_size_;
  uint8* pending_read_data_;

  csp::SecurityCallback security_callback_;
  scoped_refptr<CancelableClosure> cancelable_create_fetcher_closure_;

  loader::RequestMode request_mode_;
  loader::Origin document_origin_;
  // True if the origin is allowed to fetch resource data.
  bool is_origin_safe_;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_FETCHER_BUFFERED_DATA_SOURCE_H_
