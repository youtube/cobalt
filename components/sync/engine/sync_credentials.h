// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_CREDENTIALS_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_CREDENTIALS_H_

#include <string>

namespace syncer {

// Contains everything needed to talk to and identify a user account.
struct SyncCredentials {
  SyncCredentials() = default;
  SyncCredentials(const SyncCredentials& other) = default;
  ~SyncCredentials() = default;

  // The email associated with this account.
  std::string email;

  // The OAuth2 access token.
  std::string access_token;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_CREDENTIALS_H_
