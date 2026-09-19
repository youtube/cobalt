// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iterator>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "build/build_config.h"
#include "cobalt/shell/common/shell_switches.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_switches.h"
#include "url/gurl.h"

namespace cobalt {

namespace {

// The 83 language and regional variants officially supported by YouTube on TV.
const char* const kYouTubeSupportedLocales[] = {
    "af",  "sq",      "am", "ar",    "hy",    "as",    "az",     "bn",
    "eu",  "be",      "bs", "bg",    "ca",    "zh",    "zh-HK",  "zh-TW",
    "hr",  "cs",      "da", "nl",    "en",    "en-IN", "en-GB",  "et",
    "fil", "fi",      "fr", "fr-CA", "gl",    "ka",    "de",     "el",
    "gu",  "he",      "hi", "hu",    "is",    "id",    "it",     "ja",
    "kn",  "kk",      "km", "ko",    "ky",    "lo",    "lv",     "lt",
    "mk",  "ms",      "ml", "mr",    "mn",    "my",    "ne",     "no",
    "or",  "fa",      "pl", "pt",    "pt-PT", "pa",    "ro",     "ru",
    "sr",  "sr-Latn", "si", "sk",    "sl",    "es",    "es-419", "es-US",
    "sw",  "sv",      "ta", "te",    "th",    "tr",    "uk",     "ur",
    "uz",  "vi",      "zu",
};

std::string BuildLocalesArrayJs() {
  std::string js = "[";
  for (size_t i = 0; i < std::size(kYouTubeSupportedLocales); ++i) {
    if (i > 0) {
      js += ", ";
    }
    js += "'";
    js += kYouTubeSupportedLocales[i];
    js += "'";
  }
  js += "]";
  return js;
}

}  // namespace

// Browser test fixture for validating internationalization (i18n) behavior
// across various locales and scripts. This class is instantiated and owned by
// the gtest framework on the main test thread.
class I18nBrowserTest : public content::ContentBrowserTest {
 public:
  I18nBrowserTest() = default;
  ~I18nBrowserTest() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kSingleProcess);
#if BUILDFLAG(IS_STARBOARD) && !BUILDFLAG(IS_LINUX)
    // Physical Starboard devices (such as RDK) use hardware EGL drivers that
    // do not support ANGLE's software GL context attributes.
    command_line->AppendSwitch(switches::kUseGpuInTests);
#endif
    if (!command_line->HasSwitch(switches::kContentShellUserDataDir)) {
      CHECK(temp_user_data_dir_.CreateUniqueTempDir());
      command_line->AppendSwitchPath(switches::kContentShellUserDataDir,
                                     temp_user_data_dir_.GetPath());
    }
  }

  void SetUpPage() {
    ASSERT_TRUE(NavigateToURL(
        shell()->web_contents(),
        GURL("data:text/html,<html><head><title>i18n</title></head><body></"
             "body></html>")));
  }

 private:
  base::ScopedTempDir temp_user_data_dir_;
};

// 1. Verify Blink native UTF-8 TextDecoder/TextEncoder handles multilingual
// text without requiring ICU legacy .cnv character conversion tables.
IN_PROC_BROWSER_TEST_F(I18nBrowserTest, MultilingualUtf8TextDecoding) {
  SetUpPage();

  const std::string script = R"(
    (() => {
      const decoder = new TextDecoder('utf-8');
      const encoder = new TextEncoder();

      // Multilingual test samples across major scripts.
      const samples = [
        'Hello World!',
        'Café, résumé, naïve',
        'Ελληνική γλώσσα',
        'Русский язык',
        'اللغة العربية',
        'עִבְרִית',
        'हिन्दी भाषा',
        'ภาษาไทย',
        '简体中文与繁體中文',
        '日本語のひらがな・カタカナ・漢字',
        '한국어 텍스트',
        '🎉🚀🌏🔥✨',
      ];

      for (const sample of samples) {
        const encoded = encoder.encode(sample);
        const decoded = decoder.decode(encoded);
        if (decoded !== sample) {
          return `Mismatch for "${sample}": got "${decoded}"`;
        }
      }

      // Invalid UTF-8 sequence should decode with replacement character without throwing.
      const invalidBytes = new Uint8Array([0xFF, 0xFE, 0x41, 0x42]);
      const decodedInvalid = decoder.decode(invalidBytes);
      if (!decodedInvalid.includes('\uFFFD') || !decodedInvalid.includes('AB')) {
        return `Unexpected invalid byte decoding: "${decodedInvalid}"`;
      }

      return 'SUCCESS';
    })()
  )";

  EXPECT_EQ("SUCCESS",
            content::EvalJs(shell()->web_contents(), script).ExtractString());
}

