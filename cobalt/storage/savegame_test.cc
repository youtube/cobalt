// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/storage/savegame.h"

#include "testing/gtest/include/gtest/gtest.h"

static size_t kMaxSaveGameSizeBytes = 4 * 1024 * 1024;

namespace cobalt {
namespace storage {

namespace {
scoped_ptr<Savegame> CreateSavegame() {
  Savegame::Options options;
  FilePath test_path;
  CHECK(PathService::Get(paths::DIR_COBALT_TEST_OUT, &test_path));
  options.path_override = test_path.Append("test.db").value();
  scoped_ptr<Savegame> savegame = Savegame::Create(options);
  return savegame.Pass();
}

}  // namespace

TEST(Savegame, Basic) {
  scoped_ptr<Savegame> savegame = CreateSavegame();

  Savegame::ByteVector buf;
  for (int i = 0; i < 1024; ++i) {
    buf.push_back(static_cast<uint8>(i % 256));
  }
  EXPECT_TRUE(savegame->Write(buf));

  Savegame::ByteVector buf2;
  EXPECT_TRUE(savegame->Read(&buf2, kMaxSaveGameSizeBytes));
  EXPECT_EQ(buf, buf2);

  EXPECT_TRUE(savegame->Delete());
}

TEST(Savegame, MaxReadExceeded) {
  scoped_ptr<Savegame> savegame = CreateSavegame();

  Savegame::ByteVector buf;
  for (int i = 0; i < 1024; ++i) {
    buf.push_back(static_cast<uint8>(i % 256));
  }
  EXPECT_TRUE(savegame->Write(buf));

  Savegame::ByteVector buf2;
  EXPECT_FALSE(savegame->Read(&buf2, 1023));
  EXPECT_EQ(0, buf2.size());

  EXPECT_TRUE(savegame->Delete());
}

TEST(Savegame, DoubleRead) {
  // Verify that reading the save twice gives us the same data.
  scoped_ptr<Savegame> savegame = CreateSavegame();
  Savegame::ByteVector buf;
  for (int i = 0; i < 1024; ++i) {
    buf.push_back(static_cast<uint8>(i % 256));
  }

  EXPECT_TRUE(savegame->Write(buf));
  Savegame::ByteVector buf2;
  EXPECT_TRUE(savegame->Read(&buf2, kMaxSaveGameSizeBytes));
  EXPECT_EQ(buf, buf2);

  Savegame::ByteVector buf3;
  EXPECT_TRUE(savegame->Read(&buf3, kMaxSaveGameSizeBytes));
  EXPECT_EQ(buf2, buf3);
}

TEST(Savegame, ReadAfterDelete) {
  // Verify that reading after delete fails.
  scoped_ptr<Savegame> savegame = CreateSavegame();

  Savegame::ByteVector buf;
  for (int i = 0; i < 1024; ++i) {
    buf.push_back(static_cast<uint8>(i % 256));
  }

  EXPECT_TRUE(savegame->Write(buf));
  Savegame::ByteVector buf2;
  EXPECT_TRUE(savegame->Delete());
  EXPECT_FALSE(savegame->Read(&buf2, kMaxSaveGameSizeBytes));
}

TEST(Savegame, DoubleDelete) {
  scoped_ptr<Savegame> savegame = CreateSavegame();

  Savegame::ByteVector buf;
  for (int i = 0; i < 1024; ++i) {
    buf.push_back(static_cast<uint8>(i % 256));
  }

  EXPECT_TRUE(savegame->Write(buf));
  EXPECT_TRUE(savegame->Delete());
  EXPECT_FALSE(savegame->Delete());
}

}  // namespace storage
}  // namespace cobalt
