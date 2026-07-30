// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_EMBEDDER_PROCESS_ERROR_CALLBACK_H_
#define CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_EMBEDDER_PROCESS_ERROR_CALLBACK_H_

#include <string>

#include "base/functional/callback_forward.h"

namespace mojo_legacy {
namespace core {

using ProcessErrorCallback =
    base::RepeatingCallback<void(const std::string& error)>;

}  // namespace core
}  // namespace mojo_legacy

#endif  // CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_EMBEDDER_PROCESS_ERROR_CALLBACK_H_
