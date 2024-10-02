// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_SHELL_SHELL_RELAUNCHER_H_
#define FUCHSIA_WEB_SHELL_SHELL_RELAUNCHER_H_

#include "base/strings/string_piece.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class CommandLine;
}

// The shells need to provide capabilities to children they launch (via
// WebInstanceHost, for example). Test components are not able to do this, so
// use RealmBuilder to relaunch the shell via the given package-relative URL
// (which includes `--no-relaunch` on its command line) with the contents of
// this process's command line.
absl::optional<int> RelaunchForWebInstanceHostIfParent(
    base::StringPiece relative_component_url,
    const base::CommandLine& command_line);

#endif  // FUCHSIA_WEB_SHELL_SHELL_RELAUNCHER_H_
