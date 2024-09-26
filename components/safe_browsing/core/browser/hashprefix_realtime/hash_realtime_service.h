// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_HASH_REALTIME_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_HASH_REALTIME_SERVICE_H_

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/utils/backoff_operator.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5_alpha1.pb.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/oblivious_http_request.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace net {
struct NetworkTrafficAnnotationTag;
class HttpResponseHeaders;
}

namespace network {
struct ResourceRequest;
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {

using HPRTLookupRequestCallback =
    base::OnceCallback<void(std::unique_ptr<V5::SearchHashesRequest>)>;

using HPRTLookupResponseCallback =
    base::OnceCallback<void(bool, absl::optional<SBThreatType>, SBThreatType)>;

class OhttpKeyService;
class VerdictCacheManager;

// This class implements the backoff logic, cache logic, and lookup request for
// hash-prefix real-time lookups. For testing purposes, the request is currently
// sent to the Safe Browsing server directly. If the
// SafeBrowsingHashRealTimeOverOhttp flag is enabled, the request will be sent
// to a proxy via OHTTP
// (https://www.ietf.org/archive/id/draft-thomson-http-oblivious-01.html) in
// order to anonymize the source of the requests.
class HashRealTimeService : public KeyedService {
 public:
  HashRealTimeService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::RepeatingCallback<network::mojom::NetworkContext*()>
          get_network_context,
      VerdictCacheManager* cache_manager,
      OhttpKeyService* ohttp_key_service,
      base::RepeatingCallback<bool()> get_is_enhanced_protection_enabled);

  HashRealTimeService(const HashRealTimeService&) = delete;
  HashRealTimeService& operator=(const HashRealTimeService&) = delete;

  ~HashRealTimeService() override;

  // This function is only currently used for the hash-prefix real-time lookup
  // experiment. Once the experiment is complete, it will be deprecated.
  // TODO(crbug.com/1410253): Deprecate this (including the factory populating
  // it).
  bool IsEnhancedProtectionEnabled();

  // Start the lookup for |url|, and call |response_callback| on
  // |callback_task_runner| when response is received.
  virtual void StartLookup(
      const GURL& url,
      HPRTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner);

  // KeyedService:
  // Called before the actual deletion of the object.
  void Shutdown() override;

  // Helper function to return a weak pointer.
  base::WeakPtr<HashRealTimeService> GetWeakPtr();

 private:
  friend class HashRealTimeServiceTest;
  friend class HashRealTimeServiceDirectFetchTest;
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest, TestLookupFailure_NetError);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestLookupFailure_RetriableNetError);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestLookupFailure_NetErrorHttpCodeFailure);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestLookupFailure_OuterResponseCodeError);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestLookupFailure_InnerResponseCodeError);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestLookupFailure_ParseResponse);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestLookupFailure_IncorrectFullHashLength);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestLookupFailure_MissingCacheDuration);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest, TestBackoffModeSet);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestBackoffModeSet_RetriableError);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestBackoffModeSet_MissingOhttpKey);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestBackoffModeRespected_FullyCached);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestBackoffModeRespected_NotCached);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceTest,
                           TestLogSearchCacheWithNoQueryParamsMetric);
  FRIEND_TEST_ALL_PREFIXES(HashRealTimeServiceDirectFetchTest,
                           TestLookupFailure_RetriableNetError);

  constexpr static int kLeastSeverity = std::numeric_limits<int>::max();
  using PendingHPRTLookupRequests =
      base::flat_set<std::unique_ptr<network::SimpleURLLoader>,
                     base::UniquePtrComparator>;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class OperationResult {
    // The lookup was successful.
    kSuccess = 0,
    // Parsing the response to a string failed.
    kParseError = 1,
    // There was no cache duration in the parsed response.
    kNoCacheDurationError = 2,
    // At least one full hash in the parsed response had the wrong length.
    kIncorrectFullHashLengthError = 3,
    // There was a retriable error.
    kRetriableError = 4,
    // There was an error in the network stack.
    kNetworkError = 5,
    // There was an error in the HTTP response code.
    kHttpError = 6,
    // There is a bug in the code leading to a NOTREACHED branch.
    kNotReached = 7,
    kMaxValue = kNotReached,
  };

  // Returns the traffic annotation tag that is attached in the simple URL
  // loader.
  net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() const;

  // Get a resource request with URL, load_flags, credentials mode, and method
  // set.
  std::unique_ptr<network::ResourceRequest> GetDirectFetchResourceRequest(
      std::unique_ptr<V5::SearchHashesRequest> request) const;

  // Get the URL that will return a response containing full hashes.
  std::string GetResourceUrl(
      std::unique_ptr<V5::SearchHashesRequest> request) const;

  // Callback for getting the OHTTP key. Most parameters are used by
  // |OnURLLoaderComplete|, see the description above |OnURLLoaderComplete| for
  // details. |key| is returned from |ohttp_key_service_|.
  void OnGetOhttpKey(
      std::unique_ptr<V5::SearchHashesRequest> request,
      const GURL& url,
      const std::vector<std::string>& hash_prefixes_in_request,
      std::vector<V5::FullHash> result_full_hashes,
      base::TimeTicks request_start_time,
      scoped_refptr<base::SequencedTaskRunner> response_callback_task_runner,
      HPRTLookupResponseCallback response_callback,
      SBThreatType locally_cached_results_threat_type,
      absl::optional<std::string> key);

  // Callback for requests sent via OHTTP. Most parameters are used by
  // |OnURLLoaderComplete|, see the description above |OnURLLoaderComplete| for
  // details. |response_body|, |net_error|, |response_code| and |headers| are
  // returned from the OHTTP client. |ohttp_key| is sent to the key service.
  void OnOhttpComplete(
      const GURL& url,
      const std::vector<std::string>& hash_prefixes_in_request,
      std::vector<V5::FullHash> result_full_hashes,
      base::TimeTicks request_start_time,
      scoped_refptr<base::SequencedTaskRunner> response_callback_task_runner,
      HPRTLookupResponseCallback response_callback,
      SBThreatType locally_cached_results_threat_type,
      std::string ohttp_key,
      const absl::optional<std::string>& response_body,
      int net_error,
      int response_code,
      scoped_refptr<net::HttpResponseHeaders> headers);

  // Callback for requests sent directly to the Safe Browsing server. Most
  // parameters are used by |OnURLLoaderComplete|, see the description above
  // |OnURLLoaderComplete| for details. |url_loader| is the loader that was used
  // to send the request. |response_body| is returned from the
  // URL loader.
  void OnDirectURLLoaderComplete(
      const GURL& url,
      const std::vector<std::string>& hash_prefixes_in_request,
      std::vector<V5::FullHash> result_full_hashes,
      network::SimpleURLLoader* url_loader,
      base::TimeTicks request_start_time,
      scoped_refptr<base::SequencedTaskRunner> response_callback_task_runner,
      HPRTLookupResponseCallback response_callback,
      SBThreatType locally_cached_results_threat_type,
      std::unique_ptr<std::string> response_body);

  // Called when the response from the Safe Browsing V5 remote endpoint is
  // received. This is responsible for parsing the response, determining if
  // there were errors and updating backoff if relevant, caching the results,
  // determining the most severe threat type, and calling the callback.
  //  - |url| is used to match the full hashes in the response with the URL's
  //    full hashes.
  //  - |hash_prefixes_in_request| is used to cache the mapping of the requested
  //    hash prefixes to the results.
  //  - |result_full_hashes| starts out as the initial results from the cache.
  //    This method mutates this parameter to include the results from the
  //    server response as well, and then uses the combined results to determine
  //    the most severe threat type.
  //  - |request_start_time| represents when the request was sent, and is used
  //    for logging.
  //  - |response_callback_task_runner| is the callback the original caller
  //    passed through that will be called when the method completes.
  //  - |response_callback| is the callback the original caller passed through.
  //  - |locally_cached_results_threat_type| is the threat type based on locally
  //    cached results only. This is only used for logging purposes.
  //  - |response_body| is the unparsed response from the server.
  //  - |net_error| is the net error code from the server.
  //  - |response_code| is the HTTP status code from the server.
  //  - |allow_retriable_errors| specifies whether certain types of errors can
  //    be considered retriable, meaning they don't increment the backoff
  //    counter.
  void OnURLLoaderComplete(
      const GURL& url,
      const std::vector<std::string>& hash_prefixes_in_request,
      std::vector<V5::FullHash> result_full_hashes,
      base::TimeTicks request_start_time,
      scoped_refptr<base::SequencedTaskRunner> response_callback_task_runner,
      HPRTLookupResponseCallback response_callback,
      SBThreatType locally_cached_results_threat_type,
      std::unique_ptr<std::string> response_body,
      int net_error,
      int response_code,
      bool allow_retriable_errors);

  // Determines the most severe threat type based on |result_full_hashes|, which
  // contains the merged caching and server response results. The |url| is
  // required in order to filter down |result_full_hashes| to ones that match
  // the |url| full hashes. |log_threat_info_size| determines whether to log
  // SafeBrowsing.HPRT.ThreatInfoSize during the method call.
  static SBThreatType DetermineSBThreatType(
      const GURL& url,
      const std::vector<V5::FullHash>& result_full_hashes,
      // TODO(crbug.com/1410253): Deprecate this once the experiment is
      // complete.
      bool log_threat_info_size);

  // Returns a number representing the severity of the threat type. The lower
  // the number, the more severe it is. Severity is used to narrow down to a
  // single threat type to report in cases where there are multiple.
  static int GetThreatSeverity(const V5::ThreatType& threat_type);

  // Returns true if the |threat_type| is more severe than the
  // |baseline_severity|. Returns false if it's less severe or has equal
  // severity.
  static bool IsThreatTypeMoreSevere(const V5::ThreatType& threat_type,
                                     int baseline_severity);

  // In addition to attempting to parse the |response_body| as described in the
  // |ParseResponse| function comments, this updates the backoff state depending
  // on the lookup success.
  base::expected<std::unique_ptr<V5::SearchHashesResponse>, OperationResult>
  ParseResponseAndUpdateBackoff(
      int net_error,
      int http_error,
      std::unique_ptr<std::string> response_body,
      const std::vector<std::string>& requested_hash_prefixes,
      bool allow_retriable_errors) const;

  // Tries to parse the |response_body| into a |SearchHashesResponse|, and
  // returns either the response proto or an |OperationResult| with details on
  // why the parsing was unsuccessful. |requested_hash_prefixes| is used for a
  // sanitization call into |RemoveUnmatchedFullHashes|.
  // |allow_retriable_errors| specifies whether certain types of errors can be
  // considered retriable, meaning they don't increment the backoff counter.
  base::expected<std::unique_ptr<V5::SearchHashesResponse>, OperationResult>
  ParseResponse(int net_error,
                int http_error,
                std::unique_ptr<std::string> response_body,
                const std::vector<std::string>& requested_hash_prefixes,
                bool allow_retriable_errors) const;

  // Removes any |FullHash| within the |response| whose hash prefix is not found
  // within |requested_hash_prefixes|. This is not expected to occur, but is
  // handled out of caution.
  void RemoveUnmatchedFullHashes(
      std::unique_ptr<V5::SearchHashesResponse>& response,
      const std::vector<std::string>& requested_hash_prefixes) const;

  // Removes any |FullHashDetail| within the |response| that has invalid
  // |ThreatType| or |ThreatAttribute| enums. This is for forward compatibility,
  // for when the API starts returning new threat types or attributes that the
  // client's version of the code does not support.
  void RemoveFullHashDetailsWithInvalidEnums(
      std::unique_ptr<V5::SearchHashesResponse>& response) const;

  // Returns the hash prefixes for the URL's lookup expressions.
  std::set<std::string> GetHashPrefixesSet(const GURL& url) const;

  // Searches the local cache for the input |hash_prefixes|.
  //  - |skip_logging| specifies whether metric logging should be skipped when
  //    this function is called.
  //  - |out_missing_hash_prefixes| is an output parameter with a list of which
  //    hash prefixes were not found in the cache and need to be requested.
  //  - |out_cached_full_hashes| is an output parameter with a list of unsafe
  //    full hashes that were found in the cache for any of the |hash_prefixes|.
  // TODO(crbug.com/1432308): [Also TODO(thefrog)] Remove |skip_logging|
  // parameter after investigation is complete.
  void SearchCache(std::set<std::string> hash_prefixes,
                   bool skip_logging,
                   std::vector<std::string>* out_missing_hash_prefixes,
                   std::vector<V5::FullHash>* out_cached_full_hashes) const;

  // Used for logging only. Records whether there would be a cache hit for all
  // requested prefixes if the URL's query parameters were excluded.
  // TODO(crbug.com/1432308): [Also TODO(thefrog)] Remove function after
  // investigation is complete.
  void LogSearchCacheWithNoQueryParamsMetric(const GURL& url) const;

  SEQUENCE_CHECKER(sequence_checker_);

  // The URLLoaderFactory we use to issue network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Fetches the NetworkContext that we use to send requests over OHTTP.
  base::RepeatingCallback<network::mojom::NetworkContext*()>
      get_network_context_;

  // Unowned object used for getting and storing cache entries.
  raw_ptr<VerdictCacheManager> cache_manager_;

  // Unowned object used for getting OHTTP key.
  raw_ptr<OhttpKeyService> ohttp_key_service_;

  // All requests that are sent directly to the server but haven't received a
  // response yet.
  PendingHPRTLookupRequests pending_requests_;

  // All pending receivers that are sent via OHTTP but haven't received a
  // response yet.
  mojo::UniqueReceiverSet<network::mojom::ObliviousHttpClient>
      ohttp_client_receivers_;

  // Helper object that manages backoff state.
  std::unique_ptr<BackoffOperator> backoff_operator_;

  // Indicates whether |Shutdown| has been called. If so, |StartLookup| returns
  // early.
  bool is_shutdown_ = false;

  // Pulls whether enhanced protection is currently enabled.
  base::RepeatingCallback<bool()> get_is_enhanced_protection_enabled_;

  base::WeakPtrFactory<HashRealTimeService> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_HASH_REALTIME_SERVICE_H_
