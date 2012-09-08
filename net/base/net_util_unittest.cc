// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include <string.h>

#include <algorithm>

#include "base/file_path.h"
#include "base/format_macros.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sys_byteorder.h"
#include "base/sys_string_conversions.h"
#include "base/test/test_file_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

static const size_t kNpos = string16::npos;

struct FileCase {
  const wchar_t* file;
  const char* url;
};

struct HeaderCase {
  const char* header_name;
  const char* expected;
};

struct HeaderParamCase {
  const char* header_name;
  const char* param_name;
  const char* expected;
};

struct FileNameCDCase {
  const char* header_field;
  const char* referrer_charset;
  const wchar_t* expected;
};

const char* kLanguages[] = {
  "",      "en",    "zh-CN",    "ja",    "ko",
  "he",    "ar",    "ru",       "el",    "fr",
  "de",    "pt",    "sv",       "th",    "hi",
  "de,en", "el,en", "zh-TW,en", "ko,ja", "he,ru,en",
  "zh,ru,en"
};

struct IDNTestCase {
  const char* input;
  const wchar_t* unicode_output;
  const bool unicode_allowed[arraysize(kLanguages)];
};

// TODO(jungshik) This is just a random sample of languages and is far
// from exhaustive.  We may have to generate all the combinations
// of languages (powerset of a set of all the languages).
const IDNTestCase idn_cases[] = {
  // No IDN
  {"www.google.com", L"www.google.com",
   {true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true}},
  {"www.google.com.", L"www.google.com.",
   {true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true}},
  {".", L".",
   {true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true}},
  {"", L"",
   {true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true}},
  // IDN
  // Hanzi (Traditional Chinese)
  {"xn--1lq90ic7f1rc.cn", L"\x5317\x4eac\x5927\x5b78.cn",
   {true,  false, true,  true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, true,  true,  false,
    true}},
  // Hanzi ('video' in Simplified Chinese : will pass only in zh-CN,zh)
  {"xn--cy2a840a.com", L"\x89c6\x9891.com",
   {true,  false, true,  false,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false,  false,
    true}},
  // Hanzi + '123'
  {"www.xn--123-p18d.com", L"www.\x4e00" L"123.com",
   {true,  false, true,  true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, true,  true,  false,
    true}},
  // Hanzi + Latin : U+56FD is simplified and is regarded
  // as not supported in zh-TW.
  {"www.xn--hello-9n1hm04c.com", L"www.hello\x4e2d\x56fd.com",
   {false, false, true,  true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true,  false,
    true}},
  // Kanji + Kana (Japanese)
  {"xn--l8jvb1ey91xtjb.jp", L"\x671d\x65e5\x3042\x3055\x3072.jp",
   {true,  false, false, true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true,  false,
    false}},
  // Katakana including U+30FC
  {"xn--tckm4i2e.jp", L"\x30b3\x30de\x30fc\x30b9.jp",
   {true, false, false, true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true, false,
    }},
  {"xn--3ck7a7g.jp", L"\u30ce\u30f3\u30bd.jp",
   {true, false, false, true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true, false,
    }},
  // Katakana + Latin (Japanese)
  // TODO(jungshik): Change 'false' in the first element to 'true'
  // after upgrading to ICU 4.2.1 to use new uspoof_* APIs instead
  // of our IsIDNComponentInSingleScript().
  {"xn--e-efusa1mzf.jp", L"e\x30b3\x30de\x30fc\x30b9.jp",
   {false, false, false, true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true, false,
    }},
  {"xn--3bkxe.jp", L"\x30c8\x309a.jp",
   {false, false, false, true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true, false,
    }},
  // Hangul (Korean)
  {"www.xn--or3b17p6jjc.kr", L"www.\xc804\xc790\xc815\xbd80.kr",
   {true,  false, false, false, true,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true,  false,
    false}},
  // b<u-umlaut>cher (German)
  {"xn--bcher-kva.de", L"b\x00fc" L"cher.de",
   {true,  false, false, false, false,
    false, false, false, false, true,
    true,  false,  false, false, false,
    true,  false, false, false, false,
    false}},
  // a with diaeresis
  {"www.xn--frgbolaget-q5a.se", L"www.f\x00e4rgbolaget.se",
   {true,  false, false, false, false,
    false, false, false, false, false,
    true,  false, true, false, false,
    true,  false, false, false, false,
    false}},
  // c-cedilla (French)
  {"www.xn--alliancefranaise-npb.fr", L"www.alliancefran\x00e7" L"aise.fr",
   {true,  false, false, false, false,
    false, false, false, false, true,
    false, true,  false, false, false,
    false, false, false, false, false,
    false}},
  // caf'e with acute accent' (French)
  {"xn--caf-dma.fr", L"caf\x00e9.fr",
   {true,  false, false, false, false,
    false, false, false, false, true,
    false, true,  true,  false, false,
    false, false, false, false, false,
    false}},
  // c-cedillla and a with tilde (Portuguese)
  {"xn--poema-9qae5a.com.br", L"p\x00e3oema\x00e7\x00e3.com.br",
   {true,  false, false, false, false,
    false, false, false, false, false,
    false, true,  false, false, false,
    false, false, false, false, false,
    false}},
  // s with caron
  {"xn--achy-f6a.com", L"\x0161" L"achy.com",
   {true,  false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // TODO(jungshik) : Add examples with Cyrillic letters
  // only used in some languages written in Cyrillic.
  // Eutopia (Greek)
  {"xn--kxae4bafwg.gr", L"\x03bf\x03c5\x03c4\x03bf\x03c0\x03af\x03b1.gr",
   {true,  false, false, false, false,
    false, false, false, true,  false,
    false, false, false, false, false,
    false, true,  false, false, false,
    false}},
  // Eutopia + 123 (Greek)
  {"xn---123-pldm0haj2bk.gr",
   L"\x03bf\x03c5\x03c4\x03bf\x03c0\x03af\x03b1-123.gr",
   {true,  false, false, false, false,
    false, false, false, true,  false,
    false, false, false, false, false,
    false, true,  false, false, false,
    false}},
  // Cyrillic (Russian)
  {"xn--n1aeec9b.ru", L"\x0442\x043e\x0440\x0442\x044b.ru",
   {true,  false, false, false, false,
    false, false, true,  false, false,
    false, false, false, false, false,
    false, false, false, false, true,
    true}},
  // Cyrillic + 123 (Russian)
  {"xn---123-45dmmc5f.ru", L"\x0442\x043e\x0440\x0442\x044b-123.ru",
   {true,  false, false, false, false,
    false, false, true,  false, false,
    false, false, false, false, false,
    false, false, false, false, true,
    true}},
  // Arabic
  {"xn--mgba1fmg.ar", L"\x0627\x0641\x0644\x0627\x0645.ar",
   {true,  false, false, false, false,
    false, true,  false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // Hebrew
  {"xn--4dbib.he", L"\x05d5\x05d0\x05d4.he",
   {true,  false, false, false, false,
    true,  false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, true,
    false}},
  // Thai
  {"xn--12c2cc4ag3b4ccu.th",
   L"\x0e2a\x0e32\x0e22\x0e01\x0e32\x0e23\x0e1a\x0e34\x0e19.th",
   {true,  false, false, false, false,
    false, false, false, false, false,
    false, false, false, true,  false,
    false, false, false, false, false,
    false}},
  // Devangari (Hindi)
  {"www.xn--l1b6a9e1b7c.in", L"www.\x0905\x0915\x094b\x0932\x093e.in",
   {true,  false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, true,
    false, false, false, false, false,
    false}},
  // Invalid IDN
  {"xn--hello?world.com", NULL,
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // Unsafe IDNs
  // "payp<alpha>l.com"
  {"www.xn--paypl-g9d.com", L"payp\x03b1l.com",
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // google.gr with Greek omicron and epsilon
  {"xn--ggl-6xc1ca.gr", L"g\x03bf\x03bfgl\x03b5.gr",
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // google.ru with Cyrillic o
  {"xn--ggl-tdd6ba.ru", L"g\x043e\x043egl\x0435.ru",
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // h<e with acute>llo<China in Han>.cn
  {"xn--hllo-bpa7979ih5m.cn", L"h\x00e9llo\x4e2d\x56fd.cn",
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // <Greek rho><Cyrillic a><Cyrillic u>.ru
  {"xn--2xa6t2b.ru", L"\x03c1\x0430\x0443.ru",
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // One that's really long that will force a buffer realloc
  {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
       "aaaaaaa",
   L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
       L"aaaaaaaa",
   {true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true}},
  // Test cases for characters we blacklisted although allowed in IDN.
  // Embedded spaces will be turned to %20 in the display.
  // TODO(jungshik): We need to have more cases. This is a typical
  // data-driven trap. The following test cases need to be separated
  // and tested only for a couple of languages.
  {"xn--osd3820f24c.kr", L"\xac00\xb098\x115f.kr",
    {false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false}},
  {"www.xn--google-ho0coa.com", L"www.\x2039google\x203a.com",
    {false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
  }},
  {"google.xn--comabc-k8d", L"google.com\x0338" L"abc",
    {false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
  }},
  {"google.xn--com-oh4ba.evil.jp", L"google.com\x309a\x309a.evil.jp",
    {false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
  }},
  {"google.xn--comevil-v04f.jp", L"google.com\x30ce" L"evil.jp",
    {false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
  }},
#if 0
  // These two cases are special. We need a separate test.
  // U+3000 and U+3002 are normalized to ASCII space and dot.
  {"xn-- -kq6ay5z.cn", L"\x4e2d\x56fd\x3000.cn",
    {false, false, true,  false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, true,  false, false,
     true}},
  {"xn--fiqs8s.cn", L"\x4e2d\x56fd\x3002" L"cn",
    {false, false, true,  false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, true,  false, false,
     true}},
#endif
};

struct AdjustOffsetCase {
  size_t input_offset;
  size_t output_offset;
};

struct CompliantHostCase {
  const char* host;
  const char* desired_tld;
  bool expected_output;
};

struct GenerateFilenameCase {
  const char* url;
  const char* content_disp_header;
  const char* referrer_charset;
  const char* suggested_filename;
  const char* mime_type;
  const wchar_t* default_filename;
  const wchar_t* expected_filename;
};

struct UrlTestData {
  const char* description;
  const char* input;
  const char* languages;
  FormatUrlTypes format_types;
  UnescapeRule::Type escape_rules;
  const wchar_t* output;  // Use |wchar_t| to handle Unicode constants easily.
  size_t prefix_len;
};

// Fills in sockaddr for the given 32-bit address (IPv4.)
// |bytes| should be an array of length 4.
void MakeIPv4Address(const uint8* bytes, int port, SockaddrStorage* storage) {
  memset(&storage->addr_storage, 0, sizeof(storage->addr_storage));
  storage->addr_len = sizeof(struct sockaddr_in);
  struct sockaddr_in* addr4 = reinterpret_cast<sockaddr_in*>(storage->addr);
  addr4->sin_port = base::HostToNet16(port);
  addr4->sin_family = AF_INET;
  memcpy(&addr4->sin_addr, bytes, 4);
}

// Fills in sockaddr for the given 128-bit address (IPv6.)
// |bytes| should be an array of length 16.
void MakeIPv6Address(const uint8* bytes, int port, SockaddrStorage* storage) {
  memset(&storage->addr_storage, 0, sizeof(storage->addr_storage));
  storage->addr_len = sizeof(struct sockaddr_in6);
  struct sockaddr_in6* addr6 = reinterpret_cast<sockaddr_in6*>(storage->addr);
  addr6->sin6_port = base::HostToNet16(port);
  addr6->sin6_family = AF_INET6;
  memcpy(&addr6->sin6_addr, bytes, 16);
}

// A helper for IDN*{Fast,Slow}.
// Append "::<language list>" to |expected| and |actual| to make it
// easy to tell which sub-case fails without debugging.
void AppendLanguagesToOutputs(const char* languages,
                              string16* expected,
                              string16* actual) {
  string16 to_append = ASCIIToUTF16("::") + ASCIIToUTF16(languages);
  expected->append(to_append);
  actual->append(to_append);
}

// A pair of helpers for the FormatUrlWithOffsets() test.
void VerboseExpect(size_t expected,
                   size_t actual,
                   const std::string& original_url,
                   size_t position,
                   const string16& formatted_url) {
  EXPECT_EQ(expected, actual) << "Original URL: " << original_url
      << " (at char " << position << ")\nFormatted URL: " << formatted_url;
}

void CheckAdjustedOffsets(const std::string& url_string,
                          const std::string& languages,
                          FormatUrlTypes format_types,
                          UnescapeRule::Type unescape_rules,
                          const AdjustOffsetCase* cases,
                          size_t num_cases,
                          const size_t* all_offsets) {
  GURL url(url_string);
  for (size_t i = 0; i < num_cases; ++i) {
    size_t offset = cases[i].input_offset;
    string16 formatted_url = FormatUrl(url, languages, format_types,
                                       unescape_rules, NULL, NULL, &offset);
    VerboseExpect(cases[i].output_offset, offset, url_string, i, formatted_url);
  }

  size_t url_size = url_string.length();
  std::vector<size_t> offsets;
  for (size_t i = 0; i < url_size + 1; ++i)
    offsets.push_back(i);
  string16 formatted_url = FormatUrlWithOffsets(url, languages, format_types,
      unescape_rules, NULL, NULL, &offsets);
  for (size_t i = 0; i < url_size; ++i)
    VerboseExpect(all_offsets[i], offsets[i], url_string, i, formatted_url);
  VerboseExpect(kNpos, offsets[url_size], url_string, url_size, formatted_url);
}

// Helper to strignize an IP number (used to define expectations).
std::string DumpIPNumber(const IPAddressNumber& v) {
  std::string out;
  for (size_t i = 0; i < v.size(); ++i) {
    if (i != 0)
      out.append(",");
    out.append(base::IntToString(static_cast<int>(v[i])));
  }
  return out;
}

void RunGenerateFileNameTestCase(const GenerateFilenameCase* test_case,
                                 size_t iteration,
                                 const char* suite) {
  std::string default_filename(WideToUTF8(test_case->default_filename));
  FilePath file_path = GenerateFileName(
      GURL(test_case->url), test_case->content_disp_header,
      test_case->referrer_charset, test_case->suggested_filename,
      test_case->mime_type, default_filename);
  EXPECT_EQ(test_case->expected_filename,
            file_util::FilePathAsWString(file_path))
      << "Iteration " << iteration << " of " << suite << ": " << test_case->url;
}

}  // anonymous namespace

TEST(NetUtilTest, FileURLConversion) {
  // a list of test file names and the corresponding URLs
  const FileCase round_trip_cases[] = {
#if defined(OS_WIN)
    {L"C:\\foo\\bar.txt", "file:///C:/foo/bar.txt"},
    {L"\\\\some computer\\foo\\bar.txt",
     "file://some%20computer/foo/bar.txt"}, // UNC
    {L"D:\\Name;with%some symbols*#",
     "file:///D:/Name%3Bwith%25some%20symbols*%23"},
    // issue 14153: To be tested with the OS default codepage other than 1252.
    {L"D:\\latin1\\caf\x00E9\x00DD.txt",
     "file:///D:/latin1/caf%C3%A9%C3%9D.txt"},
    {L"D:\\otherlatin\\caf\x0119.txt",
     "file:///D:/otherlatin/caf%C4%99.txt"},
    {L"D:\\greek\\\x03B1\x03B2\x03B3.txt",
     "file:///D:/greek/%CE%B1%CE%B2%CE%B3.txt"},
    {L"D:\\Chinese\\\x6240\x6709\x4e2d\x6587\x7f51\x9875.doc",
     "file:///D:/Chinese/%E6%89%80%E6%9C%89%E4%B8%AD%E6%96%87%E7%BD%91"
         "%E9%A1%B5.doc"},
    {L"D:\\plane1\\\xD835\xDC00\xD835\xDC01.txt",  // Math alphabet "AB"
     "file:///D:/plane1/%F0%9D%90%80%F0%9D%90%81.txt"},
#elif defined(OS_POSIX)
    {L"/foo/bar.txt", "file:///foo/bar.txt"},
    {L"/foo/BAR.txt", "file:///foo/BAR.txt"},
    {L"/C:/foo/bar.txt", "file:///C:/foo/bar.txt"},
    {L"/foo/bar?.txt", "file:///foo/bar%3F.txt"},
    {L"/some computer/foo/bar.txt", "file:///some%20computer/foo/bar.txt"},
    {L"/Name;with%some symbols*#", "file:///Name%3Bwith%25some%20symbols*%23"},
    {L"/latin1/caf\x00E9\x00DD.txt", "file:///latin1/caf%C3%A9%C3%9D.txt"},
    {L"/otherlatin/caf\x0119.txt", "file:///otherlatin/caf%C4%99.txt"},
    {L"/greek/\x03B1\x03B2\x03B3.txt", "file:///greek/%CE%B1%CE%B2%CE%B3.txt"},
    {L"/Chinese/\x6240\x6709\x4e2d\x6587\x7f51\x9875.doc",
     "file:///Chinese/%E6%89%80%E6%9C%89%E4%B8%AD%E6%96%87%E7%BD"
         "%91%E9%A1%B5.doc"},
    {L"/plane1/\x1D400\x1D401.txt",  // Math alphabet "AB"
     "file:///plane1/%F0%9D%90%80%F0%9D%90%81.txt"},
#endif
  };

  // First, we'll test that we can round-trip all of the above cases of URLs
  FilePath output;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(round_trip_cases); i++) {
    // convert to the file URL
    GURL file_url(FilePathToFileURL(
                      file_util::WStringAsFilePath(round_trip_cases[i].file)));
    EXPECT_EQ(round_trip_cases[i].url, file_url.spec());

    // Back to the filename.
    EXPECT_TRUE(FileURLToFilePath(file_url, &output));
    EXPECT_EQ(round_trip_cases[i].file, file_util::FilePathAsWString(output));
  }

  // Test that various file: URLs get decoded into the correct file type
  FileCase url_cases[] = {
#if defined(OS_WIN)
    {L"C:\\foo\\bar.txt", "file:c|/foo\\bar.txt"},
    {L"C:\\foo\\bar.txt", "file:/c:/foo/bar.txt"},
    {L"\\\\foo\\bar.txt", "file://foo\\bar.txt"},
    {L"C:\\foo\\bar.txt", "file:///c:/foo/bar.txt"},
    {L"\\\\foo\\bar.txt", "file:////foo\\bar.txt"},
    {L"\\\\foo\\bar.txt", "file:/foo/bar.txt"},
    {L"\\\\foo\\bar.txt", "file://foo\\bar.txt"},
    {L"C:\\foo\\bar.txt", "file:\\\\\\c:/foo/bar.txt"},
#elif defined(OS_POSIX)
    {L"/c:/foo/bar.txt", "file:/c:/foo/bar.txt"},
    {L"/c:/foo/bar.txt", "file:///c:/foo/bar.txt"},
    {L"/foo/bar.txt", "file:/foo/bar.txt"},
    {L"/c:/foo/bar.txt", "file:\\\\\\c:/foo/bar.txt"},
    {L"/foo/bar.txt", "file:foo/bar.txt"},
    {L"/bar.txt", "file://foo/bar.txt"},
    {L"/foo/bar.txt", "file:///foo/bar.txt"},
    {L"/foo/bar.txt", "file:////foo/bar.txt"},
    {L"/foo/bar.txt", "file:////foo//bar.txt"},
    {L"/foo/bar.txt", "file:////foo///bar.txt"},
    {L"/foo/bar.txt", "file:////foo////bar.txt"},
    {L"/c:/foo/bar.txt", "file:\\\\\\c:/foo/bar.txt"},
    {L"/c:/foo/bar.txt", "file:c:/foo/bar.txt"},
    // We get these wrong because GURL turns back slashes into forward
    // slashes.
    //{L"/foo%5Cbar.txt", "file://foo\\bar.txt"},
    //{L"/c|/foo%5Cbar.txt", "file:c|/foo\\bar.txt"},
    //{L"/foo%5Cbar.txt", "file://foo\\bar.txt"},
    //{L"/foo%5Cbar.txt", "file:////foo\\bar.txt"},
    //{L"/foo%5Cbar.txt", "file://foo\\bar.txt"},
#endif
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(url_cases); i++) {
    FileURLToFilePath(GURL(url_cases[i].url), &output);
    EXPECT_EQ(url_cases[i].file, file_util::FilePathAsWString(output));
  }

  // Unfortunately, UTF8ToWide discards invalid UTF8 input.
#ifdef BUG_878908_IS_FIXED
  // Test that no conversion happens if the UTF-8 input is invalid, and that
  // the input is preserved in UTF-8
  const char invalid_utf8[] = "file:///d:/Blah/\xff.doc";
  const wchar_t invalid_wide[] = L"D:\\Blah\\\xff.doc";
  EXPECT_TRUE(FileURLToFilePath(
      GURL(std::string(invalid_utf8)), &output));
  EXPECT_EQ(std::wstring(invalid_wide), output);
#endif

  // Test that if a file URL is malformed, we get a failure
  EXPECT_FALSE(FileURLToFilePath(GURL("filefoobar"), &output));
}

TEST(NetUtilTest, GetIdentityFromURL) {
  struct {
    const char* input_url;
    const char* expected_username;
    const char* expected_password;
  } tests[] = {
    {
      "http://username:password@google.com",
      "username",
      "password",
    },
    { // Test for http://crbug.com/19200
      "http://username:p@ssword@google.com",
      "username",
      "p@ssword",
    },
    { // Special URL characters should be unescaped.
      "http://username:p%3fa%26s%2fs%23@google.com",
      "username",
      "p?a&s/s#",
    },
    { // Username contains %20.
      "http://use rname:password@google.com",
      "use rname",
      "password",
    },
    { // Keep %00 as is.
      "http://use%00rname:password@google.com",
      "use%00rname",
      "password",
    },
    { // Use a '+' in the username.
      "http://use+rname:password@google.com",
      "use+rname",
      "password",
    },
    { // Use a '&' in the password.
      "http://username:p&ssword@google.com",
      "username",
      "p&ssword",
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    tests[i].input_url));
    GURL url(tests[i].input_url);

    string16 username, password;
    GetIdentityFromURL(url, &username, &password);

    EXPECT_EQ(ASCIIToUTF16(tests[i].expected_username), username);
    EXPECT_EQ(ASCIIToUTF16(tests[i].expected_password), password);
  }
}

// Try extracting a username which was encoded with UTF8.
TEST(NetUtilTest, GetIdentityFromURL_UTF8) {
  GURL url(WideToUTF16(L"http://foo:\x4f60\x597d@blah.com"));

  EXPECT_EQ("foo", url.username());
  EXPECT_EQ("%E4%BD%A0%E5%A5%BD", url.password());

  // Extract the unescaped identity.
  string16 username, password;
  GetIdentityFromURL(url, &username, &password);

  // Verify that it was decoded as UTF8.
  EXPECT_EQ(ASCIIToUTF16("foo"), username);
  EXPECT_EQ(WideToUTF16(L"\x4f60\x597d"), password);
}

// Just a bunch of fake headers.
const char* google_headers =
    "HTTP/1.1 200 OK\n"
    "Content-TYPE: text/html; charset=utf-8\n"
    "Content-disposition: attachment; filename=\"download.pdf\"\n"
    "Content-Length: 378557\n"
    "X-Google-Google1: 314159265\n"
    "X-Google-Google2: aaaa2:7783,bbb21:9441\n"
    "X-Google-Google4: home\n"
    "Transfer-Encoding: chunked\n"
    "Set-Cookie: HEHE_AT=6666x66beef666x6-66xx6666x66; Path=/mail\n"
    "Set-Cookie: HEHE_HELP=owned:0;Path=/\n"
    "Set-Cookie: S=gmail=Xxx-beefbeefbeef_beefb:gmail_yj=beefbeef000beefbee"
       "fbee:gmproxy=bee-fbeefbe; Domain=.google.com; Path=/\n"
    "X-Google-Google2: /one/two/three/four/five/six/seven-height/nine:9411\n"
    "Server: GFE/1.3\n"
    "Transfer-Encoding: chunked\n"
    "Date: Mon, 13 Nov 2006 21:38:09 GMT\n"
    "Expires: Tue, 14 Nov 2006 19:23:58 GMT\n"
    "X-Malformed: bla; arg=test\"\n"
    "X-Malformed2: bla; arg=\n"
    "X-Test: bla; arg1=val1; arg2=val2";

TEST(NetUtilTest, GetSpecificHeader) {
  const HeaderCase tests[] = {
    {"content-type", "text/html; charset=utf-8"},
    {"CONTENT-LENGTH", "378557"},
    {"Date", "Mon, 13 Nov 2006 21:38:09 GMT"},
    {"Bad-Header", ""},
    {"", ""},
  };

  // Test first with google_headers.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string result =
        GetSpecificHeader(google_headers, tests[i].header_name);
    EXPECT_EQ(result, tests[i].expected);
  }

  // Test again with empty headers.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string result = GetSpecificHeader(std::string(), tests[i].header_name);
    EXPECT_EQ(result, std::string());
  }
}

TEST(NetUtilTest, IDNToUnicodeFast) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(idn_cases); i++) {
    for (size_t j = 0; j < arraysize(kLanguages); j++) {
      // ja || zh-TW,en || ko,ja -> IDNToUnicodeSlow
      if (j == 3 || j == 17 || j == 18)
        continue;
      string16 output(IDNToUnicode(idn_cases[i].input, kLanguages[j]));
      string16 expected(idn_cases[i].unicode_allowed[j] ?
          WideToUTF16(idn_cases[i].unicode_output) :
          ASCIIToUTF16(idn_cases[i].input));
      AppendLanguagesToOutputs(kLanguages[j], &expected, &output);
      EXPECT_EQ(expected, output);
    }
  }
}

TEST(NetUtilTest, IDNToUnicodeSlow) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(idn_cases); i++) {
    for (size_t j = 0; j < arraysize(kLanguages); j++) {
      // !(ja || zh-TW,en || ko,ja) -> IDNToUnicodeFast
      if (!(j == 3 || j == 17 || j == 18))
        continue;
      string16 output(IDNToUnicode(idn_cases[i].input, kLanguages[j]));
      string16 expected(idn_cases[i].unicode_allowed[j] ?
          WideToUTF16(idn_cases[i].unicode_output) :
          ASCIIToUTF16(idn_cases[i].input));
      AppendLanguagesToOutputs(kLanguages[j], &expected, &output);
      EXPECT_EQ(expected, output);
    }
  }
}

TEST(NetUtilTest, CompliantHost) {
  const CompliantHostCase compliant_host_cases[] = {
    {"", "", false},
    {"a", "", true},
    {"-", "", false},
    {".", "", false},
    {"9", "", false},
    {"9", "a", true},
    {"9a", "", false},
    {"9a", "a", true},
    {"a.", "", true},
    {"a.a", "", true},
    {"9.a", "", true},
    {"a.9", "", false},
    {"_9a", "", false},
    {"-9a", "", false},
    {"a.a9", "", true},
    {"a.9a", "", false},
    {"a+9a", "", false},
    {"-a.a9", "", true},
    {"1-.a-b", "", true},
    {"1_.a-b", "", false},
    {"1-2.a_b", "", true},
    {"a.b.c.d.e", "", true},
    {"1.2.3.4.e", "", true},
    {"a.b.c.d.5", "", false},
    {"1.2.3.4.e.", "", true},
    {"a.b.c.d.5.", "", false},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(compliant_host_cases); ++i) {
    EXPECT_EQ(compliant_host_cases[i].expected_output,
        IsCanonicalizedHostCompliant(compliant_host_cases[i].host,
                                     compliant_host_cases[i].desired_tld));
  }
}

TEST(NetUtilTest, StripWWW) {
  EXPECT_EQ(string16(), StripWWW(string16()));
  EXPECT_EQ(string16(), StripWWW(ASCIIToUTF16("www.")));
  EXPECT_EQ(ASCIIToUTF16("blah"), StripWWW(ASCIIToUTF16("www.blah")));
  EXPECT_EQ(ASCIIToUTF16("blah"), StripWWW(ASCIIToUTF16("blah")));
}

#if defined(OS_WIN)
#define JPEG_EXT L".jpg"
#define HTML_EXT L".htm"
#define TXT_EXT L".txt"
#define TAR_EXT L".tar"
#elif defined(OS_MACOSX)
#define JPEG_EXT L".jpeg"
#define HTML_EXT L".html"
#define TXT_EXT L".txt"
#define TAR_EXT L".tar"
#else
#define JPEG_EXT L".jpg"
#define HTML_EXT L".html"
#define TXT_EXT L".txt"
#define TAR_EXT L".tar"
#endif

TEST(NetUtilTest, GenerateSafeFileName) {
  const struct {
    const char* mime_type;
    const FilePath::CharType* filename;
    const FilePath::CharType* expected_filename;
  } safe_tests[] = {
#if defined(OS_WIN)
    {
      "text/html",
      FILE_PATH_LITERAL("C:\\foo\\bar.htm"),
      FILE_PATH_LITERAL("C:\\foo\\bar.htm")
    },
    {
      "text/html",
      FILE_PATH_LITERAL("C:\\foo\\bar.html"),
      FILE_PATH_LITERAL("C:\\foo\\bar.html")
    },
    {
      "text/html",
      FILE_PATH_LITERAL("C:\\foo\\bar"),
      FILE_PATH_LITERAL("C:\\foo\\bar.htm")
    },
    {
      "image/png",
      FILE_PATH_LITERAL("C:\\bar.html"),
      FILE_PATH_LITERAL("C:\\bar.html")
    },
    {
      "image/png",
      FILE_PATH_LITERAL("C:\\bar"),
      FILE_PATH_LITERAL("C:\\bar.png")
    },
    {
      "text/html",
      FILE_PATH_LITERAL("C:\\foo\\bar.exe"),
      FILE_PATH_LITERAL("C:\\foo\\bar.exe")
    },
    {
      "image/gif",
      FILE_PATH_LITERAL("C:\\foo\\bar.exe"),
      FILE_PATH_LITERAL("C:\\foo\\bar.exe")
    },
    {
      "text/html",
      FILE_PATH_LITERAL("C:\\foo\\google.com"),
      FILE_PATH_LITERAL("C:\\foo\\google.com")
    },
    {
      "text/html",
      FILE_PATH_LITERAL("C:\\foo\\con.htm"),
      FILE_PATH_LITERAL("C:\\foo\\_con.htm")
    },
    {
      "text/html",
      FILE_PATH_LITERAL("C:\\foo\\con"),
      FILE_PATH_LITERAL("C:\\foo\\_con.htm")
    },
    {
      "text/html",
      FILE_PATH_LITERAL("C:\\foo\\harmless.{not-really-this-may-be-a-guid}"),
      FILE_PATH_LITERAL("C:\\foo\\harmless.download")
    },
    {
      "text/html",
      FILE_PATH_LITERAL("C:\\foo\\harmless.local"),
      FILE_PATH_LITERAL("C:\\foo\\harmless.download")
    },
    {
      "text/html",
      FILE_PATH_LITERAL("C:\\foo\\harmless.lnk"),
      FILE_PATH_LITERAL("C:\\foo\\harmless.download")
    },
    {
      "text/html",
      FILE_PATH_LITERAL("C:\\foo\\harmless.{mismatched-"),
      FILE_PATH_LITERAL("C:\\foo\\harmless.{mismatched-")
    },
    // Allow extension synonyms.
    {
      "image/jpeg",
      FILE_PATH_LITERAL("C:\\foo\\bar.jpg"),
      FILE_PATH_LITERAL("C:\\foo\\bar.jpg")
    },
    {
      "image/jpeg",
      FILE_PATH_LITERAL("C:\\foo\\bar.jpeg"),
      FILE_PATH_LITERAL("C:\\foo\\bar.jpeg")
    },
#else  // !defined(OS_WIN)
    {
      "text/html",
      FILE_PATH_LITERAL("/foo/bar.htm"),
      FILE_PATH_LITERAL("/foo/bar.htm")
    },
    {
      "text/html",
      FILE_PATH_LITERAL("/foo/bar.html"),
      FILE_PATH_LITERAL("/foo/bar.html")
    },
    {
      "text/html",
      FILE_PATH_LITERAL("/foo/bar"),
      FILE_PATH_LITERAL("/foo/bar.html")
    },
    {
      "image/png",
      FILE_PATH_LITERAL("/bar.html"),
      FILE_PATH_LITERAL("/bar.html")
    },
    {
      "image/png",
      FILE_PATH_LITERAL("/bar"),
      FILE_PATH_LITERAL("/bar.png")
    },
    {
      "image/gif",
      FILE_PATH_LITERAL("/foo/bar.exe"),
      FILE_PATH_LITERAL("/foo/bar.exe")
    },
    {
      "text/html",
      FILE_PATH_LITERAL("/foo/google.com"),
      FILE_PATH_LITERAL("/foo/google.com")
    },
    {
      "text/html",
      FILE_PATH_LITERAL("/foo/con.htm"),
      FILE_PATH_LITERAL("/foo/con.htm")
    },
    {
      "text/html",
      FILE_PATH_LITERAL("/foo/con"),
      FILE_PATH_LITERAL("/foo/con.html")
    },
    // Allow extension synonyms.
    {
      "image/jpeg",
      FILE_PATH_LITERAL("/bar.jpg"),
      FILE_PATH_LITERAL("/bar.jpg")
    },
    {
      "image/jpeg",
      FILE_PATH_LITERAL("/bar.jpeg"),
      FILE_PATH_LITERAL("/bar.jpeg")
    },
#endif  // !defined(OS_WIN)
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(safe_tests); ++i) {
    FilePath file_path(safe_tests[i].filename);
    GenerateSafeFileName(safe_tests[i].mime_type, false, &file_path);
    EXPECT_EQ(safe_tests[i].expected_filename, file_path.value())
        << "Iteration " << i;
  }
}

TEST(NetUtilTest, GenerateFileName) {
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
  // This test doesn't run when the locale is not UTF-8 because some of the
  // string conversions fail. This is OK (we have the default value) but they
  // don't match our expectations.
  std::string locale = setlocale(LC_CTYPE, NULL);
  StringToLowerASCII(&locale);
  EXPECT_TRUE(locale.find("utf-8") != std::string::npos ||
              locale.find("utf8") != std::string::npos)
      << "Your locale (" << locale << ") must be set to UTF-8 "
      << "for this test to pass!";
#endif

  // Tests whether the correct filename is selected from the the given
  // parameters and that Content-Disposition headers are properly
  // handled including failovers when the header is malformed.
  const GenerateFilenameCase selection_tests[] = {
    {
      "http://www.google.com/",
      "attachment; filename=test.html",
      "",
      "",
      "",
      L"",
      L"test.html"
    },
    {
      "http://www.google.com/",
      "attachment; filename=\"test.html\"",
      "",
      "",
      "",
      L"",
      L"test.html"
    },
    {
      "http://www.google.com/",
      "attachment; filename= \"test.html\"",
      "",
      "",
      "",
      L"",
      L"test.html"
    },
    {
      "http://www.google.com/",
      "attachment; filename   =   \"test.html\"",
      "",
      "",
      "",
      L"",
      L"test.html"
    },
    { // filename is whitespace.  Should failover to URL host
      "http://www.google.com/",
      "attachment; filename=  ",
      "",
      "",
      "",
      L"",
      L"www.google.com"
    },
    { // No filename.
      "http://www.google.com/path/test.html",
      "attachment",
      "",
      "",
      "",
      L"",
      L"test.html"
    },
    { // Ditto
      "http://www.google.com/path/test.html",
      "attachment;",
      "",
      "",
      "",
      L"",
      L"test.html"
    },
    { // No C-D
      "http://www.google.com/",
      "",
      "",
      "",
      "",
      L"",
      L"www.google.com"
    },
    {
      "http://www.google.com/test.html",
      "",
      "",
      "",
      "",
      L"",
      L"test.html"
    },
    { // Now that we use googleurl's ExtractFileName, this case falls back to
      // the hostname. If this behavior is not desirable, we'd better change
      // ExtractFileName (in url_parse).
      "http://www.google.com/path/",
      "",
      "",
      "",
      "",
      L"",
      L"www.google.com"
    },
    {
      "http://www.google.com/path",
      "",
      "",
      "",
      "",
      L"",
      L"path"
    },
    {
      "file:///",
      "",
      "",
      "",
      "",
      L"",
      L"download"
    },
    {
      "file:///path/testfile",
      "",
      "",
      "",
      "",
      L"",
      L"testfile"
    },
    {
      "non-standard-scheme:",
      "",
      "",
      "",
      "",
      L"",
      L"download"
    },
    { // C-D should override default
      "http://www.google.com/",
      "attachment; filename =\"test.html\"",
      "",
      "",
      "",
      L"download",
      L"test.html"
    },
    { // But the URL shouldn't
      "http://www.google.com/",
      "",
      "",
      "",
      "",
      L"download",
      L"download"
    },
    {
      "http://www.google.com/",
      "attachment; filename=\"../test.html\"",
      "",
      "",
      "",
      L"",
      L"_test.html"
    },
    {
      "http://www.google.com/",
      "attachment; filename=\"..\\test.html\"",
      "",
      "",
      "",
      L"",
      L"test.html"
    },
    {
      "http://www.google.com/",
      "attachment; filename=\"..\\\\test.html\"",
      "",
      "",
      "",
      L"",
      L"_test.html"
    },
    { // Filename disappears after leading and trailing periods are removed.
      "http://www.google.com/",
      "attachment; filename=\"..\"",
      "",
      "",
      "",
      L"default",
      L"default"
    },
    { // C-D specified filename disappears.  Failover to final filename.
      "http://www.google.com/test.html",
      "attachment; filename=\"..\"",
      "",
      "",
      "",
      L"default",
      L"default"
    },
    // Below is a small subset of cases taken from GetFileNameFromCD test above.
    {
      "http://www.google.com/",
      "attachment; filename=\"%EC%98%88%EC%88%A0%20"
      "%EC%98%88%EC%88%A0.jpg\"",
      "",
      "",
      "",
      L"",
      L"\uc608\uc220 \uc608\uc220.jpg"
    },
    {
      "http://www.google.com/%EC%98%88%EC%88%A0%20%EC%98%88%EC%88%A0.jpg",
      "",
      "",
      "",
      "",
      L"download",
      L"\uc608\uc220 \uc608\uc220.jpg"
    },
    {
      "http://www.google.com/",
      "attachment;",
      "",
      "",
      "",
      L"\uB2E4\uC6B4\uB85C\uB4DC",
      L"\uB2E4\uC6B4\uB85C\uB4DC"
    },
    {
      "http://www.google.com/",
      "attachment; filename=\"=?EUC-JP?Q?=B7=DD=BD="
      "D13=2Epng?=\"",
      "",
      "",
      "",
      L"download",
      L"\u82b8\u88533.png"
    },
    {
      "http://www.example.com/images?id=3",
      "attachment; filename=caf\xc3\xa9.png",
      "iso-8859-1",
      "",
      "",
      L"",
      L"caf\u00e9.png"
    },
    {
      "http://www.example.com/images?id=3",
      "attachment; filename=caf\xe5.png",
      "windows-1253",
      "",
      "",
      L"",
      L"caf\u03b5.png"
    },
    {
      "http://www.example.com/file?id=3",
      "attachment; name=\xcf\xc2\xd4\xd8.zip",
      "GBK",
      "",
      "",
      L"",
      L"\u4e0b\u8f7d.zip"
    },
    { // Invalid C-D header. Extracts filename from url.
      "http://www.google.com/test.html",
      "attachment; filename==?iiso88591?Q?caf=EG?=",
      "",
      "",
      "",
      L"",
      L"test.html"
    },
    // about: and data: URLs
    {
      "about:chrome",
      "",
      "",
      "",
      "",
      L"",
      L"download"
    },
    {
      "data:,looks/like/a.path",
      "",
      "",
      "",
      "",
      L"",
      L"download"
    },
    {
      "data:text/plain;base64,VG8gYmUgb3Igbm90IHRvIGJlLg=",
      "",
      "",
      "",
      "",
      L"",
      L"download"
    },
    {
      "data:,looks/like/a.path",
      "",
      "",
      "",
      "",
      L"default_filename_is_given",
      L"default_filename_is_given"
    },
    {
      "data:,looks/like/a.path",
      "",
      "",
      "",
      "",
      L"\u65e5\u672c\u8a9e",  // Japanese Kanji.
      L"\u65e5\u672c\u8a9e"
    },
    { // The filename encoding is specified by the referrer charset.
      "http://example.com/V%FDvojov%E1%20psychologie.doc",
      "",
      "iso-8859-1",
      "",
      "",
      L"",
      L"V\u00fdvojov\u00e1 psychologie.doc"
    },
    { // Suggested filename takes precedence over URL
      "http://www.google.com/test",
      "",
      "",
      "suggested",
      "",
      L"",
      L"suggested"
    },
    { // The content-disposition has higher precedence over the suggested name.
      "http://www.google.com/test",
      "attachment; filename=test.html",
      "",
      "suggested",
      "",
      L"",
      L"test.html"
    },
#if 0
    { // The filename encoding doesn't match the referrer charset, the system
      // charset, or UTF-8.
      // TODO(jshin): we need to handle this case.
      "http://example.com/V%FDvojov%E1%20psychologie.doc",
      "",
      "utf-8",
      "",
      "",
      L"",
      L"V\u00fdvojov\u00e1 psychologie.doc",
    },
#endif
    // Raw 8bit characters in C-D
    {
      "http://www.example.com/images?id=3",
      "attachment; filename=caf\xc3\xa9.png",
      "iso-8859-1",
      "",
      "image/png",
      L"",
      L"caf\u00e9.png"
    },
    {
      "http://www.example.com/images?id=3",
      "attachment; filename=caf\xe5.png",
      "windows-1253",
      "",
      "image/png",
      L"",
      L"caf\u03b5.png"
    },
    { // No 'filename' keyword in the disposition, use the URL
      "http://www.evil.com/my_download.txt",
      "a_file_name.txt",
      "",
      "",
      "text/plain",
      L"download",
      L"my_download.txt"
    },
    { // Spaces in the disposition file name
      "http://www.frontpagehacker.com/a_download.exe",
      "filename=My Downloaded File.exe",
      "",
      "",
      "application/octet-stream",
      L"download",
      L"My Downloaded File.exe"
    },
    { // % encoded
      "http://www.examples.com/",
      "attachment; "
      "filename=\"%EC%98%88%EC%88%A0%20%EC%98%88%EC%88%A0.jpg\"",
      "",
      "",
      "image/jpeg",
      L"download",
      L"\uc608\uc220 \uc608\uc220.jpg"
    },
    { // name= parameter
      "http://www.examples.com/q.cgi?id=abc",
      "attachment; name=abc de.pdf",
      "",
      "",
      "application/octet-stream",
      L"download",
      L"abc de.pdf"
    },
    {
      "http://www.example.com/path",
      "filename=\"=?EUC-JP?Q?=B7=DD=BD=D13=2Epng?=\"",
      "",
      "",
      "image/png",
      L"download",
      L"\x82b8\x8853" L"3.png"
    },
    { // The following two have invalid CD headers and filenames come from the
      // URL.
      "http://www.example.com/test%20123",
      "attachment; filename==?iiso88591?Q?caf=EG?=",
      "",
      "",
      "image/jpeg",
      L"download",
      L"test 123" JPEG_EXT
    },
    {
      "http://www.google.com/%EC%98%88%EC%88%A0%20%EC%98%88%EC%88%A0.jpg",
      "malformed_disposition",
      "",
      "",
      "image/jpeg",
      L"download",
      L"\uc608\uc220 \uc608\uc220.jpg"
    },
    { // Invalid C-D. No filename from URL. Falls back to 'download'.
      "http://www.google.com/path1/path2/",
      "attachment; filename==?iso88591?Q?caf=E3?",
      "",
      "",
      "image/jpeg",
      L"download",
      L"download" JPEG_EXT
    },
  };

  // Tests filename generation.  Once the correct filename is
  // selected, they should be passed through the validation steps and
  // a correct extension should be added if necessary.
  const GenerateFilenameCase generation_tests[] = {
    // Dotfiles. Ensures preceeding period(s) stripped.
    {
      "http://www.google.com/.test.html",
      "",
      "",
      "",
      "",
      L"",
      L"test.html"
    },
    {
      "http://www.google.com/.test",
      "",
      "",
      "",
      "",
      L"",
      L"test"
    },
    {
      "http://www.google.com/..test",
      "",
      "",
      "",
      "",
      L"",
      L"test"
    },
    { // Disposition has relative paths, remove directory separators
      "http://www.evil.com/my_download.txt",
      "filename=../../../../././../a_file_name.txt",
      "",
      "",
      "text/plain",
      L"download",
      L"_.._.._.._._._.._a_file_name.txt"
    },
    { // Disposition has parent directories, remove directory separators
      "http://www.evil.com/my_download.txt",
      "filename=dir1/dir2/a_file_name.txt",
      "",
      "",
      "text/plain",
      L"download",
      L"dir1_dir2_a_file_name.txt"
    },
    { // Disposition has relative paths, remove directory separators
      "http://www.evil.com/my_download.txt",
      "filename=..\\..\\..\\..\\.\\.\\..\\a_file_name.txt",
      "",
      "",
      "text/plain",
      L"download",
      L"_.._.._.._._._.._a_file_name.txt"
    },
    { // Disposition has parent directories, remove directory separators
      "http://www.evil.com/my_download.txt",
      "filename=dir1\\dir2\\a_file_name.txt",
      "",
      "",
      "text/plain",
      L"download",
      L"dir1_dir2_a_file_name.txt"
    },
    { // No useful information in disposition or URL, use default
      "http://www.truncated.com/path/",
      "",
      "",
      "",
      "text/plain",
      L"download",
      L"download" TXT_EXT
    },
    { // Filename looks like HTML?
      "http://www.evil.com/get/malware/here",
      "filename=\"<blink>Hello kitty</blink>\"",
      "",
      "",
      "text/plain",
      L"default",
      L"-blink-Hello kitty-_blink-" TXT_EXT
    },
    { // A normal avi should get .avi and not .avi.avi
      "https://blah.google.com/misc/2.avi",
      "",
      "",
      "",
      "video/x-msvideo",
      L"download",
      L"2.avi"
    },
    { // Extension generation
      "http://www.example.com/my-cat",
      "filename=my-cat",
      "",
      "",
      "image/jpeg",
      L"download",
      L"my-cat" JPEG_EXT
    },
    {
      "http://www.example.com/my-cat",
      "filename=my-cat",
      "",
      "",
      "text/plain",
      L"download",
      L"my-cat.txt"
    },
    {
      "http://www.example.com/my-cat",
      "filename=my-cat",
      "",
      "",
      "text/html",
      L"download",
      L"my-cat" HTML_EXT
    },
    { // Unknown MIME type
      "http://www.example.com/my-cat",
      "filename=my-cat",
      "",
      "",
      "dance/party",
      L"download",
      L"my-cat"
    },
    {
      "http://www.example.com/my-cat.jpg",
      "filename=my-cat.jpg",
      "",
      "",
      "text/plain",
      L"download",
      L"my-cat.jpg"
    },
    // Windows specific tests
#if defined(OS_WIN)
    {
      "http://www.goodguy.com/evil.exe",
      "filename=evil.exe",
      "",
      "",
      "image/jpeg",
      L"download",
      L"evil.exe"
    },
    {
      "http://www.goodguy.com/ok.exe",
      "filename=ok.exe",
      "",
      "",
      "binary/octet-stream",
      L"download",
      L"ok.exe"
    },
    {
      "http://www.goodguy.com/evil.dll",
      "filename=evil.dll",
      "",
      "",
      "dance/party",
      L"download",
      L"evil.dll"
    },
    {
      "http://www.goodguy.com/evil.exe",
      "filename=evil",
      "",
      "",
      "application/rss+xml",
      L"download",
      L"evil"
    },
    // Test truncation of trailing dots and spaces
    {
      "http://www.goodguy.com/evil.exe ",
      "filename=evil.exe ",
      "",
      "",
      "binary/octet-stream",
      L"download",
      L"evil.exe"
    },
    {
      "http://www.goodguy.com/evil.exe.",
      "filename=evil.exe.",
      "",
      "",
      "binary/octet-stream",
      L"download",
      L"evil.exe-"
    },
    {
      "http://www.goodguy.com/evil.exe.  .  .",
      "filename=evil.exe.  .  .",
      "",
      "",
      "binary/octet-stream",
      L"download",
      L"evil.exe-------"
    },
    {
      "http://www.goodguy.com/evil.",
      "filename=evil.",
      "",
      "",
      "binary/octet-stream",
      L"download",
      L"evil-"
    },
    {
      "http://www.goodguy.com/. . . . .",
      "filename=. . . . .",
      "",
      "",
      "binary/octet-stream",
      L"download",
      L"download"
    },
    {
      "http://www.badguy.com/attachment?name=meh.exe%C2%A0",
      "attachment; filename=\"meh.exe\xC2\xA0\"",
      "",
      "",
      "binary/octet-stream",
      L"",
      L"meh.exe-"
    },
#endif  // OS_WIN
    {
      "http://www.goodguy.com/utils.js",
      "filename=utils.js",
      "",
      "",
      "application/x-javascript",
      L"download",
      L"utils.js"
    },
    {
      "http://www.goodguy.com/contacts.js",
      "filename=contacts.js",
      "",
      "",
      "application/json",
      L"download",
      L"contacts.js"
    },
    {
      "http://www.goodguy.com/utils.js",
      "filename=utils.js",
      "",
      "",
      "text/javascript",
      L"download",
      L"utils.js"
    },
    {
      "http://www.goodguy.com/utils.js",
      "filename=utils.js",
      "",
      "",
      "text/javascript;version=2",
      L"download",
      L"utils.js"
    },
    {
      "http://www.goodguy.com/utils.js",
      "filename=utils.js",
      "",
      "",
      "application/ecmascript",
      L"download",
      L"utils.js"
    },
    {
      "http://www.goodguy.com/utils.js",
     "filename=utils.js",
     "",
     "",
     "application/ecmascript;version=4",
     L"download",
     L"utils.js"
    },
    {
      "http://www.goodguy.com/program.exe",
      "filename=program.exe",
      "",
      "",
      "application/foo-bar",
      L"download",
      L"program.exe"
    },
    {
      "http://www.evil.com/../foo.txt",
      "filename=../foo.txt",
      "",
      "",
      "text/plain",
      L"download",
      L"_foo.txt"
    },
    {
      "http://www.evil.com/..\\foo.txt",
      "filename=..\\foo.txt",
      "",
      "",
      "text/plain",
      L"download",
      L"_foo.txt"
    },
    {
      "http://www.evil.com/.hidden",
      "filename=.hidden",
      "",
      "",
      "text/plain",
      L"download",
      L"hidden" TXT_EXT
    },
    {
      "http://www.evil.com/trailing.",
      "filename=trailing.",
      "",
      "",
      "dance/party",
      L"download",
#if defined(OS_WIN)
      L"trailing-"
#else
      L"trailing"
#endif //OS_WIN
    },
    {
      "http://www.evil.com/trailing.",
      "filename=trailing.",
      "",
      "",
      "text/plain",
      L"download",
#if defined(OS_WIN)
      L"trailing-" TXT_EXT
#else
      L"trailing" TXT_EXT
#endif //OS_WIN
    },
    {
      "http://www.evil.com/.",
      "filename=.",
      "",
      "",
      "dance/party",
      L"download",
      L"download"
    },
    {
      "http://www.evil.com/..",
      "filename=..",
      "",
      "",
      "dance/party",
      L"download",
      L"download"
    },
    {
      "http://www.evil.com/...",
      "filename=...",
      "",
      "",
      "dance/party",
      L"download",
      L"download"
    },
    { // Note that this one doesn't have "filename=" on it.
      "http://www.evil.com/",
      "a_file_name.txt",
      "",
      "",
      "image/jpeg",
      L"download",
      L"download" JPEG_EXT
    },
    {
      "http://www.evil.com/",
      "filename=",
      "",
      "",
      "image/jpeg",
      L"download",
      L"download" JPEG_EXT
    },
    {
      "http://www.example.com/simple",
      "filename=simple",
      "",
      "",
      "application/octet-stream",
      L"download",
      L"simple"
    },
    // Reserved words on Windows
    {
      "http://www.goodguy.com/COM1",
      "filename=COM1",
      "",
      "",
      "application/foo-bar",
      L"download",
#if defined(OS_WIN)
      L"_COM1"
#else
      L"COM1"
#endif
    },
    {
      "http://www.goodguy.com/COM4.txt",
      "filename=COM4.txt",
      "",
      "",
      "text/plain",
      L"download",
#if defined(OS_WIN)
      L"_COM4.txt"
#else
      L"COM4.txt"
#endif
    },
    {
      "http://www.goodguy.com/lpt1.TXT",
      "filename=lpt1.TXT",
      "",
      "",
      "text/plain",
      L"download",
#if defined(OS_WIN)
      L"_lpt1.TXT"
#else
      L"lpt1.TXT"
#endif
    },
    {
      "http://www.goodguy.com/clock$.txt",
      "filename=clock$.txt",
      "",
      "",
      "text/plain",
      L"download",
#if defined(OS_WIN)
      L"_clock$.txt"
#else
      L"clock$.txt"
#endif
    },
    { // Validation should also apply to sugested name
      "http://www.goodguy.com/blah$.txt",
      "filename=clock$.txt",
      "",
      "clock$.txt",
      "text/plain",
      L"download",
#if defined(OS_WIN)
      L"_clock$.txt"
#else
      L"clock$.txt"
#endif
    },
    {
      "http://www.goodguy.com/mycom1.foo",
      "filename=mycom1.foo",
      "",
      "",
      "text/plain",
      L"download",
      L"mycom1.foo"
    },
    {
      "http://www.badguy.com/Setup.exe.local",
      "filename=Setup.exe.local",
      "",
      "",
      "application/foo-bar",
      L"download",
#if defined(OS_WIN)
      L"Setup.exe.download"
#else
      L"Setup.exe.local"
#endif
    },
    {
      "http://www.badguy.com/Setup.exe.local",
      "filename=Setup.exe.local.local",
      "",
      "",
      "application/foo-bar",
      L"download",
#if defined(OS_WIN)
      L"Setup.exe.local.download"
#else
      L"Setup.exe.local.local"
#endif
    },
    {
      "http://www.badguy.com/Setup.exe.lnk",
      "filename=Setup.exe.lnk",
      "",
      "",
      "application/foo-bar",
      L"download",
#if defined(OS_WIN)
      L"Setup.exe.download"
#else
      L"Setup.exe.lnk"
#endif
    },
    {
      "http://www.badguy.com/Desktop.ini",
      "filename=Desktop.ini",
      "",
      "",
      "application/foo-bar",
      L"download",
#if defined(OS_WIN)
      L"_Desktop.ini"
#else
      L"Desktop.ini"
#endif
    },
    {
      "http://www.badguy.com/Thumbs.db",
      "filename=Thumbs.db",
      "",
      "",
      "application/foo-bar",
      L"download",
#if defined(OS_WIN)
      L"_Thumbs.db"
#else
      L"Thumbs.db"
#endif
    },
    {
      "http://www.hotmail.com",
      "filename=source.jpg",
      "",
      "",
      "application/x-javascript",
      L"download",
      L"source.jpg"
    },
    { // http://crbug.com/5772.
      "http://www.example.com/foo.tar.gz",
      "",
      "",
      "",
      "application/x-tar",
      L"download",
      L"foo.tar.gz"
    },
    { // http://crbug.com/52250.
      "http://www.example.com/foo.tgz",
      "",
      "",
      "",
      "application/x-tar",
      L"download",
      L"foo.tgz"
    },
    { // http://crbug.com/7337.
      "http://maged.lordaeron.org/blank.reg",
      "",
      "",
      "",
      "text/x-registry",
      L"download",
      L"blank.reg"
    },
    {
      "http://www.example.com/bar.tar",
      "",
      "",
      "",
      "application/x-tar",
      L"download",
      L"bar.tar"
    },
    {
      "http://www.example.com/bar.bogus",
      "",
      "",
      "",
      "application/x-tar",
      L"download",
      L"bar.bogus"
    },
    { // http://crbug.com/20337
      "http://www.example.com/.download.txt",
      "filename=.download.txt",
      "",
      "",
      "text/plain",
      L"download",
      L"download.txt"
    },
    { // http://crbug.com/56855.
      "http://www.example.com/bar.sh",
      "",
      "",
      "",
      "application/x-sh",
      L"download",
      L"bar.sh"
    },
    { // http://crbug.com/61571
      "http://www.example.com/npdf.php?fn=foobar.pdf",
      "",
      "",
      "",
      "text/plain",
      L"download",
      L"npdf" TXT_EXT
    },
    { // Shouldn't overwrite C-D specified extension.
      "http://www.example.com/npdf.php?fn=foobar.pdf",
      "filename=foobar.jpg",
      "",
      "",
      "text/plain",
      L"download",
      L"foobar.jpg"
    },
    { // http://crbug.com/87719
      "http://www.example.com/image.aspx?id=blargh",
      "",
      "",
      "",
      "image/jpeg",
      L"download",
      L"image" JPEG_EXT
    },
#if defined(OS_CHROMEOS)
    { // http://crosbug.com/26028
      "http://www.example.com/fooa%cc%88.txt",
      "",
      "",
      "",
      "image/jpeg",
      L"foo\xe4",
      L"foo\xe4.txt"
    },
#endif
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(selection_tests); ++i)
    RunGenerateFileNameTestCase(&selection_tests[i], i, "selection");

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(generation_tests); ++i)
    RunGenerateFileNameTestCase(&generation_tests[i], i, "generation");

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(generation_tests); ++i) {
    GenerateFilenameCase test_case = generation_tests[i];
    test_case.referrer_charset = "GBK";
    RunGenerateFileNameTestCase(&test_case, i, "generation (referrer=GBK)");
  }
}

