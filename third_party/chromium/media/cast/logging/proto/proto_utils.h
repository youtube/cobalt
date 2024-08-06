// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAST_LOGGING_PROTO_PROTO_UTILS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAST_LOGGING_PROTO_PROTO_UTILS_H_

#include "third_party/chromium/media/cast/logging/logging_defines.h"
#include "third_party/chromium/media/cast/logging/proto/raw_events.pb.h"

// Utility functions for cast logging protos.
namespace media_m96 {
namespace cast {

// Converts |event| to a corresponding value in |media_m96::cast::proto::EventType|.
media_m96::cast::proto::EventType ToProtoEventType(CastLoggingEvent event);

}  // namespace cast
}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAST_LOGGING_PROTO_PROTO_UTILS_H_
