// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_manager.h"

#include <memory>
#include <type_traits>

#include "base/callback_list.h"
#include "base/cancelable_callback.h"
#include "base/check.h"
#include "base/containers/circular_deque.h"
#include "base/containers/cxx20_erase_map.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_apply_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_apply_task.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_apply_waiter.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_discovery_task.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_service.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace web_app {

// This helper class acts similarly to `IsolatedWebAppUpdateDiscoveryTask`, the
// difference being that this class is for discovering local updates for
// dev-mode installed IWAs.
class IsolatedWebAppUpdateManager::LocalDevModeUpdateDiscoverer {
 public:
  using Callback =
      base::OnceCallback<void(base::expected<base::Version, std::string>)>;

  LocalDevModeUpdateDiscoverer(Profile& profile, WebAppProvider& provider)
      : profile_(profile), provider_(provider) {}

  void DiscoverLocalUpdate(const IsolatedWebAppLocation& location,
                           const IsolatedWebAppUrlInfo& url_info,
                           Callback callback) {
    if (!absl::holds_alternative<DevModeProxy>(location) &&
        !absl::holds_alternative<DevModeBundle>(location)) {
      std::move(callback).Run(
          base::unexpected("Discovering a local update is only supported for "
                           "dev mode-installed apps."));
      return;
    }

    auto keep_alive = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::ISOLATED_WEB_APP_UPDATE,
        KeepAliveRestartOption::DISABLED);
    auto profile_keep_alive =
        profile_->IsOffTheRecord()
            ? nullptr
            : std::make_unique<ScopedProfileKeepAlive>(
                  &*profile_, ProfileKeepAliveOrigin::kIsolatedWebAppUpdate);

    provider_->scheduler().PrepareAndStoreIsolatedWebAppUpdate(
        IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateInfo(
            location, /*expected_version=*/absl::nullopt),
        url_info, /*optional_keep_alive=*/nullptr,
        /*optional_profile_keep_alive=*/nullptr,
        base::BindOnce(&LocalDevModeUpdateDiscoverer::OnUpdatePrepared,
                       weak_factory_.GetWeakPtr(), std::move(keep_alive),
                       std::move(profile_keep_alive), std::move(callback)));
  }

 private:
  void OnUpdatePrepared(
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
      Callback callback,
      IsolatedWebAppUpdatePrepareAndStoreCommandResult result) {
    std::move(callback).Run(
        result
            .transform(
                [](const auto& success) { return success.update_version; })
            .transform_error([](const auto& error) { return error.message; }));
  }

  raw_ref<Profile> profile_;
  raw_ref<WebAppProvider> provider_;
  base::WeakPtrFactory<LocalDevModeUpdateDiscoverer> weak_factory_{this};
};

IsolatedWebAppUpdateManager::IsolatedWebAppUpdateManager(
    Profile& profile,
    base::TimeDelta update_discovery_frequency)
    : profile_(profile),
      automatic_updates_enabled_(
          content::IsolatedWebAppsPolicy::AreIsolatedWebAppsEnabled(&profile) &&
          // Similar to extensions, we don't do any automatic updates in guest
          // sessions.
          !profile.IsGuestSession() &&
          // Web Apps are not a thing in off the record profiles, but have this
          // here just in case - we also wouldn't want to automatically update
          // IWAs in incognito windows.
          !profile.IsOffTheRecord() &&
#if BUILDFLAG(IS_CHROMEOS)
          base::FeatureList::IsEnabled(
              features::kIsolatedWebAppAutomaticUpdates)
#else
          false
#endif
              ),
      update_discovery_frequency_(std::move(update_discovery_frequency)),
      task_queue_{*this} {
}

IsolatedWebAppUpdateManager::~IsolatedWebAppUpdateManager() = default;

void IsolatedWebAppUpdateManager::SetProvider(base::PassKey<WebAppProvider>,
                                              WebAppProvider& provider) {
  provider_ = &provider;
  local_dev_mode_update_discoverer_ =
      std::make_unique<LocalDevModeUpdateDiscoverer>(*profile_, provider);
}

