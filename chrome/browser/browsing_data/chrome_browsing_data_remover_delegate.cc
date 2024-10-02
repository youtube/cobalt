// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"

#include <stdint.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/autocomplete/zero_suggest_cache_service_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/strike_database_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/browsing_data/navigation_entry_remover.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/crash_upload_list/crash_upload_list.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/downgrade/user_data_downgrade.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/heavy_ad_intervention/heavy_ad_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/login_detection/login_detection_prefs.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "chrome/browser/media/media_device_id_salt.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/permissions/permission_actions_history_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/verdict_cache_manager_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/share/share_history.h"
#include "chrome/browser/share/share_ranking.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/strike_databases/strike_database.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/crash/core/app/crashpad.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/device_event_log/device_event_log.h"
#include "components/feed/buildflags.h"
#include "components/heavy_ad_intervention/heavy_ad_blocklist.h"
#include "components/heavy_ad_intervention/heavy_ad_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/common/pref_names.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/browser/pnacl_host.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/password_manager/core/browser/field_info_store.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/smart_bubble_stats_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/search_engines/template_url_service.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "components/webrtc_logging/browser/log_cleanup.h"
#include "components/webrtc_logging/browser/text_log_list.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "content/public/browser/prefetch_service_delegate.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_urls.h"
#include "media/base/media_switches.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "media/mojo/services/webrtc_video_perf_history.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_transaction_factory.h"
#include "net/net_buildflags.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/customtabs/chrome_origin_verifier.h"
#include "chrome/browser/android/explore_sites/explore_sites_service_factory.h"
#include "chrome/browser/android/oom_intervention/oom_intervention_decider.h"
#include "chrome/browser/android/webapps/webapp_registry.h"
#include "chrome/browser/feed/feed_service_factory.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/cdm/browser/media_drm_storage_impl.h"  // nogncheck crbug.com/1125897
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/feed_feature_list.h"
#include "components/installedapp/android/jni_headers/PackageHash_jni.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_model.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/net/system_proxy_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"  // nogncheck
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"  // nogncheck
#include "components/user_manager/user.h"
#include "device/fido/cros/credential_store.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
#include "device/fido/mac/credential_store.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/media/cdm_document_service_impl.h"
#endif  // BUILDFLAG(IS_WIN)

using base::UserMetricsAction;
using content::BrowserContext;
using content::BrowserThread;
using content::BrowsingDataFilterBuilder;

namespace constants = chrome_browsing_data_remover;

namespace {

// Timeout after which the histogram for slow tasks is recorded.
const base::TimeDelta kSlowTaskTimeout = base::Seconds(180);

// Generic functions but currently only used when ENABLE_NACL.
#if BUILDFLAG(ENABLE_NACL)
// Convenience method to create a callback that can be run on any thread and
// will post the given |callback| back to the UI thread.
base::OnceClosure UIThreadTrampoline(base::OnceClosure callback) {
  return base::BindPostTask(content::GetUIThreadTaskRunner({}),
                            std::move(callback));
}
#endif  // BUILDFLAG(ENABLE_NACL)

// Returned by ChromeBrowsingDataRemoverDelegate::GetOriginTypeMatcher().
bool DoesOriginMatchEmbedderMask(uint64_t origin_type_mask,
                                 const url::Origin& origin,
                                 storage::SpecialStoragePolicy* policy) {
  DCHECK_EQ(0ULL,
            origin_type_mask & (constants::ORIGIN_TYPE_EMBEDDER_BEGIN - 1))
      << "|origin_type_mask| can only contain origin types defined in "
      << "the embedder.";

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Packaged apps and extensions match iff EXTENSION.
  if ((origin.scheme() == extensions::kExtensionScheme) &&
      (origin_type_mask & constants::ORIGIN_TYPE_EXTENSION)) {
    return true;
  }
  origin_type_mask &= ~constants::ORIGIN_TYPE_EXTENSION;
#endif

  DCHECK(!origin_type_mask)
      << "DoesOriginMatchEmbedderMask must handle all origin types.";

  return false;
}

}  // namespace

ChromeBrowsingDataRemoverDelegate::ChromeBrowsingDataRemoverDelegate(
    BrowserContext* browser_context)
    : profile_(Profile::FromBrowserContext(browser_context))
#if BUILDFLAG(IS_ANDROID)
      ,
      webapp_registry_(std::make_unique<WebappRegistry>())
#endif
      ,
      credential_store_(MakeCredentialStore()) {
  domain_reliability_clearer_ = base::BindRepeating(
      [](BrowserContext* browser_context,
         content::BrowsingDataFilterBuilder* filter_builder,
         network::mojom::NetworkContext_DomainReliabilityClearMode mode,
         network::mojom::NetworkContext::ClearDomainReliabilityCallback
             callback) {
        network::mojom::NetworkContext* network_context =
            browser_context->GetDefaultStoragePartition()->GetNetworkContext();
        network_context->ClearDomainReliability(
            filter_builder->BuildNetworkServiceFilter(), mode,
            std::move(callback));
      },
      browser_context);
}

ChromeBrowsingDataRemoverDelegate::~ChromeBrowsingDataRemoverDelegate() =
    default;

void ChromeBrowsingDataRemoverDelegate::Shutdown() {
  auto* remover = profile_->GetBrowsingDataRemover();
  DCHECK(remover);
  remover->SetEmbedderDelegate(nullptr);
  profile_keep_alive_.reset();
  history_task_tracker_.TryCancelAll();
}

content::BrowsingDataRemoverDelegate::EmbedderOriginTypeMatcher
ChromeBrowsingDataRemoverDelegate::GetOriginTypeMatcher() {
  return base::BindRepeating(&DoesOriginMatchEmbedderMask);
}

bool ChromeBrowsingDataRemoverDelegate::MayRemoveDownloadHistory() {
  return profile_->GetPrefs()->GetBoolean(prefs::kAllowDeletingBrowserHistory);
}

std::vector<std::string>
ChromeBrowsingDataRemoverDelegate::GetDomainsForDeferredCookieDeletion(
    uint64_t remove_mask) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kEnablePasswordsAccountStorage)) {
    return {};
  }
  if ((remove_mask & constants::DEFERRED_COOKIE_DELETION_DATA_TYPES) == 0) {
    return {};
  }

  // Return Google and Gaia domains, so Google signout is deferred until
  // account-scoped data is deleted.
  auto urls = {GaiaUrls::GetInstance()->google_url(),
               GaiaUrls::GetInstance()->gaia_url()};
  std::set<std::string> domains;
  for (const GURL& url : urls) {
    std::string domain = GetDomainAndRegistry(
        url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    if (domain.empty())
      domain = url.host();
    domains.insert(domain);
  }
  return {domains.begin(), domains.end()};
}

