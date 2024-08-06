// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_REMOTING_REMOTING_CONSTANTS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_REMOTING_REMOTING_CONSTANTS_H_

namespace media_m96 {
namespace remoting {

// The src attribute for remoting media should use the URL with this scheme.
// The URL format is "media-remoting:<id>", e.g. "media-remoting:test".
constexpr char kRemotingScheme[] = "media-remoting";

}  // namespace remoting
}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_REMOTING_REMOTING_CONSTANTS_H_
