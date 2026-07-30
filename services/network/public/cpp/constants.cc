// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/constants.h"

#include <ostream>

#include "base/check.h"
#include "base/command_line.h"
#include "base/not_fatal_until.h"
#include "base/unguessable_token.h"

namespace network {

namespace {

void CheckBrowserProcess() {
  CHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch("type"),
        base::NotFatalUntil::M165)
      << "This function should only be called in the browser process.";
}

}  // namespace

const char kDefaultAcceptHeaderValue[] = "*/*";

const base::UnguessableToken& GetNoOpNetworkRestrictionsId() {
  // This is a process-wide singleton so it's only safe to call from the
  // browser process. It's still defined in services/network since it's used in
  // a few directories like device/ which cannot access content/. It cannot be
  // invoked from another process as it will then have a different value.
  // TODO(crbug.com/520464337): Consider creating a wrapper class such that
  // callers can never set base::UnguessableToken() or
  // base::UnguessableToken::Null() for network_restrictions_id.
  CheckBrowserProcess();
  static const base::UnguessableToken g_noop_id =
      base::UnguessableToken::Create();
  return g_noop_id;
}

const base::UnguessableToken& GetTODONetworkRestrictionsId() {
  static const base::UnguessableToken g_todo_id =
      base::UnguessableToken::Create();
  return g_todo_id;
}

const base::UnguessableToken& GetTestNetworkRestrictionsId() {
  static const base::UnguessableToken g_test_id =
      base::UnguessableToken::Create();
  return g_test_id;
}

}  // namespace network