void ChromeBrowsingDataRemoverDelegate::RemoveEmbedderData(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    uint64_t remove_mask,
    BrowsingDataFilterBuilder* filter_builder,
    uint64_t origin_type_mask,
    base::OnceCallback<void(uint64_t)> callback) {
  DCHECK(((remove_mask &
           ~content::BrowsingDataRemover::DATA_TYPE_AVOID_CLOSING_CONNECTIONS &
           ~constants::FILTERABLE_DATA_TYPES) == 0) ||
         filter_builder->MatchesAllOriginsAndDomains());
  DCHECK(!should_clear_password_account_storage_settings_);

  TRACE_EVENT0("browsing_data",
               "ChromeBrowsingDataRemoverDelegate::RemoveEmbedderData");

  // To detect tasks that are causing slow deletions, record running sub tasks
  // after a delay.
  slow_pending_tasks_closure_.Reset(base::BindOnce(
      &ChromeBrowsingDataRemoverDelegate::RecordUnfinishedSubTasks,
      weak_ptr_factory_.GetWeakPtr()));
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE, slow_pending_tasks_closure_.callback(), kSlowTaskTimeout);

  // Embedder-defined DOM-accessible storage currently contains only
  // one datatype, which is the durable storage permission.
  if (remove_mask &
      content::BrowsingDataRemover::DATA_TYPE_EMBEDDER_DOM_STORAGE) {
    remove_mask |= constants::DATA_TYPE_DURABLE_PERMISSION;
  }

  if (origin_type_mask &
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB) {
    base::RecordAction(
        UserMetricsAction("ClearBrowsingData_MaskContainsUnprotectedWeb"));
  }
  if (origin_type_mask &
      content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB) {
    base::RecordAction(
        UserMetricsAction("ClearBrowsingData_MaskContainsProtectedWeb"));
  }
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (origin_type_mask & constants::ORIGIN_TYPE_EXTENSION) {
    base::RecordAction(
        UserMetricsAction("ClearBrowsingData_MaskContainsExtension"));
  }
#endif
  // If this fires, we added a new BrowsingDataHelper::OriginTypeMask without
  // updating the user metrics above.
  static_assert(
      constants::ALL_ORIGIN_TYPES ==
          (content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
#if BUILDFLAG(ENABLE_EXTENSIONS)
           constants::ORIGIN_TYPE_EXTENSION |
#endif
           content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB),
      "OriginTypeMask has been updated without updating user metrics");

  //////////////////////////////////////////////////////////////////////////////
  // INITIALIZATION
  base::ScopedClosureRunner synchronous_clear_operations(
      CreateTaskCompletionClosure(TracingDataType::kSynchronous));
  callback_ = std::move(callback);

  delete_begin_ = delete_begin;
  delete_end_ = delete_end;

  failed_data_types_ = 0;

  base::RepeatingCallback<bool(const GURL& url)> filter =
      filter_builder->BuildUrlFilter();

  // Some backends support a filter that |is_null()| to make complete deletion
  // more efficient.
  base::RepeatingCallback<bool(const GURL&)> nullable_filter =
      filter_builder->MatchesAllOriginsAndDomains()
          ? base::RepeatingCallback<bool(const GURL&)>()
          : filter;

  HostContentSettingsMap::PatternSourcePredicate website_settings_filter =
      browsing_data::CreateWebsiteSettingsFilter(filter_builder);

  // Managed devices and supervised users can have restrictions on history
  // deletion.
  PrefService* prefs = profile_->GetPrefs();
  bool may_delete_history =
      prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory);

  // All the UI entry points into the BrowsingDataRemoverImpl should be
  // disabled, but this will fire if something was missed or added.
  DCHECK(may_delete_history ||
         (remove_mask & content::BrowsingDataRemover::DATA_TYPE_NO_CHECKS) ||
         (!(remove_mask & constants::DATA_TYPE_HISTORY) &&
          !(remove_mask & content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS)));

  HostContentSettingsMap* host_content_settings_map_ =
      HostContentSettingsMapFactory::GetForProfile(profile_);

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_HISTORY
  if ((remove_mask & constants::DATA_TYPE_HISTORY) && may_delete_history) {
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);
    if (history_service) {
      // TODO(dmurph): Support all backends with filter (crbug.com/113621).
      base::RecordAction(UserMetricsAction("ClearBrowsingData_History"));
      history_service->DeleteLocalAndRemoteHistoryBetween(
          WebHistoryServiceFactory::GetForProfile(profile_), delete_begin_,
          delete_end_, CreateTaskCompletionClosure(TracingDataType::kHistory),
          &history_task_tracker_);
    }
    if (ClipboardRecentContent::GetInstance())
      ClipboardRecentContent::GetInstance()->SuppressClipboardContent();

    language::UrlLanguageHistogram* language_histogram =
        UrlLanguageHistogramFactory::GetForBrowserContext(profile_);
    if (language_histogram) {
      language_histogram->ClearHistory(delete_begin_, delete_end_);
    }

#if BUILDFLAG(ENABLE_EXTENSIONS)
    // The extension activity log contains details of which websites extensions
    // were active on. It therefore indirectly stores details of websites a
    // user has visited so best clean from here as well.
    // TODO(msramek): Support all backends with filter (crbug.com/589586).
    extensions::ActivityLog::GetInstance(profile_)->RemoveURLs(
        std::set<GURL>());

    // Clear launch times as they are a form of history.
    // BrowsingDataFilterBuilder currently doesn't support extension origins.
    // Therefore, clearing history for a small set of origins (deletelist)
    // should never delete any extension launch times, while clearing for almost
    // all origins (preservelist) should always delete all of extension launch
    // times.
    if (filter_builder->MatchesAllOriginsAndDomains()) {
      extensions::ExtensionPrefs* extension_prefs =
          extensions::ExtensionPrefs::Get(profile_);
      extension_prefs->ClearLastLaunchTimes();
    }
#endif

    // Need to clear the host cache and accumulated speculative data, as it also
    // reveals some history. We have no mechanism to track when these items were
    // created, so we'll not honor the time range.
    profile_->GetDefaultStoragePartition()->GetNetworkContext()->ClearHostCache(
        filter_builder->BuildNetworkServiceFilter(),
        CreateTaskCompletionClosureForMojo(TracingDataType::kHostCache));

    // The NoStatePrefetchManager keeps history of pages scanned for prefetch,
    // so clear that. It also may have a scanned page. If so, the page could be
    // considered to have a small amount of historical information, so delete
    // it, too.
    prerender::NoStatePrefetchManager* no_state_prefetch_manager =
        prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
            profile_);
    if (no_state_prefetch_manager) {
      // TODO(dmurph): Support all backends with filter (crbug.com/113621).
      no_state_prefetch_manager->ClearData(
          prerender::NoStatePrefetchManager::CLEAR_PRERENDER_CONTENTS |
          prerender::NoStatePrefetchManager::CLEAR_PRERENDER_HISTORY);
    }

    // The saved Autofill profiles and credit cards can include the origin from
    // which these profiles and credit cards were learned.  These are a form of
    // history, so clear them as well.
    // TODO(dmurph): Support all backends with filter (crbug.com/113621).
    scoped_refptr<autofill::AutofillWebDataService> web_data_service =
        WebDataServiceFactory::GetAutofillWebDataForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);
    if (web_data_service.get()) {
      web_data_service->RemoveOriginURLsModifiedBetween(delete_begin_,
                                                        delete_end_);
      // Ask for a call back when the above call is finished.
      web_data_service->GetDBTaskRunner()->PostTaskAndReply(
          FROM_HERE, base::DoNothing(),
          CreateTaskCompletionClosure(TracingDataType::kAutofillOrigins));

      autofill::PersonalDataManager* data_manager =
          autofill::PersonalDataManagerFactory::GetForProfile(profile_);
      if (data_manager)
        data_manager->Refresh();
    }

    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(
            &webrtc_logging::DeleteOldAndRecentWebRtcLogFiles,
            webrtc_logging::TextLogList::
                GetWebRtcLogDirectoryForBrowserContextPath(profile_->GetPath()),
            delete_begin_),
        CreateTaskCompletionClosure(TracingDataType::kWebrtcLogs));

