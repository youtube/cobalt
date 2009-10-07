// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_DUMP_CACHE_URL_TO_FILE_ENCODER_H_
#define NET_TOOLS_DUMP_CACHE_URL_TO_FILE_ENCODER_H_

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"

namespace net {

// Helper class for converting a URL into a filename.
class UrlToFilenameEncoder {
 public:
  // Given a |url| and a |base_path|, returns a FilePath which represents this
  // |url|.
  static FilePath Encode(const std::string& url, FilePath base_path) {
    std::string clean_url(url);
    if (clean_url.length() && clean_url[clean_url.length()-1] == '/')
      clean_url.append("index.html");

    GURL gurl(clean_url);
    FilePath filename(base_path);
    filename = filename.AppendASCII(gurl.host());

    std::string url_filename = gurl.PathForRequest();
    // Strip the leading '/'
    if (url_filename[0] == '/')
      url_filename = url_filename.substr(1);

    // replace '/' with '\'
    ConvertToSlashes(&url_filename);

    // strip double slashes ("\\")
    StripDoubleSlashes(&url_filename);

    // Save path as filesystem-safe characters
    url_filename = Escape(url_filename);
    filename = filename.AppendASCII(url_filename);

    return filename;
  }

 private:
  // This is the length at which we chop individual subdirectories.
  // Technically, we shouldn't need to do this, but I found that
  // even with long-filename support, windows had trouble creating
  // long subdirectories, and making them shorter helps.
  static const size_t kMaximumSubdirectoryLength = 128;

  // Escape the given input |path| and chop any individual components
  // of the path which are greater than kMaximumSubdirectoryLength characters
  // into two chunks.
  static std::string Escape(const std::string& path) {
    std::string output;
    int last_slash = 0;
    for (size_t index = 0; index < path.length(); index++) {
      char ch = path[index];
      if (ch == 0x5C)
        last_slash = index;
      if ((ch == 0x2D) ||                   // hyphen
          (ch == 0x5C) || (ch == 0x5F) ||   // backslash, underscore
          ((0x30 <= ch) && (ch <= 0x39)) || // Digits [0-9]
          ((0x41 <= ch) && (ch <= 0x5A)) || // Uppercase [A-Z]
          ((0x61 <= ch) && (ch <= 0x7A))) { // Lowercase [a-z]
        output.append(&path[index],1);
      } else {
        char encoded[3];
        encoded[0] = 'x';
        encoded[1] = ch / 16;
        encoded[1] += (encoded[1] >= 10) ? 'A' - 10 : '0';
        encoded[2] = ch % 16;
        encoded[2] += (encoded[2] >= 10) ? 'A' - 10 : '0';
        output.append(encoded, 3);
      }
      if (index - last_slash > kMaximumSubdirectoryLength) {
        char backslash = '\\';
        output.append(&backslash, 1);
        last_slash = index;
      }
    }
    return output;
  }

  // Replace all instances of |from| within |str| as |to|.
  static void ReplaceAll(const std::string& from,
                         const std::string& to,
                         std::string* str) {
    std::string::size_type pos(0);
    while((pos = str->find(from, pos)) != std::string::npos) {
      str->replace(pos, from.size(), to);
      pos += from.size();
    }
  }

  // Replace all instances of "/" with "\" in |path|.
  static void ConvertToSlashes(std::string* path) {
    static const char slash[] = { '/', '\0' };
    static const char backslash[] = { '\\', '\0' };
    ReplaceAll(slash, backslash, path);
  }

  // Replace all instances of "\\" with "%5C%5C" in |path|.
  static void StripDoubleSlashes(std::string* path) {
    static const char doubleslash[] = { '\\', '\\', '\0' };
    static const char escaped_doubleslash[] =
      { '%', '5', 'C', '%', '5', 'C','\0' };
    ReplaceAll(doubleslash, escaped_doubleslash, path);
  }
};

} // namespace net

#endif  // NET_TOOLS_DUMP_CACHE_URL_TO_FILE_ENCODER_H__

