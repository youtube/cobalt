// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_NETWORK_H_
#define COMPONENTS_UPDATE_CLIENT_NETWORK_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace update_client {

class NetworkFetcher {
 public:
  using PostRequestCompleteCallback =
      base::OnceCallback<void(std::unique_ptr<std::string> response_body,
                              int net_error,
                              const std::string& header_etag,
                              int64_t xheader_retry_after_sec)>;
#if defined(IN_MEMORY_UPDATES)
  using DownloadToStringCompleteCallback = base::OnceCallback<
      void(std::string* dst, int net_error, int64_t content_size)>;
#else
  using DownloadToFileCompleteCallback = base::OnceCallback<
      void(base::FilePath path, int net_error, int64_t content_size)>;
#endif
  using ResponseStartedCallback = base::OnceCallback<
      void(const GURL& final_url, int response_code, int64_t content_length)>;
  using ProgressCallback = base::RepeatingCallback<void(int64_t current)>;

  // The ETag header carries the ECSDA signature of the POST response, if
  // signing has been used.
  static constexpr char kHeaderEtag[] = "ETag";

  // The server uses the optional X-Retry-After header to indicate that the
  // current request should not be attempted again.
  //
  // The value of the header is the number of seconds to wait before trying to
  // do a subsequent update check. Only the values retrieved over HTTPS are
  // trusted.
  static constexpr char kHeaderXRetryAfter[] = "X-Retry-After";

  virtual ~NetworkFetcher() = default;

  virtual void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) = 0;
#if defined(IN_MEMORY_UPDATES)
  // Does not take ownership of |dst|, which must refer to a valid string that
  // outlives this object.
  virtual void DownloadToString(
      const GURL& url,
      std::string* dst,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadToStringCompleteCallback download_to_string_complete_callback) = 0;
#else
  virtual void DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadToFileCompleteCallback download_to_file_complete_callback) = 0;
#endif

#if defined(STARBOARD)
  virtual void Cancel() = 0;
#endif

 protected:
  NetworkFetcher() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkFetcher);
};

class NetworkFetcherFactory : public base::RefCounted<NetworkFetcherFactory> {
 public:
  virtual std::unique_ptr<NetworkFetcher> Create() const = 0;

 protected:
  friend class base::RefCounted<NetworkFetcherFactory>;
  NetworkFetcherFactory() = default;
  virtual ~NetworkFetcherFactory() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkFetcherFactory);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_NETWORK_H_
