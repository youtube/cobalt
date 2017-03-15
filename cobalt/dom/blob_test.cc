// Copyright 2016 Google Inc. All Rights Reserved.
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

#include <algorithm>

#include "cobalt/dom/blob.h"
#include "cobalt/dom/data_view.h"
#include "cobalt/script/testing/mock_exception_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {
namespace {

using script::testing::MockExceptionState;
using testing::_;
using testing::SaveArg;
using testing::StrictMock;

TEST(BlobTest, Constructors) {
  scoped_refptr<Blob> blob_default_buffer = new Blob(NULL);

  EXPECT_EQ(0, blob_default_buffer->size());

  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<ArrayBuffer> array_buffer = new ArrayBuffer(NULL, 5);
  scoped_refptr<DataView> data_view =
      new DataView(array_buffer, &exception_state);
  data_view->SetInt16(0, static_cast<int16>(0x0607), &exception_state);
  data_view->SetInt16(3, static_cast<int16>(0xABCD), &exception_state);
  scoped_refptr<Blob> blob_with_buffer = new Blob(NULL, array_buffer);

  ASSERT_EQ(5, blob_with_buffer->size());
  ASSERT_TRUE(blob_with_buffer->data());

  EXPECT_EQ(0x6, blob_with_buffer->data()[0]);
  EXPECT_EQ(0x7, blob_with_buffer->data()[1]);
  EXPECT_EQ(0, blob_with_buffer->data()[2]);
  EXPECT_EQ(0xAB, blob_with_buffer->data()[3]);
  EXPECT_EQ(0xCD, blob_with_buffer->data()[4]);

  scoped_refptr<DataView> data_view_mid_3 =
      new DataView(array_buffer, 1, 3, &exception_state);
  script::Sequence<Blob::BlobPart> parts;
  parts.push_back(Blob::BlobPart(blob_with_buffer));
  parts.push_back(Blob::BlobPart(data_view_mid_3));
  parts.push_back(Blob::BlobPart(array_buffer));
  scoped_refptr<Blob> blob_with_parts = new Blob(NULL, parts);

  ASSERT_EQ(13UL, blob_with_parts->size());
  ASSERT_TRUE(blob_with_parts->data());
  EXPECT_EQ(0x6, blob_with_parts->data()[0]);
  EXPECT_EQ(0x7, blob_with_parts->data()[5]);
  EXPECT_EQ(0, blob_with_parts->data()[6]);
  EXPECT_EQ(0xAB, blob_with_parts->data()[11]);
  EXPECT_EQ(0xCD, blob_with_parts->data()[12]);
}

// Tests that further changes to a buffer from which a blob was constructed
// no longer affect the blob's buffer, since it must be a separate copy of
// its construction arguments.
TEST(BlobTest, HasOwnBuffer) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<ArrayBuffer> array_buffer = new ArrayBuffer(NULL, 2);
  scoped_refptr<DataView> data_view =
      new DataView(array_buffer, &exception_state);
  data_view->SetInt16(0, static_cast<int16>(0x0607), &exception_state);

  scoped_refptr<Blob> blob_with_buffer = new Blob(NULL, array_buffer);

  ASSERT_EQ(2, blob_with_buffer->size());
  ASSERT_TRUE(blob_with_buffer->data());
  EXPECT_NE(array_buffer->data(), blob_with_buffer->data());

  data_view->SetUint8(1, static_cast<uint8>(0xff), &exception_state);

  EXPECT_EQ(0x6, blob_with_buffer->data()[0]);
  EXPECT_EQ(0x7, blob_with_buffer->data()[1]);
}

}  // namespace
}  // namespace dom
}  // namespace cobalt
