// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mojo_proxy/mojo_core/public/cpp/system/message.h"

#include <string_view>

#include "base/check.h"
#include "base/numerics/safe_conversions.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/c/system/message_pipe.h"

namespace mojo_legacy {

MojoResult NotifyBadMessage(MessageHandle message,
                            const std::string_view& error) {
  DCHECK(message.is_valid());
  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(error.size()));
  return MojoNotifyBadMessage(message.value(), error.data(),
                              static_cast<uint32_t>(error.size()), nullptr);
}

}  // namespace mojo_legacy
