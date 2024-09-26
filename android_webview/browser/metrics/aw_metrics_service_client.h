// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_AW_METRICS_SERVICE_CLIENT_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_AW_METRICS_SERVICE_CLIENT_H_

#include <memory>
#include <string>

#include "android_webview/browser/lifecycle/webview_app_state_observer.h"
#include "android_webview/common/metrics/app_package_name_logging_rule.h"
#include "base/metrics/field_trial.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/embedder_support/android/metrics/android_metrics_service_client.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace android_webview {

namespace prefs {
extern const char kMetricsAppPackageNameLoggingRule[];
extern const char kAppPackageNameLoggingRuleLastUpdateTime[];
}  // namespace prefs

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// TODO(https://crbug.com/1012025): remove this when the kInstallDate pref has
// been persisted for one or two milestones. Visible for testing.
enum class BackfillInstallDate {
  kValidInstallDatePref = 0,
  kCouldNotGetPackageManagerInstallDate = 1,
  kPersistedPackageManagerInstallDate = 2,
  kMaxValue = kPersistedPackageManagerInstallDate,
};

// The amount of delay before calculating and recording the app data directory
// size, intended for avoiding IO contention when an app is initializing.
//
// Visible for testing.
extern const base::TimeDelta kRecordAppDataDirectorySizeDelay;

// AwMetricsServiceClient is a singleton which manages WebView metrics
// collection.
//
// Metrics should be enabled iff all these conditions are met:
//  - The user has not opted out (controlled by GMS).
//  - The app has not opted out (controlled by manifest tag).
//  - This client is in the 2% sample (controlled by client ID hash).
// The first two are recorded in |user_consent_| and |app_consent_|, which are
// set by SetHaveMetricsConsent(). The last is recorded in |is_in_sample_|.
//
// Metrics are pseudonymously identified by a randomly-generated "client ID".
// WebView stores this in prefs, written to the app's data directory. There's a
// different such directory for each user, for each app, on each device. So the
// ID should be unique per (device, app, user) tuple.
//
// To avoid the appearance that we're doing anything sneaky, the client ID
// should only be created and retained when neither the user nor the app have
// opted out. Otherwise, the presence of the ID could give the impression that
// metrics were being collected.
//
// WebView metrics set up happens like so:
//
//   startup
//      │
//      ├────────────┐
//      │            ▼
//      │         query GMS for consent
//      ▼            │
//   Initialize()    │
//      │            ▼
//      │         SetHaveMetricsConsent()
//      │            │
//      │ ┌──────────┘
//      ▼ ▼
//   MaybeStartMetrics()
//      │
//      ▼
//   MetricsService::Start()
//
// All the named functions in this diagram happen on the UI thread. Querying GMS
// happens in the background, and the result is posted back to the UI thread, to
// SetHaveMetricsConsent(). Querying GMS is slow, so SetHaveMetricsConsent()
// typically happens after Initialize(), but it may happen before.
//
// Each path sets a flag, |init_finished_| or |set_consent_finished_|, to show
// that path has finished, and then calls MaybeStartMetrics(). When
// MaybeStartMetrics() is called the first time, it sees only one flag is true,
// and does nothing. When MaybeStartMetrics() is called the second time, it
// decides whether to start metrics.
//
// If consent was granted, MaybeStartMetrics() determines sampling by hashing
// the client ID (generating a new ID if there was none). If this client is in
// the sample, it then calls MetricsService::Start(). If consent was not
// granted, MaybeStartMetrics() instead clears the client ID, if any.
//
// Similarly, when
// `android_webview::features::kWebViewAppsPackageNamesAllowlist` is enabled,
// WebView will try to lookup the embedding app's package name in a list of apps
// whose package names are allowed to be recorded. This operation takes place on
// a background thread. The result of the lookup is then posted back on the UI
// thread and SetAppPackageNameLoggingRule() will be called. Unlike user's
// consent, the metrics service doesn't currently block on the allowlist lookup
// result. If the result isn't present at the moment of creating a metrics log,
// it assumes that the app package name isn't allowed to be logged.

