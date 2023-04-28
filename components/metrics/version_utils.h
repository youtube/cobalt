// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_VERSION_UTILS_H_
#define COMPONENTS_METRICS_VERSION_UTILS_H_

#include <string>

#include "third_party/metrics_proto/system_profile.pb.h"

namespace version_info {
enum class Channel;
}

namespace metrics {

// Build a string including the Chrome app version, suffixed by "-64" on 64-bit
// platforms, and "-devel" on developer builds.
std::string GetVersionString();

// Translates version_info::Channel to the equivalent
// SystemProfileProto::Channel.
SystemProfileProto::Channel AsProtobufChannel(version_info::Channel channel);

}  // namespace metrics

#endif  // COMPONENTS_METRICS_VERSION_UTILS_H_
