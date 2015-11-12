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

#include "cobalt/dom/data_view.h"

#include <algorithm>

#include "cobalt/script/testing/mock_exception_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {
namespace {

using script::testing::MockExceptionState;
using testing::_;
using testing::SaveArg;
using testing::StrictMock;

TEST(DataViewTest, Constructors) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<ArrayBuffer> array_buffer = new ArrayBuffer(NULL, 10);

  scoped_refptr<DataView> data_view =
      new DataView(array_buffer, &exception_state);
  EXPECT_EQ(array_buffer, data_view->buffer());
  EXPECT_EQ(0, data_view->byte_offset());
  EXPECT_EQ(array_buffer->byte_length(), data_view->byte_length());

  data_view = new DataView(array_buffer, 1, &exception_state);
  EXPECT_EQ(array_buffer, data_view->buffer());
  EXPECT_EQ(1, data_view->byte_offset());
  EXPECT_EQ(array_buffer->byte_length(),
            data_view->byte_offset() + data_view->byte_length());

  data_view = new DataView(array_buffer, 1, 5, &exception_state);
  EXPECT_EQ(array_buffer, data_view->buffer());
  EXPECT_EQ(1, data_view->byte_offset());
  EXPECT_EQ(5, data_view->byte_length());
}

// DataView whose byte_length is 0 is allowed.
TEST(DataViewTest, ConstructingEmptyDataView) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<ArrayBuffer> array_buffer = new ArrayBuffer(NULL, 0);

  scoped_refptr<DataView> data_view =
      new DataView(array_buffer, &exception_state);
  EXPECT_EQ(0, data_view->byte_length());

  data_view = new DataView(array_buffer, 0, &exception_state);
  EXPECT_EQ(0, data_view->byte_length());

  data_view = new DataView(array_buffer, 0, 0, &exception_state);
  EXPECT_EQ(0, data_view->byte_length());
}

TEST(DataViewTest, ExceptionInConstructors) {
  StrictMock<MockExceptionState> exception_state;
  script::ExceptionState::SimpleExceptionType exception_type;
  scoped_refptr<ArrayBuffer> array_buffer = new ArrayBuffer(NULL, 10);

  EXPECT_CALL(exception_state, SetSimpleException(_, _))
      .WillOnce(SaveArg<0>(&exception_type));
  scoped_refptr<DataView> data_view = new DataView(NULL, &exception_state);
  EXPECT_EQ(script::ExceptionState::kTypeError, exception_type);

  EXPECT_CALL(exception_state, SetSimpleException(_, _))
      .WillOnce(SaveArg<0>(&exception_type));
  data_view = new DataView(array_buffer, array_buffer->byte_length() + 1,
                           &exception_state);
  EXPECT_EQ(script::ExceptionState::kRangeError, exception_type);

  EXPECT_CALL(exception_state, SetSimpleException(_, _))
      .WillOnce(SaveArg<0>(&exception_type));
  data_view = new DataView(array_buffer, 0, array_buffer->byte_length() + 1,
                           &exception_state);
  EXPECT_EQ(script::ExceptionState::kRangeError, exception_type);
}

TEST(DataViewTest, Getters) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<ArrayBuffer> array_buffer = new ArrayBuffer(NULL, 13);
  scoped_refptr<DataView> data_view =
      new DataView(array_buffer, &exception_state);

// For test of each type, we write 0 to the data that is going to be read and
// 0xff to the data that won't be read.  So all values we read out will be 0.
#define DEFINE_DATA_VIEW_GETTER_TEST(DomType, CppType)                        \
  {                                                                           \
    memset(array_buffer->data(), 0, sizeof(CppType));                         \
    memset(array_buffer->data() + sizeof(CppType), 0xff,                      \
           array_buffer->byte_length() - sizeof(CppType));                    \
    CppType value = data_view->Get##DomType(0, &exception_state);             \
    EXPECT_EQ(0, value);                                                      \
                                                                              \
    memset(array_buffer->data(), 0xff,                                        \
           array_buffer->byte_length() - sizeof(CppType));                    \
    memset(                                                                   \
        array_buffer->data() + array_buffer->byte_length() - sizeof(CppType), \
        0, sizeof(CppType));                                                  \
    value = data_view->Get##DomType(                                          \
        array_buffer->byte_length() - sizeof(CppType), &exception_state);     \
    EXPECT_EQ(0, value);                                                      \
  }
  DATA_VIEW_ACCESSOR_FOR_EACH(DEFINE_DATA_VIEW_GETTER_TEST)
#undef DEFINE_DATA_VIEW_GETTER_TEST
}

