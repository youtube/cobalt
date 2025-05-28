// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_URL_FILTER_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_URL_FILTER_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/safe_search_api/url_checker.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/supervised_user_error_page.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

class PrefService;

namespace version_info {
enum class Channel;
}

namespace supervised_user {

// This class manages the filtering behavior for URLs, i.e. it tells callers
// if a URL should be allowed or blocked. It uses information
// from multiple sources:
//   * A default setting (allow or block).
//   * User-specified manual overrides (allow or block) for either sites
//     (hostnames) or exact URLs, which take precedence over the previous
//     sources.
class SupervisedUserURLFilter {
 public:
  // This enum describes whether the approved list or blocked list is used on
  // Chrome on Chrome OS, which is set by Family Link App or at
  // families.google.com/families via "manage sites" setting. This is also
  // referred to as manual behavior/filter as parent need to add everything one
  // by one. These values are logged to UMA. Entries should not be renumbered
  // and numeric values should never be reused. Please keep in sync with
  // "FamilyLinkManagedSiteList" in src/tools/metrics/histograms/enums.xml.
  enum class ManagedSiteList {
    // The web filter has both empty blocked and approved list.
    kEmpty = 0,

    // The web filter has approved list only.
    kApprovedListOnly = 1,

    // The web filter has blocked list only.
    kBlockedListOnly = 2,

    // The web filter has both approved list and blocked list.
    kBoth = 3,

    // Used for UMA. Update kMaxValue to the last value. Add future entries
    // above this comment. Sync with enums.xml.
    kMaxValue = kBoth,
  };

  // This enum describes the kind of conflicts between allow and block list
  // entries that match a given input host and resolve to different filtering
  // results.
  // They distinguish between conflicts:
  // 1) entirely due to trivial subdomain differences,
  // 2) due to differences other than the trivial subdomain and
  // 3) due to both kinds of differences.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "FamilyLinkFilteringSubdomainConflictType" in
  // src/tools/metrics/histograms/enums.xml.
  enum class FilteringSubdomainConflictType {
    kTrivialSubdomainConflictOnly = 0,
    kOtherConflictOnly = 1,
    kTrivialSubdomainConflictAndOtherConflict = 2,
    kMaxValue = kTrivialSubdomainConflictAndOtherConflict,
  };

  // Represents the result of url filtering request.
  struct Result {
    // The URL that was subject to filtering
    GURL url;
    // How the URL should be handled.
    FilteringBehavior behavior;
    // Why the URL is handled as indicated in `behavior`.
    FilteringBehaviorReason reason;
    // Details of asynchronous check if it was performed, otherwise empty.
    std::optional<safe_search_api::ClassificationDetails> async_check_details;

    bool IsFromManualList() const {
      return reason == FilteringBehaviorReason::MANUAL;
    }
    bool IsFromDefaultSetting() const {
      return reason == FilteringBehaviorReason::DEFAULT;
    }

    // True when the result of the classification means that the url is safe.
    // See `::IsClassificationSuccessful` for caveats.
    bool IsAllowed() const { return behavior == FilteringBehavior::kAllow; }
    // True when the result of the classification means that the url is not
    // safe. See `::IsClassificationSuccessful` for caveats.
    bool IsBlocked() const { return behavior == FilteringBehavior::kBlock; }

    // True when remote classification was successful. It's useful to understand
    // if the result is "allowed" because the classification succeeded, or
    // because it failed and the system uses a default classification.
    bool IsClassificationSuccessful() const {
      return !async_check_details.has_value() ||
             async_check_details->reason !=
                 safe_search_api::ClassificationDetails::Reason::
                     kFailedUseDefault;
    }
  };

  // Provides access to functionality from services on which we don't want
  // to depend directly.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Returns true if the webstore extension URL is eligible for downloading
    // for a supervised user.
    virtual bool SupportsWebstoreURL(const GURL& url) const = 0;
  };

  using ResultCallback = base::OnceCallback<void(Result)>;

  class Observer {
   public:
    // Called whenever a check started via
    // GetFilteringBehaviorWithAsyncChecks completes.
    virtual void OnURLChecked(Result result) {}
  };

  SupervisedUserURLFilter(PrefService& user_prefs,
                          std::unique_ptr<Delegate> delegate);

  virtual ~SupervisedUserURLFilter();

  static const char* GetWebFilterTypeHistogramNameForTest();
  static const char* GetManagedSiteListHistogramNameForTest();
  static const char* GetApprovedSitesCountHistogramNameForTest();
  static const char* GetBlockedSitesCountHistogramNameForTest();
  static const char* GetManagedSiteListConflictHistogramNameForTest();
  static const char* GetManagedSiteListConflictTypeHistogramNameForTest();

  static FilteringBehavior BehaviorFromInt(int behavior_value);

