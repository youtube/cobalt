// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_NOOP_LOCK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_NOOP_LOCK_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/locks/lock.h"

namespace content {
struct PartitionedLockHolder;
}

namespace web_app {

class WebAppLockManager;

// This lock essentially doesn't lock anything in the system. However, if a
// `AllAppsLock` is used, then that will block the acquisition of this lock.
//
// Locks can be acquired by using the `WebAppLockManager`.
class NoopLockDescription : public LockDescription {
 public:
  NoopLockDescription();
  ~NoopLockDescription();
};

// Holding a NoopLock is required when a locked operation needs to be executed,
// but there is no explicit resource to be locked yet. If a resource shows up in
// the middle of the command, an upgrade from a NoopLock to an AppLock or
// SharedWebContentsWithAppLock may occur.
//
// See `WebAppLockManager` for how to use locks. Destruction of this class will
// release the lock or cancel the lock request if it is not acquired yet.
class NoopLock : public Lock {
 public:
  using LockDescription = NoopLockDescription;

  ~NoopLock();

  base::WeakPtr<NoopLock> AsWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  friend WebAppLockManager;
  NoopLock(std::unique_ptr<content::PartitionedLockHolder> holder,
           base::WeakPtr<WebAppLockManager> lock_manager);

  base::WeakPtrFactory<NoopLock> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_NOOP_LOCK_H_
