// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_NETWORK_H_
#define COMPONENTS_UPDATE_CLIENT_NETWORK_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace update_client {

class NetworkFetcher {
 public:
  // If the request does not have an X-Retry-After header, implementations
  // should pass -1 for |xheader_retry_after_sec|.
  using PostRequestCompleteCallback =
      base::OnceCallback<void(std::unique_ptr<std::string> response_body,
                              int net_error,
                              const std::string& header_etag,
                              const std::string& header_x_cup_server_proof,
                              int64_t xheader_retry_after_sec)>;
#if defined(IN_MEMORY_UPDATES)
  using DownloadToStringCompleteCallback = base::OnceCallback<
      void(std::string* dst, int net_error, int64_t content_size)>;
#else
  using DownloadToFileCompleteCallback = base::OnceCallback<
      void(int net_error, int64_t content_size)>;
#endif

  // `content_length` is -1 if the value is not known.
  using ResponseStartedCallback =
      base::OnceCallback<void(int response_code, int64_t content_length)>;

  // `current` is the number of bytes received thus far.
  using ProgressCallback = base::RepeatingCallback<void(int64_t current)>;

  // The following two headers carry the ECSDA signature of the POST response,
  // if signing has been used. Two headers are used for redundancy purposes.
  // The value of the `X-Cup-Server-Proof` is preferred.
  static constexpr char kHeaderEtag[] = "ETag";
  static constexpr char kHeaderXCupServerProof[] = "X-Cup-Server-Proof";

  // The server uses the optional X-Retry-After header to indicate that the
  // current request should not be attempted again.
  //
  // The value of the header is the number of seconds to wait before trying to
  // do a subsequent update check. Only the values retrieved over HTTPS are
  // trusted.
  static constexpr char kHeaderXRetryAfter[] = "X-Retry-After";

  NetworkFetcher(const NetworkFetcher&) = delete;
  NetworkFetcher& operator=(const NetworkFetcher&) = delete;

  virtual ~NetworkFetcher() = default;

  virtual void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
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

#if BUILDFLAG(IS_STARBOARD)
  virtual void Cancel() = 0;
#endif

 protected:
  NetworkFetcher() = default;
};

class NetworkFetcherFactory
    : public base::RefCountedThreadSafe<NetworkFetcherFactory> {
 public:
  virtual std::unique_ptr<NetworkFetcher> Create() const = 0;

  NetworkFetcherFactory(const NetworkFetcherFactory&) = delete;
  NetworkFetcherFactory& operator=(const NetworkFetcherFactory&) = delete;

 protected:
  friend class base::RefCountedThreadSafe<NetworkFetcherFactory>;
  NetworkFetcherFactory() = default;
  virtual ~NetworkFetcherFactory() = default;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_NETWORK_H_
