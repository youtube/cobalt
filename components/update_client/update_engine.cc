// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_engine.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/component.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/protocol_parser.h"
#include "components/update_client/update_checker.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"

namespace update_client {

UpdateContext::UpdateContext(
    scoped_refptr<Configurator> config,
    bool is_foreground,
    const std::vector<std::string>& ids,
    UpdateClient::CrxDataCallback crx_data_callback,
    const UpdateEngine::NotifyObserversCallback& notify_observers_callback,
    UpdateEngine::Callback callback,
    CrxDownloader::Factory crx_downloader_factory)
    : config(config),
      is_foreground(is_foreground),
      enabled_component_updates(config->EnabledComponentUpdates()),
      ids(ids),
      crx_data_callback(std::move(crx_data_callback)),
      notify_observers_callback(notify_observers_callback),
      callback(std::move(callback)),
      crx_downloader_factory(crx_downloader_factory),
      session_id(base::StrCat({"{", base::GenerateGUID(), "}"})) {
  for (const auto& id : ids) {
    components.insert(
        std::make_pair(id, std::make_unique<Component>(*this, id)));
  }
}

UpdateContext::~UpdateContext() {
#if defined(STARBOARD)
  LOG(INFO) << "UpdateContext::~UpdateContext";
#endif
}

UpdateEngine::UpdateEngine(
    scoped_refptr<Configurator> config,
    UpdateChecker::Factory update_checker_factory,
    CrxDownloader::Factory crx_downloader_factory,
    scoped_refptr<PingManager> ping_manager,
    const NotifyObserversCallback& notify_observers_callback)
    : config_(config),
      update_checker_factory_(update_checker_factory),
      crx_downloader_factory_(crx_downloader_factory),
      ping_manager_(ping_manager),
      metadata_(
          std::make_unique<PersistedData>(config->GetPrefService(),
                                          config->GetActivityDataService())),
      notify_observers_callback_(notify_observers_callback) {
#if defined(STARBOARD)
    LOG(INFO) << "UpdateEngine::UpdateEngine";
#endif
}

UpdateEngine::~UpdateEngine() {
  DCHECK(thread_checker_.CalledOnValidThread());
#if defined(STARBOARD)
  LOG(INFO) << "UpdateEngine::~UpdateEngine";
#endif
}

#if !defined(STARBOARD)
void UpdateEngine::Update(bool is_foreground,
                          const std::vector<std::string>& ids,
                          UpdateClient::CrxDataCallback crx_data_callback,
                          Callback callback) {
#else
void UpdateEngine::Update(bool is_foreground,
                          const std::vector<std::string>& ids,
                          UpdateClient::CrxDataCallback crx_data_callback,
                          Callback callback,
                          base::OnceClosure& cancelation_closure) {

#endif
  DCHECK(thread_checker_.CalledOnValidThread());

#if defined(STARBOARD)
  LOG(INFO) << "UpdateEngine::Update";
#endif

  if (ids.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), Error::INVALID_ARGUMENT));
    return;
  }

  if (IsThrottled(is_foreground)) {
    // TODO(xiaochu): remove this log after https://crbug.com/851151 is fixed.
    VLOG(1) << "Background update is throttled for following components:";
    for (const auto& id : ids) {
      VLOG(1) << "id:" << id;
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), Error::RETRY_LATER));
    return;
  }

  const auto update_context = base::MakeRefCounted<UpdateContext>(
      config_, is_foreground, ids, std::move(crx_data_callback),
      notify_observers_callback_, std::move(callback), crx_downloader_factory_);
  DCHECK(!update_context->session_id.empty());
#if defined(STARBOARD)
  cancelation_closure = base::BindOnce(&UpdateEngine::Cancel, this,
                                       update_context->session_id, ids);
