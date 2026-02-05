// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/component.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/update_client/action_runner.h"
#include "components/update_client/configurator.h"
#include "components/update_client/network.h"
#include "components/update_client/op_download.h"
#include "components/update_client/op_install.h"
#include "components/update_client/op_puffin.h"
#include "components/update_client/patcher.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/pipeline.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/protocol_serializer.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/unpacker.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/update_client_metrics.h"
#include "components/update_client/update_engine.h"
#include "components/update_client/utils.h"

#if BUILDFLAG(IS_STARBOARD)
#include "components/update_client/crx_downloader_factory.h"
#include "components/update_client/cobalt_slot_management.h"
#endif

// TODO(b/448186580): consider replacing all LOG with D(V)LOG.

namespace update_client {

// TODO(b/455004672): Investigate the legacy functions from m114. 
// These functions are ported from m114 because Cobalt customizations
// depend on them.
#if defined(USE_M114_FUNCTIONS)
namespace {

using InstallOnBlockingTaskRunnerCompleteCallback = base::OnceCallback<
    void(ErrorCategory error_category, int error_code, int extra_code1)>;

void StartInstallOnBlockingTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    const std::vector<uint8_t>& pk_hash,
#if defined(IN_MEMORY_UPDATES)
    const base::FilePath& installation_dir,
    const std::string* crx_str,
#else
    const base::FilePath& crx_path,
#endif
#if BUILDFLAG(USE_EVERGREEN)
    int installation_index,
    PersistedData* metadata,
    const std::string& id,
    const std::string& version,
#endif
    const std::string& fingerprint,
    std::unique_ptr<CrxInstaller::InstallParams> install_params,
    scoped_refptr<CrxInstaller> installer,
    std::unique_ptr<Unzipper> unzipper_,
    scoped_refptr<Patcher> patcher_,
    crx_file::VerifierFormat crx_format,
    CrxInstaller::ProgressCallback progress_callback,
    InstallOnBlockingTaskRunnerCompleteCallback callback) {
// TODO(b/455004672): Investigate the conflicts with Cobalt customizations
    return;
}

}  // namespace
#endif // defined(USE_M114_FUNCTIONS)

Component::Component(const UpdateContext& update_context, const std::string& id)
    : id_(id),
      state_(std::make_unique<StateNew>(this)),
      update_context_(update_context) {
  CHECK(!id_.empty());
}

Component::~Component() = default;

scoped_refptr<Configurator> Component::config() const {
  return update_context_->config;
}

std::string Component::session_id() const {
  return update_context_->session_id;
}

bool Component::is_foreground() const {
  return update_context_->is_foreground;
}

void Component::Handle(CallbackHandleComplete callback_handle_complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(state_);

  callback_handle_complete_ = std::move(callback_handle_complete);

  state_->Handle(
      base::BindOnce(&Component::ChangeState, base::Unretained(this)));
}

#if BUILDFLAG(IS_STARBOARD)
void Component::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_cancelled_ = true;
  state_->Cancel();
}
#endif

void Component::ChangeState(std::unique_ptr<State> next_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_STARBOARD)
  LOG(INFO) << "Component::ChangeState next_state="
    << ((next_state)? next_state->state_name(): "nullptr");
#endif

  if (next_state) {
    state_ = std::move(next_state);
  } else {
    is_handled_ = true;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback_handle_complete_));
}

