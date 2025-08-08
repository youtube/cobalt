// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_DEVICE_DISABLED_SCREEN_VIEW_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_DEVICE_DISABLED_SCREEN_VIEW_H_

#include "chrome/browser/ui/webui/ash/login/device_disabled_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockDeviceDisabledScreenView : public DeviceDisabledScreenView {
 public:
  MockDeviceDisabledScreenView();
  ~MockDeviceDisabledScreenView() override;

  MOCK_METHOD(
      void,
      Show,
      (const std::string&, const std::string&, const std::string&, bool),
      (override));

  MOCK_METHOD(void, UpdateMessage, (const std::string&), (override));
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_DEVICE_DISABLED_SCREEN_VIEW_H_
