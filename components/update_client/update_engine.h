// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_ENGINE_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_ENGINE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/update_client/buildflags.h"
#include "components/update_client/component.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/ping_manager.h"
#include "components/update_client/update_checker.h"
#include "components/update_client/update_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace update_client {

class Configurator;
class PersistedData;
struct UpdateContext;

// Handles updates for a group of components. Updates for different groups
// are run concurrently but within the same group of components, updates are
// applied one at a time.
class UpdateEngine : public base::RefCountedThreadSafe<UpdateEngine> {
 public:
  using Callback = base::OnceCallback<void(Error error)>;
  using NotifyObserversCallback =
      base::RepeatingCallback<void(UpdateClient::Observer::Events event,
                                   const std::string& id)>;
  using CrxDataCallback = UpdateClient::CrxDataCallback;

  UpdateEngine(scoped_refptr<Configurator> config,
               UpdateChecker::Factory update_checker_factory,
               scoped_refptr<PingManager> ping_manager,
               const NotifyObserversCallback& notify_observers_callback);
  UpdateEngine(const UpdateEngine&) = delete;
  UpdateEngine& operator=(const UpdateEngine&) = delete;

  // Returns true and the state of the component identified by |id|, if the
  // component is found in any update context. Returns false if the component
  // is not found.
  bool GetUpdateState(const std::string& id, CrxUpdateItem* update_state);

  // Does an update check for `id` but stops after receiving the update check
  // response.
  void CheckForUpdate(
      bool is_foreground,
      const std::string& id,
      UpdateClient::CrxDataCallback crx_data_callback,
      UpdateClient::CrxStateChangeCallback crx_state_change_callback,
      Callback update_callback);

  // Updates the given app ids. Returns a closure that can be called to trigger
  // cancellation of the operation. `update_callback` is called when the
  // operation is complete (even if cancelled). The cancellation callback
  // must be called only on the main sequence.
  base::RepeatingClosure Update(
      bool is_foreground,
      bool is_install,
      const std::vector<std::string>& ids,
      UpdateClient::CrxDataCallback crx_data_callback,
      UpdateClient::CrxStateChangeCallback crx_state_change_callback,
      Callback update_callback);

  void SendUninstallPing(const CrxComponent& crx_component,
                         int reason,
                         Callback update_callback);

 private:
  friend class base::RefCountedThreadSafe<UpdateEngine>;
  ~UpdateEngine();

  // Maps a session id to an update context.
  using UpdateContexts = std::map<std::string, scoped_refptr<UpdateContext>>;

  base::RepeatingClosure InvokeOperation(
      bool is_foreground,
      bool is_update_check_only,
      bool is_install,
      const std::vector<std::string>& ids,
      UpdateClient::CrxDataCallback crx_data_callback,
      UpdateClient::CrxStateChangeCallback crx_state_change_callback,
      Callback update_callback);
  void UpdateComplete(scoped_refptr<UpdateContext> update_context, Error error);

  void DoUpdateCheck(scoped_refptr<UpdateContext> update_context);
  void UpdateCheckResultsAvailable(
      scoped_refptr<UpdateContext> update_context,
      const absl::optional<ProtocolParser::Results>& results,
      ErrorCategory error_category,
      int error,
      int retry_after_sec);
  void UpdateCheckComplete(scoped_refptr<UpdateContext> update_context);

  void HandleComponent(scoped_refptr<UpdateContext> update_context);
  void HandleComponentComplete(scoped_refptr<UpdateContext> update_context);

  // Returns true if the update engine rejects this update call because it
  // occurs too soon.
  bool IsThrottled(bool is_foreground) const;

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<Configurator> config_;
  UpdateChecker::Factory update_checker_factory_;
  scoped_refptr<PingManager> ping_manager_;
  std::unique_ptr<PersistedData> metadata_;

  // Called when CRX state changes occur.
  const NotifyObserversCallback notify_observers_callback_;

#if BUILDFLAG(ENABLE_PUFFIN_PATCHES)
  // TODO(crbug.com/1349060) once Puffin patches are fully implemented,
  // we should remove this #if.
  absl::optional<scoped_refptr<CrxCache>> crx_cache_;
#endif