#if BUILDFLAG(IS_ANDROID)
    // Clear the history information (last launch time and origin URL) of any
    // registered webapps.
    webapp_registry_->ClearWebappHistoryForUrls(filter);

    // The ChromeOriginVerifier caches origins for Trusted Web Activities that
    // have been verified and stores them in Android Preferences.
    customtabs::ChromeOriginVerifier::ClearBrowsingData();
#endif

    heavy_ad_intervention::HeavyAdService* heavy_ad_service =
        HeavyAdServiceFactory::GetForBrowserContext(profile_);
    if (heavy_ad_service && heavy_ad_service->heavy_ad_blocklist()) {
      heavy_ad_service->heavy_ad_blocklist()->ClearBlockList(delete_begin_,
                                                             delete_end_);
    }

    OptimizationGuideKeyedService* optimization_guide_keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);
    if (optimization_guide_keyed_service)
      optimization_guide_keyed_service->ClearData();

    content::PrefetchServiceDelegate::ClearData(profile_);

#if BUILDFLAG(IS_ANDROID)
    OomInterventionDecider* oom_intervention_decider =
        OomInterventionDecider::GetForBrowserContext(profile_);
    if (oom_intervention_decider)
      oom_intervention_decider->ClearData();
#endif

    // The SSL Host State that tracks SSL interstitial "proceed" decisions may
    // include origins that the user has visited, so it must be cleared.
    // TODO(msramek): We can reuse the plugin filter here, since both plugins
    // and SSL host state are scoped to hosts and represent them as std::string.
    // Rename the method to indicate its more general usage.
    if (profile_->GetSSLHostStateDelegate()) {
      profile_->GetSSLHostStateDelegate()->Clear(
          filter_builder->MatchesAllOriginsAndDomains()
              ? base::RepeatingCallback<bool(const std::string&)>()
              : filter_builder->BuildPluginFilter());
    }

    // Clear VideoDecodePerfHistory and WebrtcVideoPerfHistory only if asked to
    // clear from the beginning of time. The perf history is a simple summing of
    // decode/encode statistics with no record of when the stats were written
    // nor what site the video was played on.
    if (IsForAllTime()) {
      // TODO(chcunningham): Add UMA to track how often this gets deleted.
      media::VideoDecodePerfHistory* video_decode_perf_history =
          profile_->GetVideoDecodePerfHistory();
      if (video_decode_perf_history) {
        video_decode_perf_history->ClearHistory(
            CreateTaskCompletionClosure(TracingDataType::kVideoDecodeHistory));
      }

      media::WebrtcVideoPerfHistory* webrtc_video_perf_history =
          profile_->GetWebrtcVideoPerfHistory();
      if (webrtc_video_perf_history) {
        webrtc_video_perf_history->ClearHistory(CreateTaskCompletionClosure(
            TracingDataType::kWebrtcVideoPerfHistory));
      }
    }

    device_event_log::Clear(delete_begin_, delete_end_);

#if BUILDFLAG(IS_ANDROID)
    explore_sites::ExploreSitesService* explore_sites_service =
        explore_sites::ExploreSitesServiceFactory::GetForBrowserContext(
            profile_);
    if (explore_sites_service) {
      explore_sites_service->ClearActivities(
          delete_begin_, delete_end_,
          CreateTaskCompletionClosure(TracingDataType::kExploreSites));
    }
#endif

    CreateCrashUploadList()->Clear(delete_begin_, delete_end_);

    FindBarStateFactory::GetForBrowserContext(profile_)->SetLastSearchText(
        std::u16string());

#if BUILDFLAG(IS_ANDROID)
    if (auto* share_history = sharing::ShareHistory::Get(profile_))
      share_history->Clear(delete_begin_, delete_end_);
    if (auto* share_ranking = sharing::ShareRanking::Get(profile_))
      share_ranking->Clear(delete_begin_, delete_end_);
