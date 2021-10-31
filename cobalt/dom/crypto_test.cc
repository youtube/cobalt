// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "cobalt/dom/crypto.h"

#include "base/test/scoped_task_environment.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/script/array_buffer_view.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/script_exception.h"
#include "cobalt/script/testing/mock_exception_state.h"
#include "cobalt/script/typed_arrays.h"
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
  base::test::ScopedTaskEnvironment task_env_;
  std::unique_ptr<script::JavaScriptEngine> javascript_engine =
      script::JavaScriptEngine::CreateEngine();
  scoped_refptr<script::GlobalEnvironment> global_environment =
      javascript_engine->CreateGlobalEnvironment();
  global_environment->CreateGlobalObject();
  script::Handle<script::Uint8Array> array1 =
      script::Uint8Array::New(global_environment, 16);
  script::Handle<script::Uint8Array> array2 =
      script::Uint8Array::New(global_environment, 16);
  auto* array1_data = static_cast<uint8*>(array1->RawData());
  for (uint32 i = 0; i < array1->Length(); ++i) {
    DCHECK_EQ(0, array1_data[i]);
  }
  auto* array2_data = static_cast<uint8*>(array2->RawData());
  for (uint32 i = 0; i < array2->Length(); ++i) {
    DCHECK_EQ(0, array2_data[i]);
  }

  scoped_refptr<Crypto> crypto = new Crypto;

  script::Handle<script::ArrayBufferView> casted_array1 =
      script::ArrayBufferView::New(global_environment, *array1);

  script::Handle<script::ArrayBufferView> result =
      crypto->GetRandomValues(casted_array1, &exception_state);
  EXPECT_TRUE(
      casted_array1.GetScriptValue()->EqualTo(*(result.GetScriptValue())));

  script::Handle<script::ArrayBufferView> casted_array2 =
      script::ArrayBufferView::New(global_environment, *array2);
  result = crypto->GetRandomValues(casted_array2, &exception_state);
  EXPECT_TRUE(
      casted_array2.GetScriptValue()->EqualTo(*(result.GetScriptValue())));

  // This check is theoretically flaky as the two random sequences could be
  // the same.  But the probability is extremely low and we do want to catch
  // the case when the two sequences are the same.
  bool all_equal = true;
  for (uint32 i = 0; i < array1->Length(); ++i) {
    if (array1_data[i] != array2_data[i]) {
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
  base::test::ScopedTaskEnvironment task_env_;
  std::unique_ptr<script::JavaScriptEngine> javascript_engine =
      script::JavaScriptEngine::CreateEngine();
  scoped_refptr<script::GlobalEnvironment> global_environment =
      javascript_engine->CreateGlobalEnvironment();
  global_environment->CreateGlobalObject();

  EXPECT_CALL(exception_state, SetException(_))
      .WillOnce(SaveArg<0>(&exception));

  script::Handle<script::ArrayBufferView> empty_handle;
  script::Handle<script::ArrayBufferView> result =
      crypto->GetRandomValues(empty_handle, &exception_state);
  EXPECT_TRUE(result.IsEmpty());
  ASSERT_TRUE(exception);
  EXPECT_EQ(DOMException::kTypeMismatchErr,
            base::polymorphic_downcast<DOMException*>(exception.get())->code());
}

TEST(CryptoTest, LargeArray) {
  // When array length exceeds 65536 GetRandomValues() should throw
  // QuotaExceededErr.
  scoped_refptr<Crypto> crypto = new Crypto;
  StrictMock<MockExceptionState> exception_state;
  base::test::ScopedTaskEnvironment task_env_;
  std::unique_ptr<script::JavaScriptEngine> javascript_engine =
      script::JavaScriptEngine::CreateEngine();
  scoped_refptr<script::GlobalEnvironment> global_environment =
      javascript_engine->CreateGlobalEnvironment();
  global_environment->CreateGlobalObject();
  script::Handle<script::Uint8Array> array65537 =
      script::Uint8Array::New(global_environment, 65537);
  scoped_refptr<script::ScriptException> exception;

  EXPECT_CALL(exception_state, SetException(_))
      .WillOnce(SaveArg<0>(&exception));

  script::Handle<script::ArrayBufferView> casted_array65537 =
      script::ArrayBufferView::New(global_environment, *array65537);
  script::Handle<script::ArrayBufferView> result =
      crypto->GetRandomValues(casted_array65537, &exception_state);
  EXPECT_TRUE(result.IsEmpty());
  ASSERT_TRUE(exception);
  EXPECT_EQ(DOMException::kQuotaExceededErr,
            base::polymorphic_downcast<DOMException*>(exception.get())->code());

  // Also ensure that we can work with array whose length is exactly 65536.
  script::Handle<script::Uint8Array> array65536 =
      script::Uint8Array::New(global_environment, 65536);
  script::Handle<script::ArrayBufferView> casted_array65536 =
      script::ArrayBufferView::New(global_environment, *array65536);
  result = crypto->GetRandomValues(casted_array65536, &exception_state);
  EXPECT_TRUE(
      casted_array65536.GetScriptValue()->EqualTo(*(result.GetScriptValue())));
}

}  // namespace dom
}  // namespace cobalt