  // Contains the contexts associated with each update in progress.
  UpdateContexts update_contexts_;

  // Implements a rate limiting mechanism for background update checks. Has the
  // effect of rejecting the update call if the update call occurs before
  // a certain time, which is negotiated with the server as part of the
  // update protocol. See the comments for X-Retry-After header.
  base::TimeTicks throttle_updates_until_;
};

// Describes a group of components which are installed or updated together.
struct UpdateContext : public base::RefCountedThreadSafe<UpdateContext> {
#if BUILDFLAG(ENABLE_PUFFIN_PATCHES)
  // TODO(crbug.com/1349060) once Puffin patches are fully implemented,
  // we should remove this #if.
  UpdateContext(
      scoped_refptr<Configurator> config,
      absl::optional<scoped_refptr<CrxCache>> crx_cache,
      bool is_foreground,
      bool is_install,
      const std::vector<std::string>& ids,
      UpdateClient::CrxStateChangeCallback crx_state_change_callback,
      const UpdateEngine::NotifyObserversCallback& notify_observers_callback,
      UpdateEngine::Callback callback,
      PersistedData* persisted_data,
      bool is_update_check_only);
#else
  UpdateContext(
      scoped_refptr<Configurator> config,
      bool is_foreground,
      bool is_install,
      const std::vector<std::string>& ids,
      UpdateClient::CrxStateChangeCallback crx_state_change_callback,
      const UpdateEngine::NotifyObserversCallback& notify_observers_callback,
      UpdateEngine::Callback callback,
      PersistedData* persisted_data,
      bool is_update_check_only);
#endif
  UpdateContext(const UpdateContext&) = delete;
  UpdateContext& operator=(const UpdateContext&) = delete;

  scoped_refptr<Configurator> config;

#if BUILDFLAG(ENABLE_PUFFIN_PATCHES)
  // TODO(crbug.com/1349060) once Puffin patches are fully implemented,
  // we should remove this #if.
  absl::optional<scoped_refptr<CrxCache>> crx_cache_;
#endif

  // True if the component is updated as a result of user interaction.
  bool is_foreground = false;

  // True if the component is updating in an installation flow.
  bool is_install = false;

  // True if and only if this operation has been canceled.
  bool is_cancelled = false;

  // Contains the ids of all CRXs in this context in the order specified
  // by the caller of |UpdateClient::Update| or |UpdateClient:Install|.
  const std::vector<std::string> ids;

  // Contains the map of ids to components for all the CRX in this context.
  IdToComponentPtrMap components;

  // Called when the observable state of the CRX component has changed.
  UpdateClient::CrxStateChangeCallback crx_state_change_callback;

  // Called when there is a state change for any update in this context.
  const UpdateEngine::NotifyObserversCallback notify_observers_callback;

  // Called when the all updates associated with this context have completed.
  UpdateEngine::Callback callback;

  std::unique_ptr<UpdateChecker> update_checker;

  // The time in seconds to wait until doing further update checks.
  int retry_after_sec = 0;

  // Contains the ids of the components to check for updates. It is possible
  // for a component to be uninstalled after it has been added in this context
  // but before an update check is made. When this happens, the component won't
  // have a CrxComponent instance, therefore, it can't be included in an
  // update check.
  std::vector<std::string> components_to_check_for_updates;

  // The error reported by the update checker.
  int update_check_error = 0;

  // Contains the ids of the components that the state machine must handle.
  base::queue<std::string> component_queue;

  // The time to wait before handling the update for a component.
  // The wait time is proportional with the cost incurred by updating
  // the component. The more time it takes to download and apply the
  // update for the current component, the longer the wait until the engine
  // is handling the next component in the queue.
  base::TimeDelta next_update_delay;

  // The unique session id of this context. The session id is serialized in
  // every protocol request. It is also used as a key in various data stuctures
  // to uniquely identify an update context.
  const std::string session_id;

  // Persists data using the prefs service. Not owned by this class.
  raw_ptr<PersistedData> persisted_data = nullptr;

  // True if this context is for an update check operation.
  bool is_update_check_only = false;

 private:
  friend class base::RefCountedThreadSafe<UpdateContext>;
  ~UpdateContext();
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_ENGINE_H_
