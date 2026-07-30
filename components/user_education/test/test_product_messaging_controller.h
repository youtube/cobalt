// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_TEST_TEST_PRODUCT_MESSAGING_CONTROLLER_H_
#define COMPONENTS_USER_EDUCATION_TEST_TEST_PRODUCT_MESSAGING_CONTROLLER_H_

#include <initializer_list>

#include "base/memory/weak_ptr.h"
#include "components/user_education/product_messaging/product_messaging_controller.h"

namespace user_education::test {

// Simulates a notice that requests to show in the `ProductMessagingController`.
// Will hold the handle until `Release()` is called.
class TestProductMessage {
 public:
  explicit TestProductMessage(
      ProductMessagingController& controller,
      ProductMessageKey key,
      std::optional<base::TimeDelta> timeout = std::nullopt);
  TestProductMessage(const TestProductMessage&) = delete;
  void operator=(const TestProductMessage&) = delete;
  ~TestProductMessage();

  // Mark that the notice was shown.
  void SetShown();

  // Release the handle (which must be held).
  void Release();

  ProductMessageKey key() const { return key_; }
  bool received_priority() const { return shown_; }
  bool has_priority() const { return static_cast<bool>(handle_); }

  void SetSupersededCallback(ProductMessageStatusCallback callback);

 private:
  void OnReadyToShow(ProductMessagingHandle handle);

  const ProductMessageKey key_;
  bool shown_ = false;
  ProductMessageStatusCallback pending_status_callback_;
  ProductMessagingHandle handle_;
  base::WeakPtrFactory<TestProductMessage> weak_ptr_factory_{this};
};

}  // namespace user_education::test

#endif  // COMPONENTS_USER_EDUCATION_TEST_TEST_PRODUCT_MESSAGING_CONTROLLER_H_
