// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_util.h"

#include <vector>

#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"

// For examples of Unix<->VMS path conversions, see the unit test file. On VMS
// a path looks differently depending on whether it's a file or directory.

namespace net {

// static
std::string FtpUtil::UnixFilePathToVMS(const std::string& unix_path) {
  if (unix_path.empty())
    return std::string();

  StringTokenizer tokenizer(unix_path, "/");
  std::vector<std::string> tokens;
  while (tokenizer.GetNext())
    tokens.push_back(tokenizer.token());

  if (unix_path[0] == '/') {
    // It's an absolute path.

    if (tokens.empty()) {
      DCHECK_EQ(1U, unix_path.length());
      return "[]";
    }

    if (tokens.size() == 1)
      return unix_path.substr(1);  // Drop the leading slash.

    std::string result(tokens[0] + ":[");
    if (tokens.size() == 2) {
      // Don't ask why, it just works that way on VMS.
      result.append("000000");
    } else {
      result.append(tokens[1]);
      for (size_t i = 2; i < tokens.size() - 1; i++)
        result.append("." + tokens[i]);
    }
    result.append("]" + tokens[tokens.size() - 1]);
    return result;
  }

  if (tokens.size() == 1)
    return unix_path;

  std::string result("[");
  for (size_t i = 0; i < tokens.size() - 1; i++)
    result.append("." + tokens[i]);
  result.append("]" + tokens[tokens.size() - 1]);
  return result;
}

// static
std::string FtpUtil::UnixDirectoryPathToVMS(const std::string& unix_path) {
  if (unix_path.empty())
    return std::string();

  std::string path(unix_path);

  if (path[path.length() - 1] != '/')
    path.append("/");

  // Reuse logic from UnixFilePathToVMS by appending a fake file name to the
  // real path and removing it after conversion.
  path.append("x");
  path = UnixFilePathToVMS(path);
  return path.substr(0, path.length() - 1);
}

// static
std::string FtpUtil::VMSPathToUnix(const std::string& vms_path) {
  if (vms_path.empty())
    return ".";

  if (vms_path == "[]")
    return "/";

  std::string result(vms_path);
  if (vms_path[0] == '[') {
    // It's a relative path.
    ReplaceFirstSubstringAfterOffset(&result, 0, "[.", "");
  } else {
    // It's an absolute path.
    result.insert(0, "/");
    ReplaceSubstringsAfterOffset(&result, 0, ":[000000]", "/");
    ReplaceSubstringsAfterOffset(&result, 0, ":[", "/");
  }
  std::replace(result.begin(), result.end(), '.', '/');
  std::replace(result.begin(), result.end(), ']', '/');

  // Make sure the result doesn't end with a slash.
  if (result[result.length() - 1] == '/')
    result = result.substr(0, result.length() - 1);

  return result;
}

}  // namespace