CrxUpdateItem Component::GetCrxUpdateItem() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!id_.empty());

  CrxUpdateItem crx_update_item;
  crx_update_item.state = state_->state();
  if (crx_update_item.state == ComponentState::kUpdating &&
      state_hint_ != ComponentState::kNew) {
    // TODO(crbug.com/353249967): Move state_hint_ into
    // Component::StateUpdating. Component::StateUpdating aggregates three
    // historical substates: kDownloading, kUpdating, and kRun. Callers may be
    // sensitive to which substate the pipeline is in.
    crx_update_item.state = state_hint_;
  }
  crx_update_item.id = id_;
  if (crx_component_) {
    crx_update_item.component = *crx_component_;
  }
  crx_update_item.last_check = last_check_;
  crx_update_item.next_version = next_version_;
  crx_update_item.downloaded_bytes = downloaded_bytes_;
  crx_update_item.install_progress = install_progress_;
  crx_update_item.total_bytes = total_bytes_;
  crx_update_item.error_category = error_category_;
  crx_update_item.error_code = error_code_;
  crx_update_item.extra_code1 = extra_code1_;
  crx_update_item.custom_updatecheck_data = custom_attrs_;
  crx_update_item.installer_result = installer_result_;

  return crx_update_item;
}

void Component::SetUpdateCheckResult(std::optional<ProtocolParser::App> result,
                                     ErrorCategory error_category,
                                     int error,
                                     base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(ComponentState::kChecking, state());

  error_category_ = error_category;
  error_code_ = error;

  if (result) {
    CHECK(crx_component_);
    custom_attrs_ = result->custom_attributes;
    if (result->nextversion.IsValid()) {
      next_version_ = base::Version(result->nextversion);
    } else {
      // When the updatecheck response doesn't contain any packages, use the
      // current version and fingerprint as the "next" version and fingerprint
      // for any events emitted (such as a RunAction event).
      next_version_ = crx_component_->version;
    }
    MakePipeline(
        update_context_->config, update_context_->get_available_space,
        update_context_->is_foreground, update_context_->session_id,
        update_context_->config->GetCrxCache(),
        crx_component_->crx_format_requirement, crx_component_->app_id,
        crx_component_->pk_hash, crx_component_->install_data_index,
        crx_component_->installer,
        base::BindRepeating(
            [](base::raw_ref<Component> component, ComponentState state) {
              component->state_hint_ = state;
            },
            base::raw_ref(*this)),
        base::BindRepeating(&Component::AppendEvent, base::Unretained(this)),
        base::BindRepeating(
            [](base::raw_ref<Component> component, int64_t downloaded_bytes,
               int64_t total_bytes) {
              component->downloaded_bytes_ = downloaded_bytes;
              component->total_bytes_ = total_bytes;
              component->NotifyObservers();
            },
            base::raw_ref(*this)),
        base::BindRepeating(
            [](base::raw_ref<Component> component, int progress) {
              if (progress >= 0 && progress <= 100) {
                component->install_progress_ = progress;
              }
              component->NotifyObservers();
            },
            base::raw_ref(*this)),
        base::BindRepeating(
            [](base::raw_ref<Component> component,
               const CrxInstaller::Result& result) {
              component->installer_result_ = result;
              component->error_category_ = result.result.category;
              component->error_code_ = result.result.code;
              component->extra_code1_ = result.result.extra;
            },
            base::raw_ref(*this)),
        crx_component_->action_handler, result.value(),
        base::BindOnce(
            base::BindOnce(
                [](base::raw_ref<Component> component,
                   base::expected<
                       base::OnceCallback<base::OnceClosure(
                           base::OnceCallback<void(const CategorizedError&)>)>,
                       CategorizedError> pipeline) {
                  component->pipeline_ = std::move(pipeline);
                  return true;
                },
                base::raw_ref(*this)))
            .Then(std::move(callback)));
  } else {
    pipeline_ = base::unexpected(
        CategorizedError({.category = error_category, .code = error}));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
  }
}

base::Value::Dict WrapFingerprint(const std::string& fp) {
  base::Value::Dict wrapper;
  wrapper.Set("fingerprint", fp);
  return wrapper;
}

void Component::AppendEvent(base::Value::Dict event) {
  if (previous_version().IsValid()) {
    event.Set("previousversion", previous_version().GetString());
  }
  if (next_version().IsValid()) {
    event.Set("nextversion", next_version().GetString());
  }
  events_.push_back(std::move(event));
}