// 2. Verify that Intl.Collator and String.prototype.localeCompare work across
// all 83 YouTube locales without throwing exceptions, using V8's root UCA
// fallback.
IN_PROC_BROWSER_TEST_F(I18nBrowserTest, CollationAcrossAllYouTubeLocales) {
  SetUpPage();

  const std::string script = R"(
    (() => {
      const locales = )" + BuildLocalesArrayJs() +
                             R"(;
      // Note: Testing ASCII words across all locales verifies that V8's root
      // DUCET (Default Unicode Collation Element Table) collation fallback
      // is intact and functional without throwing exceptions, even for locales
      // where language-specific collation tailorings have been pruned.
      for (const loc of locales) {
        try {
          const collator = new Intl.Collator(loc);
          const cmp = collator.compare('apple', 'banana');
          if (cmp >= 0) {
            return `Intl.Collator failed for locale ${loc}: expected negative, got ${cmp}`;
          }

          const eq = 'test'.localeCompare('test', loc);
          if (eq !== 0) {
            return `localeCompare equality failed for locale ${loc}: expected 0, got ${eq}`;
          }

          const words = ['orange', 'apple', 'banana'];
          words.sort((a, b) => a.localeCompare(b, loc));
          if (words[0] !== 'apple' || words[1] !== 'banana' || words[2] !== 'orange') {
            return `Sorting failed for locale ${loc}: got [${words.join(', ')}]`;
          }
        } catch (e) {
          return `Exception for locale ${loc}: ${e.message}`;
        }
      }
      return 'SUCCESS';
    })()
  )";

  EXPECT_EQ("SUCCESS",
            content::EvalJs(shell()->web_contents(), script).ExtractString());
}

// 3. Verify Gregorian Date/Time formatting across all 83 YouTube locales.
IN_PROC_BROWSER_TEST_F(I18nBrowserTest,
                       GregorianDateTimeFormattingAcrossLocales) {
  SetUpPage();

  const std::string script = R"(
    (() => {
      const locales = )" + BuildLocalesArrayJs() +
                             R"(;
      const testDate = new Date(Date.UTC(2026, 8, 14, 12, 0, 0));

      for (const loc of locales) {
        try {
          const formatter = new Intl.DateTimeFormat(loc, {
            year: 'numeric',
            month: 'long',
            day: 'numeric'
          });
          const formatted = formatter.format(testDate);
          if (!formatted || formatted.length === 0) {
            return `Empty date string for locale ${loc}`;
          }

          const localString = testDate.toLocaleDateString(loc);
          if (!localString || localString.length === 0) {
            return `Empty toLocaleDateString for locale ${loc}`;
          }
        } catch (e) {
          return `Exception in date formatting for locale ${loc}: ${e.message}`;
        }
      }
      // Specific check for French to ensure localized month names are resolved correctly
      // and not falling back to English or numeric month representations.
      const frFormatter = new Intl.DateTimeFormat('fr', { month: 'long' });
      const frMonth = frFormatter.format(testDate);
      if (!frMonth.toLowerCase().includes('septembre')) {
        return `French month name mismatch: expected "septembre", got "${frMonth}"`;
      }

      return 'SUCCESS';
    })()
  )";

  EXPECT_EQ("SUCCESS",
            content::EvalJs(shell()->web_contents(), script).ExtractString());
}

