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

#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/storage/savegame.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace storage {

TEST(Savegame, Basic) {
  Savegame::Options options;
  FilePath test_path;
  CHECK(PathService::Get(paths::DIR_COBALT_TEST_OUT, &test_path));
  options.path_override = test_path.Append("test.db").value();

  scoped_ptr<Savegame> savegame = Savegame::Create(options);

  Savegame::ByteVector buf;
  for (int i = 0; i < 1024; ++i) {
    buf.push_back(static_cast<uint8>(i % 256));
  }
  bool ok = savegame->Write(buf);
  EXPECT_EQ(true, ok);

  Savegame::ByteVector buf2;
  ok = savegame->Read(&buf2);
  EXPECT_EQ(true, ok);
  EXPECT_EQ(buf, buf2);

  ok = savegame->Delete();
}

}  // namespace storage
}  // namespace cobalt
