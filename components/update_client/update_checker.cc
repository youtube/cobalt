// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_checker.h"

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/cancellation.h"
#include "components/update_client/component.h"
#include "components/update_client/configurator.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/protocol_serializer.h"
#include "components/update_client/request_sender.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_engine.h"
#include "components/update_client/utils.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_STARBOARD)
#include "components/update_client/cobalt_slot_management.h"
#include "starboard/extension/free_space.h"
#endif

namespace update_client {
namespace {

class UpdateCheckerImpl : public UpdateChecker {
 public:
  explicit UpdateCheckerImpl(scoped_refptr<Configurator> config);
  UpdateCheckerImpl(const UpdateCheckerImpl&) = delete;
  UpdateCheckerImpl& operator=(const UpdateCheckerImpl&) = delete;
  ~UpdateCheckerImpl() override;

  // Overrides for UpdateChecker.
  void CheckForUpdates(
      scoped_refptr<UpdateContext> context,
      const base::flat_map<std::string, std::string>& additional_attributes,
      UpdateCheckCallback update_check_callback) override;

#if BUILDFLAG(USE_EVERGREEN)
  PersistedData* GetPersistedData() override { return config_->GetPersistedData(); }
  void Cancel() override;
  bool SkipUpdate(const CobaltExtensionInstallationManagerApi* installation_api) override;
#endif

 private:
  static UpdaterStateAttributes ReadUpdaterStateAttributes(
      UpdaterStateProvider update_state_provider,
      bool is_machine);

  void CheckForUpdatesHelper(
      scoped_refptr<UpdateContext> context,
      const std::vector<GURL>& urls,
      const base::flat_map<std::string, std::string>& additional_attributes,
      const std::multimap<std::string, std::string>& cache_contents,
      const UpdaterStateAttributes& updater_state_attributes,
      const std::set<std::string>& active_ids);

  void OnRequestSenderComplete(scoped_refptr<UpdateContext> context,
                               std::optional<base::OnceClosure> fallback,
                               int error,
                               const std::string& response,
                               int retry_after_sec);

  void UpdateCheckSucceeded(scoped_refptr<UpdateContext> context,
                            const ProtocolParser::Results& results,
                            int retry_after_sec);

  void UpdateCheckFailed(ErrorCategory error_category,
                         int error,
                         int retry_after_sec);

