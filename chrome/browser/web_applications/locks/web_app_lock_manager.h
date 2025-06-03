// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_WEB_APP_LOCK_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_WEB_APP_LOCK_MANAGER_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/webapps/common/web_app_id.h"

namespace base {
class Value;
}

namespace web_app {

class AppLock;
class AppLockDescription;
class LockDescription;
class WebAppCommandManager;
class NoopLock;
class SharedWebContentsLock;
class SharedWebContentsWithAppLock;
class SharedWebContentsWithAppLockDescription;
class WebAppProvider;

// Locks allow types of exclusive access to resources in the WebAppProvider
// system, depending on the lock. These are not for multi-sequence access, but
// instead required due to the async nature of operations in the system. Locks
// do NOT protect against common problems like handling profile shutdown. In
// fact, locks will CHECK-fail if they are called accessed during profile
// shutdown. Thus using a WebAppCommand is a better option, as commands are
// destroyed automatically during shutdown.
//
// Locks can be a great way to make synchronous operations composable. For
// example, the following method call guarantees that it is done in an isolated
// context:
//
// void UpdateWidget(WithAppResources& lock_with_app_exclusivity, webapps::AppId
// id) {
//    widget_.SetTitle(lock_with_app_exclusivity.registrar().GetShortName(id));
//    ...
// }
//
// To access data across an async call chain, then
// 1) The brokering of the lock needs to be done through a command to make sure
//    shutdown is handled.
// 2) a WeakPtr of the lock can be used so the async logic can correctly handle
//    this shutdown.
//
// Example of using a lock across an async boundary:
//
// void UpdateWidget(base::WeakPtr<WithAppResources> lock_with_app_exclusivity,
//                   webapps::AppId id) {
//    widget_.SetTitle(lock_with_app_exclusivity.registrar().GetShortName(id));
//    TalkToAsyncSystem(..., base::BindOnce(&OnAsyncSystemUpdated,
//                                          lock_with_app_exclusivity));
// }
//
// void OnAsyncSystemUpdated(base::WeakPtr<WithAppResources>
//                           lock_with_app_exclusivity) {
//   if (!lock_with_app_exclusivity) {
//     // Do cleanup?
//     return;
//   }
//   ... do things with the lock.
// }
class WebAppLockManager {
 public:
  using PassKey = base::PassKey<WebAppLockManager>;

  WebAppLockManager();
  ~WebAppLockManager();

  void SetProvider(base::PassKey<WebAppCommandManager>,
                   WebAppProvider& provider);

  // Returns if the lock for the shared web contents is free.  This means no one
  // is using the shared web contents.
  bool IsSharedWebContentsLockFree();

  // Acquires the lock for the given `lock_description`, calling
  // `on_lock_acquired` when complete with the given lock. The lock
  // is considered released when the `lock` given to the callback is destroyed.
  template <class LockType>
  void AcquireLock(
      const LockDescription& lock_description,
      base::OnceCallback<void(std::unique_ptr<LockType> lock)> on_lock_acquired,
      const base::Location& location);

  // Upgrades the given lock to a new one, and will call `on_lock_acquired` on
  // when the new lock has been acquired.
  std::unique_ptr<SharedWebContentsWithAppLockDescription>
  UpgradeAndAcquireLock(
      std::unique_ptr<SharedWebContentsLock> lock,
      const base::flat_set<webapps::AppId>& app_ids,
      base::OnceCallback<void(std::unique_ptr<SharedWebContentsWithAppLock>)>
          on_lock_acquired,
      const base::Location& location = FROM_HERE);

  // Upgrades the given lock to a new one, and will call `on_lock_acquired` on
  // when the new lock has been acquired.
  std::unique_ptr<AppLockDescription> UpgradeAndAcquireLock(
      std::unique_ptr<NoopLock> lock,
      const base::flat_set<webapps::AppId>& app_ids,
      base::OnceCallback<void(std::unique_ptr<AppLock>)> on_lock_acquired,
      const base::Location& location = FROM_HERE);

  base::Value ToDebugValue() const;

  WebAppProvider& provider() const { return *provider_; }

 private:
  // Acquires the lock for the given `lock`, calling `on_lock_acquired` when
  // complete.
  void AcquireLock(base::WeakPtr<content::PartitionedLockHolder> holder,
                   const LockDescription& lock,
                   base::OnceClosure on_lock_acquired,
                   const base::Location& location);

  content::PartitionedLockManager lock_manager_;
  raw_ptr<WebAppProvider> provider_ = nullptr;
  base::WeakPtrFactory<WebAppLockManager> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_WEB_APP_LOCK_MANAGER_H_
