// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/file_path.h"
#include "base/scoped_temp_dir.h"
#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/platform_test.h"

namespace {

// Returns true if PathService::Get returns true and sets the path parameter
// to non-empty for the given PathService::DirType enumeration value.
bool ReturnsValidPath(int dir_type) {
  FilePath path;
  bool result = PathService::Get(dir_type, &path);
#if defined(OS_POSIX)
  // If chromium has never been started on this account, the cache path may not
  // exist.
  if (dir_type == base::DIR_CACHE)
    return result && !path.value().empty();
#endif
  return result && !path.value().empty() && file_util::PathExists(path);
}

#if defined(OS_WIN)
// Function to test DIR_LOCAL_APP_DATA_LOW on Windows XP. Make sure it fails.
bool ReturnsInvalidPath(int dir_type) {
  FilePath path;
  bool result = PathService::Get(base::DIR_LOCAL_APP_DATA_LOW, &path);
  return !result && path.empty();
}
#endif

}  // namespace

// On the Mac this winds up using some autoreleased objects, so we need to
// be a PlatformTest.
typedef PlatformTest PathServiceTest;

// Test that all PathService::Get calls return a value and a true result
// in the development environment.  (This test was created because a few
// later changes to Get broke the semantics of the function and yielded the
// correct value while returning false.)
TEST_F(PathServiceTest, Get) {
  for (int key = base::DIR_CURRENT; key < base::PATH_END; ++key) {
#if defined(OS_ANDROID)
    if (key == base::FILE_MODULE)
      continue;  // Android doesn't implement FILE_MODULE;
#endif
    EXPECT_PRED1(ReturnsValidPath, key);
  }
#if defined(OS_WIN)
  for (int key = base::PATH_WIN_START + 1; key < base::PATH_WIN_END; ++key) {
    if (key == base::DIR_LOCAL_APP_DATA_LOW &&
        base::win::GetVersion() < base::win::VERSION_VISTA) {
      // DIR_LOCAL_APP_DATA_LOW is not supported prior Vista and is expected to
      // fail.
      EXPECT_TRUE(ReturnsInvalidPath(key)) << key;
    } else {
      EXPECT_TRUE(ReturnsValidPath(key)) << key;
    }
  }
#elif defined(OS_MACOSX)
  for (int key = base::PATH_MAC_START + 1; key < base::PATH_MAC_END; ++key) {
      EXPECT_PRED1(ReturnsValidPath, key);
  }
#endif
}

// test that all versions of the Override function of PathService do what they
// are supposed to do.
TEST_F(PathServiceTest, Override) {
  int my_special_key = 666;
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath fake_cache_dir(temp_dir.path().AppendASCII("cache"));
  // PathService::Override should always create the path provided if it doesn't
  // exist.
  EXPECT_TRUE(PathService::Override(my_special_key, fake_cache_dir));
  EXPECT_TRUE(file_util::PathExists(fake_cache_dir));

  FilePath fake_cache_dir2(temp_dir.path().AppendASCII("cache2"));
  // PathService::OverrideAndCreateIfNeeded should obey the |create| parameter.
  PathService::OverrideAndCreateIfNeeded(my_special_key,
                                         fake_cache_dir2,
                                         false);
  EXPECT_FALSE(file_util::PathExists(fake_cache_dir2));
  EXPECT_TRUE(PathService::OverrideAndCreateIfNeeded(my_special_key,
                                                     fake_cache_dir2,
                                                     true));
  EXPECT_TRUE(file_util::PathExists(fake_cache_dir2));
}
