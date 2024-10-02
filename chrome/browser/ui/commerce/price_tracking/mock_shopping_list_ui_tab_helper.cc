// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/price_tracking/mock_shopping_list_ui_tab_helper.h"

#include "base/task/sequenced_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

// static
void MockShoppingListUiTabHelper::CreateForWebContents(
    content::WebContents* content) {
  content->SetUserData(
      UserDataKey(),
      std::make_unique<testing::NiceMock<MockShoppingListUiTabHelper>>(
          content));
}

MockShoppingListUiTabHelper::MockShoppingListUiTabHelper(
    content::WebContents* content)
    : ShoppingListUiTabHelper(content, nullptr, nullptr, nullptr) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  valid_product_image_ = gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));

  // Set up a response so the default is success.
  ON_CALL(*this, SetPriceTrackingState)
      .WillByDefault([](bool enable, bool is_new_bookmark,
                        base::OnceCallback<void(bool)> callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), true));
      });
}

MockShoppingListUiTabHelper::~MockShoppingListUiTabHelper() = default;

const gfx::Image& MockShoppingListUiTabHelper::GetValidProductImage() {
  return valid_product_image_;
}

const gfx::Image& MockShoppingListUiTabHelper::GetInvalidProductImage() {
  return empty_product_image_;
}
