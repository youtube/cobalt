// Copyright (C) 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "protostore/proto-data-store.h"

#include <cstdint>

#include "absl/strings/str_cat.h"
#include "google/protobuf/message.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "protostore/file-storage.h"
#include "protostore/testing-matchers.h"
#include "protostore/testfile-fixture.h"
#include "protostore/test.pb.h"

namespace protostore {
namespace {

using ::testing::Not;
using ::testing::Pointee;

using testing::IsOk;
using testing::IsOkAndHolds;
using testing::EqualsProto;

class ProtoDataStoreTest : public testing::TestFileFixture {};

TEST_F(ProtoDataStoreTest, SimpleReadWriteTest) {
  FileStorage storage;
  TestProto testproto;
  testproto.set_string_value("SimpleReadWriteTest");
  std::string testfile = TestFile("SimpleReadWriteTest");
  ProtoDataStore<TestProto> pds(storage, testfile);

  ASSERT_OK(pds.Write(absl::make_unique<TestProto>(testproto)));
  EXPECT_THAT(pds.Read(), IsOkAndHolds(Pointee(EqualsProto(testproto))));
  // Multiple reads work.
  EXPECT_THAT(pds.Read(), IsOkAndHolds(Pointee(EqualsProto(testproto))));
  EXPECT_THAT(pds.Read(), IsOkAndHolds(Pointee(EqualsProto(testproto))));
}

TEST_F(ProtoDataStoreTest, DataPersistsAcrossMultipleInstancesTest) {
  FileStorage storage;
  TestProto testproto;
  testproto.set_string_value("DataPersistsAcrossMultipleInstancesTest");
  std::string testfile = TestFile("DataPersistsAcrossMultipleInstancesTest");
  {
    ProtoDataStore<TestProto> pds(storage, testfile);
    EXPECT_THAT(pds.Read(), Not(IsOk()));  // Nothing to read.
    ASSERT_OK(
        pds.Write(absl::make_unique<TestProto>(testproto)));
    EXPECT_THAT(pds.Read(),
                IsOkAndHolds(Pointee(EqualsProto(testproto))));
  }
  {
    // Different instance of ProtoDataStore.
    ProtoDataStore<TestProto> pds(storage, testfile);
    EXPECT_THAT(pds.Read(),
                IsOkAndHolds(Pointee(EqualsProto(testproto))));
  }
}

TEST_F(ProtoDataStoreTest, MultipleUpdatesToProtoTest) {
  FileStorage storage;
  std::string testfile = TestFile("MultipleUpdatesToProtoTest");
  ProtoDataStore<TestProto> pds(storage, testfile);
  for (int i = 0; i < 10; i++) {
    TestProto testproto;
    testproto.set_string_value("MultipleUpdatesToProtoTest");
    testproto.set_int_value(i);
    ASSERT_OK(pds.Write(absl::make_unique<TestProto>(testproto)));
    EXPECT_THAT(pds.Read(),
                IsOkAndHolds(Pointee(EqualsProto(testproto))));
  }
}

TEST_F(ProtoDataStoreTest, InvalidtestfileTest) {
  FileStorage storage;
  TestProto testproto;
  testproto.set_string_value("InvalidtestfileTest");
  std::string invalid_testfile = "";
  ProtoDataStore<TestProto> pds(storage, invalid_testfile);
  EXPECT_THAT(pds.Read(), Not(IsOk()));
  EXPECT_THAT(pds.Write(absl::make_unique<TestProto>(testproto)),
              Not(IsOk()));
}

TEST_F(ProtoDataStoreTest, FileCorruptionTest) {
  FileStorage storage;
  std::string testfile = TestFile("FileCorruptionTest");
  TestProto testproto;
  testproto.set_string_value("FileCorruptionTest");
  {
    ProtoDataStore<TestProto> pds(storage, testfile);
    ASSERT_OK(pds.Write(absl::make_unique<TestProto>(testproto)));
  }
  {
    auto out = storage.OpenForWrite(testfile);
    ASSERT_THAT(out, IsOk());
    ASSERT_OK((*out)->Append("junk"));
  }
  {
    ProtoDataStore<TestProto> pds(storage, testfile);
    EXPECT_THAT(pds.Read(), Not(IsOk()));
  }
}

}  // namespace
}  // namespace protostore
