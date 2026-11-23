// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/utils/extension_utils.h"

#include <algorithm>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/switches.h"

namespace extensions {

const ExtensionId& MaybeGetExtensionId(const Extension* extension) {
  return extension ? extension->id() : base::EmptyString();
}

bool IsExtensionAllowlistedByCommandLine(const Extension& extension) {
  const std::string allowlisted_extension_ids =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAllowlistedExtensionID);

  const std::vector<std::string_view> allowlist =
      base::SplitStringPiece(allowlisted_extension_ids, ",",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  return std::ranges::contains(allowlist, extension.id());
}

}  // namespace extensions
