// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_CLIENT_SIDE_DETECTION_SERVICE_BASE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_CLIENT_SIDE_DETECTION_SERVICE_BASE_H_

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "net/base/ip_address.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

class PrefService;

namespace safe_browsing {

enum class CSDModelType;

// Enum used to keep stats on classification using threshold comparison.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SBClientDetectionClassifyThresholdsResult {
  kSuccess = 0,
  kModelSizeMismatch = 1,
  kModelLabelNotFound = 2,
  kMaxValue = kModelLabelNotFound,
};

// Base class for ClientSideDetectionService to remove Blink dependencies.
class ClientSideDetectionServiceBase : public KeyedService {
 public:
  static const int kReportsIntervalDays;
  static const int kMaxReportsPerInterval;
  static const int kNegativeCacheIntervalDays;
  static const int kPositiveCacheIntervalMinutes;

  // void(GURL phishing_url, bool is_phishing,
  // std::optional<net::HttpStatusCode> response_code,
  // std::optional<IntelligentScanVerdict> intelligent_scan_verdict).
  typedef base::OnceCallback<void(GURL,
                                  bool,
                                  std::optional<net::HttpStatusCode>,
                                  std::optional<IntelligentScanVerdict>)>
      ClientReportPhishingRequestCallback;

  explicit ClientSideDetectionServiceBase(PrefService* prefs);
  ClientSideDetectionServiceBase(const ClientSideDetectionServiceBase&) =
      delete;

  ClientSideDetectionServiceBase& operator=(
      const ClientSideDetectionServiceBase&) = delete;
  ~ClientSideDetectionServiceBase() override;

  // Sends a request to the SafeBrowsing servers with the ClientPhishingRequest.
  // The URL scheme of the |url()| in the request should be HTTP.  This method
  // takes ownership of the |verdict| as well as the |callback| and calls the
  // callback once the result has come back from the server or if an error
  // occurs during the fetch.  If the service is disabled or an error occurs the
  // phishing verdict will always be false.  The callback is always called after
  // SendClientReportPhishingRequest() returns and on the same thread as
  // SendClientReportPhishingRequest() was called.  You may set |callback| to
  // NULL if you don't care about the server verdict.  If |access_token| is not
  // empty, it is set in the "Authorization: Bearer" header.
  virtual void SendClientReportPhishingRequest(
      std::unique_ptr<ClientPhishingRequest> verdict,
      ClientReportPhishingRequestCallback callback,
      const std::string& access_token) = 0;

  // Returns the model type (protobuf or flatbuffer). Virtual so that mock
  // implementation can override it.
  virtual CSDModelType GetModelType() = 0;

  // Returns the ReadOnlySharedMemoryRegion for the flatbuffer model. Virtual so
  // that mock implementation can override it.
  virtual base::ReadOnlySharedMemoryRegion GetModelSharedMemoryRegion() = 0;

  // Registers a callback that will be invoked whenever a new client-side
  // phishing model has been downloaded and is ready to be distributed.
  virtual base::CallbackListSubscription RegisterCallbackForModelUpdates(
      base::RepeatingClosure callback) = 0;

  // Returns true if the given IP address falls within a private
  // (unroutable) network block.  Pages which are hosted on these IP addresses
  // are exempt from client-side phishing detection.  This is called by the
  // ClientSideDetectionHost prior to sending the renderer a
  // SafeBrowsingMsg_StartPhishingDetection IPC.
  virtual bool IsPrivateIPAddress(const net::IPAddress& address) const;

  // Returns true and sets is_phishing if url is in the cache and valid.
  virtual bool GetValidCachedResult(const GURL& url, bool* is_phishing);

  // Returns true if we have sent at least kMaxReportsPerInterval phishing
  // reports in the last kReportsInterval.
  virtual bool AtPhishingReportLimit();

 protected:
  friend class ClientSideDetectionServiceTest;
  friend class ClientSideDetectionServiceBaseTest;
  // CacheState holds all information necessary to respond to a caller without
  // actually making a HTTP request.
  struct CacheState {
    bool is_phishing;
    base::Time timestamp;

    CacheState(bool phish, base::Time time);
  };

  // Helper methods to allow subclasses to mutate the cache safely.
  void AddCacheEntry(const GURL& url, bool is_phishing, base::Time timestamp);
  void ClearCache();

  // Invalidate cache results which are no longer useful.
  void UpdateCache();

  // Get the number of phishing reports that we have sent over kReportsInterval.
  int GetPhishingNumReports();

  // Returns true if we can successfully add a phishing report to
  // |phishing_report_times_| and stores the result in prefs. Returns false if
  // we're at the ping limit or prefs is null.
  bool AddPhishingReport(base::Time timestamp);

  // Populates |phishing_report_times_| with the data stored in local prefs.
  void LoadPhishingReportTimesFromPrefs();

 private:
  // Cache of completed requests. Used to satisfy requests for the same urls
  // as long as the next request falls within our caching window (which is
  // determined by kNegativeCacheInterval and kPositiveCacheInterval). The
  // size of this cache is limited by kMaxReportsPerDay *
  // ceil(InDays(max(kNegativeCacheInterval, kPositiveCacheInterval))).
  // TODO(gcasto): Serialize this so that it doesn't reset on browser restart.
  std::map<GURL, std::unique_ptr<CacheState>> cache_;

  raw_ptr<PrefService> prefs_ = nullptr;

  // Timestamp of when we sent a phishing request. Used to limit the number
  // of phishing requests that we send in a day.
  std::deque<base::Time> phishing_report_times_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_CLIENT_SIDE_DETECTION_SERVICE_BASE_H_
