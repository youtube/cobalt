// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_ENGINE_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_ENGINE_H_

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/update_client/component.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/ping_manager.h"
#include "components/update_client/update_checker.h"
#include "components/update_client/update_client.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace update_client {

class Configurator;
struct UpdateContext;

// Handles updates for a group of components. Updates for different groups
// are run concurrently but within the same group of components, updates are
// applied one at a time.
class UpdateEngine : public base::RefCounted<UpdateEngine> {
 public:
  using Callback = base::OnceCallback<void(Error error)>;
  using NotifyObserversCallback =
      base::Callback<void(UpdateClient::Observer::Events event,
                          const std::string& id)>;
  using CrxDataCallback = UpdateClient::CrxDataCallback;

  UpdateEngine(scoped_refptr<Configurator> config,
               UpdateChecker::Factory update_checker_factory,
               CrxDownloader::Factory crx_downloader_factory,
               scoped_refptr<PingManager> ping_manager,
               const NotifyObserversCallback& notify_observers_callback);

  // Returns true and the state of the component identified by |id|, if the
  // component is found in any update context. Returns false if the component
  // is not found.
  bool GetUpdateState(const std::string& id, CrxUpdateItem* update_state);

#if !defined(STARBOARD)
  void Update(bool is_foreground,
              const std::vector<std::string>& ids,
              UpdateClient::CrxDataCallback crx_data_callback,
              Callback update_callback);
#else
  // |cancelation_closure| is populated with a closure that can be run to cancel
  // the update requested by the caller.
  void Update(bool is_foreground,
              const std::vector<std::string>& ids,
              UpdateClient::CrxDataCallback crx_data_callback,
              Callback update_callback,
              base::OnceClosure& cancelation_closure);
#endif

  void SendUninstallPing(const std::string& id,
                         const base::Version& version,
                         int reason,
                         Callback update_callback);

 private:
  friend class base::RefCounted<UpdateEngine>;
  ~UpdateEngine();

  using UpdateContexts = std::map<std::string, scoped_refptr<UpdateContext>>;

  void UpdateComplete(scoped_refptr<UpdateContext> update_context, Error error);

  void ComponentCheckingForUpdatesStart(
      scoped_refptr<UpdateContext> update_context,
      const std::string& id);
  void ComponentCheckingForUpdatesComplete(
      scoped_refptr<UpdateContext> update_context);
  void UpdateCheckComplete(scoped_refptr<UpdateContext> update_context);

  void DoUpdateCheck(scoped_refptr<UpdateContext> update_context);
  void UpdateCheckResultsAvailable(
      scoped_refptr<UpdateContext> update_context,
      const base::Optional<ProtocolParser::Results>& results,
      ErrorCategory error_category,
      int error,
      int retry_after_sec);

  void HandleComponent(scoped_refptr<UpdateContext> update_context);
  void HandleComponentComplete(scoped_refptr<UpdateContext> update_context);

  // Returns true if the update engine rejects this update call because it
  // occurs too soon.
  bool IsThrottled(bool is_foreground) const;

#if defined(STARBOARD)
  // Cancels updates currently handled by the engine for each component
  // identified by one of |crx_component_ids| for the update context identified
  // by the |update_context_session_id|. Also cancels the |UpdateChecker| for 
  // the component and the |PingManager|.
  void Cancel(const std::string& update_context_session_id,
              const std::vector<std::string>& crx_component_ids);
#endif

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<Configurator> config_;
  UpdateChecker::Factory update_checker_factory_;
  CrxDownloader::Factory crx_downloader_factory_;
  scoped_refptr<PingManager> ping_manager_;
  std::unique_ptr<PersistedData> metadata_;

  // Called when CRX state changes occur.
  const NotifyObserversCallback notify_observers_callback_;

  // Contains the contexts associated with each update in progress.
  UpdateContexts update_contexts_;

  // Implements a rate limiting mechanism for background update checks. Has the
  // effect of rejecting the update call if the update call occurs before
  // a certain time, which is negotiated with the server as part of the
  // update protocol. See the comments for X-Retry-After header.
  base::TimeTicks throttle_updates_until_;

#if defined(STARBOARD)
  bool is_cancelled_ = false;
#endif
  DISALLOW_COPY_AND_ASSIGN(UpdateEngine);
};

// Describes a group of components which are installed or updated together.
struct UpdateContext : public base::RefCounted<UpdateContext> {
  UpdateContext(
      scoped_refptr<Configurator> config,
      bool is_foreground,
      const std::vector<std::string>& ids,
      UpdateClient::CrxDataCallback crx_data_callback,
      const UpdateEngine::NotifyObserversCallback& notify_observers_callback,
      UpdateEngine::Callback callback,
      CrxDownloader::Factory crx_downloader_factory);

  scoped_refptr<Configurator> config;

  // True if the component is updated as a result of user interaction.
  bool is_foreground = false;

  // True if the component updates are enabled in this context.
  const bool enabled_component_updates;

  // Contains the ids of all CRXs in this context in the order specified
  // by the caller of |UpdateClient::Update| or |UpdateClient:Install|.
  const std::vector<std::string> ids;

  // Contains the map of ids to components for all the CRX in this context.
  IdToComponentPtrMap components;

  // Called before an update check, when update metadata is needed.
  UpdateEngine::CrxDataCallback crx_data_callback;

  // Called when there is a state change for any update in this context.
  const UpdateEngine::NotifyObserversCallback notify_observers_callback;

  // Called when the all updates associated with this context have completed.
  UpdateEngine::Callback callback;

  // Creates instances of CrxDownloader;
  CrxDownloader::Factory crx_downloader_factory;

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

  size_t num_components_ready_to_check = 0;
  size_t num_components_checked = 0;

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

 private:
  friend class base::RefCounted<UpdateContext>;
  ~UpdateContext();

  DISALLOW_COPY_AND_ASSIGN(UpdateContext);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_ENGINE_H_