TEST(DataViewTest, GettersWithException) {
  StrictMock<MockExceptionState> exception_state;
  script::ExceptionState::SimpleExceptionType exception_type;
  scoped_refptr<ArrayBuffer> array_buffer = new ArrayBuffer(NULL, 13);
  scoped_refptr<DataView> data_view =
      new DataView(array_buffer, &exception_state);

#define DEFINE_DATA_VIEW_GETTER_WITH_EXCEPTION_TEST(DomType, CppType)          \
  {                                                                            \
    EXPECT_CALL(exception_state, SetSimpleException(_, _))                     \
        .WillOnce(SaveArg<0>(&exception_type));                                \
    data_view->Get##DomType(array_buffer->byte_length() - sizeof(CppType) + 1, \
                            &exception_state);                                 \
    EXPECT_EQ(script::ExceptionState::kRangeError, exception_type);            \
  }
  DATA_VIEW_ACCESSOR_FOR_EACH(DEFINE_DATA_VIEW_GETTER_WITH_EXCEPTION_TEST)
#undef DEFINE_DATA_VIEW_GETTER_WITH_EXCEPTION_TEST
}

TEST(DataViewTest, GettersWithOffset) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<ArrayBuffer> array_buffer = new ArrayBuffer(NULL, 13);
  scoped_refptr<DataView> data_view =
      new DataView(array_buffer, 3, &exception_state);

// For test of each type, we write 0 to the data that is going to be read and
// 0xff to the data that won't be read.  So all values we read out will be 0.
#define DEFINE_DATA_VIEW_GETTER_WITH_OFFSET_TEST(DomType, CppType)   \
  {                                                                  \
    memset(array_buffer->data(), 0xff, array_buffer->byte_length()); \
    memset(array_buffer->data() + data_view->byte_offset(), 0x0,     \
           sizeof(CppType));                                         \
    CppType value = data_view->Get##DomType(0, &exception_state);    \
    EXPECT_EQ(0, value);                                             \
  }
  DATA_VIEW_ACCESSOR_FOR_EACH(DEFINE_DATA_VIEW_GETTER_WITH_OFFSET_TEST)
#undef DEFINE_DATA_VIEW_GETTER_WITH_OFFSET_TEST
}

TEST(DataViewTest, Setters) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<ArrayBuffer> array_buffer = new ArrayBuffer(NULL, 13);
  scoped_refptr<DataView> data_view =
      new DataView(array_buffer, &exception_state);

// For test of each type, we fill the array_buffer to 0xff and then set the
// value of the particular index to 0.  Then we read it out to verify that it
// is indeed 0.
#define DEFINE_DATA_VIEW_SETTER_TEST(DomType, CppType)                        \
  {                                                                           \
    memset(array_buffer->data(), 0xff, array_buffer->byte_length());          \
    data_view->Set##DomType(0, 0, &exception_state);                          \
    CppType value = data_view->Get##DomType(0, &exception_state);             \
    EXPECT_EQ(0, value);                                                      \
                                                                              \
    memset(array_buffer->data(), 0xff, array_buffer->byte_length());          \
    data_view->Set##DomType(array_buffer->byte_length() - sizeof(CppType), 0, \
                            &exception_state);                                \
    value = data_view->Get##DomType(                                          \
        array_buffer->byte_length() - sizeof(CppType), &exception_state);     \
    EXPECT_EQ(0, value);                                                      \
  }
  DATA_VIEW_ACCESSOR_FOR_EACH(DEFINE_DATA_VIEW_SETTER_TEST)
#undef DEFINE_DATA_VIEW_SETTER_TEST
}

TEST(DataViewTest, SettersWithException) {
  StrictMock<MockExceptionState> exception_state;
  script::ExceptionState::SimpleExceptionType exception_type;
  scoped_refptr<ArrayBuffer> array_buffer = new ArrayBuffer(NULL, 13);
  scoped_refptr<DataView> data_view =
      new DataView(array_buffer, &exception_state);

#define DEFINE_DATA_VIEW_SETTER_WITH_EXCEPTION_TEST(DomType, CppType)          \
  {                                                                            \
    EXPECT_CALL(exception_state, SetSimpleException(_, _))                     \
        .WillOnce(SaveArg<0>(&exception_type));                                \
    data_view->Set##DomType(array_buffer->byte_length() - sizeof(CppType) + 1, \
                            0, &exception_state);                              \
    EXPECT_EQ(script::ExceptionState::kRangeError, exception_type);            \
  }
  DATA_VIEW_ACCESSOR_FOR_EACH(DEFINE_DATA_VIEW_SETTER_WITH_EXCEPTION_TEST)
#undef DEFINE_DATA_VIEW_SETTER_WITH_EXCEPTION_TEST
}

TEST(DataViewTest, SettersWithOffset) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<ArrayBuffer> array_buffer = new ArrayBuffer(NULL, 13);
  scoped_refptr<DataView> data_view =
      new DataView(array_buffer, 3, &exception_state);