void Component::NotifyObservers() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_STARBOARD)
  if (is_cancelled_) {
    LOG(WARNING) << "Component::NotifyObservers: skip callback";
    return;
  }
#endif
  update_context_->crx_state_change_callback.Run(GetCrxUpdateItem());
}

base::TimeDelta Component::GetUpdateDuration() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (update_begin_.is_null()) {
    return base::TimeDelta();
  }
  const base::TimeDelta update_cost(base::TimeTicks::Now() - update_begin_);
  if (update_cost.is_negative()) {
    return base::TimeDelta();
  }
  return std::min(update_cost, update_context_->config->UpdateDelay());
}

base::Value::Dict Component::MakeEventUpdateComplete() const {
  base::Value::Dict event;
  event.Set("eventtype", update_context_->is_install
                             ? protocol_request::kEventInstall
                             : protocol_request::kEventUpdate);
  event.Set("eventresult",
            static_cast<int>(state() == ComponentState::kUpdated));
  if (error_category() != ErrorCategory::kNone) {
    event.Set("errorcat", static_cast<int>(error_category()));
  }
  if (error_code()) {
    event.Set("errorcode", error_code());
  }
  if (extra_code1()) {
    event.Set("extracode1", extra_code1());
  }
  if (!previous_fp().empty()) {
    event.Set("previousfp", WrapFingerprint(previous_fp()));
  }
  if (!next_fp().empty()) {
    event.Set("nextfp", WrapFingerprint(next_fp()));
  }
  return event;
}

std::vector<base::Value::Dict> Component::GetEvents() const {
  std::vector<base::Value::Dict> events;
  for (const auto& event : events_) {
    events.push_back(event.Clone());
  }
  return events;
}

std::unique_ptr<CrxInstaller::InstallParams> Component::install_params() const {
  return install_params_
             ? std::make_unique<CrxInstaller::InstallParams>(*install_params_)
             : nullptr;
}

Component::State::State(Component* component, ComponentState state)
    : state_(state), component_(*component) {}

Component::State::~State() = default;

void Component::State::Handle(CallbackNextState callback_next_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  callback_next_state_ = std::move(callback_next_state);

  DoHandle();
}

#if BUILDFLAG(IS_STARBOARD)
void Component::State::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Further work may be needed to ensure cancellation during any state results
  // in a clear result and no memory leaks.
}
#endif

void Component::State::TransitionState(std::unique_ptr<State> next_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(next_state);
#if BUILDFLAG(IS_STARBOARD)
  LOG(INFO) << "Component::State::TransitionState next_state="
    << ((next_state)? next_state->state_name(): "nullptr");
#endif

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_next_state_), std::move(next_state)));
}

void Component::State::EndState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_next_state_), nullptr));
}

Component::StateNew::StateNew(Component* component)
    : State(component, ComponentState::kNew) {}

Component::StateNew::~StateNew() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateNew::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = State::component();
  if (component.crx_component()) {
    TransitionState(std::make_unique<StateChecking>(&component));

    // Notify that the component is being checked for updates after the
    // transition to `StateChecking` occurs. This event indicates the start
    // of the update check. The component receives the update check results when
    // the update checks completes, and after that, `UpdateEngine` invokes the
    // function `StateChecking::DoHandle` to transition the component out of
    // the `StateChecking`. The current design allows for notifying observers
    // on state transitions but it does not allow such notifications when a
    // new state is entered. Hence, posting the task below is a workaround for
    // this design oversight.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&Component::NotifyObservers,
                                  base::Unretained(&component)));
  } else {
    component.error_code_ = static_cast<int>(Error::CRX_NOT_FOUND);
    component.error_category_ = ErrorCategory::kService;
    TransitionState(std::make_unique<StateUpdateError>(&component));
  }
}

Component::StateChecking::StateChecking(Component* component)
    : State(component, ComponentState::kChecking) {
  component->last_check_ = base::TimeTicks::Now();
}

Component::StateChecking::~StateChecking() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateChecking::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = State::component();
  CHECK(component.crx_component());