#endif

  const auto result = update_contexts_.insert(
      std::make_pair(update_context->session_id, update_context));
  DCHECK(result.second);

  // Calls out to get the corresponding CrxComponent data for the CRXs in this
  // update context.
  const auto crx_components =
      std::move(update_context->crx_data_callback).Run(update_context->ids);
  DCHECK_EQ(update_context->ids.size(), crx_components.size());

  for (size_t i = 0; i != update_context->ids.size(); ++i) {
    const auto& id = update_context->ids[i];

    DCHECK(update_context->components[id]->state() == ComponentState::kNew);

    const auto crx_component = crx_components[i];
    if (crx_component) {
      // This component can be checked for updates.
      DCHECK_EQ(id, GetCrxComponentID(*crx_component));
      auto& component = update_context->components[id];
      component->set_crx_component(*crx_component);
      component->set_previous_version(component->crx_component()->version);
      component->set_previous_fp(component->crx_component()->fingerprint);
      update_context->components_to_check_for_updates.push_back(id);
    } else {
      // |CrxDataCallback| did not return a CrxComponent instance for this
      // component, which most likely, has been uninstalled. This component
      // is going to be transitioned to an error state when the its |Handle|
      // method is called later on by the engine.
      update_context->component_queue.push(id);
    }
  }

  if (update_context->components_to_check_for_updates.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&UpdateEngine::HandleComponent,
                                  base::Unretained(this), update_context));
    return;
  }

  for (const auto& id : update_context->components_to_check_for_updates)
    update_context->components[id]->Handle(
        base::BindOnce(&UpdateEngine::ComponentCheckingForUpdatesStart,
                       base::Unretained(this), update_context, id));
}

void UpdateEngine::ComponentCheckingForUpdatesStart(
    scoped_refptr<UpdateContext> update_context,
    const std::string& id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(update_context);

  DCHECK_EQ(1u, update_context->components.count(id));
  DCHECK(update_context->components.at(id));

#if defined(STARBOARD)
  LOG(INFO) << "UpdateEngine::ComponentCheckingForUpdatesStart";
#endif

  // Handle |kChecking| state.
  auto& component = *update_context->components.at(id);
  component.Handle(
      base::BindOnce(&UpdateEngine::ComponentCheckingForUpdatesComplete,
                     base::Unretained(this), update_context));

  ++update_context->num_components_ready_to_check;
  if (update_context->num_components_ready_to_check <
      update_context->components_to_check_for_updates.size()) {
    return;
  }

#if defined(STARBOARD)
  if (is_cancelled_) {
    LOG(WARNING) << "UpdateEngine::ComponentCheckingForUpdatesStart cancelled";
    return;
  }
#endif

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&UpdateEngine::DoUpdateCheck,
                                base::Unretained(this), update_context));
}

void UpdateEngine::DoUpdateCheck(scoped_refptr<UpdateContext> update_context) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(update_context);

#if defined(STARBOARD)
  LOG(INFO) << "UpdateEngine::DoUpdateCheck";
#endif

  update_context->update_checker =
      update_checker_factory_(config_, metadata_.get());

  update_context->update_checker->CheckForUpdates(
      update_context->session_id,
      update_context->components_to_check_for_updates,
      update_context->components, config_->ExtraRequestParams(),
      update_context->enabled_component_updates,
      base::BindOnce(&UpdateEngine::UpdateCheckResultsAvailable,
                     base::Unretained(this), update_context));
}

void UpdateEngine::UpdateCheckResultsAvailable(
    scoped_refptr<UpdateContext> update_context,
    const base::Optional<ProtocolParser::Results>& results,
    ErrorCategory error_category,
    int error,
    int retry_after_sec) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(update_context);

#if defined(STARBOARD)
  LOG(INFO) << "UpdateEngine::UpdateCheckResultsAvailable";
