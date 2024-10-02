// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_RANDOM_SESSION_ID_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_RANDOM_SESSION_ID_H_

#include <array>
#include <ostream>

#include "base/containers/span.h"

namespace ash::quick_start {

// An immutable, copyable type representing six random bytes, or eight
// characters when encoded in base64.
class RandomSessionId {
 public:
  // This length is chosen to be 6 bytes in order to match the format used by
  // SmartSetup on Android for interoperability.
  static constexpr size_t kLength = 6;

  RandomSessionId();
  explicit RandomSessionId(base::span<const uint8_t, kLength> bytes);
  RandomSessionId(RandomSessionId&) = default;
  RandomSessionId& operator=(RandomSessionId&) = default;
  ~RandomSessionId() = default;

  base::span<const uint8_t, kLength> AsBytes() const { return bytes_; }

  // Convert to base64 (url-encoded, no padding). 6 bytes becomes 8 characters.
  std::string ToString() const;

  // Derive a 3-digit code from the session ID. Appended to the EndpointInfo
  // display name to help the user disambiguate devices.
  std::string GetDisplayCode() const;

 private:
  std::array<uint8_t, kLength> bytes_;
};

// Write the RandomSessionId to the ostream in hexadecimal for logging.
std::ostream& operator<<(std::ostream& stream,
                         const RandomSessionId& random_session_id);

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_RANDOM_SESSION_ID_H_
