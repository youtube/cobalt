// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/shopping_user_model.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

class ShoppingUserModelTest : public DefaultModelTestBase {
 public:
  ShoppingUserModelTest()
      : DefaultModelTestBase(std::make_unique<ShoppingUserModel>()) {}
  ~ShoppingUserModelTest() override = default;
};

TEST_F(ShoppingUserModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(ShoppingUserModelTest, ExecuteModelWithInput) {
  // When shopping related features count is less than or equal to 1,
  // the user shouldn't be considered a shopping user.
  ExpectExecutionWithInput(/*inputs=*/{0, 0}, /*expected_error=*/false,
                           /*expected_result=*/{0});
  ExpectExecutionWithInput(/*inputs=*/{1, 0}, /*expected_error=*/false,
                           /*expected_result=*/{0});
  ExpectExecutionWithInput(/*inputs=*/{1, 1}, /*expected_error=*/false,
                           /*expected_result=*/{0});

  // When shopping related features count is greater than or equal to 1,
  // the user should be considered shopping user.
  ExpectExecutionWithInput(/*inputs=*/{1, 2}, /*expected_error=*/false,
                           /*expected_result=*/{1});
  ExpectExecutionWithInput(/*inputs=*/{2, 2}, /*expected_error=*/false,
                           /*expected_result=*/{1});

  // Invalid input
  ExpectExecutionWithInput(/*inputs=*/{1, 1, 1, 1, 1}, /*expected_error=*/true,
                           /*expected_result=*/{0});
  ExpectExecutionWithInput(/*inputs=*/{0}, /*expected_error=*/true,
                           /*expected_result=*/{0});
  ExpectExecutionWithInput(/*inputs=*/{2, 2, 2, 2, 2, 2, 2, 2},
                           /*expected_error=*/true, /*expected_result=*/{0});
}

}  // namespace segmentation_platform
