// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_filename_util.h"

#import <string>
#import <string_view>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using ::download_model::NormalizeFileName;

using DownloadFilenameUtilTest = PlatformTest;

// Empty input must not crash.
TEST_F(DownloadFilenameUtilTest, EmptyString) {
  EXPECT_EQ("", NormalizeFileName(""));
  EXPECT_EQ("", NormalizeFileName(std::string_view()));
}

// Plain ASCII: case-folded, otherwise unchanged.
TEST_F(DownloadFilenameUtilTest, AsciiCaseFold) {
  EXPECT_EQ("report.pdf", NormalizeFileName("report.pdf"));
  EXPECT_EQ("report.pdf", NormalizeFileName("REPORT.PDF"));
  EXPECT_EQ("report.pdf", NormalizeFileName("Report.Pdf"));
  EXPECT_EQ("my file_v2.zip", NormalizeFileName("My File_v2.zip"));
}

// NFC and NFD inputs of the same text must produce identical output.
TEST_F(DownloadFilenameUtilTest, NfcAndNfdProduceSameOutput) {
  // NFC: U+00E9 ("é").
  const std::string nfc = "caf\xC3\xA9.pdf";
  // NFD: "e" + U+0301 (combining acute).
  const std::string nfd = "cafe\xCC\x81.pdf";
  EXPECT_EQ(NormalizeFileName(nfc), NormalizeFileName(nfd));
  EXPECT_EQ("cafe.pdf", NormalizeFileName(nfc));
}

// Common Latin diacritics are stripped after NFD decomposition.
TEST_F(DownloadFilenameUtilTest, StripsLatinDiacritics) {
  // "naïve" → "naive".
  EXPECT_EQ("naive.txt", NormalizeFileName("na\xC3\xAFve.txt"));
  // "Über" → "uber".
  EXPECT_EQ("uber.log",
            NormalizeFileName("\xC3\x9C"
                              "ber.log"));
  // "résumé" → "resume".
  EXPECT_EQ("resume.docx",
            NormalizeFileName("r\xC3\xA9sum\xC3\xA9.docx"));
  // "façade" → "facade" (cedilla is Mn after NFD).
  EXPECT_EQ("facade.pdf",
            NormalizeFileName("fa\xC3\xA7"
                              "ade.pdf"));
  // "piñata" → "pinata".
  EXPECT_EQ("pinata.jpg",
            NormalizeFileName("pi\xC3\xB1"
                              "ata.jpg"));
}

// Stacked combining marks are all stripped.
TEST_F(DownloadFilenameUtilTest, StripsMultipleCombiningMarks) {
  // "a" + U+0301 + U+0302 + U+0303.
  EXPECT_EQ("a.txt", NormalizeFileName("a\xCC\x81\xCC\x82\xCC\x83.txt"));
}

// Greek base letters survive; case is folded. Default foldCase maps Σ to
// the mid form σ (not the final form ς), so "ΕΛΛΑΣ" → "ελλασ".
TEST_F(DownloadFilenameUtilTest, GreekCaseFold) {
  EXPECT_EQ("\xCE\xB5\xCE\xBB\xCE\xBB\xCE\xB1\xCF\x83",
            NormalizeFileName("\xCE\x95\xCE\x9B\xCE\x9B\xCE\x91\xCE\xA3"));
}

// CJK has no case and no Mn marks; bytes round-trip unchanged.
TEST_F(DownloadFilenameUtilTest, CjkUnchanged) {
  // "下载文件.pdf".
  const std::string cjk =
      "\xE4\xB8\x8B\xE8\xBD\xBD\xE6\x96\x87\xE4\xBB\xB6.pdf";
  EXPECT_EQ(cjk, NormalizeFileName(cjk));
  // "あいウエ.txt".
  const std::string jp =
      "\xE3\x81\x82\xE3\x81\x84\xE3\x82\xA6\xE3\x82\xA8.txt";
  EXPECT_EQ(jp, NormalizeFileName(jp));
}

// Each script in a mixed-script name is handled independently.
TEST_F(DownloadFilenameUtilTest, MixedScripts) {
  // "Über_测试_файл.txt" → "uber_测试_фаил.txt".
  // Cyrillic "й" (U+0439) decomposes to "и" + combining breve; the breve is
  // a non-spacing mark and is stripped, just like Latin diacritics.
  const std::string input =
      "\xC3\x9C"
      "ber_\xE6\xB5\x8B\xE8\xAF\x95_\xD1\x84\xD0\xB0\xD0\xB9\xD0\xBB.txt";
  const std::string expected =
      "uber_\xE6\xB5\x8B\xE8\xAF\x95_\xD1\x84\xD0\xB0\xD0\xB8\xD0\xBB.txt";
  EXPECT_EQ(expected, NormalizeFileName(input));
}

// Default foldCase is full folding: "ß" → "ss".
TEST_F(DownloadFilenameUtilTest, SharpSExpandsToSs) {
  // "Straße" → "strasse".
  EXPECT_EQ("strasse.txt",
            NormalizeFileName("Stra\xC3\x9F"
                              "e.txt"));
}

// Cyrillic case folding.
TEST_F(DownloadFilenameUtilTest, CyrillicCaseFold) {
  // "ПРИВЕТ" → "привет".
  EXPECT_EQ(
      "\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82",
      NormalizeFileName("\xD0\x9F\xD0\xA0\xD0\x98\xD0\x92\xD0\x95\xD0\xA2"));
}

// Normalizing twice is a no-op.
TEST_F(DownloadFilenameUtilTest, Idempotent) {
  const std::string inputs[] = {
      "report.pdf",
      "Caf\xC3\xA9.PDF",
      "Stra\xC3\x9F"
      "e.txt",
      "\xE4\xB8\x8B\xE8\xBD\xBD.zip",  // "下载.zip"
  };
  for (const auto& input : inputs) {
    const std::string once = NormalizeFileName(input);
    EXPECT_EQ(once, NormalizeFileName(once)) << "input=" << input;
  }
}

// Primary use case: normalized substring search ignores case and diacritics
// on both sides.
TEST_F(DownloadFilenameUtilTest, SubstringSearchUseCase) {
  // "Mon Café Résumé.PDF" → "mon cafe resume.pdf".
  const std::string norm = NormalizeFileName(
      "Mon Caf\xC3\xA9 R\xC3\xA9sum\xC3\xA9.PDF");

  EXPECT_NE(std::string::npos, norm.find(NormalizeFileName("cafe")));
  EXPECT_NE(std::string::npos, norm.find(NormalizeFileName("CAFE")));
  EXPECT_NE(std::string::npos, norm.find(NormalizeFileName("café")));
  EXPECT_NE(std::string::npos, norm.find(NormalizeFileName("résumé")));
  EXPECT_NE(std::string::npos, norm.find(NormalizeFileName(".pdf")));
  EXPECT_EQ(std::string::npos, norm.find(NormalizeFileName("xyz")));
}

// Whitespace and punctuation are neither Mn nor cased: preserved as-is.
TEST_F(DownloadFilenameUtilTest, PreservesWhitespaceAndPunctuation) {
  EXPECT_EQ("my report (final) - v2.pdf",
            NormalizeFileName("My Report (Final) - v2.PDF"));
  EXPECT_EQ("\t \nfoo.txt", NormalizeFileName("\t \nFoo.txt"));
}

// A string of only combining marks reduces to empty.
TEST_F(DownloadFilenameUtilTest, OnlyCombiningMarks) {
  EXPECT_EQ("", NormalizeFileName("\xCC\x81\xCC\x82\xCC\x83"));
}

}  // namespace
