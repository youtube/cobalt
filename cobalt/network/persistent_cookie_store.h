// Copyright 2015 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_NETWORK_PERSISTENT_COOKIE_STORE_H_
#define COBALT_NETWORK_PERSISTENT_COOKIE_STORE_H_

#include <string>
#include <vector>

#include "base/single_thread_task_runner.h"
#include "cobalt/storage/storage_manager.h"
#include "net/cookies/cookie_monster.h"

namespace cobalt {
namespace network {

class PersistentCookieStore : public net::CookieMonster::PersistentCookieStore {
 public:
  explicit PersistentCookieStore(
      storage::StorageManager* storage,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner);
  ~PersistentCookieStore() override;

  // net::CookieMonster::PersistentCookieStore methods
  void Load(const LoadedCallback& loaded_callback,
            const net::NetLogWithSource& net_log) override;

  void LoadCookiesForKey(const std::string& key,
                         const LoadedCallback& loaded_callback) override;

  void AddCookie(const net::CanonicalCookie& cc) override;
  void UpdateCookieAccessTime(const net::CanonicalCookie& cc) override;
  void DeleteCookie(const net::CanonicalCookie& cc) override;

  void SetForceKeepSessionState() override;

  void SetBeforeFlushCallback(base::RepeatingClosure callback) override;
  void Flush(base::OnceClosure callback) override;

 private:
  storage::StorageManager* storage_;
  // This is required because for example cookie store callbacks can only be
  // executed on the network thread.
  scoped_refptr<base::SingleThreadTaskRunner> loaded_callback_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(PersistentCookieStore);
};

}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_PERSISTENT_COOKIE_STORE_H_
