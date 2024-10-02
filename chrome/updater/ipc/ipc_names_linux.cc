// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/ipc_names.h"

#include "chrome/updater/linux/ipc_constants.h"
#include "chrome/updater/updater_scope.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

mojo::NamedPlatformChannel::ServerName GetUpdateServiceInternalServerName(
    UpdaterScope scope) {
  return GetActiveDutyInternalSocketPath(scope).MaybeAsASCII();
}

mojo::NamedPlatformChannel::ServerName GetUpdateServiceServerName(
    UpdaterScope scope) {
  absl::optional<base::FilePath> socket = GetActiveDutySocketPath(scope);
  CHECK(socket);
  return socket->MaybeAsASCII();
}

}  // namespace updater