#endif

    // Also clear the last used time in bookmarks.
    auto* bookmark_model = BookmarkModelFactory::GetForBrowserContext(profile_);
    if (bookmark_model && bookmark_model->loaded()) {
      bookmark_model->ClearLastUsedTimeInRange(delete_begin, delete_end);
    }

    // Cleared for DATA_TYPE_HISTORY, DATA_TYPE_COOKIES and DATA_TYPE_PASSWORDS.
    browsing_data::RemoveFederatedSiteSettingsData(delete_begin_, delete_end_,
                                                   website_settings_filter,
                                                   host_content_settings_map_);
  }

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_DOWNLOADS
  if ((remove_mask & content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS) &&
      may_delete_history) {
    DownloadPrefs* download_prefs =
        DownloadPrefs::FromDownloadManager(profile_->GetDownloadManager());
    download_prefs->SetSaveFilePath(download_prefs->DownloadPath());
  }

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_COOKIES
  // We ignore the DATA_TYPE_COOKIES request if UNPROTECTED_WEB is not set,
  // so that callers who request DATA_TYPE_SITE_DATA with PROTECTED_WEB
  // don't accidentally remove the cookies that are associated with the
  // UNPROTECTED_WEB origin. This is necessary because cookies are not separated
  // between UNPROTECTED_WEB and PROTECTED_WEB.
  if ((remove_mask & content::BrowsingDataRemover::DATA_TYPE_COOKIES) &&
      (origin_type_mask &
       content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB)) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_Cookies"));

    network::mojom::NetworkContext* safe_browsing_context = nullptr;
    safe_browsing::SafeBrowsingService* sb_service =
        g_browser_process->safe_browsing_service();
    if (sb_service)
      safe_browsing_context = sb_service->GetNetworkContext(profile_);

    // Cleared for DATA_TYPE_HISTORY, DATA_TYPE_COOKIES and DATA_TYPE_PASSWORDS.
    browsing_data::RemoveFederatedSiteSettingsData(delete_begin_, delete_end_,
                                                   website_settings_filter,
                                                   host_content_settings_map_);

    if (!filter_builder->IsCrossSiteClearSiteDataForCookies()) {
      browsing_data::RemoveEmbedderCookieData(
          delete_begin, delete_end, filter_builder, host_content_settings_map_,
          safe_browsing_context,
          base::BindOnce(
              &ChromeBrowsingDataRemoverDelegate::CreateTaskCompletionClosure,
              base::Unretained(this), TracingDataType::kCookies));
      safe_browsing::VerdictCacheManagerFactory::GetForProfile(profile_)
          ->OnCookiesDeleted();
    }

    if (filter_builder->GetMode() ==
        BrowsingDataFilterBuilder::Mode::kPreserve) {
      auto* privacy_sandbox_settings =
          PrivacySandboxSettingsFactory::GetForProfile(profile_);
      if (privacy_sandbox_settings)
        privacy_sandbox_settings->OnCookiesCleared();

      MediaDeviceIDSalt::Reset(profile_->GetPrefs());

#if BUILDFLAG(IS_ANDROID)
      Java_PackageHash_onCookiesDeleted(
          base::android::AttachCurrentThread(),
          ProfileAndroid::FromProfile(profile_)->GetJavaObject());
#endif
    }

    if (nullable_filter.is_null() ||
        (!filter_builder->IsCrossSiteClearSiteDataForCookies() &&
         nullable_filter.Run(GaiaUrls::GetInstance()->google_url()))) {
      // Set a flag to clear account storage settings later instead of clearing
      // it now as we can not reset this setting before passwords are deleted.
      should_clear_password_account_storage_settings_ = true;
    }

    // Persistent Origin Trial tokens are only saved until the next page
    // load from the same origin. For that reason, they are not saved with
    // last-modified information, so deletion will clear all stored information.
    // Sites should omit setting the Origin-Trial header to clear their
    // individual information, so rather than filtering origins, we only perform
    // the removal if we are removing information for all origins.
    if (filter_builder->MatchesAllOriginsAndDomains()) {
      content::OriginTrialsControllerDelegate* delegate =
          profile_->GetOriginTrialsControllerDelegate();
      if (delegate)
        delegate->ClearPersistedTokens();
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_CONTENT_SETTINGS
  if (remove_mask & constants::DATA_TYPE_CONTENT_SETTINGS) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_ContentSettings"));

    browsing_data::RemoveSiteSettingsData(delete_begin, delete_end,
                                          host_content_settings_map_);

    auto* handler_registry =
        ProtocolHandlerRegistryFactory::GetForBrowserContext(profile_);
    if (handler_registry)
      handler_registry->ClearUserDefinedHandlers(delete_begin_, delete_end_);

    ChromeTranslateClient::CreateTranslatePrefs(prefs)
        ->DeleteNeverPromptSitesBetween(delete_begin_, delete_end_);

    host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
        ContentSettingsType::PERMISSION_AUTOREVOCATION_DATA, delete_begin,
        delete_end, website_settings_filter);

    if (auto* privacy_sandbox_settings =
            PrivacySandboxSettingsFactory::GetForProfile(profile_)) {
      privacy_sandbox_settings->ClearFledgeJoiningAllowedSettings(delete_begin_,
                                                                  delete_end_);
      privacy_sandbox_settings->ClearTopicSettings(delete_begin_, delete_end_);
    }

#if !BUILDFLAG(IS_ANDROID)
    content::HostZoomMap* zoom_map =
        content::HostZoomMap::GetDefaultForBrowserContext(profile_);
    zoom_map->ClearZoomLevels(delete_begin, delete_end_);
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_BOOKMARKS
  if (remove_mask & constants::DATA_TYPE_BOOKMARKS) {
    auto* bookmark_model = BookmarkModelFactory::GetForBrowserContext(profile_);
    if (bookmark_model && bookmark_model->loaded()) {
      if (delete_begin_.is_null() &&
          (delete_end_.is_null() || delete_end_.is_max())) {
        bookmark_model->RemoveAllUserBookmarks();
      } else {
        // Bookmark deletion is only implemented to remove all data after a
        // profile deletion. A full implementation would need to traverse the
        // whole tree and check timestamps against delete_begin and delete_end.
        NOTIMPLEMENTED();
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_LOCAL_CUSTOM_DICTIONARY
  if (remove_mask & constants::DATA_TYPE_LOCAL_CUSTOM_DICTIONARY) {
    auto* spellcheck = SpellcheckServiceFactory::GetForContext(profile_);
    if (spellcheck) {
      auto* dict = spellcheck->GetCustomDictionary();
      if (dict)
        dict->Clear();
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_DURABLE_PERMISSION
  if (remove_mask & constants::DATA_TYPE_DURABLE_PERMISSION) {
    host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
        ContentSettingsType::DURABLE_STORAGE, base::Time(), base::Time::Max(),
        website_settings_filter);
  }

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_SITE_USAGE_DATA
  if (remove_mask & constants::DATA_TYPE_SITE_USAGE_DATA) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_SiteUsageData"));

    host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
        ContentSettingsType::SITE_ENGAGEMENT, base::Time(), base::Time::Max(),
        website_settings_filter);

    if (MediaEngagementService::IsEnabled()) {
      MediaEngagementService::Get(profile_)->ClearDataBetweenTime(delete_begin_,
                                                                  delete_end_);
    }

    PermissionActionsHistoryFactory::GetForProfile(profile_)->ClearHistory(
        delete_begin_, delete_end_);
  }

  if ((remove_mask & constants::DATA_TYPE_SITE_USAGE_DATA) ||
      (remove_mask & constants::DATA_TYPE_HISTORY)) {
    host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
        ContentSettingsType::FORMFILL_METADATA, delete_begin_, delete_end_,
        website_settings_filter);

    host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
        ContentSettingsType::APP_BANNER, base::Time(), base::Time::Max(),
        website_settings_filter);

#if !BUILDFLAG(IS_ANDROID)
    host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
        ContentSettingsType::INTENT_PICKER_DISPLAY, delete_begin_, delete_end_,
        website_settings_filter);

    host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
        ContentSettingsType::FILE_SYSTEM_LAST_PICKED_DIRECTORY, delete_begin,
        delete_end, website_settings_filter);

    host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
        ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW, delete_begin_,
        delete_end_, website_settings_filter);

    host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
        ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS, delete_begin_,
        delete_end_, website_settings_filter);
