// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_media_source_win.h"

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "testing/gtest/include/gtest/gtest.h"

class SystemMediaSourceWinTest : public testing::Test {
 protected:
  SystemMediaSourceWin& CreateSource() {
    source_ = new SystemMediaSourceWin();
    return *source_;
  }

  void TearDown() override {
    SystemMediaSourceWin* raw = source_.ExtractAsDangling();
    delete raw;
  }

  base::test::TaskEnvironment task_environment_;
  raw_ptr<SystemMediaSourceWin> source_ = nullptr;
};

// Verify that construction completes without crashing or pumping window
// messages. The COM message filter suppresses message dispatching during
// RoGetActivationFactory, preventing re-entrancy crashes.
TEST_F(SystemMediaSourceWinTest, ConstructionDoesNotCrash) {
  auto& source = CreateSource();

  // Status is available immediately after construction.
  auto camera =
      source.SystemPermissionStatus(ContentSettingsType::MEDIASTREAM_CAMERA);
  auto mic =
      source.SystemPermissionStatus(ContentSettingsType::MEDIASTREAM_MIC);

  // Cached COM objects yield consistent results across calls.
  EXPECT_EQ(camera, source.SystemPermissionStatus(
                        ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_EQ(
      mic, source.SystemPermissionStatus(ContentSettingsType::MEDIASTREAM_MIC));
}
