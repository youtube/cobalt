/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/dom/crypto.h"

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/array_buffer_view.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/uint8_array.h"
#include "cobalt/script/script_exception.h"
#include "cobalt/script/testing/mock_exception_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

using script::testing::MockExceptionState;
using testing::_;
using testing::SaveArg;
using testing::StrictMock;

// TODO: Test arrays in other integer types.
// TODO: Test arrays in float types.

TEST(CryptoTest, GetRandomValues) {
  StrictMock<MockExceptionState> exception_state;

  scoped_refptr<Uint8Array> array1 = new Uint8Array(NULL, 16, &exception_state);
  scoped_refptr<Uint8Array> array2 = new Uint8Array(NULL, 16, &exception_state);
  for (uint32 i = 0; i < array1->length(); ++i) {
    DCHECK_EQ(0, array1->Get(i));
  }
  for (uint32 i = 0; i < array2->length(); ++i) {
    DCHECK_EQ(0, array2->Get(i));
  }

  scoped_refptr<Crypto> crypto = new Crypto;
  scoped_refptr<ArrayBufferView> result =
      crypto->GetRandomValues(array1, &exception_state);
  EXPECT_EQ(array1, result);

  result = crypto->GetRandomValues(array2, &exception_state);
  EXPECT_EQ(array2, result);

  // This check is theoretically flaky as the two random sequences could be
  // the same.  But the probability is extremely low and we do want to catch
  // the case when the two sequences are the same.
  bool all_equal = true;
  for (uint32 i = 0; i < array1->length(); ++i) {
    if (array1->Get(i) != array2->Get(i)) {
      all_equal = false;
      break;
    }
  }

  EXPECT_FALSE(all_equal);
}

TEST(CryptoTest, NullArray) {
  scoped_refptr<Crypto> crypto = new Crypto;
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<script::ScriptException> exception;

  EXPECT_CALL(exception_state, SetException(_))
      .WillOnce(SaveArg<0>(&exception));

  scoped_refptr<ArrayBufferView> result =
      crypto->GetRandomValues(NULL, &exception_state);
  EXPECT_FALSE(result);
  ASSERT_TRUE(exception);
  EXPECT_EQ(DOMException::kTypeMismatchErr,
            base::polymorphic_downcast<DOMException*>(exception.get())->code());
}

TEST(CryptoTest, LargeArray) {
  // When array length exceeds 65536 GetRandomValues() should throw
  // QuotaExceededErr.
  scoped_refptr<Crypto> crypto = new Crypto;
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<Uint8Array> array65537 =
      new Uint8Array(NULL, 65537, &exception_state);
  scoped_refptr<script::ScriptException> exception;

  EXPECT_CALL(exception_state, SetException(_))
      .WillOnce(SaveArg<0>(&exception));

  scoped_refptr<ArrayBufferView> result =
      crypto->GetRandomValues(array65537, &exception_state);
  EXPECT_FALSE(result);
  ASSERT_TRUE(exception);
  EXPECT_EQ(DOMException::kQuotaExceededErr,
            base::polymorphic_downcast<DOMException*>(exception.get())->code());

  // Also ensure that we can work with array whose length is exactly 65536.
  scoped_refptr<Uint8Array> array65536 =
      new Uint8Array(NULL, 65536, &exception_state);
  result = crypto->GetRandomValues(array65536, &exception_state);
  EXPECT_EQ(array65536, result);
}

}  // namespace dom
}  // namespace cobalt
