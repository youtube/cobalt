// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/scoped_channel_override.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/environment.h"
#include "base/strings/cstring_view.h"

namespace chrome {

namespace {

// Exchanges the value of the environment variable `name` with `new_value`;
// returning its previous value or null if it was not set. The variable is
// removed from the environment if `new_value` is null.
std::optional<std::string> ExchangeEnvironmentVariable(
    base::cstring_view name,
    std::optional<std::string> new_value) {
  auto environment = base::Environment::Create();
  std::optional<std::string> old_value = environment->GetVar(name);
  if (new_value)
    environment->SetVar(name, *new_value);
  else
    environment->UnSetVar(name);
  return old_value;
}

constexpr base::cstring_view kChromeVersionExtra = "CHROME_VERSION_EXTRA";

std::string GetVersionExtra(ScopedChannelOverride::Channel channel) {
  switch (channel) {
    case ScopedChannelOverride::Channel::kExtendedStable:
      return "extended";
    case ScopedChannelOverride::Channel::kStable:
      return "stable";
    case ScopedChannelOverride::Channel::kBeta:
      return "beta";
    case ScopedChannelOverride::Channel::kDev:
      return "unstable";
#if BUILDFLAG(IS_LINUX)
    case ScopedChannelOverride::Channel::kCanary:
      return "canary";
#endif  // BUILDFLAG(IS_LINUX)
  }
}

}  // namespace

ScopedChannelOverride::ScopedChannelOverride(Channel channel)
    : old_env_var_(ExchangeEnvironmentVariable(kChromeVersionExtra,
                                               GetVersionExtra(channel))) {}

ScopedChannelOverride::~ScopedChannelOverride() {
  ExchangeEnvironmentVariable(kChromeVersionExtra, old_env_var_);
}

}  // namespace chrome
