// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SESSION_PROTO_PROTO_UTIL_H_
#define IOS_WEB_PUBLIC_SESSION_PROTO_PROTO_UTIL_H_

#include "base/time/time.h"
#include "ios/web/common/user_agent.h"
#include "ios/web/public/session/proto/common.pb.h"

namespace web {

// Creates a base::Time value from serialized `storage`.
base::Time TimeFromProto(const proto::Timestamp& storage);

// Serializes `time` into `storage`.
void SerializeTimeToProto(base::Time time, proto::Timestamp& storage);

// Converts a web::proto::UserAgentType and web::UserAgentType.
UserAgentType UserAgentTypeFromProto(proto::UserAgentType value);

// Converts a web::UserAgentType and web::proto::UserAgentType.
proto::UserAgentType UserAgentTypeToProto(UserAgentType value);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_SESSION_PROTO_PROTO_UTIL_H_