#if BUILDFLAG(USE_EVERGREEN)
  auto& config = component.update_context_->config;
  auto metadata = component.update_context_->update_checker->GetPersistedData();
  if (metadata) {
    auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&PersistedData::SetUpdaterChannel,
                                  base::Unretained(metadata), component.id_,
                                  config->GetChannel()));
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&PersistedData::SetLatestChannel,
                                  base::Unretained(metadata),
                                  config->GetChannel()));
  } else {
    LOG(WARNING) << "Failed to get the persisted data store to write the "
                       "updater channel.";
  }
#endif

  if (component.error_code_) {
    metrics::RecordUpdateCheckResult(metrics::UpdateCheckResult::kError);
    TransitionState(std::make_unique<StateUpdateError>(&component));
    return;
  }

  if (component.update_context_->is_cancelled) {
    metrics::RecordUpdateCheckResult(metrics::UpdateCheckResult::kCanceled);
    TransitionState(std::make_unique<StateUpdateError>(&component));
    component.error_category_ = ErrorCategory::kService;
    component.error_code_ = static_cast<int>(ServiceError::CANCELLED);
    return;
  }

  if (component.pipeline_.has_value()) {
    metrics::RecordUpdateCheckResult(metrics::UpdateCheckResult::kHasUpdate);
    TransitionState(std::make_unique<StateCanUpdate>(&component));
    return;
  }

  if (component.pipeline_.error().category == ErrorCategory::kNone) {
    metrics::RecordUpdateCheckResult(metrics::UpdateCheckResult::kNoUpdate);
    TransitionState(std::make_unique<StateUpToDate>(&component));
    return;
  }

  metrics::RecordUpdateCheckResult(metrics::UpdateCheckResult::kError);
  TransitionState(std::make_unique<StateUpdateError>(&component));
}

Component::StateUpdateError::StateUpdateError(Component* component)
    : State(component, ComponentState::kUpdateError) {}

Component::StateUpdateError::~StateUpdateError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateUpdateError::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = State::component();

  CHECK_NE(ErrorCategory::kNone, component.error_category_);
  CHECK_NE(0, component.error_code_);

#if BUILDFLAG(IS_STARBOARD)
  // Create an event when the server response included an update, or when it's
  // an update check error, like quick roll-forward or out of space
  if (component.IsUpdateAvailable() ||
      component.error_category_ == ErrorCategory::kUpdateCheck) {
#else
  // Create an event only when the server response included an update.
  if (component.IsUpdateAvailable()) {
#endif
    component.AppendEvent(component.MakeEventUpdateComplete());
  }

  EndState();
  component.NotifyObservers();
}

Component::StateCanUpdate::StateCanUpdate(Component* component)
    : State(component, ComponentState::kCanUpdate) {}

Component::StateCanUpdate::~StateCanUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateCanUpdate::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = State::component();
  CHECK(component.crx_component());

  component.is_update_available_ = true;
  component.NotifyObservers();

  if (!component.crx_component()->updates_enabled ||
      (!component.crx_component()->allow_updates_on_metered_connection &&
       component.config()->IsConnectionMetered())) {
    component.error_category_ = ErrorCategory::kService;
    component.error_code_ = static_cast<int>(ServiceError::UPDATE_DISABLED);
    component.extra_code1_ = 0;
    TransitionState(std::make_unique<StateUpdateError>(&component));
    return;
  }

  if (component.update_context_->is_cancelled) {
    TransitionState(std::make_unique<StateUpdateError>(&component));
    component.error_category_ = ErrorCategory::kService;
    component.error_code_ = static_cast<int>(ServiceError::CANCELLED);
    return;
  }

  if (component.update_context_->is_update_check_only) {
    component.error_category_ = ErrorCategory::kService;
    component.error_code_ =
        static_cast<int>(ServiceError::CHECK_FOR_UPDATE_ONLY);
    component.extra_code1_ = 0;
    component.AppendEvent(component.MakeEventUpdateComplete());
    EndState();
    return;
  }

  // Start computing the cost of the this update from here on.
  component.update_begin_ = base::TimeTicks::Now();
  TransitionState(std::make_unique<StateUpdating>(&component));
}