  SEQUENCE_CHECKER(sequence_checker_);
  const scoped_refptr<Configurator> config_;
  UpdateCheckCallback update_check_callback_;
  scoped_refptr<Cancellation> cancellation_ =
      base::MakeRefCounted<Cancellation>();
  base::WeakPtrFactory<UpdateCheckerImpl> weak_factory_{this};
};

UpdateCheckerImpl::UpdateCheckerImpl(scoped_refptr<Configurator> config)
    : config_(config) {}

UpdateCheckerImpl::~UpdateCheckerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cancellation_->Cancel();
}

void UpdateCheckerImpl::CheckForUpdates(
    scoped_refptr<UpdateContext> context,
    const base::flat_map<std::string, std::string>& additional_attributes,
    UpdateCheckCallback update_check_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  update_check_callback_ = std::move(update_check_callback);

  auto check_for_updates_invoker = base::BindOnce(
      &UpdateCheckerImpl::CheckForUpdatesHelper, weak_factory_.GetWeakPtr(),
      context, config_->UpdateUrl(), additional_attributes);

  context->config->GetCrxCache()->ListHashesByAppId(base::BindOnce(
      [](base::OnceCallback<void(const std::multimap<std::string, std::string>&,
                                 const UpdaterStateAttributes&,
                                 const std::set<std::string>&)>
             check_for_updates_invoker,
         scoped_refptr<Configurator> config, std::vector<std::string> ids,
         const std::multimap<std::string, std::string>& cache_contents) {
        base::ThreadPool::PostTaskAndReplyWithResult(
            FROM_HERE, kTaskTraits,
            base::BindOnce(&UpdateCheckerImpl::ReadUpdaterStateAttributes,
                           config->GetUpdaterStateProvider(),
                           !config->IsPerUserInstall()),
            base::BindOnce(
                [](base::OnceCallback<void(const UpdaterStateAttributes&,
                                           const std::set<std::string>&)>
                       check_for_updates_invoker,
                   scoped_refptr<Configurator> config,
                   std::vector<std::string> ids,
                   const UpdaterStateAttributes& updater_state_attributes) {
                  config->GetPersistedData()->GetActiveBits(
                      ids, base::BindOnce(std::move(check_for_updates_invoker),
                                          updater_state_attributes));
                },
                base::BindOnce(std::move(check_for_updates_invoker),
                               cache_contents),
                config, ids));
      },
      std::move(check_for_updates_invoker), config_,
      context->components_to_check_for_updates));
}

// This function runs on the blocking pool task runner.
UpdaterStateAttributes UpdateCheckerImpl::ReadUpdaterStateAttributes(
    UpdaterStateProvider update_state_provider,
    bool is_machine) {
#if BUILDFLAG(IS_WIN)
  // On Windows, the Chrome and the updater install modes are matched by design.
  return update_state_provider.Run(is_machine);
#elif BUILDFLAG(IS_MAC)
  return update_state_provider.Run(false);
#else
  return {};
#endif  // BUILDFLAG(IS_WIN)
}

void UpdateCheckerImpl::CheckForUpdatesHelper(
    scoped_refptr<UpdateContext> context,
    const std::vector<GURL>& urls,
    const base::flat_map<std::string, std::string>& additional_attributes,
    const std::multimap<std::string, std::string>& cache_contents,
    const UpdaterStateAttributes& updater_state_attributes,
    const std::set<std::string>& active_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (urls.empty()) {
    UpdateCheckFailed(ErrorCategory::kUpdateCheck,
                      static_cast<int>(ProtocolError::MISSING_URLS), 0);
    return;
  }
  GURL url = urls.front();

  // Components in this update check are either all foreground, or all
  // background since this member is inherited from the component's update
  // context. Pick the state of the first component to use in the update check.
  CHECK(!context->components.empty());
  const bool is_foreground =
      context->components.cbegin()->second->is_foreground();
  CHECK(std::ranges::all_of(
      context->components,
      [is_foreground](IdToComponentPtrMap::const_reference& elem) {
        return is_foreground == elem.second->is_foreground();
      }));

  PersistedData* metadata = config_->GetPersistedData();

  std::vector<std::string> sent_ids;

  std::vector<protocol_request::App> apps;
  for (const auto& app_id : context->components_to_check_for_updates) {
    CHECK_EQ(1u, context->components.count(app_id));
    const auto& component = context->components.at(app_id);
    CHECK_EQ(component->id(), app_id);
    const auto& crx_component = component->crx_component();
    CHECK(crx_component);

    if (crx_component->requires_network_encryption &&
        !url.SchemeIsCryptographic()) {
      continue;
    }

    sent_ids.push_back(app_id);

    std::string install_source;
    if (!crx_component->install_source.empty()) {
      install_source = crx_component->install_source;
    } else if (component->is_foreground()) {
      install_source = "ondemand";
    }

    std::vector<std::string> cached_hashes;
    auto range = cache_contents.equal_range(app_id);
    for (auto i = range.first; i != range.second; i++) {
      cached_hashes.push_back(i->second);
    }
#if BUILDFLAG(USE_EVERGREEN)
    base::Version current_version = crx_component->version;

    // Check if there is an available update already for quick roll-forward
    auto installation_api =
        static_cast<const CobaltExtensionInstallationManagerApi*>(
            SbSystemGetExtension(kCobaltExtensionInstallationManagerName));
    if (!installation_api) {
      LOG(ERROR) << "UpdaterChecker: "
                 << "Failed to get installation manager extension.";
      return;
    }

    if (SkipUpdate(installation_api)) {
      LOG(WARNING) << "UpdaterChecker is skipping";
      UpdateCheckFailed(ErrorCategory::kUpdateCheck,
        static_cast<int>(UpdateCheckError::OUT_OF_SPACE),
        /*retry_after_sec=*/-1);
      return;
    }

    if (CobaltQuickUpdate(installation_api, current_version)) {
      // The last parameter in UpdateCheckFailed below, which is to be passed to
      // update_check_callback_, indicates a throttling by the update server.
      // Only non-negative values are valid. Negative values are not trusted
      // and are ignored.
      UpdateCheckFailed(ErrorCategory::kUpdateCheck,
                        static_cast<int>(UpdateCheckError::QUICK_ROLL_FORWARD),
                        /*retry_after_sec=*/-1);

      return;
    }

    std::string last_installed_version =
        GetPersistedData()->GetLastInstalledVersion(app_id);
    std::string last_installed_starboard =
        GetPersistedData()->GetLastInstalledSbVersion(app_id);
    // If the version of the last installed update package is higher than the
    // version of the running binary and the starboard version of the last
    // installed update matched the binary currently running, use the last
    // installed version to indicate the current update version in the update
    // check request.
    if (!last_installed_version.empty() &&
        last_installed_starboard == std::to_string(SB_API_VERSION) &&
        base::Version(last_installed_version).CompareTo(current_version) > 0) {
      current_version = base::Version(last_installed_version);
    }
// If the quick roll forward update slot candidate doesn't exist, continue
// with update check.
#endif
    apps.push_back(MakeProtocolApp(
        app_id, crx_component->version, crx_component->ap, crx_component->brand,
        active_ids.find(app_id) != active_ids.end()
            ? metadata->GetInstallId(app_id)
            : "",
        crx_component->lang.empty() ? config_->GetLang() : crx_component->lang,
        metadata->GetInstallDate(app_id), install_source,
        crx_component->install_location, crx_component->installer_attributes,
        metadata->GetCohort(app_id), metadata->GetCohortHint(app_id),
        metadata->GetCohortName(app_id), crx_component->channel,
        crx_component->disabled_reasons, cached_hashes,
        MakeProtocolUpdateCheck(
            !crx_component->updates_enabled ||
                (!crx_component->allow_updates_on_metered_connection &&
                 config_->IsConnectionMetered()),
            crx_component->target_version_prefix,
            crx_component->rollback_allowed,
            crx_component->same_version_update_allowed),
        [](const std::string& install_data_index)
            -> std::vector<protocol_request::Data> {
          if (install_data_index.empty()) {
            return {};
          } else {
            return {{"install", install_data_index, ""}};
          }
        }(crx_component->install_data_index),
        MakeProtocolPing(app_id, config_->GetPersistedData(),
                         active_ids.find(app_id) != active_ids.end()),
        std::nullopt));
  }

  if (sent_ids.empty()) {
    // No apps could be checked over this URL.
    UpdateCheckFailed(ErrorCategory::kUpdateCheck,
                      static_cast<int>(ProtocolError::MISSING_URLS), 0);
    return;
  }

  const auto request = MakeProtocolRequest(
      !config_->IsPerUserInstall(), context->session_id, config_->GetProdId(),
      config_->GetBrowserVersion().GetString(), config_->GetChannel(),
      config_->GetOSLongName(), config_->GetDownloadPreference(),
      config_->IsMachineExternallyManaged(), additional_attributes,
      updater_state_attributes, std::move(apps));

  cancellation_->OnCancel(
      base::MakeRefCounted<RequestSender>(config_->GetNetworkFetcherFactory())
          ->Send({url},
                 BuildUpdateCheckExtraRequestHeaders(
                     config_->GetProdId(), config_->GetBrowserVersion(),
                     sent_ids, is_foreground),
                 config_->GetProtocolHandlerFactory()
                     ->CreateSerializer()
                     ->Serialize(request),
                 config_->EnabledCupSigning(),
                 base::BindOnce(
                     &UpdateCheckerImpl::OnRequestSenderComplete,
                     weak_factory_.GetWeakPtr(), context,
                     urls.size() > 1
                         ? std::optional<base::OnceClosure>(base::BindOnce(
                               &UpdateCheckerImpl::CheckForUpdatesHelper,
                               weak_factory_.GetWeakPtr(), context,
                               std::vector<GURL>(urls.begin() + 1, urls.end()),
                               additional_attributes, cache_contents,
                               updater_state_attributes, active_ids))
                         : std::nullopt)));
#if BUILDFLAG(IS_STARBOARD)
  // Reset |is_forced_update| flag to false if it is true
  config_->CompareAndSwapForcedUpdate(/*old_value=*/1, /*new_value=*/0);
#endif
}

#if BUILDFLAG(USE_EVERGREEN)
void UpdateCheckerImpl::Cancel() {
  // TODO(b/448186580): Replace LOG with D(V)LOG
  LOG(INFO) << "UpdateCheckerImpl::Cancel";
  auto request_sender = base::MakeRefCounted<RequestSender>(
    config_->GetNetworkFetcherFactory());
  if (request_sender) {
    request_sender->Cancel();
  }
}

bool UpdateCheckerImpl::SkipUpdate(
  const CobaltExtensionInstallationManagerApi* installation_api) {
  auto free_space_ext = static_cast<const CobaltExtensionFreeSpaceApi*>(
      SbSystemGetExtension(kCobaltExtensionFreeSpaceName));
  if (!installation_api) {
    LOG(WARNING) << "UpdaterChecker::SkipUpdate: missing installation api";
    return false;
  }
  if (!free_space_ext) {
    LOG(WARNING) << "UpdaterChecker::SkipUpdate: No FreeSpace Cobalt extension";
    return false;
  }
  //TODO(b/446250454): Use Posix API to measure free storage space for updates.
  return CobaltSkipUpdate(installation_api, config_->GetMinFreeSpaceBytes(),
     free_space_ext->MeasureFreeSpace(kSbSystemPathStorageDirectory),
     CobaltInstallationCleanupSize(installation_api)) ;
}
#endif

void UpdateCheckerImpl::OnRequestSenderComplete(
    scoped_refptr<UpdateContext> context,
    std::optional<base::OnceClosure> fallback,
    int error,
    const std::string& response,
    int retry_after_sec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cancellation_->Clear();

  if (error) {
    VLOG(1) << "RequestSender failed " << error;
    if (fallback && retry_after_sec <= 0) {
      std::move(*fallback).Run();
    } else {
      UpdateCheckFailed(ErrorCategory::kUpdateCheck, error, retry_after_sec);
    }
    return;
  }

  auto parser = config_->GetProtocolHandlerFactory()->CreateParser();
  if (!parser->Parse(response)) {
    VLOG(1) << "Parse failed " << parser->errors();
    UpdateCheckFailed(ErrorCategory::kUpdateCheck,
                      static_cast<int>(ProtocolError::PARSE_FAILED),
                      retry_after_sec);
    return;
  }

  CHECK_EQ(0, error);
  UpdateCheckSucceeded(context, parser->results(), retry_after_sec);
}

void UpdateCheckerImpl::UpdateCheckSucceeded(
    scoped_refptr<UpdateContext> context,
    const ProtocolParser::Results& results,
    int retry_after_sec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PersistedData* metadata = config_->GetPersistedData();
  const int daynum = results.daystart_elapsed_days;
  for (const auto& result : results.apps) {
    if (result.cohort) {
      metadata->SetCohort(result.app_id, *result.cohort);
    }
    if (result.cohort_name) {
      metadata->SetCohortName(result.app_id, *result.cohort_name);
    }
    if (result.cohort_hint) {
      metadata->SetCohortHint(result.app_id, *result.cohort_hint);
    }
  }

  base::OnceClosure reply =
      base::BindOnce(std::move(update_check_callback_),
                     std::make_optional<ProtocolParser::Results>(results),
                     ErrorCategory::kNone, 0, retry_after_sec);

  if (daynum != ProtocolParser::kNoDaystart) {
    metadata->SetDateLastData(context->components_to_check_for_updates, daynum,
                              std::move(reply));
    return;
  }
  metadata->SetLastUpdateCheckError({});

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(reply));
}

void UpdateCheckerImpl::UpdateCheckFailed(ErrorCategory error_category,
                                          int error,
                                          int retry_after_sec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_NE(0, error);
  config_->GetPersistedData()->SetLastUpdateCheckError(
      {.category = error_category, .code = error});

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(update_check_callback_), std::nullopt,
                                error_category, error, retry_after_sec));
}

}  // namespace

std::unique_ptr<UpdateChecker> UpdateChecker::Create(
    scoped_refptr<Configurator> config) {
  return std::make_unique<UpdateCheckerImpl>(config);
}

}  // namespace update_client
