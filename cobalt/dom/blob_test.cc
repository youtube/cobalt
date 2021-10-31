// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/testing/stub_window.h"
#include "cobalt/script/data_view.h"
#include "cobalt/script/testing/mock_exception_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {
namespace {

using script::testing::MockExceptionState;
using ::testing::_;
using ::testing::SaveArg;
using ::testing::StrictMock;

TEST(BlobTest, Constructors) {
  // Initialize JavaScriptEngine and its environment.
  testing::StubWindow stub_window;
  script::GlobalEnvironment* global_environment =
      stub_window.global_environment();
  script::EnvironmentSettings* environment_settings =
      stub_window.environment_settings();

  script::Handle<script::ArrayBuffer> array_buffer =
      script::ArrayBuffer::New(global_environment, 5);
  scoped_refptr<Blob> blob_default_buffer = new Blob(environment_settings);

  EXPECT_EQ(0, blob_default_buffer->size());

  script::Handle<script::DataView> data_view = script::DataView::New(
      global_environment, array_buffer, 0, array_buffer->ByteLength());
  uint8 test_data[] = {0x06, 0x07, 0x00, 0x7B, 0xCD};
  memcpy(data_view->RawData(), test_data, 5);
  scoped_refptr<Blob> blob_with_buffer =
      new Blob(environment_settings, array_buffer);

  ASSERT_EQ(5, blob_with_buffer->size());
  ASSERT_TRUE(blob_with_buffer->data());

  EXPECT_EQ(0x6, blob_with_buffer->data()[0]);
  EXPECT_EQ(0x7, blob_with_buffer->data()[1]);
  EXPECT_EQ(0, blob_with_buffer->data()[2]);
  EXPECT_EQ(0x7B, blob_with_buffer->data()[3]);
  EXPECT_EQ(0xCD, blob_with_buffer->data()[4]);

  script::Handle<script::DataView> data_view_mid_3 =
      script::DataView::New(global_environment, array_buffer, 1, 3);
  std::string text("text");

  script::Sequence<Blob::BlobPart> parts;
  parts.push_back(Blob::BlobPart(blob_with_buffer));
  parts.push_back(Blob::BlobPart(data_view_mid_3));
  parts.push_back(Blob::BlobPart(array_buffer));
  parts.push_back(Blob::BlobPart(text));
  scoped_refptr<Blob> blob_with_parts = new Blob(environment_settings, parts);

  ASSERT_EQ(17UL, blob_with_parts->size());
  ASSERT_TRUE(blob_with_parts->data());
  EXPECT_EQ(0x6, blob_with_parts->data()[0]);
  EXPECT_EQ(0x7, blob_with_parts->data()[5]);
  EXPECT_EQ(0, blob_with_parts->data()[6]);
  EXPECT_EQ(0x7B, blob_with_parts->data()[11]);
  EXPECT_EQ(0xCD, blob_with_parts->data()[12]);
  EXPECT_EQ('t', blob_with_parts->data()[13]);
  EXPECT_EQ('x', blob_with_parts->data()[15]);
}

// Tests that further changes to a buffer from which a blob was constructed
// no longer affect the blob's buffer, since it must be a separate copy of
// its construction arguments.

TEST(BlobTest, HasOwnBuffer) {
  // Initialize JavaScriptEngine and its environment.
  testing::StubWindow stub_window;
  script::GlobalEnvironment* global_environment =
      stub_window.global_environment();
  script::EnvironmentSettings* environment_settings =
      stub_window.environment_settings();
  StrictMock<MockExceptionState> exception_state;

  script::Handle<script::ArrayBuffer> array_buffer =
      script::ArrayBuffer::New(global_environment, 2);
  script::Handle<script::DataView> data_view = script::DataView::New(
      global_environment, array_buffer, 0, array_buffer->ByteLength());

  uint8 test_data[2] = {0x06, 0x07};
  memcpy(data_view->RawData(), test_data, 2);

  scoped_refptr<Blob> blob_with_buffer =
      new Blob(environment_settings, array_buffer);

  ASSERT_EQ(2, blob_with_buffer->size());
  ASSERT_TRUE(blob_with_buffer->data());
  EXPECT_NE(array_buffer->Data(), blob_with_buffer->data());

  uint8 test_data2[1] = {0xff};
  memcpy(data_view->RawData(), test_data2, 1);

  EXPECT_EQ(0x6, blob_with_buffer->data()[0]);
  EXPECT_EQ(0x7, blob_with_buffer->data()[1]);
}

}  // namespace
}  // namespace dom
}  // namespace cobalt