#endif

    host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
        ContentSettingsType::NOTIFICATION_INTERACTIONS, delete_begin_,
        delete_end_, website_settings_filter);
    host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
        ContentSettingsType::PRIVATE_NETWORK_GUARD, delete_begin_, delete_end_,
        website_settings_filter);
    host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
        ContentSettingsType::PRIVATE_NETWORK_CHOOSER_DATA, delete_begin_,
        delete_end_, website_settings_filter);

    PermissionDecisionAutoBlockerFactory::GetForProfile(profile_)
        ->RemoveEmbargoAndResetCounts(filter);
  }

  // Different types of DIPS events are cleared for DATA_TYPE_HISTORY and
  // DATA_TYPE_COOKIES.
  DIPSEventRemovalType dips_mask = DIPSEventRemovalType::kNone;
  if ((remove_mask & content::BrowsingDataRemover::DATA_TYPE_COOKIES) &&
      !filter_builder->IsCrossSiteClearSiteDataForCookies()) {
    dips_mask |= DIPSEventRemovalType::kStorage;
  }
  if (remove_mask & constants::DATA_TYPE_HISTORY) {
    dips_mask |= DIPSEventRemovalType::kHistory;
  }

  if (dips_mask != DIPSEventRemovalType::kNone) {
    auto* dips_service = DIPSServiceFactory::GetForBrowserContext(profile_);
    if (dips_service) {
      dips_service->RemoveEvents(delete_begin_, delete_end_,
                                 filter_builder->BuildNetworkServiceFilter(),
                                 dips_mask);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // Password manager
  if (remove_mask & constants::DATA_TYPE_PASSWORDS) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_Passwords"));
    auto password_store = PasswordStoreFactory::GetForProfile(
        profile_, ServiceAccessType::EXPLICIT_ACCESS);

    if (password_store) {
      // TODO:(crbug.com/1167715) - Test that associated compromised credentials
      // are removed.
      password_store->RemoveLoginsByURLAndTime(
          filter, delete_begin_, delete_end_,
          CreateTaskCompletionClosure(TracingDataType::kPasswords));
    }

    profile_->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->ClearHttpAuthCache(
            delete_begin_.is_null() ? base::Time::Min() : delete_begin_,
            delete_end_.is_null() ? base::Time::Max() : delete_end_,
            CreateTaskCompletionClosureForMojo(
                TracingDataType::kHttpAuthCache));

    scoped_refptr<payments::PaymentManifestWebDataService> web_data_service =
        webdata_services::WebDataServiceWrapperFactory::
            GetPaymentManifestWebDataServiceForBrowserContext(
                profile_, ServiceAccessType::EXPLICIT_ACCESS);
    if (web_data_service) {
      web_data_service->ClearSecurePaymentConfirmationCredentials(
          delete_begin_, delete_end_,
          CreateTaskCompletionClosure(
              TracingDataType::kSecurePaymentConfirmationCredentials));
    }

#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (ash::SystemProxyManager::Get()) {
      // Sends a request to the System-proxy daemon to clear the proxy user
      // credentials. System-proxy retrieves proxy username and password from
      // the NetworkService, but not the creation time of the credentials. The
      // |ClearUserCredentials| request will remove all the cached proxy
      // credentials. If credentials prior to |delete_begin_| are removed from
      // System-proxy, the daemon will send a D-Bus request to Chrome to fetch
      // them from the NetworkService when needed.
      ash::SystemProxyManager::Get()->ClearUserCredentials();
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    if (credential_store_) {
      credential_store_->DeleteCredentials(
          delete_begin_, delete_end_,
          CreateTaskCompletionClosure(TracingDataType::kWebAuthnCredentials));
    }

    // Cleared for DATA_TYPE_HISTORY, DATA_TYPE_COOKIES and DATA_TYPE_PASSWORDS.
    browsing_data::RemoveFederatedSiteSettingsData(delete_begin_, delete_end_,
                                                   website_settings_filter,
                                                   host_content_settings_map_);
  }

  if (remove_mask & constants::DATA_TYPE_ACCOUNT_PASSWORDS) {
    auto account_store = AccountPasswordStoreFactory::GetForProfile(
        profile_, ServiceAccessType::EXPLICIT_ACCESS);

    if (account_store) {
      // TODO:(crbug.com/1167715) - Test that associated compromised credentials
      // are removed.
      account_store->RemoveLoginsByURLAndTime(
          filter, delete_begin_, delete_end_,
          CreateTaskCompletionClosure(TracingDataType::kAccountPasswords),
          CreateTaskCompletionCallback(TracingDataType::kAccountPasswordsSynced,
                                       constants::DATA_TYPE_ACCOUNT_PASSWORDS));
    }
  }

  if (remove_mask & content::BrowsingDataRemover::DATA_TYPE_COOKIES) {
    password_manager::PasswordStoreInterface* password_store =
        PasswordStoreFactory::GetForProfile(profile_,
                                            ServiceAccessType::EXPLICIT_ACCESS)
            .get();

    if (password_store &&
        !filter_builder->IsCrossSiteClearSiteDataForCookies()) {
      password_store->DisableAutoSignInForOrigins(
          filter,
          CreateTaskCompletionClosure(TracingDataType::kDisableAutoSignin));
    }
  }

  if (remove_mask & constants::DATA_TYPE_HISTORY) {
    password_manager::PasswordStoreInterface* password_store =
        PasswordStoreFactory::GetForProfile(profile_,
                                            ServiceAccessType::EXPLICIT_ACCESS)
            .get();

    if (password_store) {
      password_manager::SmartBubbleStatsStore* stats_store =
          password_store->GetSmartBubbleStatsStore();
      if (stats_store) {
        stats_store->RemoveStatisticsByOriginAndTime(
            nullable_filter, delete_begin_, delete_end_,
            CreateTaskCompletionClosure(TracingDataType::kPasswordsStatistics));
      }
      password_manager::FieldInfoStore* field_store =
          password_store->GetFieldInfoStore();
      if (field_store) {
        field_store->RemoveFieldInfoByTime(
            delete_begin_, delete_end_,
            CreateTaskCompletionClosure(TracingDataType::kFieldInfo));
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_FORM_DATA
  // TODO(dmurph): Support all backends with filter (crbug.com/113621).
  if (remove_mask & constants::DATA_TYPE_FORM_DATA) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_Autofill"));
    scoped_refptr<autofill::AutofillWebDataService> web_data_service =
        WebDataServiceFactory::GetAutofillWebDataForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);

    if (web_data_service.get()) {
      web_data_service->RemoveFormElementsAddedBetween(delete_begin_,
                                                       delete_end_);
      web_data_service->RemoveAutofillDataModifiedBetween(delete_begin_,
                                                          delete_end_);
      // Clear out the Autofill StrikeDatabase in its entirety.
      // TODO(crbug.com/884817): Respect |delete_begin_| and |delete_end_| and
      // only clear out entries whose last strikes were created in that
      // timeframe.
      autofill::StrikeDatabase* strike_database =
          autofill::StrikeDatabaseFactory::GetForProfile(profile_);
      if (strike_database)
        strike_database->ClearAllStrikes();

      // Ask for a call back when the above calls are finished.
      web_data_service->GetDBTaskRunner()->PostTaskAndReply(
          FROM_HERE, base::DoNothing(),
          CreateTaskCompletionClosure(TracingDataType::kAutofillData));

      autofill::PersonalDataManager* data_manager =
          autofill::PersonalDataManagerFactory::GetForProfile(profile_);
      if (data_manager)
        data_manager->Refresh();
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_CACHE
  if (remove_mask & content::BrowsingDataRemover::DATA_TYPE_CACHE) {
    // Tell the renderers associated with |profile_| to clear their cache.
    // TODO(crbug.com/668114): Renderer cache is a platform concept, and should
    // live in BrowsingDataRemoverImpl. However, WebCacheManager itself is
    // a component with dependency on content/browser. Untangle these
    // dependencies or reimplement the relevant part of WebCacheManager
    // in content/browser.
    // TODO(crbug.com/1022757): add a test for this.
    for (content::RenderProcessHost::iterator iter =
             content::RenderProcessHost::AllHostsIterator();
         !iter.IsAtEnd(); iter.Advance()) {
      content::RenderProcessHost* render_process_host = iter.GetCurrentValue();
      if (render_process_host->GetBrowserContext() == profile_ &&
          render_process_host->IsInitializedAndNotDead()) {
        web_cache::WebCacheManager::GetInstance()->ClearCacheForProcess(
            render_process_host->GetID());
      }
    }

#if BUILDFLAG(ENABLE_NACL)
    nacl::NaClBrowser::GetInstance()->ClearValidationCache(UIThreadTrampoline(
        CreateTaskCompletionClosure(TracingDataType::kNaclCache)));

    pnacl::PnaclHost::GetInstance()->ClearTranslationCacheEntriesBetween(
        delete_begin_, delete_end_,
        UIThreadTrampoline(
            CreateTaskCompletionClosure(TracingDataType::kPnaclCache)));
#endif

    browsing_data::RemovePrerenderCacheData(
        prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
            profile_));

#if BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(ENABLE_FEED_V2)
    // Don't bridge through if the service isn't present, which means we're
    // probably running in a native unit test.
    feed::FeedService* service =
        feed::FeedServiceFactory::GetForBrowserContext(profile_);
    if (service) {
      service->ClearCachedData();
    }
#endif  // BUILDFLAG(ENABLE_FEED_V2)

#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
    // For now we're considering offline pages as cache, so if we're removing
    // cache we should remove offline pages as well.
    if (remove_mask & content::BrowsingDataRemover::DATA_TYPE_CACHE) {
      auto* offline_page_model =
          offline_pages::OfflinePageModelFactory::GetForBrowserContext(
              profile_);
      if (offline_page_model)
        offline_page_model->DeleteCachedPagesByURLPredicate(
            filter,
            base::IgnoreArgs<offline_pages::OfflinePageModel::DeletePageResult>(
                CreateTaskCompletionClosure(TracingDataType::kOfflinePages)));
    }
#endif

    // TODO(crbug.com/829321): Remove null-check.
    auto* webrtc_event_log_manager = WebRtcEventLogManager::GetInstance();
    if (webrtc_event_log_manager) {
      webrtc_event_log_manager->ClearCacheForBrowserContext(
          profile_, delete_begin_, delete_end_,
          CreateTaskCompletionClosure(TracingDataType::kWebrtcEventLogs));
    } else {
      LOG(ERROR) << "WebRtcEventLogManager not instantiated.";
    }

    // Mark cached favicons as expired to force redownload on next visit.
    if (filter_builder->GetMode() ==
        BrowsingDataFilterBuilder::Mode::kPreserve) {
      history::HistoryService* history_service =
          HistoryServiceFactory::GetForProfile(
              profile_, ServiceAccessType::EXPLICIT_ACCESS);
      if (history_service) {
        history_service->SetFaviconsOutOfDateBetween(
            delete_begin_, delete_end_,
            CreateTaskCompletionClosure(
                TracingDataType::kFaviconCacheExpiration),
            &history_task_tracker_);
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_MEDIA_LICENSES
  if (remove_mask & content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES) {
    // TODO(jrummell): This UMA should be renamed to indicate it is for Media
    // Licenses.
    base::RecordAction(UserMetricsAction("ClearBrowsingData_ContentLicenses"));

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // On Chrome OS, delete any content protection platform keys.
    // Platform keys do not support filtering by domain, so skip this if
    // clearing only a specified set of sites.
    if (filter_builder->GetMode() != BrowsingDataFilterBuilder::Mode::kDelete) {
      const user_manager::User* user =
          ash::ProfileHelper::Get()->GetUserByProfile(profile_);
      if (!user) {
        LOG(WARNING) << "Failed to find user for current profile.";
      } else {
        ::attestation::DeleteKeysRequest request;
        request.set_username(cryptohome::CreateAccountIdentifierFromAccountId(
                                 user->GetAccountId())
                                 .account_id());
        request.set_key_label_match(
            ash::attestation::kContentProtectionKeyPrefix);
        request.set_match_behavior(
            ::attestation::DeleteKeysRequest::MATCH_BEHAVIOR_PREFIX);

        auto clear_platform_keys_callback = base::BindOnce(
            &ChromeBrowsingDataRemoverDelegate::OnClearPlatformKeys,
            weak_ptr_factory_.GetWeakPtr(),
            CreateTaskCompletionClosure(TracingDataType::kTpmAttestationKeys));
        ash::AttestationClient::Get()->DeleteKeys(
            request, base::BindOnce(
                         [](decltype(clear_platform_keys_callback) cb,
                            const ::attestation::DeleteKeysReply& reply) {
                           std::move(cb).Run(reply.status() ==
                                             ::attestation::STATUS_SUCCESS);
                         },
                         std::move(clear_platform_keys_callback)));
      }
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
    cdm::MediaDrmStorageImpl::ClearMatchingLicenses(
        prefs, delete_begin_, delete_end, nullable_filter,
        CreateTaskCompletionClosure(TracingDataType::kCdmLicenses));
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
    CdmDocumentServiceImpl::ClearCdmData(
        profile_, delete_begin, delete_end, nullable_filter,
        CreateTaskCompletionClosure(TracingDataType::kCdmLicenses));
#endif  // BUILDFLAG(IS_WIN)
  }

  //////////////////////////////////////////////////////////////////////////////
  // Zero suggest, Search prefetch, and search session token.
  // Remove omnibox zero-suggest cache results and Search Prefetch cached
  // results only when their respective URLs are in the filter.
  if ((remove_mask & (content::BrowsingDataRemover::DATA_TYPE_CACHE |
                      content::BrowsingDataRemover::DATA_TYPE_COOKIES)) &&
      !filter_builder->IsCrossSiteClearSiteDataForCookies()) {
    // If there is no template service or DSE, clear the caches.
    bool should_clear_zero_suggest_and_session_token = true;
    bool should_clear_search_prefetch = true;

    auto* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile_);

    // If there is no default search engine, clearing the cache is fine.
    if (template_url_service &&
        template_url_service->GetDefaultSearchProvider()) {
      // The suggest URL is used for zero suggest.
      GURL suggest_url(
          template_url_service->GetDefaultSearchProvider()->suggestions_url());
      should_clear_zero_suggest_and_session_token =
          nullable_filter.is_null() || nullable_filter.Run(suggest_url);

      // The search URL is used for search prefetch.
      GURL search_url(template_url_service->GetDefaultSearchProvider()->url());
      should_clear_search_prefetch =
          nullable_filter.is_null() || nullable_filter.Run(search_url);
    }

    if (should_clear_zero_suggest_and_session_token) {
      if (base::FeatureList::IsEnabled(omnibox::kZeroSuggestInMemoryCaching)) {
        auto* zero_suggest_cache_service =
            ZeroSuggestCacheServiceFactory::GetForProfile(profile_);
        if (zero_suggest_cache_service) {
          zero_suggest_cache_service->ClearCache();
        }
      } else {
        prefs->SetString(omnibox::kZeroSuggestCachedResults, std::string());
        prefs->SetDict(omnibox::kZeroSuggestCachedResultsWithURL,
                       base::Value::Dict());
      }
    }

    // |search_prefetch_service| is null if |profile_| is off the record.
    auto* search_prefetch_service =
        SearchPrefetchServiceFactory::GetForProfile(profile_);
    if (should_clear_search_prefetch && search_prefetch_service)
      search_prefetch_service->ClearPrefetches();

    if (should_clear_zero_suggest_and_session_token && template_url_service)
      template_url_service->ClearSessionToken();
  }

  //////////////////////////////////////////////////////////////////////////////
  // Domain reliability.
  if (remove_mask & (content::BrowsingDataRemover::DATA_TYPE_COOKIES |
                     constants::DATA_TYPE_HISTORY)) {
    network::mojom::NetworkContext_DomainReliabilityClearMode mode;
    if (remove_mask & content::BrowsingDataRemover::DATA_TYPE_COOKIES)
      mode = network::mojom::NetworkContext::DomainReliabilityClearMode::
          CLEAR_CONTEXTS;
    else
      mode = network::mojom::NetworkContext::DomainReliabilityClearMode::
          CLEAR_BEACONS;
    domain_reliability_clearer_.Run(filter_builder, mode,
                                    CreateTaskCompletionClosureForMojo(
                                        TracingDataType::kDomainReliability));
  }

  //////////////////////////////////////////////////////////////////////////////
  // Persisted isolated origins.
  // Clear persisted isolated origins when cookies and other site data are
  // cleared (DATA_TYPE_ISOLATED_ORIGINS is part of DATA_TYPE_SITE_DATA), or
  // when history is cleared.  This is because (1) clearing cookies implies
  // forgetting that the user has logged into sites, which also implies
  // forgetting that a user has typed a password on them, and (2) saved
  // isolated origins are a form of history, since the user has visited them in
  // the past and triggered isolation via heuristics like typing a password on
  // them.  Note that the Clear-Site-Data header should not clear isolated
  // origins: they should only be cleared by user-driven actions.
  //
  // TODO(alexmos): Support finer-grained filtering based on time ranges and
  // |filter|. For now, conservatively delete all saved isolated origins.
  if (remove_mask &
      (constants::DATA_TYPE_ISOLATED_ORIGINS | constants::DATA_TYPE_HISTORY))
    browsing_data::RemoveSiteIsolationData(prefs);

  if (remove_mask & constants::DATA_TYPE_HISTORY) {
    network::mojom::NetworkContext* network_context =
        profile_->GetDefaultStoragePartition()->GetNetworkContext();
    network_context->ClearReportingCacheReports(
        filter_builder->BuildNetworkServiceFilter(),
        CreateTaskCompletionClosureForMojo(TracingDataType::kReportingCache));
    network_context->ClearNetworkErrorLogging(
        filter_builder->BuildNetworkServiceFilter(),
        CreateTaskCompletionClosureForMojo(
            TracingDataType::kNetworkErrorLogging));
  }

//////////////////////////////////////////////////////////////////////////////
// DATA_TYPE_WEB_APP_DATA
#if BUILDFLAG(IS_ANDROID)
  // Clear all data associated with registered webapps.
  if (remove_mask & constants::DATA_TYPE_WEB_APP_DATA)
    webapp_registry_->UnregisterWebappsForUrls(filter);
#endif

//////////////////////////////////////////////////////////////////////////////
// Remove web app history.
#if !BUILDFLAG(IS_ANDROID)
  if (remove_mask & constants::DATA_TYPE_HISTORY &&
      web_app::AreWebAppsEnabled(profile_)) {
    auto* web_app_provider =
        web_app::WebAppProvider::GetForLocalAppsUnchecked(profile_);
    web_app_provider->scheduler().ClearWebAppBrowsingData(
        delete_begin, delete_end,
        CreateTaskCompletionClosure(TracingDataType::kWebAppHistory));
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  //////////////////////////////////////////////////////////////////////////////
  // Remove external protocol data.
  if (remove_mask & constants::DATA_TYPE_EXTERNAL_PROTOCOL_DATA)
    ExternalProtocolHandler::ClearData(profile_);

#if BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)
  //////////////////////////////////////////////////////////////////////////////
  // Remove data for this profile contained in any snapshots.
  if (remove_mask &&
      filter_builder->GetMode() == BrowsingDataFilterBuilder::Mode::kPreserve) {
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(&downgrade::RemoveDataForProfile, delete_begin_,
                       profile_->GetPath(), remove_mask),
        CreateTaskCompletionClosure(TracingDataType::kUserDataSnapshot));
  }
#endif  // BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)

  //////////////////////////////////////////////////////////////////////////////
  // Login detection data:
  // Clear the origins where login has been detected, when cookies and other
  // site data are cleared, or when history is cleared. This is because clearing
  // cookies or history implies forgetting that the user has logged into sites.
  if (remove_mask &
      (constants::DATA_TYPE_SITE_DATA | constants::DATA_TYPE_HISTORY)) {
    login_detection::prefs::RemoveLoginDetectionData(prefs);
  }
}

void ChromeBrowsingDataRemoverDelegate::OnTaskStarted(
    TracingDataType data_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto result = pending_sub_tasks_.insert(data_type);
  DCHECK(result.second) << "Task already started: "
                        << static_cast<int>(data_type);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "browsing_data", "ChromeBrowsingDataRemoverDelegate",
      TRACE_ID_WITH_SCOPE("ChromeBrowsingDataRemoverDelegate",
                          static_cast<int>(data_type)),
      "data_type", static_cast<int>(data_type));
}

void ChromeBrowsingDataRemoverDelegate::OnTaskComplete(
    TracingDataType data_type,
    uint64_t data_type_mask,
    base::TimeTicks started,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  size_t num_erased = pending_sub_tasks_.erase(data_type);
  DCHECK_EQ(num_erased, 1U);
  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "browsing_data", "ChromeBrowsingDataRemoverDelegate",
      TRACE_ID_WITH_SCOPE("ChromeBrowsingDataRemoverDelegate",
                          static_cast<int>(data_type)),
      "data_type", static_cast<int>(data_type));
  base::UmaHistogramMediumTimes(
      base::StrCat({"History.ClearBrowsingData.Duration.ChromeTask.",
                    GetHistogramSuffix(data_type)}),
      base::TimeTicks::Now() - started);

  if (!success) {
    base::UmaHistogramEnumeration("History.ClearBrowsingData.FailedTasksChrome",
                                  data_type);
    failed_data_types_ |= data_type_mask;
  }

  if (!pending_sub_tasks_.empty())
    return;

  // Explicitly clear any opt-ins to the account-scoped password storage
  // when cookies are being cleared. This needs to happen after passwords
  // have been deleted, so it is performed when all other tasks are completed.
  // Note: These usually get cleared automatically when the Google cookies are
  // deleted, but there is one edge case where that doesn't work: If the user
  // clears cookies via CBD while they are already signed out (but their
  // account is still present in the account chooser). In that case, without the
  // code below, the settings-clearing would only happen when the Google cookies
  // are refreshed the next time, typically on the next browser restart.
  if (should_clear_password_account_storage_settings_) {
    should_clear_password_account_storage_settings_ = false;
#if !BUILDFLAG(IS_ANDROID)
    password_manager::features_util::ClearAccountStorageSettingsForAllUsers(
        profile_->GetPrefs());
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  slow_pending_tasks_closure_.Cancel();

  DCHECK(!callback_.is_null());
  std::move(callback_).Run(failed_data_types_);
}

const char* ChromeBrowsingDataRemoverDelegate::GetHistogramSuffix(
    TracingDataType task) {
  switch (task) {
    case TracingDataType::kSynchronous:
      return "Synchronous";
    case TracingDataType::kHistory:
      return "History";
    case TracingDataType::kHostNameResolution:
      return "HostNameResolution";
    case TracingDataType::kNaclCache:
      return "NaclCache";
    case TracingDataType::kPnaclCache:
      return "PnaclCache";
    case TracingDataType::kAutofillData:
      return "AutofillData";
    case TracingDataType::kAutofillOrigins:
      return "AutofillOrigins";
    case TracingDataType::kPluginData:
      return "PluginData";
    case TracingDataType::kDomainReliability:
      return "DomainReliability";
    case TracingDataType::kNetworkPredictor:
      return "NetworkPredictor";
    case TracingDataType::kWebrtcLogs:
      return "WebrtcLogs";
    case TracingDataType::kVideoDecodeHistory:
      return "VideoDecodeHistory";
    case TracingDataType::kCookies:
      return "Cookies";
    case TracingDataType::kPasswords:
      return "Passwords";
    case TracingDataType::kHttpAuthCache:
      return "HttpAuthCache";
    case TracingDataType::kDisableAutoSignin:
      return "DisableAutoSignin";
    case TracingDataType::kPasswordsStatistics:
      return "PasswordsStatistics";
    case TracingDataType::kKeywordsModel:
      return "KeywordsModel";
    case TracingDataType::kReportingCache:
      return "ReportingCache";
    case TracingDataType::kNetworkErrorLogging:
      return "NetworkErrorLogging";
    case TracingDataType::kFlashDeauthorization:
      return "FlashDeauthorization";
    case TracingDataType::kOfflinePages:
      return "OfflinePages";
    case TracingDataType::kExploreSites:
      return "ExploreSites";
    case TracingDataType::kLegacyStrikes:
      return "LegacyStrikes";
    case TracingDataType::kWebrtcEventLogs:
      return "WebrtcEventLogs";
    case TracingDataType::kCdmLicenses:
      return "CdmLicenses";
    case TracingDataType::kHostCache:
      return "HostCache";
    case TracingDataType::kTpmAttestationKeys:
      return "TpmAttestationKeys";
    case TracingDataType::kStrikes:
      return "Strikes";
    case TracingDataType::kFieldInfo:
      return "FieldInfo";
    case TracingDataType::kCompromisedCredentials:
      return "CompromisedCredentials";
    case TracingDataType::kUserDataSnapshot:
      return "UserDataSnapshot";
    case TracingDataType::kMediaFeeds:
      return "MediaFeeds";
    case TracingDataType::kAccountPasswords:
      return "AccountPasswords";
    case TracingDataType::kAccountPasswordsSynced:
      return "AccountPasswordsSynced";
    case TracingDataType::kAccountCompromisedCredentials:
      return "AccountCompromisedCredentials";
    case TracingDataType::kFaviconCacheExpiration:
      return "FaviconCacheExpiration";
    case TracingDataType::kSecurePaymentConfirmationCredentials:
      return "SecurePaymentConfirmationCredentials";
    case TracingDataType::kWebAppHistory:
      return "WebAppHistory";
    case TracingDataType::kWebAuthnCredentials:
      return "WebAuthnCredentials";
    case TracingDataType::kWebrtcVideoPerfHistory:
      return "WebrtcVideoPerfHistory";
  }
}

void ChromeBrowsingDataRemoverDelegate::OnStartRemoving() {
  profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
      profile_->GetOriginalProfile(),
      ProfileKeepAliveOrigin::kClearingBrowsingData);
}

void ChromeBrowsingDataRemoverDelegate::OnDoneRemoving() {
  profile_keep_alive_.reset();
}

base::OnceClosure
ChromeBrowsingDataRemoverDelegate::CreateTaskCompletionClosure(
    TracingDataType data_type) {
  return base::BindOnce(
      CreateTaskCompletionCallback(data_type, /*data_type_mask=*/0),
      /*success=*/true);
}

base::OnceCallback<void(bool)>
ChromeBrowsingDataRemoverDelegate::CreateTaskCompletionCallback(
    TracingDataType data_type,
    uint64_t data_type_mask) {
  OnTaskStarted(data_type);
  return base::BindOnce(&ChromeBrowsingDataRemoverDelegate::OnTaskComplete,
                        weak_ptr_factory_.GetWeakPtr(), data_type,
                        data_type_mask, base::TimeTicks::Now());
}

base::OnceClosure
ChromeBrowsingDataRemoverDelegate::CreateTaskCompletionClosureForMojo(
    TracingDataType data_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Note num_pending_tasks++ unnecessary here because it's done by the call to
  // CreateTaskCompletionClosure().
  return mojo::WrapCallbackWithDropHandler(
      CreateTaskCompletionClosure(data_type),
      base::BindOnce(&ChromeBrowsingDataRemoverDelegate::OnTaskComplete,
                     weak_ptr_factory_.GetWeakPtr(), data_type,
                     /*data_type_mask=*/0, base::TimeTicks::Now(),
                     /*success=*/true));
}

void ChromeBrowsingDataRemoverDelegate::RecordUnfinishedSubTasks() {
  DCHECK(!pending_sub_tasks_.empty());
  for (TracingDataType task : pending_sub_tasks_) {
    UMA_HISTOGRAM_ENUMERATION(
        "History.ClearBrowsingData.Duration.SlowTasks180sChrome", task);
  }
}

#if BUILDFLAG(IS_ANDROID)
void ChromeBrowsingDataRemoverDelegate::OverrideWebappRegistryForTesting(
    std::unique_ptr<WebappRegistry> webapp_registry) {
  webapp_registry_ = std::move(webapp_registry);
}
#endif

void ChromeBrowsingDataRemoverDelegate::
    OverrideDomainReliabilityClearerForTesting(
        DomainReliabilityClearer clearer) {
  domain_reliability_clearer_ = std::move(clearer);
}

bool ChromeBrowsingDataRemoverDelegate::IsForAllTime() const {
  return delete_begin_ == base::Time() && delete_end_ == base::Time::Max();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ChromeBrowsingDataRemoverDelegate::OnClearPlatformKeys(
    base::OnceClosure done,
    bool result) {
  LOG_IF(ERROR, !result) << "Failed to clear platform keys.";
  std::move(done).Run();
}
#endif

std::unique_ptr<device::fido::PlatformCredentialStore>
ChromeBrowsingDataRemoverDelegate::MakeCredentialStore() {
  return
#if BUILDFLAG(IS_MAC)
      std::make_unique<device::fido::mac::TouchIdCredentialStore>(
          ChromeWebAuthenticationDelegate::TouchIdAuthenticatorConfigForProfile(
              profile_));
#elif BUILDFLAG(IS_CHROMEOS_ASH)
      std::make_unique<
          device::fido::cros::PlatformAuthenticatorCredentialStore>();
#else
      nullptr;
#endif
}
