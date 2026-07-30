// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_MOCK_BUBBLE_MANAGER_H_
#define CHROME_BROWSER_UI_AUTOFILL_MOCK_BUBBLE_MANAGER_H_

#include "chrome/browser/ui/autofill/bubble_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockBubbleManager : public BubbleManager {
 public:
  MockBubbleManager();
  ~MockBubbleManager() override;

  MOCK_METHOD(void,
              RequestShowController,
              (BubbleControllerBase & controller_to_show, bool force_show),
              (override));
  MOCK_METHOD(void,
              OnBubbleHiddenByController,
              (BubbleControllerBase & controller_to_hide,
               bool show_next_bubble),
              (override));
  MOCK_METHOD(bool,
              HasPendingBubbleOfSameType,
              (const BubbleType bubble_type),
              (const, override));
  MOCK_METHOD(bool,
              HasConflictingPendingBubble,
              (const BubbleType bubble_type),
              (const, override));
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_MOCK_BUBBLE_MANAGER_H_
