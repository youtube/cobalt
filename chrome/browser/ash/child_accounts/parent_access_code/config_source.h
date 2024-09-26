// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_CONFIG_SOURCE_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_CONFIG_SOURCE_H_

#include <map>
#include <memory>
#include <vector>

#include "chrome/browser/ash/child_accounts/parent_access_code/authenticator.h"

class AccountId;

namespace base {
class Value;
}  // namespace base

namespace user_manager {
class User;
}  // namespace user_manager

namespace ash {
namespace parent_access {

// Base class for parent access code configuration providers.
class ConfigSource {
 public:
  // Map containing a list of Authenticators per child account logged in the
  // device.
  typedef std::map<AccountId, std::vector<std::unique_ptr<Authenticator>>>
      AuthenticatorsMap;

  ConfigSource();

  ConfigSource(const ConfigSource&) = delete;
  ConfigSource& operator=(const ConfigSource&) = delete;

  ~ConfigSource();

  const AuthenticatorsMap& config_map() const { return config_map_; }

  // Reloads the Parent Access Code config for that particular user.
  void LoadConfigForUser(const user_manager::User* user);

 private:
  // Creates and adds an authenticator to the |config_map_|. |dict| corresponds
  // to an AccessCodeConfig in its serialized format.
  void AddAuthenticator(const base::Value::Dict& dict,
                        const user_manager::User* user);

  // Holds the Parent Access Code Authenticators for all children signed in this
  // device.
  AuthenticatorsMap config_map_;
};

}  // namespace parent_access
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_CONFIG_SOURCE_H_