Component::StateUpToDate::StateUpToDate(Component* component)
    : State(component, ComponentState::kUpToDate) {}

Component::StateUpToDate::~StateUpToDate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateUpToDate::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = State::component();
  CHECK(component.crx_component());

  component.NotifyObservers();
  EndState();
}

#if defined(USE_M114_FUNCTIONS)
Component::StateDownloading::StateDownloading(Component* component)
    : State(component, ComponentState::kDownloading) {}

Component::StateDownloading::~StateDownloading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

#if BUILDFLAG(IS_STARBOARD)
void Component::StateDownloading::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  crx_downloader_->CancelDownload();
}
#endif

void Component::StateDownloading::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();
  CHECK(component.crx_component());

  component.downloaded_bytes_ = -1;
  component.total_bytes_ = -1;

#if BUILDFLAG(IS_STARBOARD)
  crx_downloader_ = component.config()->GetCrxDownloaderFactory()->MakeCrxDownloader(
          component.config());
#else
  crx_downloader_ =
      component.config()->GetCrxDownloaderFactory()->MakeCrxDownloader(
          component.CanDoBackgroundDownload());
#endif
  crx_downloader_->set_progress_callback(base::BindRepeating(
      &Component::StateDownloading::DownloadProgress, base::Unretained(this)));

  // TODO(b/455004672): Investigate the legacy functions from m114.
  // Call crx_downloader_->StartDownload() with appropriate parameters.
}

// Called when progress is being made downloading a CRX. Can be called multiple
// times due to how the CRX downloader switches between different downloaders
// and fallback urls.
void Component::StateDownloading::DownloadProgress(int64_t downloaded_bytes,
                                                   int64_t total_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto& component = Component::State::component();
  component.downloaded_bytes_ = downloaded_bytes;
  component.total_bytes_ = total_bytes;
  component.NotifyObservers();
}

void Component::StateDownloading::DownloadComplete(
    const CrxDownloader::Result& download_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();

  for (const auto& download_metrics : crx_downloader_->download_metrics())
    component.AppendEvent(component.MakeEventDownloadMetrics(download_metrics));

  crx_downloader_ = nullptr;

  if (component.update_context_->is_cancelled) {
    TransitionState(std::make_unique<StateUpdateError>(&component));
    component.error_category_ = ErrorCategory::kService;
    component.error_code_ = static_cast<int>(ServiceError::CANCELLED);
    return;
  }

  if (download_result.error) {
    CHECK(download_result.response.empty());
    component.error_category_ = ErrorCategory::kDownload;
    component.error_code_ = download_result.error;

    TransitionState(std::make_unique<StateUpdateError>(&component));
    return;
  }

#if defined(IN_MEMORY_UPDATES)
  component.installation_dir_ = download_result.installation_dir;
#else
  component.payload_path_ = download_result.response;
#endif

#if BUILDFLAG(IS_STARBOARD)
  component.installation_index_ = download_result.installation_index;
#endif

  TransitionState(std::make_unique<StateUpdating>(&component));
}
#endif // defined(USE_M114_FUNCTIONS)

Component::StateUpdating::StateUpdating(Component* component)
    : State(component, ComponentState::kUpdating) {}

Component::StateUpdating::~StateUpdating() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

#if defined(USE_M114_FUNCTIONS)
void Component::StateUpdating::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();
  const auto& update_context = *component.update_context_;

  CHECK(component.crx_component());

  component.install_progress_ = -1;
  component.NotifyObservers();

  // Adapts the repeating progress callback invoked by the installer so that
  // the callback can be posted to the main sequence instead of running
  // the callback on the sequence the installer is running on.
  auto main_task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  base::ThreadPool::CreateSequencedTaskRunner(kTaskTraits)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              &update_client::StartInstallOnBlockingTaskRunner,
              main_task_runner, component.crx_component()->pk_hash,