void IsolatedWebAppUpdateManager::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  has_started_ = true;
  install_manager_observation_.Observe(&provider_->install_manager());

  if (!IsAnyIwaInstalled()) {
    // If no IWA is installed, then we do not need to regularly check for
    // updates and can therefore be a little more efficient.
    // `install_manager_observation_` will take care of starting the timer once
    // an IWA is installed.
    return;
  }

  for (WebApp web_app : provider_->registrar_unsafe().GetApps()) {
    if (!web_app.isolation_data().has_value() ||
        !web_app.isolation_data()->pending_update_info().has_value()) {
      continue;
    }
    auto url_info = IsolatedWebAppUrlInfo::Create(web_app.start_url());
    if (!url_info.has_value()) {
      LOG(ERROR) << "Unable to calculate IsolatedWebAppUrlInfo from "
                 << web_app.start_url();
      continue;
    }

    // Off the record profiles cannot have `ScopedProfileKeepAlive`s.
    auto profile_keep_alive =
        profile_->IsOffTheRecord()
            ? nullptr
            : std::make_unique<ScopedProfileKeepAlive>(
                  &*profile_, ProfileKeepAliveOrigin::kIsolatedWebAppUpdate);

    // During startup of the `IsolatedWebAppUpdateManager`, we do not use
    // `IsolatedWebAppUpdateApplyWaiter`s to wait for all windows to close
    // before applying the update. Instead, we schedule the update apply tasks
    // directly (but do not start them yet). These tasks will be started one
    // after the other eventually (see the `BEST_EFFORT` call below), or via
    // `MaybeWaitUntilPendingUpdateIsApplied` if a window for an to-be-updated
    // app is opened.
    //
    // At this point, it is guaranteed that no IWA window has loaded, since the
    // `IsolatedWebAppURLLoaderFactory` can only create `URLLoader`s for IWAs
    // after the `WebAppProvider` has fully started, and this code runs as part
    // of the startup process of the `WebAppProvider`.
    task_queue_.Push(std::make_unique<IsolatedWebAppUpdateApplyTask>(
        *url_info,
        std::make_unique<ScopedKeepAlive>(
            KeepAliveOrigin::ISOLATED_WEB_APP_UPDATE,
            KeepAliveRestartOption::DISABLED),
        std::move(profile_keep_alive), provider_->scheduler()));
  }

  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&IsolatedWebAppUpdateManager::DelayedStart,
                                weak_factory_.GetWeakPtr()));
}

void IsolatedWebAppUpdateManager::DelayedStart() {
  // Kick-off task processing. The task queue can already contain
  // `IsolatedWebAppUpdateApplyTask`s for updates that are pending from the last
  // browser session and were created in `IsolatedWebAppUpdateManager::Start`.
  task_queue_.MaybeStartNextTask();

  QueueUpdateDiscoveryTasks();
}

void IsolatedWebAppUpdateManager::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Stop all potentially ongoing tasks and avoid scheduling new tasks.
  install_manager_observation_.Reset();
  next_update_discovery_check_.Reset();
  task_queue_.Clear();
  update_apply_waiters_.clear();
}

base::Value IsolatedWebAppUpdateManager::AsDebugValue() const {
  base::Value::List update_apply_waiters;
  for (const auto& [app_id, waiter] : update_apply_waiters_) {
    update_apply_waiters.Append(waiter->AsDebugValue());
  }

  return base::Value(
      base::Value::Dict()
          .Set("automatic_updates_enabled", automatic_updates_enabled_)
          .Set("update_discovery_frequency_in_minutes",
               update_discovery_frequency_.InSecondsF() /
                   base::Time::kSecondsPerMinute)
          .Set("next_update_discovery_check",
               next_update_discovery_check_.AsDebugValue())
          .Set("task_queue", task_queue_.AsDebugValue())
          .Set("update_apply_waiters", std::move(update_apply_waiters)));
}

bool IsolatedWebAppUpdateManager::IsUpdateBeingApplied(
    base::PassKey<IsolatedWebAppURLLoaderFactory>,
    const webapps::AppId app_id) const {
  return task_queue_.IsUpdateApplyTaskQueued(app_id);
}

