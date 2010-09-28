// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"
#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "net/test/python_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PythonUtils, Append) {
  const FilePath::CharType kAppendDir1[] =
      FILE_PATH_LITERAL("test/path_append1");
  const FilePath::CharType kAppendDir2[] =
      FILE_PATH_LITERAL("test/path_append2");

  scoped_ptr<base::Environment> env(base::Environment::Create());

  std::string python_path;
  FilePath append_path1(kAppendDir1);
  FilePath append_path2(kAppendDir2);

  // Get a clean start
  env->UnSetVar(kPythonPathEnv);

  // Append the path
  AppendToPythonPath(append_path1);
  env->GetVar(kPythonPathEnv, &python_path);
  ASSERT_EQ(python_path, "test/path_append1");

  // Append the safe path again, nothing changes
  AppendToPythonPath(append_path2);
  env->GetVar(kPythonPathEnv, &python_path);
#if defined(OS_WIN)
  ASSERT_EQ(std::string("test/path_append1;test/path_append2"), python_path);
#elif defined(OS_POSIX)
  ASSERT_EQ(std::string("test/path_append1:test/path_append2"), python_path);
#endif
}

TEST(PythonUtils, PythonRunTime) {
  FilePath dir;
  EXPECT_TRUE(GetPythonRunTime(&dir));
#if defined(OS_WIN)
  EXPECT_NE(std::wstring::npos,
            dir.value().find(L"\\third_party\\python_24\\python.exe"));
#elif defined(OS_POSIX)
  EXPECT_NE(std::string::npos, dir.value().find("python"));
#endif
}

