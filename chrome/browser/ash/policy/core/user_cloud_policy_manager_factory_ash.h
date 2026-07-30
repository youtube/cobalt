// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_USER_CLOUD_POLICY_MANAGER_FACTORY_ASH_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_USER_CLOUD_POLICY_MANAGER_FACTORY_ASH_H_

#include <memory>

#include "base/memory/scoped_refptr.h"

class PrefService;
class Profile;

namespace base {
class SequencedTaskRunner;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

class BrowserPolicyConnectorAsh;
class UserCloudPolicyManagerAsh;

// Create a UserCloudPolicyManagerAsh for the given Profile.
// Will return `nullptr` if
//   - `profile` is not a user profile
//   - the user corresponding to `profile` is not an enterprise or child user
//   - the user has no Gaia account
//   - `force_immediate_load` and policy check is still required
// `local_state` and `browser_policy_connector_ash` must not be null and must
// outlive the returned manager. `shared_url_loader_factory` must not be null.
std::unique_ptr<UserCloudPolicyManagerAsh> CreateUserCloudPolicyManagerAsh(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    BrowserPolicyConnectorAsh* browser_policy_connector_ash,
    Profile* profile,
    bool force_immediate_load,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_USER_CLOUD_POLICY_MANAGER_FACTORY_ASH_H_
