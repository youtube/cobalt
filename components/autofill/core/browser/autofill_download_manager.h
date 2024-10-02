// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DOWNLOAD_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DOWNLOAD_MANAGER_H_

#include <stddef.h>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/signatures.h"
#include "components/variations/variations_ids_provider.h"
#include "components/version_info/channel.h"
#include "net/base/backoff_entry.h"
#include "net/base/isolation_info.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

class PrefService;

namespace autofill {

class AutofillClient;
class LogManager;

constexpr size_t kMaxQueryGetSize = 10240;  // 10 KiB

// A helper to make sure that tests which modify the set of active autofill
// experiments do not interfere with one another.
struct ScopedActiveAutofillExperiments {
  ScopedActiveAutofillExperiments();
  ~ScopedActiveAutofillExperiments();
};

// Handles getting and updating Autofill heuristics.
class AutofillDownloadManager {
 public:
  enum RequestType {
    REQUEST_QUERY,
    REQUEST_UPLOAD,
  };

  // An interface used to notify clients of AutofillDownloadManager.
  class Observer {
   public:
    // Called when field type predictions are successfully received from the
    // server. |response| contains the server response for the forms
    // represented by |queried_form_signatures|.
    virtual void OnLoadedServerPredictions(
        std::string response,
        const std::vector<FormSignature>& queried_form_signatures) = 0;

    // These notifications are used to help with testing.
    // Called when heuristic either successfully considered for upload and
    // not send or uploaded.
    virtual void OnUploadedPossibleFieldTypes() {}

    // Called when there was an error during the request.
    // |form_signature| - the signature of the requesting form.
    // |request_type| - type of request that failed.
    // |http_error| - HTTP error code.
    virtual void OnServerRequestError(FormSignature form_signature,
                                      RequestType request_type,
                                      int http_error) {}

   protected:
    virtual ~Observer() = default;
  };

  // `client` owns (and hence survives) this AutofillDownloadManager.
  // `channel` determines the value for the the Google-API-key HTTP header and
  // whether raw metadata uploading is enabled.
  AutofillDownloadManager(AutofillClient* client,
                          version_info::Channel channel,
                          LogManager* log_manager);

  virtual ~AutofillDownloadManager();

  // Starts a query request to Autofill servers. The observer is called with the
  // list of the fields of all requested forms.
  // |forms| - array of forms aggregated in this request.
  virtual bool StartQueryRequest(const std::vector<FormStructure*>& forms,
                                 net::IsolationInfo isolation_info,
                                 base::WeakPtr<Observer> observer);

  // Starts an upload request for the given |form|.
  // |available_field_types| should contain the types for which we have data
  // stored on the local client.
  // |login_form_signature| may be empty. It is non-empty when the user fills
  // and submits a login form using a generated password. In this case,
  // |login_form_signature| should be set to the submitted form's signature.
  // Note that in this case, |form.FormSignature()| gives the signature for the
  // registration form on which the password was generated, rather than the
  // submitted form's signature.
  // |observed_submission| indicates whether the upload request is the result of
  // an observed submission event.
  virtual bool StartUploadRequest(
      const FormStructure& form,
      bool form_was_autofilled,
      const ServerFieldTypeSet& available_field_types,
      const std::string& login_form_signature,
      bool observed_submission,
      PrefService* pref_service,
      base::WeakPtr<Observer> observer);

  // Returns true if the autofill server communication is enabled.
  bool IsEnabled() const;

  // Reset the upload history. This reduced space history prevents the autofill
  // download manager from uploading a multiple votes for a given form/event
  // pair.
  static void ClearUploadHistory(PrefService* pref_service);

 protected:
  AutofillDownloadManager(AutofillClient* client,
                          const std::string& api_key,
                          bool is_raw_metadata_uploading_enabled,
                          LogManager* log_manager);

  // Gets the length of the payload from request data. Used to simulate
  // different payload sizes when testing without the need for data. Do not use
  // this when the length is needed to read/write a buffer.
  virtual size_t GetPayloadLength(base::StringPiece payload) const;

