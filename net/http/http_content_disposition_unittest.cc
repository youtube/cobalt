// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_content_disposition.h"

#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

struct FileNameCDCase {
  const char* header;
  const char* referrer_charset;
  const wchar_t* expected;
};

}  // anonymous namespace

TEST(HttpContentDispositionTest, Filename) {
  const FileNameCDCase tests[] = {
    // Test various forms of C-D header fields emitted by web servers.
    {"inline; filename=\"abcde.pdf\"", "", L"abcde.pdf"},
    {"inline; name=\"abcde.pdf\"", "", L"abcde.pdf"},
    {"attachment; filename=abcde.pdf", "", L"abcde.pdf"},
    {"attachment; name=abcde.pdf", "", L"abcde.pdf"},
    {"attachment; filename=abc,de.pdf", "", L"abc,de.pdf"},
    {"filename=abcde.pdf", "", L"abcde.pdf"},
    {"filename= abcde.pdf", "", L"abcde.pdf"},
    {"filename =abcde.pdf", "", L"abcde.pdf"},
    {"filename = abcde.pdf", "", L"abcde.pdf"},
    {"filename\t=abcde.pdf", "", L"abcde.pdf"},
    {"filename \t\t  =abcde.pdf", "", L"abcde.pdf"},
    {"name=abcde.pdf", "", L"abcde.pdf"},
    {"inline; filename=\"abc%20de.pdf\"", "",
     L"abc de.pdf"},
    // Unbalanced quotation mark
    {"filename=\"abcdef.pdf", "", L"abcdef.pdf"},
    // Whitespaces are converted to a space.
    {"inline; filename=\"abc  \t\nde.pdf\"", "",
     L"abc    de.pdf"},
    // %-escaped UTF-8
    {"attachment; filename=\"%EC%98%88%EC%88%A0%20"
     "%EC%98%88%EC%88%A0.jpg\"", "", L"\xc608\xc220 \xc608\xc220.jpg"},
    {"attachment; filename=\"%F0%90%8C%B0%F0%90%8C%B1"
     "abc.jpg\"", "", L"\U00010330\U00010331abc.jpg"},
    {"attachment; filename=\"%EC%98%88%EC%88%A0 \n"
     "%EC%98%88%EC%88%A0.jpg\"", "", L"\xc608\xc220  \xc608\xc220.jpg"},
    // RFC 2047 with various charsets and Q/B encodings
    {"attachment; filename=\"=?EUC-JP?Q?=B7=DD=BD="
     "D13=2Epng?=\"", "", L"\x82b8\x8853" L"3.png"},
    {"attachment; filename==?eUc-Kr?b?v7m8+iAzLnBuZw==?=",
     "", L"\xc608\xc220 3.png"},
    {"attachment; filename==?utf-8?Q?=E8=8A=B8=E8"
     "=A1=93_3=2Epng?=", "", L"\x82b8\x8853 3.png"},
    {"attachment; filename==?utf-8?Q?=F0=90=8C=B0"
     "_3=2Epng?=", "", L"\U00010330 3.png"},
    {"inline; filename=\"=?iso88591?Q?caf=e9_=2epng?=\"",
     "", L"caf\x00e9 .png"},
    // Space after an encoded word should be removed.
    {"inline; filename=\"=?iso88591?Q?caf=E9_?= .png\"",
     "", L"caf\x00e9 .png"},
    // Two encoded words with different charsets (not very likely to be emitted
    // by web servers in the wild). Spaces between them are removed.
    {"inline; filename=\"=?euc-kr?b?v7m8+iAz?="
     " =?ksc5601?q?=BF=B9=BC=FA=2Epng?=\"", "",
     L"\xc608\xc220 3\xc608\xc220.png"},
    {"attachment; filename=\"=?windows-1252?Q?caf=E9?="
     "  =?iso-8859-7?b?4eI=?= .png\"", "", L"caf\x00e9\x03b1\x03b2.png"},
    // Non-ASCII string is passed through and treated as UTF-8 as long as
    // it's valid as UTF-8 and regardless of |referrer_charset|.
    {"attachment; filename=caf\xc3\xa9.png",
     "iso-8859-1", L"caf\x00e9.png"},
    {"attachment; filename=caf\xc3\xa9.png",
     "", L"caf\x00e9.png"},
    // Non-ASCII/Non-UTF-8 string. Fall back to the referrer charset.
    {"attachment; filename=caf\xe5.png",
     "windows-1253", L"caf\x03b5.png"},
#if 0
    // Non-ASCII/Non-UTF-8 string. Fall back to the native codepage.
    // TODO(jungshik): We need to set the OS default codepage
    // to a specific value before testing. On Windows, we can use
    // SetThreadLocale().
    {"attachment; filename=\xb0\xa1\xb0\xa2.png",
     "", L"\xac00\xac01.png"},
#endif
    // Failure cases
    // Invalid hex-digit "G"
    {"attachment; filename==?iiso88591?Q?caf=EG?=", "",
     L""},
    // Incomplete RFC 2047 encoded-word (missing '='' at the end)
    {"attachment; filename==?iso88591?Q?caf=E3?", "", L""},
    // Extra character at the end of an encoded word
    {"attachment; filename==?iso88591?Q?caf=E3?==",
     "", L""},
    // Extra token at the end of an encoded word
    {"attachment; filename==?iso88591?Q?caf=E3?=?",
     "", L""},
    {"attachment; filename==?iso88591?Q?caf=E3?=?=",
     "",  L""},
    // Incomplete hex-escaped chars
    {"attachment; filename==?windows-1252?Q?=63=61=E?=",
     "", L""},
    {"attachment; filename=%EC%98%88%EC%88%A", "", L""},
    // %-escaped non-UTF-8 encoding is an "error"
    {"attachment; filename=%B7%DD%BD%D1.png", "", L""},
    // Two RFC 2047 encoded words in a row without a space is an error.
    {"attachment; filename==?windows-1252?Q?caf=E3?="
     "=?iso-8859-7?b?4eIucG5nCg==?=", "", L""},

    // RFC 5987 tests with Filename*  : see http://tools.ietf.org/html/rfc5987
    {"attachment; filename*=foo.html", "", L""},
    {"attachment; filename*=foo'.html", "", L""},
    {"attachment; filename*=''foo'.html", "", L""},
    {"attachment; filename*=''foo.html'", "", L""},
    {"attachment; filename*=''f\"oo\".html'", "", L""},
    {"attachment; filename*=bogus_charset''foo.html'",
     "", L""},
    {"attachment; filename*='en'foo.html'", "", L""},
    {"attachment; filename*=iso-8859-1'en'foo.html", "",
      L"foo.html"},
    {"attachment; filename*=utf-8'en'foo.html", "",
      L"foo.html"},
    // charset cannot be omitted.
    {"attachment; filename*='es'f\xfa.html'", "", L""},
    // Non-ASCII bytes are not allowed.
    {"attachment; filename*=iso-8859-1'es'f\xfa.html", "",
      L""},
    {"attachment; filename*=utf-8'es'f\xce\xba.html", "",
      L""},
    // TODO(jshin): Space should be %-encoded, but currently, we allow
    // spaces.
    {"inline; filename*=iso88591''cafe foo.png", "",
      L"cafe foo.png"},

    // Filename* tests converted from Q-encoded tests above.
    {"attachment; filename*=EUC-JP''%B7%DD%BD%D13%2Epng",
     "", L"\x82b8\x8853" L"3.png"},
    {"attachment; filename*=utf-8''"
      "%E8%8A%B8%E8%A1%93%203%2Epng", "", L"\x82b8\x8853 3.png"},
    {"attachment; filename*=utf-8''%F0%90%8C%B0 3.png", "",
      L"\U00010330 3.png"},
    {"inline; filename*=Euc-Kr'ko'%BF%B9%BC%FA%2Epng", "",
     L"\xc608\xc220.png"},
    {"attachment; filename*=windows-1252''caf%E9.png", "",
      L"caf\x00e9.png"},

    // http://greenbytes.de/tech/tc2231/ filename* test cases.
    // attwithisofn2231iso
    {"attachment; filename*=iso-8859-1''foo-%E4.html", "",
      L"foo-\xe4.html"},
    // attwithfn2231utf8
    {"attachment; filename*="
      "UTF-8''foo-%c3%a4-%e2%82%ac.html", "", L"foo-\xe4-\x20ac.html"},
    // attwithfn2231noc : no encoding specified but UTF-8 is used.
    {"attachment; filename*=''foo-%c3%a4-%e2%82%ac.html",
      "", L""},
    // attwithfn2231utf8comp
    {"attachment; filename*=UTF-8''foo-a%cc%88.html", "",
      L"foo-\xe4.html"},
#ifdef ICU_SHOULD_FAIL_CONVERSION_ON_INVALID_CHARACTER
    // This does not work because we treat ISO-8859-1 synonymous with
    // Windows-1252 per HTML5. For HTTP, in theory, we're not
    // supposed to.
    // attwithfn2231utf8-bad
    {"attachment; filename*="
      "iso-8859-1''foo-%c3%a4-%e2%82%ac.html", "", L""},
#endif
    // attwithfn2231ws1
    {"attachment; filename *=UTF-8''foo-%c3%a4.html", "",
      L""},
    // attwithfn2231ws2
    {"attachment; filename*= UTF-8''foo-%c3%a4.html", "",
      L"foo-\xe4.html"},
    // attwithfn2231ws3
    {"attachment; filename* =UTF-8''foo-%c3%a4.html", "",
      L"foo-\xe4.html"},
    // attwithfn2231quot
    {"attachment; filename*=\"UTF-8''foo-%c3%a4.html\"",
      "", L""},
    // attfnboth
    {"attachment; filename=\"foo-ae.html\"; "
      "filename*=UTF-8''foo-%c3%a4.html", "", L"foo-\xe4.html"},
    // attfnboth2
    {"attachment; filename*=UTF-8''foo-%c3%a4.html; "
      "filename=\"foo-ae.html\"", "", L"foo-\xe4.html"},
    // attnewandfn
    {"attachment; foobar=x; filename=\"foo.html\"", "",
      L"foo.html"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    HttpContentDisposition header(tests[i].header, tests[i].referrer_charset);
    EXPECT_EQ(tests[i].expected,
        UTF8ToWide(header.filename()))
        << "Failed on input: " << tests[i].header;
  }
}

}  // namespace net
