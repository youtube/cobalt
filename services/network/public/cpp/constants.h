// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONSTANTS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONSTANTS_H_

#include <stddef.h>

#include "base/component_export.h"
#include "base/unguessable_token.h"

namespace network {

// The default Accept header value to use if none were specified.
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kDefaultAcceptHeaderValue[];

// TODO(crbug.com/520464337): Consider creating a wrapper class such that
// callers can never set base::UnguessableToken() or
// base::UnguessableToken::Null() for network_restrictions_id.
// Returns a placeholder token used when no network restrictions (Connection
// Allowlist) should be applied (bypassing restrictions entirely).
// This should be used carefully as it bypasses Connection Allowlist. e.g. for
// browser initiated requests that do not have an associated frame.
COMPONENT_EXPORT(NETWORK_CPP)
const base::UnguessableToken& GetNoOpNetworkRestrictionsId();

// Returns a placeholder token used where a valid network restrictions ID is
// required by the interface but has not been implemented yet.
// TODO(crbug.com/492456054): Remove this once the Connection Allowlists
// feature is launched.
COMPONENT_EXPORT(NETWORK_CPP)
const base::UnguessableToken& GetTODONetworkRestrictionsId();

// Returns a placeholder token used in tests that require a valid network
// restrictions ID but do not test Connection Allowlist behavior.
COMPONENT_EXPORT(NETWORK_CPP)
const base::UnguessableToken& GetTestNetworkRestrictionsId();

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONSTANTS_H_