// This is currently a windows specific function.
#if defined(OS_WIN)
namespace {

struct GetDirectoryListingEntryCase {
  const wchar_t* name;
  const char* raw_bytes;
  bool is_dir;
  int64 filesize;
  base::Time time;
  const char* expected;
};

}  // namespace
TEST(NetUtilTest, GetDirectoryListingEntry) {
  const GetDirectoryListingEntryCase test_cases[] = {
    {L"Foo",
     "",
     false,
     10000,
     base::Time(),
     "<script>addRow(\"Foo\",\"Foo\",0,\"9.8 kB\",\"\");</script>\n"},
    {L"quo\"tes",
     "",
     false,
     10000,
     base::Time(),
     "<script>addRow(\"quo\\\"tes\",\"quo%22tes\",0,\"9.8 kB\",\"\");</script>"
         "\n"},
    {L"quo\"tes",
     "quo\"tes",
     false,
     10000,
     base::Time(),
     "<script>addRow(\"quo\\\"tes\",\"quo%22tes\",0,\"9.8 kB\",\"\");</script>"
         "\n"},
    // U+D55C0 U+AE00. raw_bytes is empty (either a local file with
    // UTF-8/UTF-16 encoding or a remote file on an ftp server using UTF-8
    {L"\xD55C\xAE00.txt",
     "",
     false,
     10000,
     base::Time(),
     "<script>addRow(\"\\uD55C\\uAE00.txt\",\"%ED%95%9C%EA%B8%80.txt\""
         ",0,\"9.8 kB\",\"\");</script>\n"},
    // U+D55C0 U+AE00. raw_bytes is the corresponding EUC-KR sequence:
    // a local or remote file in EUC-KR.
    {L"\xD55C\xAE00.txt",
     "\xC7\xD1\xB1\xDB.txt",
     false,
     10000,
     base::Time(),
     "<script>addRow(\"\\uD55C\\uAE00.txt\",\"%C7%D1%B1%DB.txt\""
         ",0,\"9.8 kB\",\"\");</script>\n"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    const std::string results = GetDirectoryListingEntry(
        WideToUTF16(test_cases[i].name),
        test_cases[i].raw_bytes,
        test_cases[i].is_dir,
        test_cases[i].filesize,
        test_cases[i].time);
    EXPECT_EQ(test_cases[i].expected, results);
  }
}

#endif

TEST(NetUtilTest, ParseHostAndPort) {
  const struct {
    const char* input;
    bool success;
    const char* expected_host;
    int expected_port;
  } tests[] = {
    // Valid inputs:
    {"foo:10", true, "foo", 10},
    {"foo", true, "foo", -1},
    {
      "[1080:0:0:0:8:800:200C:4171]:11",
      true,
      "[1080:0:0:0:8:800:200C:4171]",
      11,
    },
    // Invalid inputs:
    {"foo:bar", false, "", -1},
    {"foo:", false, "", -1},
    {":", false, "", -1},
    {":80", false, "", -1},
    {"", false, "", -1},
    {"porttoolong:300000", false, "", -1},
    {"usrname@host", false, "", -1},
    {"usrname:password@host", false, "", -1},
    {":password@host", false, "", -1},
    {":password@host:80", false, "", -1},
    {":password@host", false, "", -1},
    {"@host", false, "", -1},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string host;
    int port;
    bool ok = ParseHostAndPort(tests[i].input, &host, &port);

    EXPECT_EQ(tests[i].success, ok);

    if (tests[i].success) {
      EXPECT_EQ(tests[i].expected_host, host);
      EXPECT_EQ(tests[i].expected_port, port);
    }
  }
}

TEST(NetUtilTest, GetHostAndPort) {
  const struct {
    GURL url;
    const char* expected_host_and_port;
  } tests[] = {
    { GURL("http://www.foo.com/x"), "www.foo.com:80"},
    { GURL("http://www.foo.com:21/x"), "www.foo.com:21"},

    // For IPv6 literals should always include the brackets.
    { GURL("http://[1::2]/x"), "[1::2]:80"},
    { GURL("http://[::a]:33/x"), "[::a]:33"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string host_and_port = GetHostAndPort(tests[i].url);
    EXPECT_EQ(std::string(tests[i].expected_host_and_port), host_and_port);
  }
}

TEST(NetUtilTest, GetHostAndOptionalPort) {
  const struct {
    GURL url;
    const char* expected_host_and_port;
  } tests[] = {
    { GURL("http://www.foo.com/x"), "www.foo.com"},
    { GURL("http://www.foo.com:21/x"), "www.foo.com:21"},

    // For IPv6 literals should always include the brackets.
    { GURL("http://[1::2]/x"), "[1::2]"},
    { GURL("http://[::a]:33/x"), "[::a]:33"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string host_and_port = GetHostAndOptionalPort(tests[i].url);
    EXPECT_EQ(std::string(tests[i].expected_host_and_port), host_and_port);
  }
}

TEST(NetUtilTest, IPAddressToString) {
  uint8 addr1[4] = {0, 0, 0, 0};
  EXPECT_EQ("0.0.0.0", IPAddressToString(addr1, sizeof(addr1)));

  uint8 addr2[4] = {192, 168, 0, 1};
  EXPECT_EQ("192.168.0.1", IPAddressToString(addr2, sizeof(addr2)));

  uint8 addr3[16] = {0xFE, 0xDC, 0xBA, 0x98};
  EXPECT_EQ("fedc:ba98::", IPAddressToString(addr3, sizeof(addr3)));
}

TEST(NetUtilTest, IPAddressToStringWithPort) {
  uint8 addr1[4] = {0, 0, 0, 0};
  EXPECT_EQ("0.0.0.0:3", IPAddressToStringWithPort(addr1, sizeof(addr1), 3));

  uint8 addr2[4] = {192, 168, 0, 1};
  EXPECT_EQ("192.168.0.1:99",
            IPAddressToStringWithPort(addr2, sizeof(addr2), 99));

  uint8 addr3[16] = {0xFE, 0xDC, 0xBA, 0x98};
  EXPECT_EQ("[fedc:ba98::]:8080",
            IPAddressToStringWithPort(addr3, sizeof(addr3), 8080));
}

TEST(NetUtilTest, NetAddressToString_IPv4) {
  const struct {
    uint8 addr[4];
    const char* result;
  } tests[] = {
    {{0, 0, 0, 0}, "0.0.0.0"},
    {{127, 0, 0, 1}, "127.0.0.1"},
    {{192, 168, 0, 1}, "192.168.0.1"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SockaddrStorage storage;
    MakeIPv4Address(tests[i].addr, 80, &storage);
    std::string result = NetAddressToString(storage.addr, storage.addr_len);
    EXPECT_EQ(std::string(tests[i].result), result);
  }
}

TEST(NetUtilTest, NetAddressToString_IPv6) {
  const struct {
    uint8 addr[16];
    const char* result;
  } tests[] = {
    {{0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0xFE, 0xDC, 0xBA,
      0x98, 0x76, 0x54, 0x32, 0x10},
     "fedc:ba98:7654:3210:fedc:ba98:7654:3210"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SockaddrStorage storage;
    MakeIPv6Address(tests[i].addr, 80, &storage);
    std::string result = NetAddressToString(storage.addr, storage.addr_len);
    // Allow NetAddressToString() to fail, in case the system doesn't
    // support IPv6.
    if (!result.empty())
      EXPECT_EQ(std::string(tests[i].result), result);
  }
}

TEST(NetUtilTest, NetAddressToStringWithPort_IPv4) {
  uint8 addr[] = {127, 0, 0, 1};
  SockaddrStorage storage;
  MakeIPv4Address(addr, 166, &storage);
  std::string result = NetAddressToStringWithPort(storage.addr,
                                                  storage.addr_len);
  EXPECT_EQ("127.0.0.1:166", result);
}

TEST(NetUtilTest, NetAddressToStringWithPort_IPv6) {
  uint8 addr[] = {
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0xFE, 0xDC, 0xBA,
      0x98, 0x76, 0x54, 0x32, 0x10
  };
  SockaddrStorage storage;
  MakeIPv6Address(addr, 361, &storage);
  std::string result = NetAddressToStringWithPort(storage.addr,
                                                  storage.addr_len);

  // May fail on systems that don't support IPv6.
  if (!result.empty())
    EXPECT_EQ("[fedc:ba98:7654:3210:fedc:ba98:7654:3210]:361", result);
}

TEST(NetUtilTest, GetHostName) {
  // We can't check the result of GetHostName() directly, since the result
  // will differ across machines. Our goal here is to simply exercise the
  // code path, and check that things "look about right".
  std::string hostname = GetHostName();
  EXPECT_FALSE(hostname.empty());
}

TEST(NetUtilTest, FormatUrl) {
  FormatUrlTypes default_format_type = kFormatUrlOmitUsernamePassword;
  const UrlTestData tests[] = {
    {"Empty URL", "", "", default_format_type, UnescapeRule::NORMAL, L"", 0},

    {"Simple URL",
     "http://www.google.com/", "", default_format_type, UnescapeRule::NORMAL,
     L"http://www.google.com/", 7},

    {"With a port number and a reference",
     "http://www.google.com:8080/#\xE3\x82\xB0", "", default_format_type,
     UnescapeRule::NORMAL,
     L"http://www.google.com:8080/#\x30B0", 7},

    // -------- IDN tests --------
    {"Japanese IDN with ja",
     "http://xn--l8jvb1ey91xtjb.jp", "ja", default_format_type,
     UnescapeRule::NORMAL, L"http://\x671d\x65e5\x3042\x3055\x3072.jp/", 7},

    {"Japanese IDN with en",
     "http://xn--l8jvb1ey91xtjb.jp", "en", default_format_type,
     UnescapeRule::NORMAL, L"http://xn--l8jvb1ey91xtjb.jp/", 7},

    {"Japanese IDN without any languages",
     "http://xn--l8jvb1ey91xtjb.jp", "", default_format_type,
     UnescapeRule::NORMAL,
     // Single script is safe for empty languages.
     L"http://\x671d\x65e5\x3042\x3055\x3072.jp/", 7},

    {"mailto: with Japanese IDN",
     "mailto:foo@xn--l8jvb1ey91xtjb.jp", "ja", default_format_type,
     UnescapeRule::NORMAL,
     // GURL doesn't assume an email address's domain part as a host name.
     L"mailto:foo@xn--l8jvb1ey91xtjb.jp", 7},

    {"file: with Japanese IDN",
     "file://xn--l8jvb1ey91xtjb.jp/config.sys", "ja", default_format_type,
     UnescapeRule::NORMAL,
     L"file://\x671d\x65e5\x3042\x3055\x3072.jp/config.sys", 7},

    {"ftp: with Japanese IDN",
     "ftp://xn--l8jvb1ey91xtjb.jp/config.sys", "ja", default_format_type,
     UnescapeRule::NORMAL,
     L"ftp://\x671d\x65e5\x3042\x3055\x3072.jp/config.sys", 6},

    // -------- omit_username_password flag tests --------
    {"With username and password, omit_username_password=false",
     "http://user:passwd@example.com/foo", "",
     kFormatUrlOmitNothing, UnescapeRule::NORMAL,
     L"http://user:passwd@example.com/foo", 19},

    {"With username and password, omit_username_password=true",
     "http://user:passwd@example.com/foo", "", default_format_type,
     UnescapeRule::NORMAL, L"http://example.com/foo", 7},

    {"With username and no password",
     "http://user@example.com/foo", "", default_format_type,
     UnescapeRule::NORMAL, L"http://example.com/foo", 7},

    {"Just '@' without username and password",
     "http://@example.com/foo", "", default_format_type, UnescapeRule::NORMAL,
     L"http://example.com/foo", 7},

    // GURL doesn't think local-part of an email address is username for URL.
    {"mailto:, omit_username_password=true",
     "mailto:foo@example.com", "", default_format_type, UnescapeRule::NORMAL,
     L"mailto:foo@example.com", 7},

    // -------- unescape flag tests --------
    {"Do not unescape",
     "http://%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB.jp/"
     "%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB"
     "?q=%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB", "en", default_format_type,
     UnescapeRule::NONE,
     // GURL parses %-encoded hostnames into Punycode.
     L"http://xn--qcka1pmc.jp/%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB"
     L"?q=%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB", 7},

    {"Unescape normally",
     "http://%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB.jp/"
     "%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB"
     "?q=%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB", "en", default_format_type,
     UnescapeRule::NORMAL,
     L"http://xn--qcka1pmc.jp/\x30B0\x30FC\x30B0\x30EB"
     L"?q=\x30B0\x30FC\x30B0\x30EB", 7},

    {"Unescape normally including unescape spaces",
     "http://www.google.com/search?q=Hello%20World", "en", default_format_type,
     UnescapeRule::SPACES, L"http://www.google.com/search?q=Hello World", 7},

    /*
    {"unescape=true with some special characters",
    "http://user%3A:%40passwd@example.com/foo%3Fbar?q=b%26z", "",
    kFormatUrlOmitNothing, UnescapeRule::NORMAL,
    L"http://user%3A:%40passwd@example.com/foo%3Fbar?q=b%26z", 25},
    */
    // Disabled: the resultant URL becomes "...user%253A:%2540passwd...".

    // -------- omit http: --------
    {"omit http with user name",
     "http://user@example.com/foo", "", kFormatUrlOmitAll,
     UnescapeRule::NORMAL, L"example.com/foo", 0},

    {"omit http",
     "http://www.google.com/", "en", kFormatUrlOmitHTTP,
     UnescapeRule::NORMAL, L"www.google.com/",
     0},

    {"omit http with https",
     "https://www.google.com/", "en", kFormatUrlOmitHTTP,
     UnescapeRule::NORMAL, L"https://www.google.com/",
     8},

    {"omit http starts with ftp.",
     "http://ftp.google.com/", "en", kFormatUrlOmitHTTP,
     UnescapeRule::NORMAL, L"http://ftp.google.com/",
     7},

    // -------- omit trailing slash on bare hostname --------
    {"omit slash when it's the entire path",
     "http://www.google.com/", "en",
     kFormatUrlOmitTrailingSlashOnBareHostname, UnescapeRule::NORMAL,
     L"http://www.google.com", 7},
    {"omit slash when there's a ref",
     "http://www.google.com/#ref", "en",
     kFormatUrlOmitTrailingSlashOnBareHostname, UnescapeRule::NORMAL,
     L"http://www.google.com/#ref", 7},
    {"omit slash when there's a query",
     "http://www.google.com/?", "en",
     kFormatUrlOmitTrailingSlashOnBareHostname, UnescapeRule::NORMAL,
     L"http://www.google.com/?", 7},
    {"omit slash when it's not the entire path",
     "http://www.google.com/foo", "en",
     kFormatUrlOmitTrailingSlashOnBareHostname, UnescapeRule::NORMAL,
     L"http://www.google.com/foo", 7},
    {"omit slash for nonstandard URLs",
     "data:/", "en", kFormatUrlOmitTrailingSlashOnBareHostname,
     UnescapeRule::NORMAL, L"data:/", 5},
    {"omit slash for file URLs",
     "file:///", "en", kFormatUrlOmitTrailingSlashOnBareHostname,
     UnescapeRule::NORMAL, L"file:///", 7},

    // -------- view-source: --------
    {"view-source",
     "view-source:http://xn--qcka1pmc.jp/", "ja", default_format_type,
     UnescapeRule::NORMAL, L"view-source:http://\x30B0\x30FC\x30B0\x30EB.jp/",
     19},

    {"view-source of view-source",
     "view-source:view-source:http://xn--qcka1pmc.jp/", "ja",
     default_format_type, UnescapeRule::NORMAL,
     L"view-source:view-source:http://xn--qcka1pmc.jp/", 12},

    // view-source should omit http and trailing slash where non-view-source
    // would.
    {"view-source omit http",
     "view-source:http://a.b/c", "en", kFormatUrlOmitAll,
     UnescapeRule::NORMAL, L"view-source:a.b/c",
     12},
    {"view-source omit http starts with ftp.",
     "view-source:http://ftp.b/c", "en", kFormatUrlOmitAll,
     UnescapeRule::NORMAL, L"view-source:http://ftp.b/c",
     19},
    {"view-source omit slash when it's the entire path",
     "view-source:http://a.b/", "en", kFormatUrlOmitAll,
     UnescapeRule::NORMAL, L"view-source:a.b",
     12},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    size_t prefix_len;
    string16 formatted = FormatUrl(
        GURL(tests[i].input), tests[i].languages, tests[i].format_types,
        tests[i].escape_rules, NULL, &prefix_len, NULL);
    EXPECT_EQ(WideToUTF16(tests[i].output), formatted) << tests[i].description;
    EXPECT_EQ(tests[i].prefix_len, prefix_len) << tests[i].description;
  }
}

TEST(NetUtilTest, FormatUrlParsed) {
  // No unescape case.
  url_parse::Parsed parsed;
  string16 formatted = FormatUrl(
      GURL("http://\xE3\x82\xB0:\xE3\x83\xBC@xn--qcka1pmc.jp:8080/"
           "%E3%82%B0/?q=%E3%82%B0#\xE3\x82\xB0"),
      "ja", kFormatUrlOmitNothing, UnescapeRule::NONE, &parsed, NULL,
      NULL);
  EXPECT_EQ(WideToUTF16(
      L"http://%E3%82%B0:%E3%83%BC@\x30B0\x30FC\x30B0\x30EB.jp:8080"
      L"/%E3%82%B0/?q=%E3%82%B0#\x30B0"), formatted);
  EXPECT_EQ(WideToUTF16(L"%E3%82%B0"),
      formatted.substr(parsed.username.begin, parsed.username.len));
  EXPECT_EQ(WideToUTF16(L"%E3%83%BC"),
      formatted.substr(parsed.password.begin, parsed.password.len));
  EXPECT_EQ(WideToUTF16(L"\x30B0\x30FC\x30B0\x30EB.jp"),
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(WideToUTF16(L"8080"),
      formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(WideToUTF16(L"/%E3%82%B0/"),
      formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(WideToUTF16(L"q=%E3%82%B0"),
      formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(WideToUTF16(L"\x30B0"),
      formatted.substr(parsed.ref.begin, parsed.ref.len));

  // Unescape case.
  formatted = FormatUrl(
      GURL("http://\xE3\x82\xB0:\xE3\x83\xBC@xn--qcka1pmc.jp:8080/"
           "%E3%82%B0/?q=%E3%82%B0#\xE3\x82\xB0"),
      "ja", kFormatUrlOmitNothing, UnescapeRule::NORMAL, &parsed, NULL,
      NULL);
  EXPECT_EQ(WideToUTF16(L"http://\x30B0:\x30FC@\x30B0\x30FC\x30B0\x30EB.jp:8080"
      L"/\x30B0/?q=\x30B0#\x30B0"), formatted);
  EXPECT_EQ(WideToUTF16(L"\x30B0"),
      formatted.substr(parsed.username.begin, parsed.username.len));
  EXPECT_EQ(WideToUTF16(L"\x30FC"),
      formatted.substr(parsed.password.begin, parsed.password.len));
  EXPECT_EQ(WideToUTF16(L"\x30B0\x30FC\x30B0\x30EB.jp"),
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(WideToUTF16(L"8080"),
      formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(WideToUTF16(L"/\x30B0/"),
      formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(WideToUTF16(L"q=\x30B0"),
      formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(WideToUTF16(L"\x30B0"),
      formatted.substr(parsed.ref.begin, parsed.ref.len));

  // Omit_username_password + unescape case.
  formatted = FormatUrl(
      GURL("http://\xE3\x82\xB0:\xE3\x83\xBC@xn--qcka1pmc.jp:8080/"
           "%E3%82%B0/?q=%E3%82%B0#\xE3\x82\xB0"),
      "ja", kFormatUrlOmitUsernamePassword, UnescapeRule::NORMAL, &parsed,
      NULL, NULL);
  EXPECT_EQ(WideToUTF16(L"http://\x30B0\x30FC\x30B0\x30EB.jp:8080"
      L"/\x30B0/?q=\x30B0#\x30B0"), formatted);
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_EQ(WideToUTF16(L"\x30B0\x30FC\x30B0\x30EB.jp"),
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(WideToUTF16(L"8080"),
      formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(WideToUTF16(L"/\x30B0/"),
      formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(WideToUTF16(L"q=\x30B0"),
      formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(WideToUTF16(L"\x30B0"),
      formatted.substr(parsed.ref.begin, parsed.ref.len));

  // View-source case.
  formatted = FormatUrl(
      GURL("view-source:http://user:passwd@host:81/path?query#ref"),
      "", kFormatUrlOmitUsernamePassword, UnescapeRule::NORMAL, &parsed,
      NULL, NULL);
  EXPECT_EQ(WideToUTF16(L"view-source:http://host:81/path?query#ref"),
      formatted);
  EXPECT_EQ(WideToUTF16(L"view-source:http"),
      formatted.substr(parsed.scheme.begin, parsed.scheme.len));
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_EQ(WideToUTF16(L"host"),
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(WideToUTF16(L"81"),
      formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(WideToUTF16(L"/path"),
      formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(WideToUTF16(L"query"),
      formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(WideToUTF16(L"ref"),
      formatted.substr(parsed.ref.begin, parsed.ref.len));

  // omit http case.
  formatted = FormatUrl(
      GURL("http://host:8000/a?b=c#d"),
      "", kFormatUrlOmitHTTP, UnescapeRule::NORMAL, &parsed, NULL, NULL);
  EXPECT_EQ(WideToUTF16(L"host:8000/a?b=c#d"), formatted);
  EXPECT_FALSE(parsed.scheme.is_valid());
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_EQ(WideToUTF16(L"host"),
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(WideToUTF16(L"8000"),
      formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(WideToUTF16(L"/a"),
      formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(WideToUTF16(L"b=c"),
      formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(WideToUTF16(L"d"),
      formatted.substr(parsed.ref.begin, parsed.ref.len));

  // omit http starts with ftp case.
  formatted = FormatUrl(
      GURL("http://ftp.host:8000/a?b=c#d"),
      "", kFormatUrlOmitHTTP, UnescapeRule::NORMAL, &parsed, NULL, NULL);
  EXPECT_EQ(WideToUTF16(L"http://ftp.host:8000/a?b=c#d"), formatted);
  EXPECT_TRUE(parsed.scheme.is_valid());
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_EQ(WideToUTF16(L"http"),
      formatted.substr(parsed.scheme.begin, parsed.scheme.len));
  EXPECT_EQ(WideToUTF16(L"ftp.host"),
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(WideToUTF16(L"8000"),
      formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(WideToUTF16(L"/a"),
      formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(WideToUTF16(L"b=c"),
      formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(WideToUTF16(L"d"),
      formatted.substr(parsed.ref.begin, parsed.ref.len));

  // omit http starts with 'f' case.
  formatted = FormatUrl(
      GURL("http://f/"),
      "", kFormatUrlOmitHTTP, UnescapeRule::NORMAL, &parsed, NULL, NULL);
  EXPECT_EQ(WideToUTF16(L"f/"), formatted);
  EXPECT_FALSE(parsed.scheme.is_valid());
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_FALSE(parsed.port.is_valid());
  EXPECT_TRUE(parsed.path.is_valid());
  EXPECT_FALSE(parsed.query.is_valid());
  EXPECT_FALSE(parsed.ref.is_valid());
  EXPECT_EQ(WideToUTF16(L"f"),
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(WideToUTF16(L"/"),
      formatted.substr(parsed.path.begin, parsed.path.len));
}

// Make sure that calling FormatUrl on a GURL and then converting back to a GURL
// results in the original GURL, for each ASCII character in the path.
TEST(NetUtilTest, FormatUrlRoundTripPathASCII) {
  for (unsigned char test_char = 32; test_char < 128; ++test_char) {
    GURL url(std::string("http://www.google.com/") +
             static_cast<char>(test_char));
    size_t prefix_len;
    string16 formatted = FormatUrl(
        url, "", kFormatUrlOmitUsernamePassword, UnescapeRule::NORMAL, NULL,
        &prefix_len, NULL);
    EXPECT_EQ(url.spec(), GURL(formatted).spec());
  }
}

// Make sure that calling FormatUrl on a GURL and then converting back to a GURL
// results in the original GURL, for each escaped ASCII character in the path.
TEST(NetUtilTest, FormatUrlRoundTripPathEscaped) {
  for (unsigned char test_char = 32; test_char < 128; ++test_char) {
    std::string original_url("http://www.google.com/");
    original_url.push_back('%');
    original_url.append(base::HexEncode(&test_char, 1));

    GURL url(original_url);
    size_t prefix_len;
    string16 formatted = FormatUrl(
        url, "", kFormatUrlOmitUsernamePassword, UnescapeRule::NORMAL, NULL,
        &prefix_len, NULL);
    EXPECT_EQ(url.spec(), GURL(formatted).spec());
  }
}

// Make sure that calling FormatUrl on a GURL and then converting back to a GURL
// results in the original GURL, for each ASCII character in the query.
TEST(NetUtilTest, FormatUrlRoundTripQueryASCII) {
  for (unsigned char test_char = 32; test_char < 128; ++test_char) {
    GURL url(std::string("http://www.google.com/?") +
             static_cast<char>(test_char));
    size_t prefix_len;
    string16 formatted = FormatUrl(
        url, "", kFormatUrlOmitUsernamePassword, UnescapeRule::NORMAL, NULL,
        &prefix_len, NULL);
    EXPECT_EQ(url.spec(), GURL(formatted).spec());
  }
}

// Make sure that calling FormatUrl on a GURL and then converting back to a GURL
// only results in a different GURL for certain characters.
TEST(NetUtilTest, FormatUrlRoundTripQueryEscaped) {
  // A full list of characters which FormatURL should unescape and GURL should
  // not escape again, when they appear in a query string.
  const char* kUnescapedCharacters =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_~";
  for (unsigned char test_char = 0; test_char < 128; ++test_char) {
    std::string original_url("http://www.google.com/?");
    original_url.push_back('%');
    original_url.append(base::HexEncode(&test_char, 1));

    GURL url(original_url);
    size_t prefix_len;
    string16 formatted = FormatUrl(
        url, "", kFormatUrlOmitUsernamePassword, UnescapeRule::NORMAL, NULL,
        &prefix_len, NULL);

    if (test_char &&
        strchr(kUnescapedCharacters, static_cast<char>(test_char))) {
      EXPECT_NE(url.spec(), GURL(formatted).spec());
    } else {
      EXPECT_EQ(url.spec(), GURL(formatted).spec());
    }
  }
}

TEST(NetUtilTest, FormatUrlWithOffsets) {
  const AdjustOffsetCase null_cases[] = {
    {0, string16::npos},
  };
  CheckAdjustedOffsets(std::string(), "en", kFormatUrlOmitNothing,
      UnescapeRule::NORMAL, null_cases, arraysize(null_cases), NULL);

  const AdjustOffsetCase basic_cases[] = {
    {0, 0},
    {3, 3},
    {5, 5},
    {6, 6},
    {13, 13},
    {21, 21},
    {22, 22},
    {23, 23},
    {25, 25},
    {26, string16::npos},
    {500000, string16::npos},
    {string16::npos, string16::npos},
  };
  const size_t basic_offsets[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
     14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25};
  CheckAdjustedOffsets("http://www.google.com/foo/", "en",
                       kFormatUrlOmitNothing, UnescapeRule::NORMAL, basic_cases,
                       arraysize(basic_cases), basic_offsets);

  const AdjustOffsetCase omit_auth_cases_1[] = {
    {6, 6},
    {7, string16::npos},
    {8, string16::npos},
    {10, string16::npos},
    {12, string16::npos},
    {14, string16::npos},
    {15, 7},
    {25, 17},
  };
  const size_t omit_auth_offsets_1[] = {0, 1, 2, 3, 4, 5, 6, kNpos, kNpos,
      kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 7, 8, 9, 10, 11, 12, 13, 14, 15,
      16, 17, 18, 19, 20, 21};
  CheckAdjustedOffsets("http://foo:bar@www.google.com/", "en",
      kFormatUrlOmitUsernamePassword, UnescapeRule::NORMAL, omit_auth_cases_1,
      arraysize(omit_auth_cases_1), omit_auth_offsets_1);

  const AdjustOffsetCase omit_auth_cases_2[] = {
    {9, string16::npos},
    {11, 7},
  };
  const size_t omit_auth_offsets_2[] = {0, 1, 2, 3, 4, 5, 6, kNpos, kNpos,
      kNpos, kNpos, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};
  CheckAdjustedOffsets("http://foo@www.google.com/", "en",
      kFormatUrlOmitUsernamePassword, UnescapeRule::NORMAL, omit_auth_cases_2,
      arraysize(omit_auth_cases_2), omit_auth_offsets_2);

  // "http://foo\x30B0:\x30B0bar@www.google.com"
  const AdjustOffsetCase dont_omit_auth_cases[] = {
    {0, 0},
    /*{3, string16::npos},
    {7, 0},
    {11, 4},
    {12, string16::npos},
    {20, 5},
    {24, 9},*/
  };
  const size_t dont_omit_auth_offsets[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
      kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 11, 12, kNpos,
      kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 13, 14, 15, 16, 17, 18,
      19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
  CheckAdjustedOffsets("http://foo%E3%82%B0:%E3%82%B0bar@www.google.com/", "en",
      kFormatUrlOmitNothing, UnescapeRule::NORMAL, dont_omit_auth_cases,
      arraysize(dont_omit_auth_cases), dont_omit_auth_offsets);

  const AdjustOffsetCase view_source_cases[] = {
    {0, 0},
    {3, 3},
    {11, 11},
    {12, 12},
    {13, 13},
    {18, 18},
    {19, string16::npos},
    {20, string16::npos},
    {23, 19},
    {26, 22},
    {string16::npos, string16::npos},
  };
  const size_t view_source_offsets[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
      12, 13, 14, 15, 16, 17, 18, kNpos, kNpos, kNpos, kNpos, 19, 20, 21, 22,
      23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33};
  CheckAdjustedOffsets("view-source:http://foo@www.google.com/", "en",
      kFormatUrlOmitUsernamePassword, UnescapeRule::NORMAL, view_source_cases,
      arraysize(view_source_cases), view_source_offsets);

  // "http://\x671d\x65e5\x3042\x3055\x3072.jp/foo/"
  const AdjustOffsetCase idn_hostname_cases_1[] = {
    {8, string16::npos},
    {16, string16::npos},
    {24, string16::npos},
    {25, 12},
    {30, 17},
  };
  const size_t idn_hostname_offsets_1[] = {0, 1, 2, 3, 4, 5, 6, 7, kNpos, kNpos,
      kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
      kNpos, kNpos, kNpos, kNpos, kNpos, 12, 13, 14, 15, 16, 17, 18, 19};
  CheckAdjustedOffsets("http://xn--l8jvb1ey91xtjb.jp/foo/", "ja",
      kFormatUrlOmitNothing, UnescapeRule::NORMAL, idn_hostname_cases_1,
      arraysize(idn_hostname_cases_1), idn_hostname_offsets_1);

  // "http://test.\x89c6\x9891.\x5317\x4eac\x5927\x5b78.test/"
  const AdjustOffsetCase idn_hostname_cases_2[] = {
    {7, 7},
    {9, 9},
    {11, 11},
    {12, 12},
    {13, string16::npos},
    {23, string16::npos},
    {24, 14},
    {25, 15},
    {26, string16::npos},
    {32, string16::npos},
    {41, 19},
    {42, 20},
    {45, 23},
    {46, 24},
    {47, string16::npos},
    {string16::npos, string16::npos},
  };
  const size_t idn_hostname_offsets_2[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
      12, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
      kNpos, 14, 15, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
      kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 19, 20, 21, 22, 23, 24};
  CheckAdjustedOffsets("http://test.xn--cy2a840a.xn--1lq90ic7f1rc.test/",
                       "zh-CN", kFormatUrlOmitNothing, UnescapeRule::NORMAL,
                       idn_hostname_cases_2, arraysize(idn_hostname_cases_2),
                       idn_hostname_offsets_2);

  // "http://www.google.com/foo bar/\x30B0\x30FC\x30B0\x30EB"
  const AdjustOffsetCase unescape_cases[] = {
    {25, 25},
    {26, string16::npos},
    {27, string16::npos},
    {28, 26},
    {35, string16::npos},
    {41, 31},
    {59, 33},
    {60, string16::npos},
    {67, string16::npos},
    {68, string16::npos},
  };
  const size_t unescape_offsets[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
      13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, kNpos, kNpos, 26, 27,
      28, 29, 30, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 31,
      kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 32, kNpos, kNpos,
      kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 33, kNpos, kNpos, kNpos, kNpos,
      kNpos, kNpos, kNpos, kNpos};
  CheckAdjustedOffsets(
      "http://www.google.com/foo%20bar/%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB",
      "en", kFormatUrlOmitNothing, UnescapeRule::SPACES, unescape_cases,
      arraysize(unescape_cases), unescape_offsets);

  // "http://www.google.com/foo.html#\x30B0\x30B0z"
  const AdjustOffsetCase ref_cases[] = {
    {30, 30},
    {31, 31},
    {32, string16::npos},
    {34, 32},
    {35, string16::npos},
    {37, 33},
    {38, string16::npos},
  };
  const size_t ref_offsets[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
      14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      kNpos, kNpos, 32, kNpos, kNpos, 33};
  CheckAdjustedOffsets(
      "http://www.google.com/foo.html#\xE3\x82\xB0\xE3\x82\xB0z", "en",
      kFormatUrlOmitNothing, UnescapeRule::NORMAL, ref_cases,
      arraysize(ref_cases), ref_offsets);

  const AdjustOffsetCase omit_http_cases[] = {
    {0, string16::npos},
    {3, string16::npos},
    {7, 0},
    {8, 1},
  };
  const size_t omit_http_offsets[] = {kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
      kNpos, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
  CheckAdjustedOffsets("http://www.google.com/", "en",
      kFormatUrlOmitHTTP, UnescapeRule::NORMAL, omit_http_cases,
      arraysize(omit_http_cases), omit_http_offsets);

  const AdjustOffsetCase omit_http_start_with_ftp_cases[] = {
    {0, 0},
    {3, 3},
    {8, 8},
  };
  const size_t omit_http_start_with_ftp_offsets[] = {0, 1, 2, 3, 4, 5, 6, 7, 8,
      9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};
  CheckAdjustedOffsets("http://ftp.google.com/", "en", kFormatUrlOmitHTTP,
                       UnescapeRule::NORMAL, omit_http_start_with_ftp_cases,
                       arraysize(omit_http_start_with_ftp_cases),
                       omit_http_start_with_ftp_offsets);

  const AdjustOffsetCase omit_all_cases[] = {
    {12, 0},
    {13, 1},
    {0, string16::npos},
    {3, string16::npos},
  };
  const size_t omit_all_offsets[] = {kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
      kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 0, 1, 2, 3, 4, 5, 6, kNpos};
  CheckAdjustedOffsets("http://user@foo.com/", "en", kFormatUrlOmitAll,
                       UnescapeRule::NORMAL, omit_all_cases,
                       arraysize(omit_all_cases), omit_all_offsets);
}

TEST(NetUtilTest, SimplifyUrlForRequest) {
  struct {
    const char* input_url;
    const char* expected_simplified_url;
  } tests[] = {
    {
      // Reference section should be stripped.
      "http://www.google.com:78/foobar?query=1#hash",
      "http://www.google.com:78/foobar?query=1",
    },
    {
      // Reference section can itself contain #.
      "http://192.168.0.1?query=1#hash#10#11#13#14",
      "http://192.168.0.1?query=1",
    },
    { // Strip username/password.
      "http://user:pass@google.com",
      "http://google.com/",
    },
    { // Strip both the reference and the username/password.
      "http://user:pass@google.com:80/sup?yo#X#X",
      "http://google.com/sup?yo",
    },
    { // Try an HTTPS URL -- strip both the reference and the username/password.
      "https://user:pass@google.com:80/sup?yo#X#X",
      "https://google.com:80/sup?yo",
    },
    { // Try an FTP URL -- strip both the reference and the username/password.
      "ftp://user:pass@google.com:80/sup?yo#X#X",
      "ftp://google.com:80/sup?yo",
    },
    { // Try an nonstandard URL
      "foobar://user:pass@google.com:80/sup?yo#X#X",
      "foobar://user:pass@google.com:80/sup?yo#X#X",
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    tests[i].input_url));
    GURL input_url(GURL(tests[i].input_url));
    GURL expected_url(GURL(tests[i].expected_simplified_url));
    EXPECT_EQ(expected_url, SimplifyUrlForRequest(input_url));
  }
}

TEST(NetUtilTest, SetExplicitlyAllowedPortsTest) {
  std::string invalid[] = { "1,2,a", "'1','2'", "1, 2, 3", "1 0,11,12" };
  std::string valid[] = { "", "1", "1,2", "1,2,3", "10,11,12,13" };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(invalid); ++i) {
    SetExplicitlyAllowedPorts(invalid[i]);
    EXPECT_EQ(0, static_cast<int>(GetCountOfExplicitlyAllowedPorts()));
  }

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(valid); ++i) {
    SetExplicitlyAllowedPorts(valid[i]);
    EXPECT_EQ(i, GetCountOfExplicitlyAllowedPorts());
  }
}

TEST(NetUtilTest, GetHostOrSpecFromURL) {
  EXPECT_EQ("example.com",
            GetHostOrSpecFromURL(GURL("http://example.com/test")));
  EXPECT_EQ("example.com",
            GetHostOrSpecFromURL(GURL("http://example.com./test")));
  EXPECT_EQ("file:///tmp/test.html",
            GetHostOrSpecFromURL(GURL("file:///tmp/test.html")));
}

// Test that invalid IP literals fail to parse.
TEST(NetUtilTest, ParseIPLiteralToNumber_FailParse) {
  IPAddressNumber number;

  EXPECT_FALSE(ParseIPLiteralToNumber("bad value", &number));
  EXPECT_FALSE(ParseIPLiteralToNumber("bad:value", &number));
  EXPECT_FALSE(ParseIPLiteralToNumber("", &number));
  EXPECT_FALSE(ParseIPLiteralToNumber("192.168.0.1:30", &number));
  EXPECT_FALSE(ParseIPLiteralToNumber("  192.168.0.1  ", &number));
  EXPECT_FALSE(ParseIPLiteralToNumber("[::1]", &number));
}

// Test parsing an IPv4 literal.
TEST(NetUtilTest, ParseIPLiteralToNumber_IPv4) {
  IPAddressNumber number;
  EXPECT_TRUE(ParseIPLiteralToNumber("192.168.0.1", &number));
  EXPECT_EQ("192,168,0,1", DumpIPNumber(number));
}

// Test parsing an IPv6 literal.
TEST(NetUtilTest, ParseIPLiteralToNumber_IPv6) {
  IPAddressNumber number;
  EXPECT_TRUE(ParseIPLiteralToNumber("1:abcd::3:4:ff", &number));
  EXPECT_EQ("0,1,171,205,0,0,0,0,0,0,0,3,0,4,0,255", DumpIPNumber(number));
}

// Test mapping an IPv4 address to an IPv6 address.
TEST(NetUtilTest, ConvertIPv4NumberToIPv6Number) {
  IPAddressNumber ipv4_number;
  EXPECT_TRUE(ParseIPLiteralToNumber("192.168.0.1", &ipv4_number));

  IPAddressNumber ipv6_number =
      ConvertIPv4NumberToIPv6Number(ipv4_number);

  // ::ffff:192.168.1.1
  EXPECT_EQ("0,0,0,0,0,0,0,0,0,0,255,255,192,168,0,1",
            DumpIPNumber(ipv6_number));
}

TEST(NetUtilTest, IsIPv4Mapped) {
  IPAddressNumber ipv4_number;
  EXPECT_TRUE(ParseIPLiteralToNumber("192.168.0.1", &ipv4_number));
  EXPECT_FALSE(IsIPv4Mapped(ipv4_number));

  IPAddressNumber ipv6_number;
  EXPECT_TRUE(ParseIPLiteralToNumber("::1", &ipv4_number));
  EXPECT_FALSE(IsIPv4Mapped(ipv6_number));

  IPAddressNumber ipv4mapped_number;
  EXPECT_TRUE(ParseIPLiteralToNumber("::ffff:0101:1", &ipv4mapped_number));
  EXPECT_TRUE(IsIPv4Mapped(ipv4mapped_number));
}

TEST(NetUtilTest, ConvertIPv4MappedToIPv4) {
  IPAddressNumber ipv4mapped_number;
  EXPECT_TRUE(ParseIPLiteralToNumber("::ffff:0101:1", &ipv4mapped_number));
  IPAddressNumber expected;
  EXPECT_TRUE(ParseIPLiteralToNumber("1.1.0.1", &expected));
  IPAddressNumber result = ConvertIPv4MappedToIPv4(ipv4mapped_number);
  EXPECT_EQ(expected, result);
}

// Test parsing invalid CIDR notation literals.
TEST(NetUtilTest, ParseCIDRBlock_Invalid) {
  const char* bad_literals[] = {
      "foobar",
      "",
      "192.168.0.1",
      "::1",
      "/",
      "/1",
      "1",
      "192.168.1.1/-1",
      "192.168.1.1/33",
      "::1/-3",
      "a::3/129",
      "::1/x",
      "192.168.0.1//11"
  };

  for (size_t i = 0; i < arraysize(bad_literals); ++i) {
    IPAddressNumber ip_number;
    size_t prefix_length_in_bits;

    EXPECT_FALSE(ParseCIDRBlock(bad_literals[i],
                                     &ip_number,
                                     &prefix_length_in_bits));
  }
}

// Test parsing a valid CIDR notation literal.
TEST(NetUtilTest, ParseCIDRBlock_Valid) {
  IPAddressNumber ip_number;
  size_t prefix_length_in_bits;

  EXPECT_TRUE(ParseCIDRBlock("192.168.0.1/11",
                                  &ip_number,
                                  &prefix_length_in_bits));

  EXPECT_EQ("192,168,0,1", DumpIPNumber(ip_number));
  EXPECT_EQ(11u, prefix_length_in_bits);
}

TEST(NetUtilTest, IPNumberMatchesPrefix) {
  struct {
    const char* cidr_literal;
    const char* ip_literal;
    bool expected_to_match;
  } tests[] = {
    // IPv4 prefix with IPv4 inputs.
    {
      "10.10.1.32/27",
      "10.10.1.44",
      true
    },
    {
      "10.10.1.32/27",
      "10.10.1.90",
      false
    },
    {
      "10.10.1.32/27",
      "10.10.1.90",
      false
    },

    // IPv6 prefix with IPv6 inputs.
    {
      "2001:db8::/32",
      "2001:DB8:3:4::5",
      true
    },
    {
      "2001:db8::/32",
      "2001:c8::",
      false
    },

    // IPv6 prefix with IPv4 inputs.
    {
      "2001:db8::/33",
      "192.168.0.1",
      false
    },
    {
      "::ffff:192.168.0.1/112",
      "192.168.33.77",
      true
    },

    // IPv4 prefix with IPv6 inputs.
    {
      "10.11.33.44/16",
      "::ffff:0a0b:89",
      true
    },
    {
      "10.11.33.44/16",
      "::ffff:10.12.33.44",
      false
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s, %s", i,
                                    tests[i].cidr_literal,
                                    tests[i].ip_literal));

    IPAddressNumber ip_number;
    EXPECT_TRUE(ParseIPLiteralToNumber(tests[i].ip_literal, &ip_number));

    IPAddressNumber ip_prefix;
    size_t prefix_length_in_bits;

    EXPECT_TRUE(ParseCIDRBlock(tests[i].cidr_literal,
                               &ip_prefix,
                               &prefix_length_in_bits));

    EXPECT_EQ(tests[i].expected_to_match,
              IPNumberMatchesPrefix(ip_number,
                                    ip_prefix,
                                    prefix_length_in_bits));
  }
}

TEST(NetUtilTest, IsLocalhost) {
  EXPECT_TRUE(net::IsLocalhost("localhost"));
  EXPECT_TRUE(net::IsLocalhost("localhost.localdomain"));
  EXPECT_TRUE(net::IsLocalhost("localhost6"));
  EXPECT_TRUE(net::IsLocalhost("localhost6.localdomain6"));
  EXPECT_TRUE(net::IsLocalhost("127.0.0.1"));
  EXPECT_TRUE(net::IsLocalhost("127.0.1.0"));
  EXPECT_TRUE(net::IsLocalhost("127.1.0.0"));
  EXPECT_TRUE(net::IsLocalhost("127.0.0.255"));
  EXPECT_TRUE(net::IsLocalhost("127.0.255.0"));
  EXPECT_TRUE(net::IsLocalhost("127.255.0.0"));
  EXPECT_TRUE(net::IsLocalhost("::1"));
  EXPECT_TRUE(net::IsLocalhost("0:0:0:0:0:0:0:1"));

  EXPECT_FALSE(net::IsLocalhost("localhostx"));
  EXPECT_FALSE(net::IsLocalhost("foo.localdomain"));
  EXPECT_FALSE(net::IsLocalhost("localhost6x"));
  EXPECT_FALSE(net::IsLocalhost("localhost.localdomain6"));
  EXPECT_FALSE(net::IsLocalhost("localhost6.localdomain"));
  EXPECT_FALSE(net::IsLocalhost("127.0.0.1.1"));
  EXPECT_FALSE(net::IsLocalhost(".127.0.0.255"));
  EXPECT_FALSE(net::IsLocalhost("::2"));
  EXPECT_FALSE(net::IsLocalhost("::1:1"));
  EXPECT_FALSE(net::IsLocalhost("0:0:0:0:1:0:0:1"));
  EXPECT_FALSE(net::IsLocalhost("::1:1"));
  EXPECT_FALSE(net::IsLocalhost("0:0:0:0:0:0:0:0:1"));
}

// Verify GetNetworkList().
TEST(NetUtilTest, GetNetworkList) {
  NetworkInterfaceList list;
  ASSERT_TRUE(GetNetworkList(&list));

  for (NetworkInterfaceList::iterator it = list.begin();
       it != list.end(); ++it) {
    // Verify that the name is not empty.
    EXPECT_FALSE(it->name.empty());

    // Verify that the address is correct.
    EXPECT_TRUE(it->address.size() == kIPv4AddressSize ||
                it->address.size() == kIPv6AddressSize)
        << "Invalid address of size " << it->address.size();
    bool all_zeroes = true;
    for (size_t i = 0; i < it->address.size(); ++i) {
      if (it->address[i] != 0) {
        all_zeroes = false;
        break;
      }
    }
    EXPECT_FALSE(all_zeroes);
  }
}

}  // namespace net