#endif

  update_context->retry_after_sec = retry_after_sec;

  // Only positive values for throttle_sec are effective. 0 means that no
  // throttling occurs and it resets |throttle_updates_until_|.
  // Negative values are not trusted and are ignored.
  constexpr int kMaxRetryAfterSec = 24 * 60 * 60;  // 24 hours.
  const int throttle_sec =
      std::min(update_context->retry_after_sec, kMaxRetryAfterSec);
  if (throttle_sec >= 0) {
    throttle_updates_until_ =
        throttle_sec ? base::TimeTicks::Now() +
                           base::TimeDelta::FromSeconds(throttle_sec)
                     : base::TimeTicks();
  }

  update_context->update_check_error = error;

  if (error) {
    DCHECK(!results);
    for (const auto& id : update_context->components_to_check_for_updates) {
      DCHECK_EQ(1u, update_context->components.count(id));
      auto& component = update_context->components.at(id);
      component->SetUpdateCheckResult(base::nullopt,
                                      ErrorCategory::kUpdateCheck, error);
    }
    return;
  }

  DCHECK(results);
  DCHECK_EQ(0, error);

  std::map<std::string, ProtocolParser::Result> id_to_result;
  for (const auto& result : results->list)
    id_to_result[result.extension_id] = result;

  for (const auto& id : update_context->components_to_check_for_updates) {
    DCHECK_EQ(1u, update_context->components.count(id));
    auto& component = update_context->components.at(id);
    const auto& it = id_to_result.find(id);
    if (it != id_to_result.end()) {
      const auto result = it->second;
      const auto error = [](const std::string& status) {
        // First, handle app status literals which can be folded down as an
        // updatecheck status
        if (status == "error-unknownApplication")
          return std::make_pair(ErrorCategory::kUpdateCheck,
                                ProtocolError::UNKNOWN_APPLICATION);
        if (status == "restricted")
          return std::make_pair(ErrorCategory::kUpdateCheck,
                                ProtocolError::RESTRICTED_APPLICATION);
        if (status == "error-invalidAppId")
          return std::make_pair(ErrorCategory::kUpdateCheck,
                                ProtocolError::INVALID_APPID);
        // If the parser has return a valid result and the status is not one of
        // the literals above, then this must be a success an not a parse error.
        return std::make_pair(ErrorCategory::kNone, ProtocolError::NONE);
      }(result.status);
      component->SetUpdateCheckResult(result, error.first,
                                      static_cast<int>(error.second));
    } else {
      component->SetUpdateCheckResult(
          base::nullopt, ErrorCategory::kUpdateCheck,
          static_cast<int>(ProtocolError::UPDATE_RESPONSE_NOT_FOUND));
    }
  }
}

void UpdateEngine::ComponentCheckingForUpdatesComplete(
    scoped_refptr<UpdateContext> update_context) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(update_context);

#if defined(STARBOARD)
  LOG(INFO) << "UpdateEngine::ComponentCheckingForUpdatesComplete";
#endif

  ++update_context->num_components_checked;
  if (update_context->num_components_checked <
      update_context->components_to_check_for_updates.size()) {
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&UpdateEngine::UpdateCheckComplete,
                                base::Unretained(this), update_context));
}

void UpdateEngine::UpdateCheckComplete(
    scoped_refptr<UpdateContext> update_context) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(update_context);

#if defined(STARBOARD)
  LOG(INFO) << "UpdateEngine::UpdateCheckComplete";
#endif

  for (const auto& id : update_context->components_to_check_for_updates)
    update_context->component_queue.push(id);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&UpdateEngine::HandleComponent,
                                base::Unretained(this), update_context));
}

void UpdateEngine::HandleComponent(
    scoped_refptr<UpdateContext> update_context) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(update_context);

#if defined(STARBOARD)
  LOG(INFO) << "UpdateEngine::HandleComponent";
#endif

  auto& queue = update_context->component_queue;

  if (queue.empty()) {
    const Error error = update_context->update_check_error
                            ? Error::UPDATE_CHECK_ERROR
                            : Error::NONE;

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&UpdateEngine::UpdateComplete, base::Unretained(this),
                       update_context, error));
    return;
  }

  const auto& id = queue.front();
  DCHECK_EQ(1u, update_context->components.count(id));
  const auto& component = update_context->components.at(id);
  DCHECK(component);

  auto& next_update_delay = update_context->next_update_delay;
  if (!next_update_delay.is_zero() && component->IsUpdateAvailable()) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&UpdateEngine::HandleComponent, base::Unretained(this),
                       update_context),
        next_update_delay);
    next_update_delay = base::TimeDelta();

#if defined(STARBOARD)
  if (!is_cancelled_) {
    notify_observers_callback_.Run(UpdateClient::Observer::Events::COMPONENT_WAIT, id);
  } else {
    LOG(WARNING) << "UpdateEngine::HandleComponent skip NotifyObservers";
  }
#else
  notify_observers_callback_.Run(
        UpdateClient::Observer::Events::COMPONENT_WAIT, id);
#endif
    return;
  }

  component->Handle(base::BindOnce(&UpdateEngine::HandleComponentComplete,
                                   base::Unretained(this), update_context));
}

void UpdateEngine::HandleComponentComplete(
    scoped_refptr<UpdateContext> update_context) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(update_context);

