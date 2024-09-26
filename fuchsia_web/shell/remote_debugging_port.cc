// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/shell/remote_debugging_port.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

constexpr char kRemoteDebuggingPortSwitch[] = "remote-debugging-port";

absl::optional<uint16_t> GetRemoteDebuggingPort(
    const base::CommandLine& command_line) {
  if (!command_line.HasSwitch(kRemoteDebuggingPortSwitch)) {
    return 0;
  } else {
    std::string port_str =
        command_line.GetSwitchValueNative(kRemoteDebuggingPortSwitch);
    int port_parsed;
    if (!base::StringToInt(port_str, &port_parsed) || port_parsed < 0 ||
        port_parsed > 65535) {
      LOG(ERROR) << "Invalid value for --remote-debugging-port (must be in the "
                    "range 0-65535).";
      return absl::nullopt;
    }
    return static_cast<uint16_t>(port_parsed);
  }
}