// 4. Verify non-Gregorian calendar support for YouTube-supported locales that
// use traditional calendars (Thai Buddhist, Japanese Imperial, ROC Minguo,
// Persian, Islamic, Hebrew, Indian Saka), and graceful fallback for others.
IN_PROC_BROWSER_TEST_F(I18nBrowserTest, NonGregorianCalendarSupport) {
  SetUpPage();

  const std::string script = R"(
    (() => {
      // 2026-09-14 UTC
      const testDate = new Date(Date.UTC(2026, 8, 14));

      // Specific YouTube locales with their native non-Gregorian calendars.
      const calendarExpectations = [
        {
          loc: 'th-u-ca-buddhist',
          options: { year: 'numeric' },
          expected: ['2569'],
          desc: 'Thai Buddhist Era (2026 + 543 = 2569 BE)'
        },
        {
          loc: 'ja-u-ca-japanese',
          options: { era: 'short', year: 'numeric' },
          expected: ['\u4EE4\u548C', '8'],  // 令和8年 (Reiwa 8)
          desc: 'Japanese Imperial Era (Reiwa 8)'
        },
        {
          loc: 'zh-TW-u-ca-roc',
          options: { era: 'short', year: 'numeric' },
          expected: ['\u6C11\u570B', '115'],  // 民國115年 (Minguo 115)
          desc: 'Republic of China / Minguo Era (Minguo 115)'
        },
        {
          loc: 'fa-u-ca-persian',
          options: { year: 'numeric' },
          expected: ['\u06F1\u06F4\u06F0\u06F5', '1405'],  // 1405 Solar Hijri
          desc: 'Persian Solar Hijri calendar (1405 SH)'
        },
        {
          loc: 'ar-u-ca-islamic',
          options: { year: 'numeric' },
          expected: ['1448', '\u0661\u0664\u0664\u0638'],  // 1448 Islamic Hijri
          desc: 'Islamic Hijri calendar (1448 AH)'
        },
        {
          loc: 'he-u-ca-hebrew',
          options: { year: 'numeric' },
          expected: ['5787', '5786', '\u05EA\u05E9\u05E4'],
          desc: 'Hebrew calendar (5787 AM)'
        },
        {
          loc: 'hi-u-ca-indian',
          options: { year: 'numeric' },
          expected: ['1948', '\u0967\u096F\u096A\u096C'],  // 1948 Saka
          desc: 'Indian National Saka calendar (1948 Saka)'
        },
      ];

      for (const item of calendarExpectations) {
        try {
          const formatter = new Intl.DateTimeFormat(item.loc, item.options);
          const formatted = formatter.format(testDate);
          if (!formatted || formatted.length === 0) {
            return `Empty date string for ${item.desc} (${item.loc})`;
          }
          const matched = item.expected.some(pat => formatted.includes(pat));
          if (!matched) {
            return `Incorrect calendar formatting for ${item.desc} (${item.loc}): got "${formatted}", expected pattern in [${item.expected.join(', ')}]`;
          }
        } catch (e) {
          return `Exception for ${item.desc} (${item.loc}): ${e.message}`;
        }
      }

      // Also verify that arbitrary non-Gregorian requests gracefully format
      // without throwing exceptions even if using fallback.
      const fallbackLocales = ['en-u-ca-buddhist', 'de-u-ca-persian', 'fr-u-ca-islamic'];
      for (const loc of fallbackLocales) {
        try {
          const formatter = new Intl.DateTimeFormat(loc);
          const formatted = formatter.format(testDate);
          if (!formatted || formatted.length === 0) {
            return `Empty fallback date string for ${loc}`;
          }
        } catch (e) {
          return `Exception for fallback locale ${loc}: ${e.message}`;
        }
      }

      return 'SUCCESS';
    })()
  )";

  EXPECT_EQ("SUCCESS",
            content::EvalJs(shell()->web_contents(), script).ExtractString());
}

// 5. Verify number formatting across all 83 YouTube locales.
IN_PROC_BROWSER_TEST_F(I18nBrowserTest, NumberFormattingAcrossLocales) {
  SetUpPage();

  const std::string script = R"(
    (() => {
      const locales = )" + BuildLocalesArrayJs() +
                             R"(;
      const testNumber = 1234567.89;

      for (const loc of locales) {
        try {
          const formatter = new Intl.NumberFormat(loc);
          const formatted = formatter.format(testNumber);
          if (!formatted || formatted.length === 0) {
            return `Empty formatted number for locale ${loc}`;
          }

          const localString = testNumber.toLocaleString(loc);
          if (!localString || localString.length === 0) {
            return `Empty toLocaleString for locale ${loc}`;
          }
        } catch (e) {
          return `Exception in number formatting for locale ${loc}: ${e.message}`;
        }
      }
      return 'SUCCESS';
    })()
  )";

  EXPECT_EQ("SUCCESS",
            content::EvalJs(shell()->web_contents(), script).ExtractString());
}