void IsolatedWebAppUpdateManager::PrioritizeUpdateAndWait(
    base::PassKey<IsolatedWebAppURLLoaderFactory>,
    const webapps::AppId& app_id,
    base::OnceCallback<void(IsolatedWebAppUpdateApplyTask::CompletionStatus)>
        callback) {
  PrioritizeUpdateAndWaitImpl(app_id, std::move(callback));
}

void IsolatedWebAppUpdateManager::PrioritizeUpdateAndWaitImpl(
    const webapps::AppId& app_id,
    base::OnceCallback<void(IsolatedWebAppUpdateApplyTask::CompletionStatus)>
        callback) {
  bool task_has_started =
      task_queue_.EnsureQueuedUpdateApplyTaskHasStarted(app_id);
  if (task_has_started) {
    on_update_finished_callbacks_
        .try_emplace(app_id,
                     std::make_unique<base::OnceCallbackList<void(
                         IsolatedWebAppUpdateApplyTask::CompletionStatus)>>())
        .first->second->AddUnsafe(std::move(callback));
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::unexpected(IsolatedWebAppApplyUpdateCommandError{
                .message = "Update is not currently being applied."})));
  }
}

void IsolatedWebAppUpdateManager::SetEnableAutomaticUpdatesForTesting(
    bool automatic_updates_enabled) {
  CHECK(!has_started_);
  automatic_updates_enabled_ = automatic_updates_enabled;
}

void IsolatedWebAppUpdateManager::OnWebAppInstalled(
    const webapps::AppId& app_id) {
  MaybeScheduleUpdateDiscoveryCheck();
}

void IsolatedWebAppUpdateManager::OnWebAppUninstalled(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
  update_apply_waiters_.erase(app_id);
  task_queue_.ClearNonStartedTasksOfApp(app_id);
  MaybeResetScheduledUpdateDiscoveryCheck();
}

size_t IsolatedWebAppUpdateManager::DiscoverUpdatesNow() {
  // If an update discovery check is already scheduled, reset it, so that the
  // next update discovery happens based on `update_discovery_frequency_` time
  // after `QueueUpdateDiscoveryTasks` is called.
  next_update_discovery_check_.Reset();
  return QueueUpdateDiscoveryTasks();
}

void IsolatedWebAppUpdateManager::DiscoverApplyAndPrioritizeLocalDevModeUpdate(
    const IsolatedWebAppLocation& location,
    const IsolatedWebAppUrlInfo& url_info,
    base::OnceCallback<void(base::expected<base::Version, std::string>)>
        callback) {
  local_dev_mode_update_discoverer_->DiscoverLocalUpdate(
      location, url_info,
      base::BindOnce(&IsolatedWebAppUpdateManager::OnLocalUpdateDiscovered,
                     weak_factory_.GetWeakPtr(), url_info,
                     std::move(callback)));
}

bool IsolatedWebAppUpdateManager::IsAnyIwaInstalled() {
  for (const WebApp& app : provider_->registrar_unsafe().GetApps()) {
    if (app.isolation_data().has_value()) {
      return true;
    }
  }
  return false;
}

base::flat_map<web_package::SignedWebBundleId, GURL>
IsolatedWebAppUpdateManager::GetForceInstalledBundleIdToUpdateManifestUrlMap() {
  base::flat_map<web_package::SignedWebBundleId, GURL>
      id_to_update_manifest_map;

// TODO(crbug.com/1458725): Enable automatic updates on other platforms.
#if BUILDFLAG(IS_CHROMEOS)
  const base::Value::List& iwa_force_install_list =
      profile_->GetPrefs()->GetList(prefs::kIsolatedWebAppInstallForceList);
  for (const base::Value& policy_entry : iwa_force_install_list) {
    base::expected<IsolatedWebAppExternalInstallOptions, std::string> options =
        IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(policy_entry);
    if (!options.has_value()) {
      LOG(ERROR) << "IsolatedWebAppUpdateManager: "
                 << "Could not parse IWA force-install policy: "
                 << options.error();
      continue;
    }

    id_to_update_manifest_map.emplace(options->web_bundle_id(),
                                      options->update_manifest_url());
  }
#endif

  return id_to_update_manifest_map;
}

