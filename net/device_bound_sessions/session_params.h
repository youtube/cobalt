// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_PARAMS_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_PARAMS_H_

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "components/unexportable_keys/unexportable_key_id.h"
#include "net/base/net_export.h"
#include "url/gurl.h"

namespace net::device_bound_sessions {

// LINT.IfChange(SessionParams)
// Struct to contain the parameters from the session instruction JSON.
// https://github.com/WICG/dbsc/blob/main/README.md#session-registration-instructions-json
// This is sent on session creation and session refresh
struct NET_EXPORT SessionParams final {
  // Scope section of session instructions.
  struct NET_EXPORT Scope {
    // Specification section of the session scope instructions.
    struct NET_EXPORT Specification {
      enum class Type { kExclude, kInclude };
      bool operator==(const Specification&) const = default;
      Type type = Type::kInclude;
      std::string domain;
      std::string path;
    };

    // Defaults to false if not in the params
    bool include_site = false;
    std::vector<Specification> specifications;
    std::string origin;
  };

  // Credential section of the session instruction.
  struct NET_EXPORT Credential {
    bool operator==(const Credential&) const = default;
    std::string name;
    std::string attributes;
  };

  std::string session_id;
  // The `fetcher_url` is the registration or refresh endpoint that was called
  // into that returned the session instructions.
  GURL fetcher_url;
  std::string refresh_url;
  Scope scope;
  std::vector<Credential> credentials;
  // TODO(crbug.com/501306421): Consider making this a std::optional, so that
  // forgetting to set it won't result in a random key id.
  unexportable_keys::UnexportableSigningKeyId key_id;
  std::vector<std::string> allowed_refresh_initiators;
};
// LINT.ThenChange(//services/network/public/mojom/device_bound_sessions.mojom:DeviceBoundSessionParams)

// Struct to contain the parameters from the .well-known JSON.
struct NET_EXPORT WellKnownParams {
  std::optional<std::vector<std::string>> registering_origins;
  std::optional<std::vector<std::string>> relying_origins;
  std::optional<std::string> provider_origin;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_PARAMS_H_