#if defined(STARBOARD)
  LOG(INFO) << "UpdateEngine::HandleComponentComplete";
#endif

  auto& queue = update_context->component_queue;
  DCHECK(!queue.empty());

  const auto& id = queue.front();
  DCHECK_EQ(1u, update_context->components.count(id));
  const auto& component = update_context->components.at(id);
  DCHECK(component);

  if (component->IsHandled()) {
    update_context->next_update_delay = component->GetUpdateDuration();

    if (!component->events().empty()) {
#if defined(STARBOARD)
      if (!is_cancelled_) {
        ping_manager_->SendPing(*component,
                                  base::BindOnce([](int, const std::string&) {}));

      } else {
        LOG(WARNING) << "UpdateEngine::HandleComponentComplete skip SendPing";
      }
#else
      ping_manager_->SendPing(*component,
                              base::BindOnce([](int, const std::string&) {}));
#endif
    }

    queue.pop();
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&UpdateEngine::HandleComponent,
                                base::Unretained(this), update_context));
}

void UpdateEngine::UpdateComplete(scoped_refptr<UpdateContext> update_context,
                                  Error error) {
#if defined(STARBOARD)
  LOG(INFO) << "UpdateEngine::UpdateComplete";
#endif

  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(update_context);

  const auto num_erased = update_contexts_.erase(update_context->session_id);
  DCHECK_EQ(1u, num_erased);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(update_context->callback), error));
}

bool UpdateEngine::GetUpdateState(const std::string& id,
                                  CrxUpdateItem* update_item) {
  DCHECK(thread_checker_.CalledOnValidThread());
#if defined(STARBOARD)
  LOG(INFO) << "UpdateEngine::GetUpdateState";
#endif
  for (const auto& context : update_contexts_) {
    const auto& components = context.second->components;
    const auto it = components.find(id);
    if (it != components.end()) {
      *update_item = it->second->GetCrxUpdateItem();
      return true;
    }
  }
  return false;
}

bool UpdateEngine::IsThrottled(bool is_foreground) const {
  DCHECK(thread_checker_.CalledOnValidThread());
#if defined(STARBOARD)
  LOG(INFO) << "UpdateEngine::IsThrottled";
#endif

  if (is_foreground || throttle_updates_until_.is_null())
    return false;

  const auto now(base::TimeTicks::Now());

  // Throttle the calls in the interval (t - 1 day, t) to limit the effect of
  // unset clocks or clock drift.
  return throttle_updates_until_ - base::TimeDelta::FromDays(1) < now &&
         now < throttle_updates_until_;
}

#if defined(STARBOARD)
void UpdateEngine::Cancel(const std::string& update_context_session_id,
                          const std::vector<std::string>& crx_component_ids) {
  LOG(INFO) << "UpdateEngine::Cancel";
  is_cancelled_ = true;
  if (ping_manager_.get()) {
    ping_manager_->Cancel();
  }

  const auto& context = update_contexts_.at(update_context_session_id);
  if (context->update_checker.get()) {
    context->update_checker->Cancel();
  }
  for (const auto& crx_component_id : crx_component_ids) {
    auto& component = context->components.at(crx_component_id);
    component->Cancel();
  }
}
#endif

void UpdateEngine::SendUninstallPing(const std::string& id,
                                     const base::Version& version,
                                     int reason,
                                     Callback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

#if defined(STARBOARD)
  LOG(INFO) << "UpdateEngine::SendUninstallPing";
#endif

  const auto update_context = base::MakeRefCounted<UpdateContext>(
      config_, false, std::vector<std::string>{id},
      UpdateClient::CrxDataCallback(), UpdateEngine::NotifyObserversCallback(),
      std::move(callback), nullptr);
  DCHECK(!update_context->session_id.empty());

  const auto result = update_contexts_.insert(
      std::make_pair(update_context->session_id, update_context));
  DCHECK(result.second);

  DCHECK(update_context);
  DCHECK_EQ(1u, update_context->ids.size());
  DCHECK_EQ(1u, update_context->components.count(id));
  const auto& component = update_context->components.at(id);

  component->Uninstall(version, reason);

  update_context->component_queue.push(id);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&UpdateEngine::HandleComponent,
                                base::Unretained(this), update_context));
}

}  // namespace update_client