 private:
  friend class AutofillDownloadManagerTest;
  friend struct ScopedActiveAutofillExperiments;
  FRIEND_TEST_ALL_PREFIXES(AutofillDownloadManagerTest, QueryAndUploadTest);
  FRIEND_TEST_ALL_PREFIXES(AutofillDownloadManagerTest, BackoffLogic_Upload);
  FRIEND_TEST_ALL_PREFIXES(AutofillDownloadManagerTest, BackoffLogic_Query);
  FRIEND_TEST_ALL_PREFIXES(AutofillDownloadManagerTest, RetryLimit_Upload);
  FRIEND_TEST_ALL_PREFIXES(AutofillDownloadManagerTest, RetryLimit_Query);

  struct FormRequestData;
  typedef std::list<std::pair<std::vector<FormSignature>, std::string>>
      QueryRequestCache;

  // Returns the URL and request method to use when issuing the request
  // described by |request_data|. If the returned method is GET, the URL
  // fully encompasses the request, do not include request_data.payload when
  // transmitting the request.
  std::tuple<GURL, std::string> GetRequestURLAndMethod(
      const FormRequestData& request_data) const;

  // Initiates request to Autofill servers to download/upload type predictions.
  // |request_data| - form signature hash(es), request payload data and request
  //   type (query or upload).
  // Note: |request_data| takes ownership of request_data, call with std::move.
  bool StartRequest(FormRequestData request_data);

  // Each request is page visited. We store last |max_form_cache_size|
  // request, to avoid going over the wire. Set to 16 in constructor. Warning:
  // the search is linear (newest first), so do not make the constant very big.
  void set_max_form_cache_size(size_t max_form_cache_size) {
    max_form_cache_size_ = max_form_cache_size;
  }

  // Caches query request. |forms_in_query| is a vector of form signatures in
  // the query. |query_data| is the successful data returned over the wire.
  void CacheQueryRequest(const std::vector<FormSignature>& forms_in_query,
                         const std::string& query_data);
  // Returns true if query is in the cache, while filling |query_data|, false
  // otherwise. |forms_in_query| is a vector of form signatures in the query.
  bool CheckCacheForQueryRequest(
      const std::vector<FormSignature>& forms_in_query,
      std::string* query_data) const;
  // Concatenates |forms_in_query| into one signature.
  std::string GetCombinedSignature(
      const std::vector<std::string>& forms_in_query) const;

  // Returns the maximum number of attempts for a given autofill server request.
  static int GetMaxServerAttempts();

  void OnSimpleLoaderComplete(
      std::list<std::unique_ptr<network::SimpleURLLoader>>::iterator it,
      FormRequestData request_data,
      base::TimeTicks request_start,
      std::unique_ptr<std::string> response_body);

  static void InitActiveExperiments();
  static void ResetActiveExperiments();

  // The AutofillClient that this instance will use. Must not be null, and must
  // outlive this instance.
  const raw_ptr<AutofillClient> client_;

  // Callback function to retrieve API key.
  const std::string api_key_;

  // Access to leave log messages for chrome://autofill-internals, may be null.
  const raw_ptr<LogManager, DanglingUntriaged> log_manager_;  // WEAK

  // The autofill server URL root: scheme://host[:port]/path excluding the
  // final path component for the request and the query params.
  const GURL autofill_server_url_;

  // The period after which the tracked set of uploads to throttle is reset.
  const base::TimeDelta throttle_reset_period_;

  // The set of active autofill server experiments.
  static std::vector<variations::VariationID>* active_experiments_;

  // Loaders used for the processing the requests. Invalidated after completion.
  std::list<std::unique_ptr<network::SimpleURLLoader>> url_loaders_;

  // Cached QUERY requests.
  QueryRequestCache cached_forms_;
  size_t max_form_cache_size_;

  // Used for exponential backoff of requests.
  net::BackoffEntry loader_backoff_;

  // Whether form data (e.g. form and field names) can be uploaded in clear
  // text.
  bool is_raw_metadata_uploading_enabled_ = false;

  base::WeakPtrFactory<AutofillDownloadManager> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DOWNLOAD_MANAGER_H_
