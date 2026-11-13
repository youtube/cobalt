// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/shell/browser/cobalt_permission_manager.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

using blink::PermissionType;
using blink::mojom::PermissionStatus;

url::Origin AppOrigin() {
  return url::Origin::Create(GURL("https://www.youtube.com"));
}

url::Origin OtherOrigin() {
  return url::Origin::Create(GURL("https://attacker.example"));
}

TEST(CobaltPermissionManagerTest, GrantsProtectedMediaIdentifierToMainFrame) {
  EXPECT_EQ(PermissionStatus::GRANTED,
            CobaltPermissionManager::GetPermissionStatusForOrigins(
                PermissionType::PROTECTED_MEDIA_IDENTIFIER, AppOrigin(),
                AppOrigin()));
}

TEST(CobaltPermissionManagerTest,
     DeniesProtectedMediaIdentifierToCrossOriginFrame) {
  EXPECT_EQ(PermissionStatus::DENIED,
            CobaltPermissionManager::GetPermissionStatusForOrigins(
                PermissionType::PROTECTED_MEDIA_IDENTIFIER, OtherOrigin(),
                AppOrigin()));
}

TEST(CobaltPermissionManagerTest,
     DeniesProtectedMediaIdentifierToOpaqueOrigin) {
  url::Origin opaque;
  EXPECT_EQ(PermissionStatus::DENIED,
            CobaltPermissionManager::GetPermissionStatusForOrigins(
                PermissionType::PROTECTED_MEDIA_IDENTIFIER, opaque, opaque));
  EXPECT_EQ(PermissionStatus::DENIED,
            CobaltPermissionManager::GetPermissionStatusForOrigins(
                PermissionType::PROTECTED_MEDIA_IDENTIFIER,
                AppOrigin().DeriveNewOpaqueOrigin(), AppOrigin()));
}

TEST(CobaltPermissionManagerTest,
     DeniesProtectedMediaIdentifierToInsecureOrigin) {
  const url::Origin insecure =
      url::Origin::Create(GURL("http://insecure.example"));
  EXPECT_EQ(
      PermissionStatus::DENIED,
      CobaltPermissionManager::GetPermissionStatusForOrigins(
          PermissionType::PROTECTED_MEDIA_IDENTIFIER, insecure, insecure));
}

TEST(CobaltPermissionManagerTest, DeniesOtherPermissions) {
  for (PermissionType type :
       {PermissionType::AUDIO_CAPTURE, PermissionType::VIDEO_CAPTURE,
        PermissionType::GEOLOCATION, PermissionType::NOTIFICATIONS,
        PermissionType::MIDI_SYSEX, PermissionType::CLIPBOARD_READ_WRITE}) {
    EXPECT_EQ(PermissionStatus::DENIED,
              CobaltPermissionManager::GetPermissionStatusForOrigins(
                  type, AppOrigin(), AppOrigin()))
        << static_cast<int>(type);
  }
}

}  // namespace
}  // namespace content
