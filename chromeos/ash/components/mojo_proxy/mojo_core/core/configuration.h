// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_CONFIGURATION_H_
#define CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_CONFIGURATION_H_

#include "chromeos/ash/components/mojo_proxy/mojo_core/core/embedder/configuration.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/system_impl_export.h"

namespace mojo_legacy {
namespace core {

namespace internal {
MOJO_LEGACY_SYSTEM_IMPL_EXPORT extern Configuration g_configuration;
}  // namespace internal

MOJO_LEGACY_SYSTEM_IMPL_EXPORT inline const Configuration& GetConfiguration() {
  return internal::g_configuration;
}

}  // namespace core
}  // namespace mojo_legacy

#endif  // CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_CONFIGURATION_H_