class AwMetricsServiceClient : public ::metrics::AndroidMetricsServiceClient,
                               public WebViewAppStateObserver {
  friend class base::NoDestructor<AwMetricsServiceClient>;

 public:
  // This interface define the tasks that depend on the
  // android_webview/browser directory.
  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    // Not copyable or movable
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    Delegate(Delegate&&) = delete;
    Delegate& operator=(Delegate&&) = delete;

    virtual void RegisterAdditionalMetricsProviders(
        metrics::MetricsService* service) = 0;
    virtual void AddWebViewAppStateObserver(
        WebViewAppStateObserver* observer) = 0;
    virtual bool HasAwContentsEverCreated() const = 0;
  };

  // An enum to track the status of AppPackageNameLoggingRule used in
  // ShouldRecordPackageName. These values are persisted to logs. Entries should
  // not be renumbered and numeric values should never be reused.
  enum class AppPackageNameLoggingRuleStatus {
    kNotLoadedNoCache = 0,
    kNotLoadedUseCache = 1,
    kNewVersionFailedNoCache = 2,
    kNewVersionFailedUseCache = 3,
    kNewVersionLoaded = 4,
    kSameVersionAsCache = 5,
    kMaxValue = kSameVersionAsCache,
  };

  static AwMetricsServiceClient* GetInstance();
  static void SetInstance(
      std::unique_ptr<AwMetricsServiceClient> aw_metrics_service_client);

  static void RegisterMetricsPrefs(PrefRegistrySimple* registry);

  explicit AwMetricsServiceClient(std::unique_ptr<Delegate> delegate);

  AwMetricsServiceClient(const AwMetricsServiceClient&) = delete;
  AwMetricsServiceClient& operator=(const AwMetricsServiceClient&) = delete;

  ~AwMetricsServiceClient() override;

  // metrics::MetricsServiceClient
  int32_t GetProduct() override;

  // WebViewAppStateObserver
  void OnAppStateChanged(WebViewAppStateObserver::State state) override;

  // metrics::AndroidMetricsServiceClient:
  void OnMetricsStart() override;
  void OnMetricsNotStarted() override;
  int GetSampleRatePerMille() const override;
  int GetPackageNameLimitRatePerMille() override;
  void RegisterAdditionalMetricsProviders(
      metrics::MetricsService* service) override;

  // Gets the embedding app's package name if it's OK to log. Otherwise, this
  // returns the empty string.
  std::string GetAppPackageNameIfLoggable() override;

  // If `android_webview::features::kWebViewAppsPackageNamesAllowlist` is
  // enabled:
  // - It returns `true` if the app is in the list of allowed apps.
  // - It returns `false` if the app isn't in the allowlist or if the lookup
  //   operation fails or hasn't finished yet.
  //
  // If the feature isn't enabled, the default sampling behaviour in
  // `::metrics::AndroidMetricsServiceClient::ShouldRecordPackageName` is used.
  bool ShouldRecordPackageName() override;

  // Sets that the embedding app's package name is allowed to be recorded in
  // UMA logs. This is determened by looking up the app package name in a
  // dynamically downloaded allowlist of apps see
  // `AwAppsPackageNamesAllowlistComponentLoaderPolicy`.
  //
  // `record` If it has a null value, then it will be ignored and the cached
  //          record will be used if any.
  void SetAppPackageNameLoggingRule(
      absl::optional<AppPackageNameLoggingRule> record);

  // Get the cached record of the app package names allowlist set by
  // `SetAppPackageNameLoggingRule` if any.
  absl::optional<AppPackageNameLoggingRule>
  GetCachedAppPackageNameLoggingRule();

  // The last time the apps package name allowlist was queried from the
  // component update service, regardless if it was successful or not.
  base::Time GetAppPackageNameLoggingRuleLastUpdateTime() const;
  void SetAppPackageNameLoggingRuleLastUpdateTime(base::Time update_time);

 protected:
  // Restrict usage of the inherited AndroidMetricsServiceClient::RegisterPrefs,
  // RegisterMetricsPrefs should be used instead.
  using AndroidMetricsServiceClient::RegisterPrefs;

 private:
  bool app_in_foreground_ = false;
  base::Time time_created_;
  std::unique_ptr<Delegate> delegate_;

  absl::optional<AppPackageNameLoggingRule> cached_package_name_record_;
  AppPackageNameLoggingRuleStatus package_name_record_status_ =
      AppPackageNameLoggingRuleStatus::kNotLoadedNoCache;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_AW_METRICS_SERVICE_CLIENT_H_
