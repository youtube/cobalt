// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "cobalt/bindings/testing/arbitrary_interface.h"
#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/script/array_buffer.h"
#include "cobalt/script/data_view.h"
#include "cobalt/script/typed_arrays.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace bindings {
namespace testing {

namespace {

// TODO: Change to an idl that uses array buffers once bindings supports array
// buffers.
using ArrayBufferTest = InterfaceBindingsTest<ArbitraryInterface>;

void* IncrementPointer(void* pointer, size_t length) {
  return reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(pointer) + length);
}

}  // namespace

TEST_F(ArrayBufferTest, ArrayBufferTest) {
  {
    auto array_buffer = script::ArrayBuffer::New(global_environment_, 1024);
    EXPECT_EQ(1024, array_buffer->ByteLength());
    EXPECT_NE(nullptr, array_buffer->Data());
  }

  {
    scoped_array<uint8_t> data(new uint8_t[256]);
    for (int i = 0; i < 256; i++) {
      data[i] = i;
    }

    auto array_buffer =
        script::ArrayBuffer::New(global_environment_, data.get(), 256);
    EXPECT_EQ(256, array_buffer->ByteLength());
    for (int i = 0; i < 256; i++) {
      EXPECT_EQ(i, reinterpret_cast<uint8_t*>(array_buffer->Data())[i]);
    }
  }

  {
    script::PreallocatedArrayBufferData preallocated_data(256);
    EXPECT_EQ(256, preallocated_data.byte_length());
    for (int i = 0; i < preallocated_data.byte_length(); i++) {
      reinterpret_cast<uint8_t*>(preallocated_data.data())[i] = i;
    }

    void* data_pointer = preallocated_data.data();

    auto array_buffer =
        script::ArrayBuffer::New(global_environment_, &preallocated_data);
    EXPECT_EQ(256, array_buffer->ByteLength());
    EXPECT_EQ(data_pointer, array_buffer->Data());
    for (int i = 0; i < 256; i++) {
      EXPECT_EQ(i, reinterpret_cast<uint8_t*>(array_buffer->Data())[i]);
    }

    EXPECT_EQ(nullptr, preallocated_data.data());
  }
}

TEST_F(ArrayBufferTest, UnusedPreallocatedDataReleases) {
  script::PreallocatedArrayBufferData preallocated_data(256);
  // Expect |my_data| to be properly freed. Rely on ASan to catch anything
  // going wrong here.
}

TEST_F(ArrayBufferTest, DataViewTest) {
  auto array_buffer = script::ArrayBuffer::New(global_environment_, 1024);
  auto data_view =
      script::DataView::New(global_environment_, array_buffer, 8, 16);

  EXPECT_EQ(8, data_view->ByteOffset());
  EXPECT_EQ(16, data_view->ByteLength());
  EXPECT_EQ(IncrementPointer(array_buffer->Data(), 8), data_view->RawData());

  constexpr int64_t value1 = 0xc0ba17;
  reinterpret_cast<int64_t*>(IncrementPointer(array_buffer->Data(), 8))[0] =
      value1;
  EXPECT_EQ(value1, reinterpret_cast<int64_t*>(data_view->RawData())[0]);

  constexpr int64_t value2 = 0x15c001;
  reinterpret_cast<int64_t*>(data_view->RawData())[1] = value2;
  EXPECT_EQ(value2, reinterpret_cast<int64_t*>(
                        IncrementPointer(array_buffer->Data(), 8))[1]);
}

template <typename TypedArrayType, typename CType>
void TypedArrayTest(script::GlobalEnvironment* global_environment) {
  {
    auto array_buffer = script::ArrayBuffer::New(global_environment, 1024);
    auto typed_array =
        TypedArrayType::New(global_environment, array_buffer, 8, 16);

    EXPECT_EQ(typed_array->Buffer()->ByteLength(), 1024);
    EXPECT_EQ(typed_array->ByteOffset(), 8);
    EXPECT_EQ(typed_array->ByteLength(), 16 * sizeof(CType));
    EXPECT_EQ(typed_array->RawData(), reinterpret_cast<void*>(IncrementPointer(
                                          array_buffer->Data(), 8)));

    EXPECT_EQ(typed_array->Length(), 16);

    EXPECT_EQ(typed_array->Data(), reinterpret_cast<CType*>(IncrementPointer(
                                       array_buffer->Data(), 8)));
  }

  {
    auto typed_array = TypedArrayType::New(global_environment, 1024);

    EXPECT_EQ(typed_array->Buffer()->ByteLength(), 1024 * sizeof(CType));
    EXPECT_EQ(typed_array->ByteOffset(), 0);
    EXPECT_EQ(typed_array->ByteLength(), 1024 * sizeof(CType));

    EXPECT_EQ(typed_array->Length(), 1024);

    EXPECT_EQ(typed_array->Data(),
              reinterpret_cast<CType*>(typed_array->RawData()));
  }
}

TEST_F(ArrayBufferTest, TypedArrays) {
#define CALL_TYPED_ARRAY_TEST(array, ctype) \
  TypedArrayTest<script::array, ctype>(global_environment_);
  COBALT_SCRIPT_TYPED_ARRAY_LIST(CALL_TYPED_ARRAY_TEST)
#undef CALL_TYPED_ARRAY_TEST
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt
