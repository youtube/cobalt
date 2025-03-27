// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_file_enumerator.h"

#include "base/path_service.h"
#include "base/files/file_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {
namespace {

base::FilePath GetRoot() {
  base::FilePath root;
#if defined(STARBOARD)
  base::PathService::Get(base::DIR_TEST_DATA, &root);
#else
  base::PathService::Get(base::DIR_SOURCE_ROOT, &root);
#endif
  return root.AppendASCII("net")
      .AppendASCII("data")
      .AppendASCII("cache_tests")
      .AppendASCII("simple_file_enumerator");
}

TEST(SimpleFileEnumeratorTest, Root) {
  const base::FilePath kRoot = GetRoot();
  SimpleFileEnumerator enumerator(kRoot);

  auto filepath = kRoot.AppendASCII("test.txt");

  std::string file_data;
  if (!base::ReadFileToString(filepath, &file_data)) {
    ADD_FAILURE() << "Couldn't read file: " << filepath.value();
  }

  auto entry = enumerator.Next();
  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(entry->path, filepath);
  EXPECT_EQ(entry->size, file_data.size());
  EXPECT_FALSE(enumerator.HasError());

  // No directories should be listed, no indirect descendants should be listed.
  EXPECT_EQ(absl::nullopt, enumerator.Next());
  EXPECT_FALSE(enumerator.HasError());

  // We can call enumerator.Next() after the iteration is done.
  EXPECT_EQ(absl::nullopt, enumerator.Next());
  EXPECT_FALSE(enumerator.HasError());
}

TEST(SimpleFileEnumeratorTest, NotFound) {
  const base::FilePath kRoot = GetRoot().AppendASCII("not-found");
  SimpleFileEnumerator enumerator(kRoot);

  auto entry = enumerator.Next();
  EXPECT_EQ(absl::nullopt, enumerator.Next());
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  EXPECT_TRUE(enumerator.HasError());
#endif
}

}  // namespace
}  // namespace disk_cache