size_t IsolatedWebAppUpdateManager::QueueUpdateDiscoveryTasks() {
  // Clear the log of previously finished update discovery tasks when queueing
  // new tasks so that it doesn't grow forever.
  task_queue_.ClearUpdateDiscoveryLog();

  base::flat_map<web_package::SignedWebBundleId, GURL>
      id_to_update_manifest_map =
          GetForceInstalledBundleIdToUpdateManifestUrlMap();

  // TODO(crbug.com/1459160): In the future, we also need to automatically
  // update IWAs not installed via policy.
  size_t num_new_tasks = 0;
  for (const auto& [web_bundle_id, update_manifest_url] :
       id_to_update_manifest_map) {
    auto url_info =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id);
    const WebApp* web_app =
        provider_->registrar_unsafe().GetAppById(url_info.app_id());
    if (!web_app) {
      continue;
    }
    const absl::optional<WebApp::IsolationData>& isolation_data =
        web_app->isolation_data();
    if (!isolation_data) {
      continue;
    }
    if (!absl::holds_alternative<InstalledBundle>(isolation_data->location)) {
      // Never automatically update IWAs installed in dev mode. Updates for dev
      // mode apps will be triggerable manually from the upcoming dev mode
      // browser UI.
      continue;
    }

    task_queue_.Push(std::make_unique<IsolatedWebAppUpdateDiscoveryTask>(
        update_manifest_url, url_info, provider_->scheduler(),
        provider_->registrar_unsafe(), profile_->GetURLLoaderFactory()));
    ++num_new_tasks;
  }

  task_queue_.MaybeStartNextTask();

  MaybeScheduleUpdateDiscoveryCheck();

  return num_new_tasks;
}

void IsolatedWebAppUpdateManager::MaybeScheduleUpdateDiscoveryCheck() {
  if (automatic_updates_enabled_ &&
      !next_update_discovery_check_.IsScheduled() && IsAnyIwaInstalled()) {
    next_update_discovery_check_.ScheduleWithJitter(
        update_discovery_frequency_,
        base::BindOnce(
            base::IgnoreResult(
                &IsolatedWebAppUpdateManager::QueueUpdateDiscoveryTasks),
            // Ok to use `base::Unretained` here because `this` owns
            // `next_update_check_`.
            base::Unretained(this)));
  }
}

void IsolatedWebAppUpdateManager::MaybeResetScheduledUpdateDiscoveryCheck() {
  if (next_update_discovery_check_.IsScheduled() && !IsAnyIwaInstalled()) {
    next_update_discovery_check_.Reset();
  }
}

void IsolatedWebAppUpdateManager::CreateUpdateApplyWaiter(
    const IsolatedWebAppUrlInfo& url_info,
    base::OnceClosure on_update_apply_task_created) {
  const webapps::AppId& app_id = url_info.app_id();
  if (update_apply_waiters_.contains(app_id)) {
    return;
  }
  auto [it, was_insertion] = update_apply_waiters_.emplace(
      app_id, std::make_unique<IsolatedWebAppUpdateApplyWaiter>(
                  url_info, provider_->ui_manager()));
  CHECK(was_insertion);
  it->second->Wait(
      &*profile_,
      base::BindOnce(&IsolatedWebAppUpdateManager::OnUpdateApplyWaiterFinished,
                     weak_factory_.GetWeakPtr(), url_info,
                     std::move(on_update_apply_task_created)));
}

void IsolatedWebAppUpdateManager::OnUpdateDiscoveryTaskCompleted(
    std::unique_ptr<IsolatedWebAppUpdateDiscoveryTask> task,
    IsolatedWebAppUpdateDiscoveryTask::CompletionStatus status) {
  if (status.has_value() && *status ==
                                IsolatedWebAppUpdateDiscoveryTask::Success::
                                    kUpdateFoundAndSavedInDatabase) {
    CreateUpdateApplyWaiter(task->url_info());
  }

  task_queue_.MaybeStartNextTask();
}

