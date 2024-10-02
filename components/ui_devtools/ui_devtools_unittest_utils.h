// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_UI_DEVTOOLS_UNITTEST_UTILS_H_
#define COMPONENTS_UI_DEVTOOLS_UI_DEVTOOLS_UNITTEST_UTILS_H_

#include "components/ui_devtools/protocol.h"
#include "components/ui_devtools/ui_element_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ui_devtools {

class FakeFrontendChannel : public protocol::FrontendChannel {
 public:
  FakeFrontendChannel();

  FakeFrontendChannel(const FakeFrontendChannel&) = delete;
  FakeFrontendChannel& operator=(const FakeFrontendChannel&) = delete;

  ~FakeFrontendChannel() override;

  int CountProtocolNotificationMessageStartsWith(const std::string& message);

  int CountProtocolNotificationMessage(const std::string& message);

  void SetAllowNotifications(bool allow_notifications) {
    allow_notifications_ = allow_notifications;
  }

  // FrontendChannel:
  void SendProtocolResponse(
      int callId,
      std::unique_ptr<protocol::Serializable> message) override {}
  void FlushProtocolNotifications() override {}
  void FallThrough(int call_id,
                   crdtp::span<uint8_t> method,
                   crdtp::span<uint8_t> message) override {}
  void SendProtocolNotification(
      std::unique_ptr<protocol::Serializable> message) override;

 private:
  std::vector<std::string> protocol_notification_messages_;
  bool allow_notifications_ = true;
};

class MockUIElementDelegate : public UIElementDelegate {
 public:
  MockUIElementDelegate();
  ~MockUIElementDelegate() override;

  MOCK_METHOD2(OnUIElementAdded, void(UIElement*, UIElement*));
  MOCK_METHOD2(OnUIElementReordered, void(UIElement*, UIElement*));
  MOCK_METHOD1(OnUIElementRemoved, void(UIElement*));
  MOCK_METHOD1(OnUIElementBoundsChanged, void(UIElement*));
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_UI_DEVTOOLS_UNITTEST_UTILS_H_
