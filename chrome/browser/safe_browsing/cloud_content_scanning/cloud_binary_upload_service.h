// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_H_

#include <list>
#include <queue>

#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_fcm_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_uploader.h"
#include "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"

class Profile;

namespace safe_browsing {

// This class encapsulates the process of uploading a file for deep scanning,
// and asynchronously retrieving a verdict.
class CloudBinaryUploadService : public BinaryUploadService {
 public:
  // The maximum number of uploads that can happen in parallel.
  static size_t GetParallelActiveRequestsMax();

  explicit CloudBinaryUploadService(Profile* profile);

  // This constructor is useful in tests, if you want to keep a reference to the
  // service's `binary_fcm_service_`.
  CloudBinaryUploadService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Profile* profile,
      std::unique_ptr<BinaryFCMService> binary_fcm_service);
  ~CloudBinaryUploadService() override;

  // Upload the given file contents for deep scanning if the browser is
  // authorized to upload data, otherwise queue the request.
  void MaybeUploadForDeepScanning(std::unique_ptr<Request> request) override;
  void MaybeAcknowledge(std::unique_ptr<Ack> ack) override;
  void MaybeCancelRequests(std::unique_ptr<CancelRequests> cancel) override;

  // Indicates whether the DM token/Connector combination is allowed to upload
  // data.
  using AuthorizationCallback = base::OnceCallback<void(bool)>;
  void IsAuthorized(const GURL& url,
                    bool per_profile_request,
                    AuthorizationCallback callback,
                    const std::string& dm_token,
                    enterprise_connectors::AnalysisConnector connector);

  // Run every matching callback in `authorization_callbacks_` and remove them.
  void RunAuthorizationCallbacks(
      const std::string& dm_token,
      enterprise_connectors::AnalysisConnector connector);

  // Resets `can_upload_data_`. Called every 24 hour by `timer_`.
  void ResetAuthorizationData(const GURL& url);

  // Performs cleanup needed at shutdown.
  void Shutdown() override;

  // Sets `can_upload_data_` for tests.
  void SetAuthForTesting(const std::string& dm_token, bool authorized);

  // Returns the URL that requests are uploaded to. Scans for enterprise go to a
  // different URL than scans for Advanced Protection users and Enhanced
  // Protection users.
  static GURL GetUploadUrl(bool is_consumer_scan_eligible);

 protected:
  void FinishRequest(Request* request,
                     Result result,
                     enterprise_connectors::ContentAnalysisResponse response);

 private:
  using TokenAndConnector =
      std::pair<std::string, enterprise_connectors::AnalysisConnector>;
  friend class CloudBinaryUploadServiceTest;

  // Queue the file for deep scanning. This method should be the only caller of
  // UploadForDeepScanning to avoid consuming too many user resources.
  void QueueForDeepScanning(std::unique_ptr<Request> request);

  // Upload the given file contents for deep scanning. The results will be
  // returned asynchronously by calling `request`'s `callback`. This must be
  // called on the UI thread.
  virtual void UploadForDeepScanning(std::unique_ptr<Request> request);

  // This may destroy `request`.
  void OnGetInstanceID(Request* request, const std::string& token);

  void OnGetAccessToken(Request* request, const std::string& access_token);

  // This may destroy `request`.
  void OnGetRequestData(Request* request, Result result, Request::Data data);

  void OnUploadComplete(Request* request,
                        bool success,
                        int http_status,
                        const std::string& response_data);

  void OnGetResponse(Request* request,
                     enterprise_connectors::ContentAnalysisResponse response);

  void MaybeFinishRequest(Request* request);

  void OnTimeout(Request* request);

  bool IsActive(Request* request);

  void MaybeUploadForDeepScanningCallback(std::unique_ptr<Request> request,
                                          bool authorized);

  // Callback once the response from the backend is received.
  void ValidateDataUploadRequestConnectorCallback(
      const std::string& dm_token,
      enterprise_connectors::AnalysisConnector connector,
      CloudBinaryUploadService::Result result,
      enterprise_connectors::ContentAnalysisResponse response);

  // Callback once a request's instance ID is unregistered.
  void InstanceIDUnregisteredCallback(
      const std::string& dm_token,
      enterprise_connectors::AnalysisConnector connector,
      bool);

  void RecordRequestMetrics(Request* request, Result result);
  void RecordRequestMetrics(
      Request* request,
      Result result,
      const enterprise_connectors::ContentAnalysisResponse& response);

  // Called at the end of the FinishRequest method.
  void FinishRequestCleanup(Request* request, const std::string& instance_id);

  // Tries to start uploads from `request_queue_` depending on the number of
  // currently active requests. This should be called whenever
  // `active_requests_` shrinks so queued requests are started as soon as
  // possible.
  void PopRequestQueue();

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<BinaryFCMService> binary_fcm_service_;

  const raw_ptr<Profile> profile_;

  // Request queued for upload.
  std::queue<std::unique_ptr<Request>> request_queue_;

  // Resources associated with an in-progress request.
  base::flat_map<Request*, std::unique_ptr<Request>> active_requests_;
  base::flat_map<Request*, base::TimeTicks> start_times_;
  base::flat_map<Request*, std::unique_ptr<base::OneShotTimer>> active_timers_;
  base::flat_map<Request*, std::unique_ptr<MultipartUploadRequest>>
      active_uploads_;
  base::flat_map<Request*, std::string> active_tokens_;

  // Maps requests to each corresponding tag-result pairs.
  base::flat_map<
      Request*,
      base::flat_map<std::string,
                     enterprise_connectors::ContentAnalysisResponse::Result>>
      received_connector_results_;

  // Indicates whether this DM token + Connector combination can be used to
  // upload data for enterprise requests. Advanced Protection scans are
  // validated using the user's Advanced Protection enrollment status.
  base::flat_map<TokenAndConnector, bool> can_upload_enterprise_data_;

  // Callbacks waiting on IsAuthorized request. These are organized by DM token
  // and Connector.
  base::flat_map<TokenAndConnector, std::list<base::OnceCallback<void(bool)>>>
      authorization_callbacks_;

  // Indicates if this service is waiting on the backend to validate event
  // reporting. Used to avoid spamming the backend.
  base::flat_set<TokenAndConnector> pending_validate_data_upload_request_;

  // Ensures we validate the browser is registered with the backend every 24
  // hours.
  base::RepeatingTimer timer_;

  // Used to obtain an access token to attach to requests.
  std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher_;

  base::WeakPtrFactory<CloudBinaryUploadService> weakptr_factory_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_H_