void IsolatedWebAppUpdateManager::OnUpdateApplyWaiterFinished(
    IsolatedWebAppUrlInfo url_info,
    base::OnceClosure on_update_apply_task_created,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive) {
  update_apply_waiters_.erase(url_info.app_id());

  task_queue_.Push(std::make_unique<IsolatedWebAppUpdateApplyTask>(
      url_info, std::move(keep_alive), std::move(profile_keep_alive),
      provider_->scheduler()));
  std::move(on_update_apply_task_created).Run();

  task_queue_.MaybeStartNextTask();
}

void IsolatedWebAppUpdateManager::OnUpdateApplyTaskCompleted(
    std::unique_ptr<IsolatedWebAppUpdateApplyTask> task,
    IsolatedWebAppUpdateApplyTask::CompletionStatus status) {
  auto callbacks_it =
      on_update_finished_callbacks_.find(task->url_info().app_id());
  if (callbacks_it != on_update_finished_callbacks_.end()) {
    callbacks_it->second->Notify(status);
    if (callbacks_it->second->empty()) {
      on_update_finished_callbacks_.erase(callbacks_it);
    }
  }

  task_queue_.MaybeStartNextTask();
}

void IsolatedWebAppUpdateManager::OnLocalUpdateDiscovered(
    IsolatedWebAppUrlInfo url_info,
    base::OnceCallback<void(base::expected<base::Version, std::string>)>
        callback,
    base::expected<base::Version, std::string> update_discovery_result) {
  ASSIGN_OR_RETURN(auto update_version, update_discovery_result,
                   [&](const auto& error) {
                     std::move(callback).Run(base::unexpected(error));
                   });

  CreateUpdateApplyWaiter(
      url_info,
      /*on_update_apply_task_created=*/base::BindOnce(
          &IsolatedWebAppUpdateManager::OnLocalUpdateApplyTaskCreated,
          weak_factory_.GetWeakPtr(), url_info, std::move(update_version),
          std::move(callback)));
}

void IsolatedWebAppUpdateManager::OnLocalUpdateApplyTaskCreated(
    IsolatedWebAppUrlInfo url_info,
    base::Version update_version,
    base::OnceCallback<void(base::expected<base::Version, std::string>)>
        callback) {
  auto transform_status =
      [](base::Version update_version,
         IsolatedWebAppUpdateApplyTask::CompletionStatus status) {
        return status.transform([&]() { return update_version; })
            .transform_error(
                [](const IsolatedWebAppApplyUpdateCommandError& error) {
                  return error.message;
                });
      };

  PrioritizeUpdateAndWaitImpl(
      url_info.app_id(),
      base::BindOnce(std::move(transform_status), std::move(update_version))
          .Then(std::move(callback)));
}

IsolatedWebAppUpdateManager::NextUpdateDiscoveryCheck::
    NextUpdateDiscoveryCheck() = default;

IsolatedWebAppUpdateManager::NextUpdateDiscoveryCheck::
    ~NextUpdateDiscoveryCheck() = default;

void IsolatedWebAppUpdateManager::NextUpdateDiscoveryCheck::ScheduleWithJitter(
    const base::TimeDelta& base_delay,
    base::OnceClosure callback) {
  // 20% jitter (between 0.8 and 1.2)
  double jitter_factor = base::RandDouble() * 0.4 + 0.8;
  base::TimeDelta delay = base_delay * jitter_factor;

  auto cancelable_callback = std::make_unique<base::CancelableOnceClosure>(
      base::BindOnce(&NextUpdateDiscoveryCheck::Reset,
                     // Okay to use `base::Unretained` here, since `this`
                     // owns `next_check_`.
                     base::Unretained(this))
          .Then(std::move(callback)));

  next_check_ = {
      {base::TimeTicks::Now() + delay, std::move(cancelable_callback)}};
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, next_check_->second->callback(), delay);
}

absl::optional<base::TimeTicks>
IsolatedWebAppUpdateManager::NextUpdateDiscoveryCheck::GetScheduledTime()
    const {
  if (next_check_.has_value()) {
    return next_check_->first;
  }
  return absl::nullopt;
}

