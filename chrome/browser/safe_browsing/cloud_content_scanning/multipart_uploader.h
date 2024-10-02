// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_MULTIPART_UPLOADER_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_MULTIPART_UPLOADER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_data_pipe_getter.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace safe_browsing {

class MultipartUploadRequestFactory;

// This class encapsulates the upload of a file with metadata using the
// multipart protocol. This class is neither movable nor copyable.
class MultipartUploadRequest {
 public:
  using Callback = base::OnceCallback<
      void(bool success, int http_status, const std::string& response_data)>;

  // Creates a MultipartUploadRequest, which will upload `data` to the given
  // `base_url` with `metadata` attached.
  MultipartUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const std::string& data,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Callback callback);

  // Creates a MultipartUploadRequest, which will upload the file corresponding
  // to `path` to the given `base_url` with `metadata` attached.
  MultipartUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const base::FilePath& path,
      uint64_t file_size,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Callback callback);

  // Creates a MultipartUploadRequest, which will upload the page in
  // `page_region` to the given `base_url` with `metadata` attached.
  MultipartUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      base::ReadOnlySharedMemoryRegion page_region,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Callback callback);

  MultipartUploadRequest(const MultipartUploadRequest&) = delete;
  MultipartUploadRequest& operator=(const MultipartUploadRequest&) = delete;
  MultipartUploadRequest(MultipartUploadRequest&&) = delete;
  MultipartUploadRequest& operator=(MultipartUploadRequest&&) = delete;

  virtual ~MultipartUploadRequest();

  // Start the upload. This must be called on the UI thread. When complete, this
  // will call `callback_` on the UI thread.
  virtual void Start();

  // Makes the passed `factory` the factory used to instantiate a
  // MultipartUploadRequest. Useful for tests.
  static void RegisterFactoryForTests(MultipartUploadRequestFactory* factory) {
    factory_ = factory;
  }

  static std::unique_ptr<MultipartUploadRequest> CreateStringRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const std::string& data,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      MultipartUploadRequest::Callback callback);

  static std::unique_ptr<MultipartUploadRequest> CreateFileRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const base::FilePath& file,
      uint64_t file_size,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      MultipartUploadRequest::Callback callback);

  static std::unique_ptr<MultipartUploadRequest> CreatePageRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      base::ReadOnlySharedMemoryRegion page_region,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      MultipartUploadRequest::Callback callback);

  MultipartDataPipeGetter* data_pipe_getter_for_testing() {
    return data_pipe_getter_.get();
  }

  void set_access_token(const std::string& access_token);

  void SetRequestHeaders(network::ResourceRequest* request);

 private:
  FRIEND_TEST_ALL_PREFIXES(MultipartUploadRequestTest, GeneratesCorrectBody);
  FRIEND_TEST_ALL_PREFIXES(MultipartUploadRequestTest, RetriesCorrectly);
  FRIEND_TEST_ALL_PREFIXES(MultipartUploadRequestTest,
                           EmitsNetworkRequestResponseCodeOrErrorHistogram);
  FRIEND_TEST_ALL_PREFIXES(MultipartUploadRequestTest,
                           EmitsUploadSuccessHistogram);
  FRIEND_TEST_ALL_PREFIXES(MultipartUploadRequestTest,
                           EmitsRetriesNeededHistogram);
  FRIEND_TEST_ALL_PREFIXES(MultipartUploadDataPipeRequestTest, Retries);
  FRIEND_TEST_ALL_PREFIXES(MultipartUploadDataPipeRequestTest,
                           EquivalentToStringRequest);

  // Set the boundary between parts.
  void set_boundary(const std::string& boundary) { boundary_ = boundary; }

  // Helper method to create the multipart request body.
  std::string GenerateRequestBody(const std::string& metadata,
                                  const std::string& data);

  // Called whenever a net request finishes (on success or failure).
  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body);

  // Called whenever a net request finishes (on success or failure).
  void RetryOrFinish(int net_error,
                     int response_code,
                     std::unique_ptr<std::string> response_body);

  // Called to send a single request. Is overridden in tests.
  virtual void SendRequest();
  void SendStringRequest(std::unique_ptr<network::ResourceRequest> request);
  void SendFileRequest(std::unique_ptr<network::ResourceRequest> request);
  void SendPageRequest(std::unique_ptr<network::ResourceRequest> request);

  // Called after `data_pipe_getter_` has been initialized.
  void DataPipeCreatedCallback(
      std::unique_ptr<network::ResourceRequest> request,
      std::unique_ptr<MultipartDataPipeGetter> data_pipe_getter);

  // Called by SendFileRequest and SendPageRequest after `data_pipe_getter_`
  // is known to be initialized to a correct state.
  virtual void CompleteSendRequest(
      std::unique_ptr<network::ResourceRequest> request);

  static MultipartUploadRequestFactory* factory_;

  GURL base_url_;
  std::string metadata_;

  // Indicates what the source of the data to upload is.
  const enum { STRING = 0, FILE = 1, PAGE = 2 } data_source_;

  // String of content to upload. Only populated for STRING requests.
  std::string data_;

  // Path to read the file to upload. Only populated for FILE requests.
  base::FilePath path_;

  // Memory to upload. Only populated for PAGE requests.
  base::ReadOnlySharedMemoryRegion page_region_;

  // Size of the file or page region.
  uint64_t data_size_ = 0;

  // Data pipe getter used to stream a file or a page. Only populated for the
  // corresponding requests.
  std::unique_ptr<MultipartDataPipeGetter> data_pipe_getter_;

  std::string boundary_;
  Callback callback_;

  base::TimeDelta current_backoff_;
  int retry_count_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  net::NetworkTrafficAnnotationTag traffic_annotation_;

  base::Time start_time_;

  std::string access_token_;

  base::WeakPtrFactory<MultipartUploadRequest> weak_factory_{this};
};

class MultipartUploadRequestFactory {
 public:
  virtual ~MultipartUploadRequestFactory() = default;
  virtual std::unique_ptr<MultipartUploadRequest> CreateStringRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const std::string& data,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      MultipartUploadRequest::Callback callback) = 0;
  virtual std::unique_ptr<MultipartUploadRequest> CreateFileRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const base::FilePath& path,
      uint64_t file_size,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      MultipartUploadRequest::Callback callback) = 0;
  virtual std::unique_ptr<MultipartUploadRequest> CreatePageRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      base::ReadOnlySharedMemoryRegion page_region,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      MultipartUploadRequest::Callback callback) = 0;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_MULTIPART_UPLOADER_H_
