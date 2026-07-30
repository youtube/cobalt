// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mojo_proxy/mojo_core/core/connection_params.h"

#include <utility>

namespace mojo_legacy {
namespace core {

ConnectionParams::ConnectionParams() = default;

ConnectionParams::ConnectionParams(PlatformChannelEndpoint endpoint)
    : endpoint_(std::move(endpoint)) {}

ConnectionParams::ConnectionParams(ConnectionParams&&) = default;

ConnectionParams::~ConnectionParams() = default;

ConnectionParams& ConnectionParams::operator=(ConnectionParams&& params) =
    default;

}  // namespace core
}  // namespace mojo_legacy
