// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// URL filename encoder goals:
//
// 1. Allow URLs with arbitrary path-segment length, generating filenames
//    with a maximum of 128 characters.
// 2. Provide a somewhat human readable filenames, for easy debugging flow.
// 3. Provide reverse-mapping from filenames back to URLs.
// 4. Be able to distinguish http://x from http://x/ from http://x/index.html.
//    Those can all be different URLs.
// 5. Be able to represent http://a/b/c and http://a/b/c/d, a pattern seen
//    with Facebook Connect.
//
// We need an escape-character for representing characters that are legal
// in URL paths, but not in filenames, such as '?'.  Illegal characters
// in Windows are <>:"/\|?*.  For reference, see
//   http://msdn.microsoft.com/en-us/library/aa365247(VS.85).aspx
//
// We can pick any legal character as an escape, as long as we escape it too.
// But as we have a goal of having filenames that humans can correlate with
// URLs, we should pick one that doesn't show up frequently in URLs. Candidates
// are ~`!@#$%^&()-=_+{}[],. but we would prefer to avoid characters that are
// shell escapes, and characters that occur frequently in URLs.
//
// .#&%-=_+ occur frequently in URLs.
// ~`!$^&(){}[] are special to Unix shells
//
// @ might seem like a reasonble option, but some build tools don't appreciate
// filenames with @ in testdata.  Perforce does not appreciate # in a filename.
//
// Though a web-site http://www.vias.org/linux-knowhow/lnag_05_05_09.html
// identifies ^ as a special shell character, it did not appear to be an
// issue to use it unquoted as a filename in bash or tcsh.
//
// Here are some frequencies of some special characters in a data set from Fall
// '09.  We find only 3 occurences of "x5E" (^ is ascii 0x53):
//   ^   3               build tools don't like ^ in testdata filenames
//   @   10              build tools don't like @ in testdata filenames
//   .   1676            too frequent in URLs
//   ,   76              THE WINNER
//   #   0               build tools doesn't like it
//   &   487             Prefer to avoid shell escapes
//   %   374             g4 doesn't like it
//   =   579             very frequent in URLs -- leave unmodified
//   -   464             very frequent in URLs -- leave unmodified
//   _   798             very frequent in URLs -- leave unmodified
//
// It is interesting that there were no slurped URLs with #, but I suspect this
// might be due to the slurping methdology.  So let's stick with the relatively
// rare ','.
//
// Here's the escaping methodology:
//
//     URL               File
//     /                 /,
//     /.                /.,
//     //                /,/,
//     /./               /,./,
//     /../              /,../,
//     /,                /,2C,
//     /,/               /,2C/,
//     /a/b              /a/b,     (, at the end of a name indicates a leaf).
//     /a/b/             /a/b/,
//
// path segments greater than 128 characters (after escape expansion) are
// suffixed with ,- so we can know that the next "/" is not part of the URL:
//
//    /verylongname/    /verylong,-/name

// NOTE: we avoid using some classes here (like FilePath and GURL) because we
//       share this code with other projects externally.

#ifndef NET_TOOLS_DUMP_CACHE_URL_TO_FILE_ENCODER_H_
#define NET_TOOLS_DUMP_CACHE_URL_TO_FILE_ENCODER_H_

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "net/tools/dump_cache/url_utilities.h"

namespace net {

// Helper class for converting a URL into a filename.
class UrlToFilenameEncoder {
 public:
  // Given a |url| and a |base_path|, returns a string which represents this
  // |url|.
  // |legacy_escape| indicates that this function should use the old-style
  // of encoding.
  // TODO(mbelshe): delete the legacy_escape code.
  static std::string Encode(const std::string& url, std::string base_path,
                            bool legacy_escape) {
    std::string clean_url(url);
    if (clean_url.length() && clean_url[clean_url.length()-1] == '/')
      clean_url.append("index.html");

    std::string host = UrlUtilities::GetUrlHost(clean_url);
    std::string filename(base_path);
    filename.append("\\");
    filename = filename.append(host);
    filename.append("\\");

    std::string url_filename = UrlUtilities::GetUrlPath(clean_url);
    // Strip the leading '/'
    if (url_filename[0] == '/')
      url_filename = url_filename.substr(1);

    // replace '/' with '\'
    ConvertToSlashes(&url_filename);

    // strip double slashes ("\\")
    StripDoubleSlashes(&url_filename);

    // Save path as filesystem-safe characters
    if (legacy_escape) {
      url_filename = LegacyEscape(url_filename);
    } else {
      url_filename = Escape(url_filename);
    }
    filename = filename.append(url_filename);

#ifndef WIN32
    // Last step - convert to native slashes!
    const std::string slash("/");
    const std::string backslash("\\");
    ReplaceAll(&filename, backslash, slash);
#endif

    return filename;
  }

  // Rewrite HTML in a form that the SPDY in-memory server
  // can read.
  // |filename_prefix| is prepended without escaping.
  // |filename_ending| is the URL to be encoded into a filename.
  // |dir_separator| is "/" on Unix, "\" on Windows.
  // |encoded_filename| is the resultant filename.
  static void EncodeSegment(
      const std::string& filename_prefix,
      const std::string& filename_ending,
      char dir_separator,
      std::string* encoded_filename);

  // Decodes a filename that was encoded with EncodeSegment,
  // yielding back the original URL.
  static bool Decode(const std::string& encoded_filename,
                     char dir_separator,
                     std::string* decoded_url);

 private:
  // Appends a segment of the path, special-casing ".", "..", and "", and
  // ensuring that the segment does not exceed the path length.  If it does,
  // it chops the end off the segment, writes the segment with a separator of
  // ",-/", and then rewrites segment to contain just the truncated piece so
  // it can be used in the next iteration.
  // |dir_separator| is "/" on Unix, "\" on Windows.
  // |segment| is a read/write parameter containing segment to write
  static void AppendSegment(
      char dir_separator,
      std::string* segment,
      std::string* dest);

  // Escapes the given input |path| and chop any individual components
  // of the path which are greater than kMaximumSubdirectoryLength characters
  // into two chunks.
  static std::string Escape(const std::string& path) {
    std::string output;
    EncodeSegment("", path, '\\', &output);
    return output;
  }

  // Allow reading of old slurped files.
  static std::string LegacyEscape(const std::string& path);

  // Replace all instances of |from| within |str| as |to|.
  static void ReplaceAll(std::string* str, const std::string& from,
                  const std::string& to) {
    std::string::size_type pos(0);
    while ((pos = str->find(from, pos)) != std::string::npos) {
      str->replace(pos, from.size(), to);
      pos += from.size();
    }
  }

  // Replace all instances of "/" with "\" in |path|.
  static void ConvertToSlashes(std::string* path) {
    const std::string slash("/");
    const std::string backslash("\\");
    ReplaceAll(path, slash, backslash);
  }

  // Replace all instances of "\\" with "%5C%5C" in |path|.
  static void StripDoubleSlashes(std::string* path) {
    const std::string doubleslash("\\\\");
    const std::string escaped_doubleslash("%5C%5C");
    ReplaceAll(path, doubleslash, escaped_doubleslash);
  }
};

}  // namespace net

#endif  // NET_TOOLS_DUMP_CACHE_URL_TO_FILE_ENCODER_H_

