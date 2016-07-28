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

#include "cobalt/dom/typed_array.h"

#include <limits>

#include "cobalt/dom/float32_array.h"
#include "cobalt/dom/float64_array.h"
#include "cobalt/dom/uint8_array.h"
#include "cobalt/script/testing/mock_exception_state.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {
namespace {

using script::testing::MockExceptionState;
using testing::_;
using testing::SaveArg;
using testing::StrictMock;

template <typename SubArrayType>
class TypedArrayTest : public ::testing::Test {};

typedef ::testing::Types<Uint8Array, Float32Array, Float64Array> TypedArrays;
TYPED_TEST_CASE(TypedArrayTest, TypedArrays);

TYPED_TEST(TypedArrayTest, CreateArray) {
  static const uint32 kLength = 5;
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<TypeParam> array =
      new TypeParam(NULL, kLength, &exception_state);
  EXPECT_EQ(kLength, array->length());
  for (uint32 i = 0; i < kLength; ++i) {
    EXPECT_EQ(0, array->Get(i));
  }
}

TYPED_TEST(TypedArrayTest, CreateArrayCopy) {
  static const uint32 kLength = 5;
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<TypeParam> source =
      new TypeParam(NULL, kLength, &exception_state);
  for (uint32 i = 0; i < kLength; ++i) {
    source->Set(i, static_cast<uint8>(i));
  }
  scoped_refptr<TypeParam> copy = new TypeParam(NULL, source, &exception_state);
  for (uint32 i = 0; i < kLength; ++i) {
    EXPECT_EQ(source->Get(i), copy->Get(i));
  }
}

TYPED_TEST(TypedArrayTest, CopyArrayWithSet) {
  static const uint32 kLength = 5;
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<TypeParam> source =
      new TypeParam(NULL, kLength, &exception_state);
  for (uint32 i = 0; i < kLength; ++i) {
    source->Set(i, static_cast<uint8>(i));
  }
  scoped_refptr<TypeParam> copy =
      new TypeParam(NULL, kLength, &exception_state);
  copy->Set(source, &exception_state);
  for (uint32 i = 0; i < kLength; ++i) {
    EXPECT_EQ(source->Get(i), copy->Get(i));
  }
}

TYPED_TEST(TypedArrayTest, CopyArrayWithSetOffset) {
  static const uint32 kLength = 5;
  static const uint32 kOffset = 3;
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<TypeParam> source =
      new TypeParam(NULL, kLength, &exception_state);
  for (uint32 i = 0; i < kLength; ++i) {
    source->Set(i, static_cast<uint8>(i));
  }
  scoped_refptr<TypeParam> copy =
      new TypeParam(NULL, kLength + kOffset, &exception_state);
  copy->Set(source, kOffset, &exception_state);
  for (uint32 i = 0; i < kOffset; ++i) {
    EXPECT_EQ(0, copy->Get(i));
  }
  for (uint32 i = 0; i < kLength; ++i) {
    EXPECT_EQ(source->Get(i), copy->Get(i + kOffset));
  }
}

TYPED_TEST(TypedArrayTest, CopyArrayWithSetOffsetOnTypeParamWithNonZeroOffset) {
  // As the following illustration, |dst_buffer| is an array buffer providing
  // the storage.  |dst| is a three element view starts from the second byte
  // of |dst_buffer|.
  // | 0 | 0 | 0 | 0 | 0 |  <==  dst_buffer
  //     <-   dst   ->
  // |src_buffer| and |src| are independent of |dst_buffer| and |dst|.
  // | 1 | 2 | 3 | 4 | 5 | 6 | <==  src_buffer
  //         <- src ->
  // So after calling set on |dst| with offset of 1 the content of
  // |dst_buffer| and |dst| should be:
  // | 0 | 0 | 3 | 4 | 0 |  <==  dst_buffer
  //     <-   dst   ->

  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<ArrayBuffer> dst_buffer =
      new ArrayBuffer(NULL, 5 * TypeParam::kBytesPerElement);
  scoped_refptr<TypeParam> dst = new TypeParam(
      NULL, dst_buffer, 1 * TypeParam::kBytesPerElement, 3, &exception_state);

  scoped_refptr<ArrayBuffer> src_buffer =
      new ArrayBuffer(NULL, 6 * TypeParam::kBytesPerElement);
  scoped_refptr<TypeParam> src = new TypeParam(
      NULL, src_buffer, 2 * TypeParam::kBytesPerElement, 2, &exception_state);

  // Use a TypedArray to fill |src_buffer| with values.
  scoped_refptr<TypeParam> fill =
      new TypeParam(NULL, src_buffer, &exception_state);
  for (uint32 i = 0; i < fill->length(); ++i) {
    fill->Set(i, static_cast<uint8>(i + 1));
  }

  dst->Set(src, 1, &exception_state);

  EXPECT_EQ(0, dst->Get(0));
  EXPECT_EQ(3, dst->Get(1));
  EXPECT_EQ(4, dst->Get(2));
}

TYPED_TEST(TypedArrayTest, SetItem) {
  static const uint32 kLength = 3;
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<TypeParam> array =
      new TypeParam(NULL, kLength, &exception_state);
  array->Set(1, 0xff);
  EXPECT_EQ(0, array->Get(0));
  EXPECT_EQ(0xff, array->Get(1));
  EXPECT_EQ(0, array->Get(2));
}

TYPED_TEST(TypedArrayTest, ArrayView) {
  // Test creating a second array that's a view of the first one.
  // Write to the view and then check the original was modified.
  static const uint32 kLength = 5;
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<TypeParam> array =
      new TypeParam(NULL, kLength, &exception_state);
  scoped_refptr<TypeParam> view =
      new TypeParam(NULL, array->buffer(), &exception_state);
  for (uint32 i = 0; i < kLength; ++i) {
    view->Set(i, static_cast<uint8>(i + 1));
  }
  for (uint32 i = 0; i < kLength; ++i) {
    EXPECT_EQ(i + 1, array->Get(i));
  }
}

TYPED_TEST(TypedArrayTest, ArrayViewLengthZero) {
  static const uint32 kLength = 5;
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<TypeParam> array =
      new TypeParam(NULL, kLength, &exception_state);
  scoped_refptr<TypeParam> view =
      new TypeParam(NULL, array->buffer(),
                    kLength * TypeParam::kBytesPerElement, &exception_state);
  EXPECT_EQ(0, view->length());
}

TYPED_TEST(TypedArrayTest, ArrayViewOffset) {
  // Test creating a second array that's a view of the first one.
  // Write to the view and then check the original was modified.
  static const uint32 kLength = 5;
  static const uint32 kViewOffset = 3;
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<TypeParam> array =
      new TypeParam(NULL, kLength, &exception_state);
  scoped_refptr<TypeParam> view = new TypeParam(
      NULL, array->buffer(), kViewOffset * TypeParam::kBytesPerElement,
      &exception_state);
  for (uint32 i = 0; i < kLength - kViewOffset; ++i) {
    view->Set(i, 0xff);
  }
  EXPECT_EQ(0, array->Get(0));
  EXPECT_EQ(0, array->Get(1));
  EXPECT_EQ(0, array->Get(2));
  EXPECT_EQ(0xff, array->Get(3));
  EXPECT_EQ(0xff, array->Get(4));
}

TYPED_TEST(TypedArrayTest, ArrayViewOffsetAndLength) {
  // Test creating a second array that's a view of the first one.
  // Write to the view and then check the original was modified.
  static const uint32 kLength = 5;
  static const uint32 kViewOffset = 3;
  static const uint32 kViewLength = kLength - kViewOffset;
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<TypeParam> array =
      new TypeParam(NULL, kLength, &exception_state);
  scoped_refptr<TypeParam> view = new TypeParam(
      NULL, array->buffer(), kViewOffset * TypeParam::kBytesPerElement,
      kViewLength, &exception_state);
  for (uint32 i = 0; i < kViewLength; ++i) {
    view->Set(i, static_cast<uint8>(kViewOffset + i));
  }
  EXPECT_EQ(0, array->Get(0));
  EXPECT_EQ(0, array->Get(1));
  EXPECT_EQ(0, array->Get(2));
  EXPECT_EQ(3, array->Get(3));
  EXPECT_EQ(4, array->Get(4));
}

TYPED_TEST(TypedArrayTest, CreateSubArrayOnlyStart) {
  static const uint32 kLength = 5;
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<TypeParam> array =
      new TypeParam(NULL, kLength, &exception_state);
  scoped_refptr<TypeParam> subarray = array->Subarray(NULL, 1);
  for (uint32 i = 0; i < kLength; ++i) {
    array->Set(i, static_cast<uint8>(i));
  }
  EXPECT_EQ(4, subarray->length());
  EXPECT_EQ(1, subarray->Get(0));
  EXPECT_EQ(2, subarray->Get(1));
  EXPECT_EQ(3, subarray->Get(2));
  EXPECT_EQ(4, subarray->Get(3));
}

TYPED_TEST(TypedArrayTest, CreateSubArrayStartEnd) {
  static const uint32 kLength = 5;
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<TypeParam> array =
      new TypeParam(NULL, kLength, &exception_state);
  scoped_refptr<TypeParam> subarray = array->Subarray(NULL, 1, 4);
  for (uint32 i = 0; i < kLength; ++i) {
    array->Set(i, static_cast<uint8>(i));
  }
  EXPECT_EQ(3, subarray->length());
  EXPECT_EQ(1, subarray->Get(0));
  EXPECT_EQ(2, subarray->Get(1));
  EXPECT_EQ(3, subarray->Get(2));
}

TYPED_TEST(TypedArrayTest, CreateSubArrayStartEndOnTypeParamWithNonZeroOffset) {
  // As the following illustration, |array_buffer| owns the whole buffer,
  // |array| is a view starts from the second byte of |array_buffer|, |subarray|
  // is a subarray of |array| starts from the second byte, which is in turn a
  // view of |array_buffer| starts from the third byte.
  // | 0 | 1 | 2 | 3 | 4 |  <==  array_buffer
  //     <-    array    ->
  //         <-subarray ->
  static const uint32 kLength = 5;
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<ArrayBuffer> array_buffer =
      new ArrayBuffer(NULL, kLength * TypeParam::kBytesPerElement);
  scoped_refptr<TypeParam> array =
      new TypeParam(NULL, array_buffer, 1 * TypeParam::kBytesPerElement,
                    kLength - 1, &exception_state);
  // The length passed to Subarray(NULL, ) is actually one element longer than
  // what
  // |array| has.  This should be clamped inside Subarray(NULL, ) and the size
  // of
  // |subarray| should still be three.
  scoped_refptr<TypeParam> subarray =
      array->Subarray(NULL, 1, static_cast<int>(array->length()));

  // Use a TypedArray to fill |array_buffer| with values.
  scoped_refptr<TypeParam> fill =
      new TypeParam(NULL, array_buffer, &exception_state);
  for (uint32 i = 0; i < fill->length(); ++i) {
    fill->Set(i, static_cast<uint8>(i));
  }

  EXPECT_EQ(1, array->Get(0));
  EXPECT_EQ(2, subarray->Get(0));
}

TYPED_TEST(TypedArrayTest, CreateSubArrayNegativeOnlyStart) {
  static const uint32 kLength = 5;
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<TypeParam> array =
      new TypeParam(NULL, kLength, &exception_state);
  scoped_refptr<TypeParam> subarray = array->Subarray(NULL, -2);
  for (uint32 i = 0; i < kLength; ++i) {
    array->Set(i, static_cast<uint8>(i));
  }

  // subarray[0] -> array[3]
  // subarray[1] -> array[4]
  EXPECT_EQ(2, subarray->length());
  EXPECT_EQ(3, subarray->Get(0));
  EXPECT_EQ(4, subarray->Get(1));
}

TYPED_TEST(TypedArrayTest, CreateSubArrayNegativeStartEnd) {
  static const uint32 kLength = 5;
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<TypeParam> array =
      new TypeParam(NULL, kLength, &exception_state);
  scoped_refptr<TypeParam> subarray = array->Subarray(NULL, -3, -1);
  for (uint32 i = 0; i < kLength; ++i) {
    array->Set(i, static_cast<uint8>(i));
  }

  // subarray[0] -> array[2]
  // subarray[1] -> array[3]
  EXPECT_EQ(2, subarray->length());
  EXPECT_EQ(2, subarray->Get(0));
  EXPECT_EQ(3, subarray->Get(1));
}

TYPED_TEST(TypedArrayTest, CreateSubArrayInvalidLength) {
  static const uint32 kLength = 5;
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<TypeParam> array =
      new TypeParam(NULL, kLength, &exception_state);
  scoped_refptr<TypeParam> subarray = array->Subarray(NULL, 12, 5);
  EXPECT_EQ(0, subarray->length());
}

TYPED_TEST(TypedArrayTest, ExceptionInConstructor) {
  // This test doesn't apply to any typed array whose element size is 1.
  if (TypeParam::kBytesPerElement == 1) {
    return;
  }

  StrictMock<MockExceptionState> exception_state;
  script::ExceptionState::SimpleExceptionType exception_type;

  EXPECT_CALL(exception_state, SetSimpleException(_, _))
      .WillOnce(SaveArg<0>(&exception_type));

  // Create an ArrayBuffer whose size isn't a multiple of
  // TypeParam::kBytesPerElement.
  scoped_refptr<ArrayBuffer> array_buffer =
      new ArrayBuffer(NULL, TypeParam::kBytesPerElement + 1);
  // The size of the array_buffer isn't a multiple of
  // TypeParam::kBytesPerElement.
  scoped_refptr<TypeParam> array =
      new TypeParam(NULL, array_buffer, &exception_state);
  EXPECT_EQ(script::ExceptionState::kRangeError, exception_type);

  EXPECT_CALL(exception_state, SetSimpleException(_, _))
      .WillOnce(SaveArg<0>(&exception_type));
  // Neither the size of the array_buffer nor the byte_offset is a multiple of
  // TypeParam::kBytesPerElement, but SetSimpleException() should only be called
  // once.
  array = new TypeParam(NULL, array_buffer, 1, &exception_state);
  EXPECT_EQ(script::ExceptionState::kRangeError, exception_type);

  EXPECT_CALL(exception_state, SetSimpleException(_, _))
      .WillOnce(SaveArg<0>(&exception_type));
  // Now the size of the array_buffer is a multiple of
  // TypeParam::kBytesPerElement but the byte_offset isn't.
  array_buffer = new ArrayBuffer(NULL, TypeParam::kBytesPerElement);
  array = new TypeParam(NULL, array_buffer, 1, &exception_state);
  EXPECT_EQ(script::ExceptionState::kRangeError, exception_type);

  EXPECT_CALL(exception_state, SetSimpleException(_, _))
      .WillOnce(SaveArg<0>(&exception_type));
  // Both the size of the array_buffer and the byte_offset are multiples of
  // TypeParam::kBytesPerElement but array_buffer cannot hold 2 elements.
  array = new TypeParam(NULL, array_buffer, 0, 2, &exception_state);
  EXPECT_EQ(script::ExceptionState::kRangeError, exception_type);

  // The size of the array_buffer isn't a multiple of
  // TypeParam::kBytesPerElement but the byte_offset is.  Also the whole typed
  // array can fit into the array_buffer.  The constructor should run without
  // raising any exception.
  array_buffer = new ArrayBuffer(NULL, TypeParam::kBytesPerElement * 2 + 1);
  array = new TypeParam(NULL, array_buffer, TypeParam::kBytesPerElement, 1,
                        &exception_state);
}

TYPED_TEST(TypedArrayTest, ExceptionInSet) {
  // This test doesn't apply to any typed array whose element size is 1.
  if (TypeParam::kBytesPerElement == 1) {
    return;
  }

  StrictMock<MockExceptionState> exception_state;
  script::ExceptionState::SimpleExceptionType exception_type;

  EXPECT_CALL(exception_state, SetSimpleException(_, _))
      .WillOnce(SaveArg<0>(&exception_type));

  static const uint32 kLength = 5;
  scoped_refptr<TypeParam> source =
      new TypeParam(NULL, kLength + 1, &exception_state);
  scoped_refptr<TypeParam> dest =
      new TypeParam(NULL, kLength, &exception_state);
  dest->Set(source, &exception_state);
  EXPECT_EQ(script::ExceptionState::kRangeError, exception_type);

  EXPECT_CALL(exception_state, SetSimpleException(_, _))
      .WillOnce(SaveArg<0>(&exception_type));
  source = new TypeParam(NULL, kLength, &exception_state);
  dest = new TypeParam(NULL, kLength + 1, &exception_state);
  dest->Set(source, 2, &exception_state);
  EXPECT_EQ(script::ExceptionState::kRangeError, exception_type);

  dest->Set(source, 1, &exception_state);
}

}  // namespace
}  // namespace dom
}  // namespace cobalt