bool IsolatedWebAppUpdateManager::NextUpdateDiscoveryCheck::IsScheduled()
    const {
  return next_check_.has_value();
}

void IsolatedWebAppUpdateManager::NextUpdateDiscoveryCheck::Reset() {
  // This will cancel any scheduled callbacks, because it deletes the
  // `base::CancelableOnceCallback`.
  next_check_.reset();
}

base::Value
IsolatedWebAppUpdateManager::NextUpdateDiscoveryCheck::AsDebugValue() const {
  if (!next_check_.has_value()) {
    return base::Value("not scheduled");
  }

  base::TimeDelta next_update_check =
      next_check_->first - base::TimeTicks::Now();
  double next_update_check_in_minutes =
      next_update_check.InSecondsF() / base::Time::kSecondsPerMinute;

  return base::Value(base::Value::Dict().Set("next_update_check_in_minutes",
                                             next_update_check_in_minutes));
}

IsolatedWebAppUpdateManager::TaskQueue::TaskQueue(
    IsolatedWebAppUpdateManager& update_manager)
    : update_manager_(update_manager) {}

IsolatedWebAppUpdateManager::TaskQueue::~TaskQueue() = default;

base::Value IsolatedWebAppUpdateManager::TaskQueue::AsDebugValue() const {
  base::Value::List update_discovery_tasks;
  for (const auto& task : update_discovery_tasks_) {
    update_discovery_tasks.Append(task->AsDebugValue());
  }

  base::Value::List update_apply_tasks;
  for (const auto& task : update_apply_tasks_) {
    update_apply_tasks.Append(task->AsDebugValue());
  }

  return base::Value(
      base::Value::Dict()
          .Set("update_discovery_tasks", std::move(update_discovery_tasks))
          .Set("update_discovery_log", update_discovery_results_log_.Clone())
          .Set("update_apply_tasks", std::move(update_apply_tasks))
          .Set("update_apply_log", update_apply_results_log_.Clone()));
}

void IsolatedWebAppUpdateManager::TaskQueue::ClearUpdateDiscoveryLog() {
  update_discovery_results_log_.clear();
}

void IsolatedWebAppUpdateManager::TaskQueue::Push(
    std::unique_ptr<IsolatedWebAppUpdateDiscoveryTask> task) {
  update_discovery_tasks_.push_back(std::move(task));
}

void IsolatedWebAppUpdateManager::TaskQueue::Push(
    std::unique_ptr<IsolatedWebAppUpdateApplyTask> task) {
  update_apply_tasks_.push_back(std::move(task));
}

void IsolatedWebAppUpdateManager::TaskQueue::Clear() {
  update_discovery_tasks_.clear();
  update_apply_tasks_.clear();
}

bool IsolatedWebAppUpdateManager::TaskQueue::
    EnsureQueuedUpdateApplyTaskHasStarted(const webapps::AppId& app_id) {
  auto task_it =
      base::ranges::find_if(update_apply_tasks_, [&app_id](const auto& task) {
        return task->url_info().app_id() == app_id;
      });
  if (task_it == update_apply_tasks_.end()) {
    return false;
  }

  if (!task_it->get()->has_started()) {
    StartUpdateApplyTask(task_it->get());
  }
  return true;
}

void IsolatedWebAppUpdateManager::TaskQueue::ClearNonStartedTasksOfApp(
    const webapps::AppId& app_id) {
  base::EraseIf(update_discovery_tasks_, [&app_id](const auto& task) {
    return !task->has_started() && task->url_info().app_id() == app_id;
  });
  base::EraseIf(update_apply_tasks_, [&app_id](const auto& task) {
    return !task->has_started() && task->url_info().app_id() == app_id;
  });
}

void IsolatedWebAppUpdateManager::TaskQueue::MaybeStartNextTask() {
  if (IsAnyTaskRunning()) {
    return;
  }

  auto next_update_apply_task_it = update_apply_tasks_.begin();
  if (next_update_apply_task_it != update_apply_tasks_.end()) {
    StartUpdateApplyTask(next_update_apply_task_it->get());
    return;
  }

  auto next_update_discovery_task_it = update_discovery_tasks_.begin();
  if (next_update_discovery_task_it != update_discovery_tasks_.end()) {
    StartUpdateDiscoveryTask(next_update_discovery_task_it->get());
    return;
  }
}