// 6. Verify graceful fallback for non-supported regional sub-locales versus
// officially supported locales and their regional variants.
//
// YouTube officially supports 83 languages and specific regional variants
// (e.g., fr, fr-CA, es-ES, es-419, es-US, en, en-GB, en-IN, pt-BR, pt-PT,
// zh, zh-HK, zh-TW, sr, sr-Latn).
//
// Other regional sub-locales (such as fr-SN, fr-BE, es-CO, es-AR, en-AU,
// pt-AO, ar-EG, zh-SG) are not part of the officially supported YouTube list.
// This test verifies that:
// 1. Officially supported locales and their regional variants resolve and
// format
//    successfully.
// 2. Non-supported regional sub-locales gracefully fall back to their parent
//    language (e.g., fr-SN -> fr, es-CO -> es, en-AU -> en) without throwing
//    exceptions, producing valid formatted date, number, and collation outputs.
IN_PROC_BROWSER_TEST_F(I18nBrowserTest, RegionalSubLocalesGracefulFallback) {
  SetUpPage();

  const std::string script = R"(
    (() => {
      // Officially supported locales with regional variants.
      const officialRegionalLocales = [
        'fr', 'fr-CA',
        'es-ES', 'es-419', 'es-US',
        'en', 'en-GB', 'en-IN',
        'pt-BR', 'pt-PT',
        'zh', 'zh-HK', 'zh-TW',
        'sr', 'sr-Latn'
      ];

      // Non-supported regional sub-locales that should gracefully fall back
      // to their base language.
      const nonSupportedRegionalSubLocales = [
        'fr-SN', 'fr-BE', 'fr-CH',
        'es-CO', 'es-AR', 'es-PE', 'es-CL',
        'en-AU', 'en-NZ', 'en-ZA',
        'pt-AO', 'pt-MZ',
        'ar-EG', 'ar-MA', 'ar-KW',
        'zh-SG'
      ];

      const testDate = new Date(Date.UTC(2026, 8, 14, 12, 0, 0));
      const testNumber = 9876543.21;

      // Verify officially supported locales and their regional variants.
      for (const loc of officialRegionalLocales) {
        try {
          const dateStr = new Intl.DateTimeFormat(loc).format(testDate);
          const numStr = new Intl.NumberFormat(loc).format(testNumber);
          const coll = new Intl.Collator(loc).compare('a', 'b');
          if (!dateStr || !numStr || coll >= 0) {
            return `Official locale ${loc} verification failed`;
          }
        } catch (e) {
          return `Exception in official locale ${loc}: ${e.message}`;
        }
      }

      // Verify non-supported regional sub-locales gracefully fall back.
      for (const loc of nonSupportedRegionalSubLocales) {
        try {
          const dtf = new Intl.DateTimeFormat(loc);
          const dateStr = dtf.format(testDate);
          const nf = new Intl.NumberFormat(loc);
          const numStr = nf.format(testNumber);
          const coll = new Intl.Collator(loc).compare('a', 'b');
          if (!dateStr || dateStr.length === 0) {
            return `Empty date for non-supported sub-locale ${loc}`;
          }
          if (!numStr || numStr.length === 0) {
            return `Empty number for non-supported sub-locale ${loc}`;
          }
          if (coll >= 0) {
            return `Invalid collation comparison for non-supported sub-locale ${loc}`;
          }
        } catch (e) {
          return `Exception in non-supported sub-locale ${loc}: ${e.message}`;
        }
      }

      return 'SUCCESS';
    })()
  )";

  EXPECT_EQ("SUCCESS",
            content::EvalJs(shell()->web_contents(), script).ExtractString());
}

// Parameterized test fixture for validating browser-level locale configuration
// via the --lang command-line switch. This class is instantiated and owned
// by the gtest framework on the main test thread.
class I18nBrowserLanguageParamTest
    : public I18nBrowserTest,
      public testing::WithParamInterface<const char*> {
 public:
  I18nBrowserLanguageParamTest() = default;
  ~I18nBrowserLanguageParamTest() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    I18nBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("lang", GetParam());
  }
};

IN_PROC_BROWSER_TEST_P(I18nBrowserLanguageParamTest, VerifyBrowserLanguage) {
  SetUpPage();

  const std::string expected_prefix = GetParam();
  const std::string script = "navigator.language";
  const std::string actual_lang =
      content::EvalJs(shell()->web_contents(), script).ExtractString();

  // navigator.language should start with the requested language code.
  std::string expected_lang = expected_prefix.substr(0, 2);
  EXPECT_EQ(expected_lang, actual_lang.substr(0, 2))
      << "Expected language starting with " << expected_lang << " but got "
      << actual_lang;

  // Verify that default Intl date/number formatters work with the active
  // language.
  const std::string intl_script = R"(
    (() => {
      const dateStr = new Date().toLocaleDateString();
      const numStr = (1234.56).toLocaleString();
      const cmp = 'a'.localeCompare('b');
      if (!dateStr || !numStr || cmp >= 0) {
        return 'FAILED';
      }
      return 'SUCCESS';
    })()
  )";
  EXPECT_EQ(
      "SUCCESS",
      content::EvalJs(shell()->web_contents(), intl_script).ExtractString());
}

INSTANTIATE_TEST_SUITE_P(
    RepresentativeLanguages,
    I18nBrowserLanguageParamTest,
    testing::Values("en-US", "ja-JP", "ar-SA", "hi-IN", "th-TH"));

}  // namespace cobalt
