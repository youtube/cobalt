// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/device_name_policy_handler_name_generator.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"

namespace policy {

namespace {

constexpr char kAssetIDPlaceholder[] = "${ASSET_ID}";
constexpr char kMachineNamePlaceholder[] = "${MACHINE_NAME}";
constexpr char kSerialNumPlaceholder[] = "${SERIAL_NUM}";
constexpr char kMACAddressPlaceholder[] = "${MAC_ADDR}";
constexpr char kLocationPlaceholder[] = "${LOCATION}";

// As per RFC 1035, hostname should be 63 characters or less.
const int kMaxHostnameLength = 63;

bool inline IsValidHostnameCharacter(char c) {
  return base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) || c == '_' || c == '-';
}

bool IsValidHostname(base::StringPiece hostname) {
  if ((hostname.size() > kMaxHostnameLength) || (hostname.size() == 0))
    return false;
  if (hostname[0] == '-')
    return false;  // '-' is not valid for the first char
  for (const char& c : hostname) {
    if (!IsValidHostnameCharacter(c))
      return false;
  }
  return true;
}

}  // namespace

std::string FormatHostname(base::StringPiece name_template,
                           base::StringPiece asset_id,
                           base::StringPiece serial,
                           base::StringPiece mac,
                           base::StringPiece machine_name,
                           base::StringPiece location) {
  std::string result = std::string(name_template);
  base::ReplaceSubstringsAfterOffset(&result, 0, kAssetIDPlaceholder, asset_id);
  base::ReplaceSubstringsAfterOffset(&result, 0, kSerialNumPlaceholder, serial);
  base::ReplaceSubstringsAfterOffset(&result, 0, kMACAddressPlaceholder, mac);
  base::ReplaceSubstringsAfterOffset(&result, 0, kMachineNamePlaceholder,
                                     machine_name);
  base::ReplaceSubstringsAfterOffset(&result, 0, kLocationPlaceholder,
                                     location);

  if (!IsValidHostname(result))
    return std::string();
  return result;
}

}  // namespace policy