bool IsolatedWebAppUpdateManager::TaskQueue::IsUpdateApplyTaskQueued(
    const webapps::AppId& app_id) const {
  return base::ranges::any_of(update_apply_tasks_, [&app_id](const auto& task) {
    return task->url_info().app_id() == app_id;
  });
}

void IsolatedWebAppUpdateManager::TaskQueue::StartUpdateDiscoveryTask(
    IsolatedWebAppUpdateDiscoveryTask* task_ptr) {
  task_ptr->Start(base::BindOnce(
      &TaskQueue::OnUpdateDiscoveryTaskCompleted,
      // We can use `base::Unretained` here, because `this` owns the task.
      base::Unretained(this), task_ptr));
}

void IsolatedWebAppUpdateManager::TaskQueue::StartUpdateApplyTask(
    IsolatedWebAppUpdateApplyTask* task_ptr) {
  task_ptr->Start(base::BindOnce(
      &TaskQueue::OnUpdateApplyTaskCompleted,
      // We can use `base::Unretained` here, because `this` owns the task.
      base::Unretained(this), task_ptr));
}

bool IsolatedWebAppUpdateManager::TaskQueue::IsAnyTaskRunning() const {
  return base::ranges::any_of(
             update_discovery_tasks_,
             [](const auto& task) { return task->has_started(); }) ||
         base::ranges::any_of(update_apply_tasks_, [](const auto& task) {
           return task->has_started();
         });
}

void IsolatedWebAppUpdateManager::TaskQueue::OnUpdateDiscoveryTaskCompleted(
    IsolatedWebAppUpdateDiscoveryTask* task_ptr,
    IsolatedWebAppUpdateDiscoveryTask::CompletionStatus status) {
  auto task_it = base::ranges::find_if(update_discovery_tasks_,
                                       base::MatchesUniquePtr(task_ptr));
  CHECK(task_it != update_discovery_tasks_.end());
  std::unique_ptr<IsolatedWebAppUpdateDiscoveryTask> task = std::move(*task_it);
  update_discovery_tasks_.erase(task_it);

  update_discovery_results_log_.Append(task->AsDebugValue());
  if (!status.has_value()) {
    LOG(ERROR) << "Isolated Web App update discovery for "
               << task->url_info().web_bundle_id().id()
               << " failed: " << status.error()
               << " debug log: " << task->AsDebugValue();
  } else {
    VLOG(1) << "Isolated Web App update discovery for "
            << task->url_info().web_bundle_id().id()
            << " succeeded: " << status.value();
  }

  update_manager_->OnUpdateDiscoveryTaskCompleted(std::move(task), status);
}

void IsolatedWebAppUpdateManager::TaskQueue::OnUpdateApplyTaskCompleted(
    IsolatedWebAppUpdateApplyTask* task_ptr,
    IsolatedWebAppUpdateApplyTask::CompletionStatus status) {
  auto task_it = base::ranges::find_if(update_apply_tasks_,
                                       base::MatchesUniquePtr(task_ptr));
  CHECK(task_it != update_apply_tasks_.end());
  std::unique_ptr<IsolatedWebAppUpdateApplyTask> task = std::move(*task_it);
  update_apply_tasks_.erase(task_it);

  update_apply_results_log_.Append(task->AsDebugValue());
  if (!status.has_value()) {
    LOG(ERROR) << "Applying an Isolated Web App update for "
               << task->url_info().web_bundle_id().id()
               << " failed: " << status.error()
               << " debug log: " << task->AsDebugValue();
  } else {
    VLOG(1) << "Applying an Isolated Web App update for "
            << task->url_info().web_bundle_id().id() << " succeeded.";
    static_assert(
        std::is_void_v<
            IsolatedWebAppUpdateApplyTask::CompletionStatus::value_type>,
        "Log `status.value()` above should it become non-void.");
  }

  update_manager_->OnUpdateApplyTaskCompleted(std::move(task), status);
}

}  // namespace web_app
