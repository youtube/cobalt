// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/test/test_product_messaging_controller.h"

#include "components/user_education/product_messaging/product_messaging_controller.h"

namespace user_education::test {

TestProductMessage::TestProductMessage(ProductMessagingController& controller,
                                       ProductMessageKey key,
                                       std::optional<base::TimeDelta> timeout)
    : key_(key) {
  controller.QueueMessage(key_,
                          base::BindOnce(&TestProductMessage::OnReadyToShow,
                                         weak_ptr_factory_.GetWeakPtr()),
                          timeout);
}

TestProductMessage::~TestProductMessage() = default;

void TestProductMessage::SetShown() {
  CHECK(handle_);
  handle_->SetShown();
}

void TestProductMessage::Release() {
  CHECK(handle_);
  handle_.reset();
}

void TestProductMessage::OnReadyToShow(ProductMessagingHandle handle) {
  CHECK(handle);
  shown_ = true;
  handle_ = std::move(handle);
  if (pending_status_callback_) {
    handle_->SetSupersededCallback(std::move(pending_status_callback_));
  }
}

void TestProductMessage::SetSupersededCallback(
    ProductMessageStatusCallback callback) {
  if (handle_) {
    handle_->SetSupersededCallback(std::move(callback));
  } else {
    pending_status_callback_ = std::move(callback);
  }
}

}  // namespace user_education::test
