/*
 * Copyright 2020 The Chromium Authors. All Rights Reserved.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "base/strings/utf_string_conversions.h"
#include "gtest-utils.h"

#if defined(OS_WIN)
std::wstring GetTargetDirectory() {
  base::FilePath path;
  base::PathService::Get(base::DIR_CURRENT, &path);
  return base::UTF8ToWide(path.MaybeAsASCII());
}
#else
std::string GetTargetDirectory() {
#if defined(ANDROID)
  return "/sdcard";
#else
  base::FilePath path;
  base::PathService::Get(base::DIR_CURRENT, &path);
  return path.MaybeAsASCII();
#endif
}
#endif

void GetTestFilePath(base::FilePath* path, const std::string filename) {
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, path));
  *path = path->AppendASCII("third_party");
  *path = path->AppendASCII("libjpeg_turbo");
  *path = path->AppendASCII("testimages");
  *path = path->AppendASCII(filename);
  ASSERT_TRUE(base::PathExists(*path));
}

bool CompareFileAndMD5(const base::FilePath& path,
                       const std::string expected_md5) {
  // Read the output file and compute the MD5.
  std::string output;
  if (!base::ReadFileToString(path, &output)) {
    return false;
  }
  const std::string md5 = base::MD5String(output);
  return expected_md5 == md5;
}