#if defined(IN_MEMORY_UPDATES)
              component.installation_dir_,
              &component.crx_str_,
#else
              component.payload_path_,
#endif
#if BUILDFLAG(USE_EVERGREEN)
              component.installation_index_,
              update_context.update_checker->GetPersistedData(),
              component.id_, component.next_version_.GetString(),
#endif
              component.next_fp_,
              component.install_params(), component.crx_component()->installer,
              update_context.config->GetUnzipperFactory()->Create(),
              update_context.config->GetPatcherFactory()->Create(),
              component.crx_component()->crx_format_requirement,
              base::BindRepeating(&Component::StateUpdating::InstallProgress,
                                  base::Unretained(this)),
              base::BindOnce(&Component::StateUpdating::InstallComplete,
                             base::Unretained(this))));
}

void Component::StateUpdating::InstallProgress(int install_progress) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();
  if (install_progress >= 0 && install_progress <= 100)
    component.install_progress_ = install_progress;
  component.NotifyObservers();
}

void Component::StateUpdating::InstallComplete(ErrorCategory error_category,
                                               int error_code,
                                               int extra_code1) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();

  component.error_category_ = error_category;
  component.error_code_ = error_code;
  component.extra_code1_ = extra_code1;

  if (component.error_code_ != 0) {
    TransitionState(std::make_unique<StateUpdateError>(&component));
    return;
  }

  CHECK_EQ(ErrorCategory::kNone, component.error_category_);
  TransitionState(std::make_unique<StateUpdated>(&component));
}
#else // defined(USE_M114_FUNCTIONS)
void Component::StateUpdating::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto& component = Component::State::component();
  if (component.pipeline_.has_value()) {
    cancel_callback_ =
        std::move(component.pipeline_.value())
            .Run(base::BindOnce(&Component::StateUpdating::PipelineComplete,
                                base::Unretained(this)));
    return;
  }
  component.error_category_ = component.pipeline_.error().category;
  component.error_code_ = component.pipeline_.error().code;
  component.extra_code1_ = component.pipeline_.error().extra;
  TransitionState(std::make_unique<StateUpdateError>(&component));
}

void Component::StateUpdating::PipelineComplete(
    const CategorizedError& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = Component::State::component();

  if (result.category != ErrorCategory::kNone) {
    component.error_category_ = result.category;
    component.error_code_ = result.code;
    component.extra_code1_ = result.extra;
  }

  CHECK(component.crx_component_);
  if (!component.crx_component_->allow_cached_copies) {
    component.config()->GetCrxCache()->RemoveAll(
        component.crx_component()->app_id, base::DoNothing());
  }

  if (component.error_category_ != ErrorCategory::kNone) {
    TransitionState(std::make_unique<StateUpdateError>(&component));
    return;
  }

  CHECK_EQ(ErrorCategory::kNone, component.error_category_);
  TransitionState(std::make_unique<StateUpdated>(&component));
}
#endif // defined(USE_M114_FUNCTIONS)

Component::StateUpdated::StateUpdated(Component* component)
    : State(component, ComponentState::kUpdated) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Component::StateUpdated::~StateUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Component::StateUpdated::DoHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& component = State::component();
  CHECK(component.crx_component());

  component.crx_component_->version = component.next_version_;

  component.update_context_->persisted_data->SetProductVersion(
      component.id(), component.crx_component_->version);
  component.update_context_->persisted_data->SetMaxPreviousProductVersion(
      component.id(), component.previous_version_);
  component.update_context_->persisted_data->SetFingerprint(
      component.id(), component.crx_component_->fingerprint);

  component.AppendEvent(component.MakeEventUpdateComplete());

  component.NotifyObservers();
  metrics::RecordComponentUpdated();
  EndState();
}

}  // namespace update_client