// For test of each type, we fill the array_buffer to 0xff and then set the
// value at the particular index to 0.  Then we read it out to verify that it
// is indeed 0.
#define DEFINE_DATA_VIEW_SETTER_WITH_OFFSET_TEST(DomType, CppType)   \
  {                                                                  \
    memset(array_buffer->data(), 0xff, array_buffer->byte_length()); \
    data_view->Set##DomType(0, 0, &exception_state);                 \
    CppType value = data_view->Get##DomType(0, &exception_state);    \
    EXPECT_EQ(0, value);                                             \
  }
  DATA_VIEW_ACCESSOR_FOR_EACH(DEFINE_DATA_VIEW_SETTER_WITH_OFFSET_TEST)
#undef DEFINE_DATA_VIEW_SETTER_WITH_OFFSET_TEST
}

TEST(DataViewTest, GetterSetterEndianness) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<ArrayBuffer> array_buffer = new ArrayBuffer(NULL, 13);
  scoped_refptr<DataView> data_view =
      new DataView(array_buffer, 3, &exception_state);

// For test of each type, we set the value at the particular index to 1.  Then
// we read it out to verify that it is indeed 1.  This assumes that the memory
// layout of 1 of any type in little endian is reversed to its memory layout
// in big endian.  Note that when the endianness flag is true, it means little
// endian, and when endianess is not set, it is default to big endian.
#define DEFINE_DATA_VIEW_GETTER_SETTER_ENDIANNESS_TEST(DomType, CppType) \
  {                                                                      \
    data_view->Set##DomType(0, 1, true, &exception_state);               \
    CppType value = data_view->Get##DomType(0, true, &exception_state);  \
    EXPECT_EQ(1, value);                                                 \
                                                                         \
    data_view->Set##DomType(0, 1, &exception_state);                     \
    value = data_view->Get##DomType(0, false, &exception_state);         \
    EXPECT_EQ(1, value);                                                 \
                                                                         \
    data_view->Set##DomType(0, 1, false, &exception_state);              \
    value = data_view->Get##DomType(0, &exception_state);                \
    EXPECT_EQ(1, value);                                                 \
                                                                         \
    data_view->Set##DomType(0, 1, false, &exception_state);              \
    value = data_view->Get##DomType(0, false, &exception_state);         \
    EXPECT_EQ(1, value);                                                 \
                                                                         \
    data_view->Set##DomType(0, 1, true, &exception_state);               \
    value = data_view->Get##DomType(0, false, &exception_state);         \
    std::reverse(reinterpret_cast<uint8*>(&value),                       \
                 reinterpret_cast<uint8*>(&value) + sizeof(value));      \
    EXPECT_EQ(1, value);                                                 \
                                                                         \
    data_view->Set##DomType(0, 1, false, &exception_state);              \
    value = data_view->Get##DomType(0, true, &exception_state);          \
    std::reverse(reinterpret_cast<uint8*>(&value),                       \
                 reinterpret_cast<uint8*>(&value) + sizeof(value));      \
    EXPECT_EQ(1, value);                                                 \
  }
  DATA_VIEW_ACCESSOR_FOR_EACH(DEFINE_DATA_VIEW_GETTER_SETTER_ENDIANNESS_TEST)
#undef DEFINE_DATA_VIEW_GETTER_SETTER_ENDIANNESS_TEST
}

TEST(DataViewTest, PlatformEndianness) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<ArrayBuffer> array_buffer = new ArrayBuffer(NULL, 13);
  scoped_refptr<DataView> data_view =
      new DataView(array_buffer, 3, &exception_state);

#if defined(ARCH_CPU_LITTLE_ENDIAN)
  const bool kCpuIsLittleEndian = true;
#else  // defined(ARCH_CPU_LITTLE_ENDIAN)
  const bool kCpuIsLittleEndian = false;
#endif  // defined(ARCH_CPU_LITTLE_ENDIAN)

// For test of each type, we write 0x00, 0x01, ... to the array_buffer and
// verify the read out value with platform endianness taking into account.
#define DEFINE_DATA_VIEW_PLATFORM_ENDIANNESS_TEST(DomType, CppType)       \
  {                                                                       \
    for (uint32 i = 0; i < sizeof(CppType); ++i) {                        \
      array_buffer->data()[data_view->byte_offset() + i] =                \
          static_cast<uint8>(i);                                          \
    }                                                                     \
    CppType value =                                                       \
        data_view->Get##DomType(0, kCpuIsLittleEndian, &exception_state); \
    EXPECT_EQ(0, memcmp(reinterpret_cast<uint8*>(&value),                 \
                        array_buffer->data() + data_view->byte_offset(),  \
                        sizeof(value)));                                  \
  }
  DATA_VIEW_ACCESSOR_FOR_EACH(DEFINE_DATA_VIEW_PLATFORM_ENDIANNESS_TEST)
#undef DEFINE_DATA_VIEW_PLATFORM_ENDIANNESS_TEST
}

}  // namespace
}  // namespace dom
}  // namespace cobalt
