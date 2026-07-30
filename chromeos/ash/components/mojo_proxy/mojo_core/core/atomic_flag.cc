// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mojo_proxy/mojo_core/core/atomic_flag.h"

namespace mojo_legacy {
namespace core {

AtomicFlag::AtomicFlag() : flag_(false) {}

}  // namespace core
}  // namespace mojo_legacy