  // Returns true if the |host| matches the pattern. A pattern is a hostname
  // with one or both of the following modifications:
  // - If the pattern starts with "*.", it matches the host or any subdomain
  //   (e.g. the pattern "*.google.com" would match google.com, www.google.com,
  //   or accounts.google.com).
  // - If the pattern ends with ".*", it matches the host on any known TLD
  //   (e.g. the pattern "google.*" would match google.com or google.co.uk).
  // If the |host| starts with "www." but the |pattern| starts with neither
  // "www." nor "*.", the function strips the "www." part of |host| and tries to
  // match again. See the SupervisedUserURLFilterTest.HostMatchesPattern unit
  // test for more examples. Asterisks in other parts of the pattern are not
  // allowed. |host| and |pattern| are assumed to be normalized to lower-case.
  // This method is public for testing.
  static bool HostMatchesPattern(const std::string& canonical_host,
                                 const std::string& pattern);

  // Returns the filtering status for a given URL which includes both behavior
  // and reason, based on the default behavior and whether it is on a site list.
  virtual Result GetFilteringBehavior(const GURL& url);

  // Like `GetFilteringBehavior`, but also includes asynchronous checks
  // against a remote service. If the result is already determined by the
  // synchronous checks, then `callback` will be called synchronously.
  // Returns true if `callback` was called synchronously. If
  // `skip_manual_parent_filter` is set to true, it only uses the asynchronous
  // safe search checks.
  //
  // The `url` argument is included in `callback` invocation.
  //
  // `context` and `transition_type` parameters control metric-related flow:
  // `transition_type` is used to record detailed metric in the navigation
  // throttle `context`; otherwise high-level metrics are split by `context`.
  bool GetFilteringBehaviorWithAsyncChecks(
      const GURL& url,
      ResultCallback callback,
      bool skip_manual_parent_filter,
      FilteringContext filtering_context = FilteringContext::kDefault,
      std::optional<ui::PageTransition> transition_type = std::nullopt);

  // Like `GetFilteringBehaviorWithAsyncChecks` but used for subframes. The
  // `url` argument is included in `callback` invocation.
  bool GetFilteringBehaviorForSubFrameWithAsyncChecks(
      const GURL& url,
      const GURL& main_frame_url,
      ResultCallback callback,
      FilteringContext filtering_context = FilteringContext::kDefault,
      std::optional<ui::PageTransition> transition_type = std::nullopt);

  // Sets the filtering behavior for pages not on a list (default is ALLOW).
  void SetDefaultFilteringBehavior(FilteringBehavior behavior);

  FilteringBehavior GetDefaultFilteringBehavior() const;

  // Sets the set of manually allowed or blocked hosts.
  void SetManualHosts(std::map<std::string, bool> host_map);

  bool IsManualHostsEmpty() const;

  // Sets the set of manually allowed or blocked URLs.
  void SetManualURLs(std::map<GURL, bool> url_map);

  // Removes all filter entries, clears the async checker if present, and resets
  // the default behavior to "allow".
  void Clear();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  WebFilterType GetWebFilterType() const;

  // Emits URL filter metrics based on the parent web filter configuration
  // applied to the supervised user. Returns true if one or more metrics were
  // emitted.
  bool EmitURLFilterMetrics() const;

  // Reports FamilyUser.WebFilterType metrics based on parent web filter type
  // configuration.
  void ReportWebFilterTypeMetrics() const;

  // Reports FamilyUser.ManagedSiteList metrics based on parent web filter allow
  // and blocklist configuration.
  void ReportManagedSiteListMetrics() const;

  // Set value for `is_filter_initialized_`.
  void SetFilterInitialized(bool is_filter_initialized);

  // Sets safe_search_api::URLCheckerClient for SafeSites classification.
  void SetURLCheckerClient(
      std::unique_ptr<safe_search_api::URLCheckerClient> url_checker_client);

  // Checks if an exact match for a host exists in the host blocklist.
  bool IsHostInBlocklist(const std::string& host) const;

 private:
  friend class SupervisedUserURLFilterTest;
  friend class SupervisedUserURLFilteringWithConflictsTest;

  bool IsExemptedFromGuardianApproval(const GURL& effective_url);

  virtual bool RunAsyncChecker(const GURL& url, ResultCallback callback) const;

  FilteringBehavior GetManualFilteringBehaviorForURL(const GURL& url);

  // `requested_url` is the system's input, and `checked_url` is what was sent
  // to the remote async service. Since synchronous checks use the same
  // normalization methods but report actual requested url as filtered, the
  // async checks will behave the same. Normalized url that is sent to async
  // services is implementation detail.
  void CheckCallback(ResultCallback callback,
                     const GURL& requested_url,
                     const GURL& checked_url,
                     safe_search_api::Classification classification,
                     safe_search_api::ClassificationDetails details) const;

  void NotifyCallerAndObservers(ResultCallback callback, Result result) const;

  base::ObserverList<Observer>::Unchecked observers_;

  FilteringBehavior default_behavior_;

  // Maps from a URL to whether it is manually allowed (true) or blocked
  // (false).
  std::map<GURL, bool> url_map_;

  // Blocked and Allowed host lists.
  std::set<std::string> blocked_host_list_;
  std::set<std::string> allowed_host_list_;

  const raw_ref<PrefService> user_prefs_;

  std::unique_ptr<Delegate> delegate_;

  std::unique_ptr<safe_search_api::URLChecker> async_url_checker_;

  SEQUENCE_CHECKER(sequence_checker_);

  bool is_filter_initialized_ = false;

  base::WeakPtrFactory<SupervisedUserURLFilter> weak_ptr_factory_{this};
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_URL_FILTER_H_
