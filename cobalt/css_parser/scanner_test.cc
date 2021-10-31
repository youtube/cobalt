// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/css_parser/scanner.h"

#include <locale.h>

#include "cobalt/css_parser/grammar.h"
#include "cobalt/css_parser/string_pool.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace css_parser {

// When we support any of the at rule which already has a DISABLED test listed
// below, this DISABLED test should be enabled.

const char* kNumericLocales[] = {
  "",       // default locale
  "C",      // dot radix separator
  "de_DE",  // comma radix separator
};

class ScannerTest : public ::testing::Test,
                    public ::testing::WithParamInterface<const char*> {
 public:
  virtual void SetUp() {
    // Don't mess with the locale for non-parameterized tests.
    if (!testing::UnitTest::GetInstance()->current_test_info()->value_param()) {
      old_locale_.clear();
      return;
    }

#if SB_IS(EVERGREEN)
    // We don't support changing locale for Evergreen so run only
    // if the default/empty local is passed in the param.
    locale_okay_ = (GetParam()[0] == 0);
#else
    // Save the old locale.
    char* old_locale_cstr = setlocale(LC_NUMERIC, nullptr);
    EXPECT_TRUE(old_locale_cstr != nullptr) << "Cant' save original locale";
    old_locale_ = old_locale_cstr;

    // Keep the default locale when the param is empty, and for other param
    // values we'll skip the test if we can't change the locale to it.
    locale_okay_ =
        (GetParam()[0] == 0 || setlocale(LC_NUMERIC, GetParam()) != nullptr);
#endif
  }

  virtual void TearDown() {
#if !SB_IS(EVERGREEN)
    if (!old_locale_.empty()) setlocale(LC_NUMERIC, old_locale_.c_str());
#endif
  }

  // TODO: Use GTEST_SKIP in |SetUp| when we have a newer version of gtest.
  bool SkipLocale() {
    if (!locale_okay_) {
      SbLogFormatF("[  SKIPPED ] Can't set locale to %s \n", GetParam());
    }
    return !locale_okay_;
  }

 protected:
  std::string old_locale_;
  bool locale_okay_;

  StringPool string_pool_;
  TokenValue token_value_;
  YYLTYPE token_location_;
};

INSTANTIATE_TEST_CASE_P(LocaleNumeric, ScannerTest,
                        ::testing::ValuesIn(kNumericLocales));

namespace {
// Array of property tokens corresponding to each cssom::PropertyKey.
const int property_tokens[] = {
    kAlignContentToken,
    kAlignItemsToken,
    kAlignSelfToken,
    kAnimationDelayToken,
    kAnimationDirectionToken,
    kAnimationDurationToken,
    kAnimationFillModeToken,
    kAnimationIterationCountToken,
    kAnimationNameToken,
    kAnimationTimingFunctionToken,
    kBackgroundColorToken,
    kBackgroundImageToken,
    kBackgroundPositionToken,
    kBackgroundRepeatToken,
    kBackgroundSizeToken,
    kBorderBottomColorToken,
    kBorderBottomLeftRadiusToken,
    kBorderBottomRightRadiusToken,
    kBorderBottomStyleToken,
    kBorderBottomWidthToken,
    kBorderLeftColorToken,
    kBorderLeftStyleToken,
    kBorderLeftWidthToken,
    kBorderRightColorToken,
    kBorderRightStyleToken,
    kBorderRightWidthToken,
    kBorderTopColorToken,
    kBorderTopLeftRadiusToken,
    kBorderTopRightRadiusToken,
    kBorderTopStyleToken,
    kBorderTopWidthToken,
    kBottomToken,
    kBoxShadowToken,
    kColorToken,
    kContentToken,
    kDisplayToken,
    kFilterToken,
    kFlexBasisToken,
    kFlexDirectionToken,
    kFlexGrowToken,
    kFlexShrinkToken,
    kFlexWrapToken,
    kFontFamilyToken,
    kFontSizeToken,
    kFontStyleToken,
    kFontWeightToken,
    kHeightToken,
    kIntersectionObserverRootMarginToken,
    kJustifyContentToken,
    kLeftToken,
    kLineHeightToken,
    kMarginBottomToken,
    kMarginLeftToken,
    kMarginRightToken,
    kMarginTopToken,
    kMaxHeightToken,
    kMaxWidthToken,
    kMinHeightToken,
    kMinWidthToken,
    kOpacityToken,
    kOrderToken,
    kOutlineColorToken,
    kOutlineStyleToken,
    kOutlineWidthToken,
    kOverflowToken,
    kOverflowWrapToken,
    kPaddingBottomToken,
    kPaddingLeftToken,
    kPaddingRightToken,
    kPaddingTopToken,
    kPointerEventsToken,
    kPositionToken,
    kRightToken,
    kTextAlignToken,
    kTextDecorationColorToken,
    kTextDecorationLineToken,
    kTextIndentToken,
    kTextOverflowToken,
    kTextShadowToken,
    kTextTransformToken,
    kTopToken,
    kTransformOriginToken,
    kTransformToken,
    kTransitionDelayToken,
    kTransitionDurationToken,
    kTransitionPropertyToken,
    kTransitionTimingFunctionToken,
    kVerticalAlignToken,
    kVisibilityToken,
    kWhiteSpacePropertyToken,
    kWidthToken,
    kZIndexToken,
    kAllToken,
    kSrcToken,
    kUnicodeRangePropertyToken,
    kWordWrapToken,
    kAnimationToken,
    kBackgroundToken,
    kBorderBottomToken,
    kBorderColorToken,
    kBorderLeftToken,
    kBorderToken,
    kBorderRadiusToken,
    kBorderRightToken,
    kBorderStyleToken,
    kBorderTopToken,
    kBorderWidthToken,
    kFlexToken,
    kFlexFlowToken,
    kFontToken,
    kMarginToken,
    kOutlineToken,
    kPaddingToken,
    kTextDecorationToken,
    kTransitionToken,
};
}  // namespace

TEST_F(ScannerTest, ScansPropertyName) {
  static_assert(
      SB_ARRAY_SIZE(property_tokens) == 1 + cssom::kMaxEveryPropertyKey,
      "property_tokens[] should have a value for each cssom::PropertyKey");
  // Test that all property names are scanned into tokens correctly.
  ASSERT_EQ(SB_ARRAY_SIZE(property_tokens), 1 + cssom::kMaxEveryPropertyKey);
  for (int i = 0; i < 1 + cssom::kMaxEveryPropertyKey; i++) {
    cssom::PropertyKey property_key = static_cast<cssom::PropertyKey>(i);
    Scanner scanner(cssom::GetPropertyName(property_key), &string_pool_);

    ASSERT_EQ(property_tokens[i],
              yylex(&token_value_, &token_location_, &scanner));
    ASSERT_EQ(kEndOfFileToken,
              yylex(&token_value_, &token_location_, &scanner));
  }
}

namespace {
// Array of keyword tokens corresponding to each cssom::KeywordValue.
const int keyword_tokens[] = {
    kAbsoluteToken,
    kAlternateToken,
    kAlternateReverseToken,
    kAutoToken,
    kBackwardsToken,
    kBaselineToken,
    kBlockToken,
    kBothToken,
    kBottomToken,
    kBreakWordToken,
    kCenterToken,
    kClipToken,
    kCollapseToken,
    kColumnToken,
    kColumnReverseToken,
    kContainToken,
    kContentToken,
    kCoverToken,
    kIdentifierToken,  // kCurrentColor
    kCursiveToken,
    kEllipsisToken,
    kEndToken,
    kEquirectangularToken,
    kFantasyToken,
    kFixedToken,
    kFlexToken,
    kFlexEndToken,
    kFlexStartToken,
    kForwardsToken,
    kHiddenToken,
    kInfiniteToken,
    kInheritToken,
    kInitialToken,
    kInlineToken,
    kInlineBlockToken,
    kInlineFlexToken,
    kLeftToken,
    kLineThroughToken,
    kMiddleToken,
    kMonoscopicToken,
    kMonospaceToken,
    kNoneToken,
    kNoRepeatToken,
    kNormalToken,
    kNowrapToken,
    kPreToken,
    kPreLineToken,
    kPreWrapToken,
    kRelativeToken,
    kRepeatToken,
    kReverseToken,
    kRightToken,
    kRowToken,
    kRowReverseToken,
    kSansSerifToken,
    kScrollToken,
    kSerifToken,
    kSolidToken,
    kSpaceAroundToken,
    kSpaceBetweenToken,
    kStartToken,
    kStaticToken,
    kStereoscopicLeftRightToken,
    kStereoscopicTopBottomToken,
    kStretchToken,
    kTopToken,
    kUppercaseToken,
    kVisibleToken,
    kWrapToken,
    kWrapReverseToken,
};
}  // namespace

TEST_F(ScannerTest, ScansKeywordValue) {
  static_assert(SB_ARRAY_SIZE(keyword_tokens) ==
                    1 + cssom::KeywordValue::kMaxKeywordValue,
                "keyword_tokens[] should have a value for each "
                "cssom::KeywordValue::Value");
  // Test that all keyword values are scanned into tokens correctly.
  ASSERT_EQ(SB_ARRAY_SIZE(keyword_tokens),
            1 + cssom::KeywordValue::kMaxKeywordValue);
  for (int i = 0; i < 1 + cssom::KeywordValue::kMaxKeywordValue; i++) {
    cssom::KeywordValue::Value keyword_value =
        static_cast<cssom::KeywordValue::Value>(i);
    std::string keyword_name = cssom::KeywordValue::GetName(keyword_value);
    Scanner scanner(keyword_name.c_str(), &string_pool_);

    ASSERT_EQ(keyword_tokens[i],
              yylex(&token_value_, &token_location_, &scanner));
    ASSERT_EQ(kEndOfFileToken,
              yylex(&token_value_, &token_location_, &scanner));
  }
}

TEST_F(ScannerTest, ScansSingleCodePointUnicodeRange) {
  Scanner scanner("u+1f4a9 U+1F4A9", &string_pool_);

  ASSERT_EQ(kUnicodeRangeToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(0x1f4a9, token_value_.integer_pair.first);
  ASSERT_EQ(0x1f4a9, token_value_.integer_pair.second);

  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kUnicodeRangeToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(0x1f4a9, token_value_.integer_pair.first);
  ASSERT_EQ(0x1f4a9, token_value_.integer_pair.second);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansLowerToUpperBoundUnicodeRange) {
  Scanner scanner("u+32-ff", &string_pool_);

  ASSERT_EQ(kUnicodeRangeToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(0x32, token_value_.integer_pair.first);
  ASSERT_EQ(0xff, token_value_.integer_pair.second);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansWildcardUnicodeRange) {
  Scanner scanner("u+04??", &string_pool_);

  ASSERT_EQ(kUnicodeRangeToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(0x400, token_value_.integer_pair.first);
  ASSERT_EQ(0x4ff, token_value_.integer_pair.second);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansIdentifierThatStartsWithU) {
  // Ensure all words below are not mistakenly recognized as kUnicodeRangeToken.
  Scanner scanner("u U umami Ukraine", &string_pool_);

  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("u", token_value_.string);

  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("U", token_value_.string);

  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("umami", token_value_.string);

  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("Ukraine", token_value_.string);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansSixDigitUnicodeRange) {
  Scanner scanner("u+11111111", &string_pool_);

  ASSERT_EQ(kUnicodeRangeToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(0x111111, token_value_.integer_pair.first);
  ASSERT_EQ(0x111111, token_value_.integer_pair.second);

  ASSERT_EQ(kIntegerToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(11, token_value_.integer);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansUnquotedUri) {
  Scanner scanner("url(https://www.youtube.com/tv#/)", &string_pool_);

  ASSERT_EQ(kUriToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("https://www.youtube.com/tv#/", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansUriWithoutEndBrace) {
  Scanner scanner("url(https://www.youtube.com/tv#/", &string_pool_);

  ASSERT_EQ(kUriToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("https://www.youtube.com/tv#/", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansQuotedUri) {
  Scanner scanner("url('https://www.youtube.com/tv#/')", &string_pool_);

  ASSERT_EQ(kUriToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("https://www.youtube.com/tv#/", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansInvalidUri) {
  Scanner scanner("url(no pasaran)", &string_pool_);

  ASSERT_EQ(kInvalidFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("url", token_value_.string);

  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("no", token_value_.string);

  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("pasaran", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansInvalidFunction) {
  Scanner scanner("sample-function()", &string_pool_);

  ASSERT_EQ(kInvalidFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("sample-function", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansInvalidFunctionWithNumber) {
  Scanner scanner("sample-matrix4d()", &string_pool_);

  ASSERT_EQ(kInvalidFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("sample-matrix4d", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansFunctionLikeMediaAnd) {
  Scanner scanner("@media tv and(monochrome)", &string_pool_);

  ASSERT_EQ(kMediaToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kTVMediaTypeToken,
            yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kMediaAndToken, yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ('(', yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kNonNegativeIntegerMediaFeatureTypeToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(static_cast<int>(cssom::kMonochromeMediaFeature),
            token_value_.integer);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansNPlusOne) {
  Scanner scanner("nth-child(n+1)", &string_pool_);

  ASSERT_EQ(kNthChildFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kNthToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("n+1", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansNPlusOneWithoutClosingAtTheEnd) {
  Scanner scanner("nth-child(n+1", &string_pool_);

  ASSERT_EQ(kNthChildFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kNthToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("n+1", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansNMinusOne) {
  Scanner scanner("nth-child(n-1)", &string_pool_);

  ASSERT_EQ(kNthChildFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kNthToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("n-1", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansIdentifierN) {
  Scanner scanner("nth-child(n)", &string_pool_);

  ASSERT_EQ(kNthChildFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("n", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansNDashIdentifier) {
  Scanner scanner("nth-child(n-)", &string_pool_);

  ASSERT_EQ(kNthChildFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("n-", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansIdentifier) {
  Scanner scanner("sample-identifier", &string_pool_);

  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("sample-identifier", token_value_.string);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansIdentifierWithNumber) {
  Scanner scanner("matrix3d5s7wss-47", &string_pool_);

  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("matrix3d5s7wss-47", token_value_.string);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansDot) {
  Scanner scanner(".", &string_pool_);

  ASSERT_EQ('.', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansNthThatStartsWithNumber) {
  Scanner scanner("nth-child(4n+2)", &string_pool_);

  ASSERT_EQ(kNthChildFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kNthToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("4n+2", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansInvalidDimension) {
  Scanner scanner("12monkeys", &string_pool_);

  ASSERT_EQ(kInvalidDimensionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("12monkeys", token_value_.string);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansPercentage) {
  if (SkipLocale()) return;
  Scanner scanner("2.71828%", &string_pool_);

  ASSERT_EQ(kPercentageToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(2.71828f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansInteger) {
  Scanner scanner("911", &string_pool_);

  ASSERT_EQ(kIntegerToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(911, token_value_.integer);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansReal) {
  if (SkipLocale()) return;
  Scanner scanner("2.71828", &string_pool_);

  ASSERT_EQ(kRealToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(2.71828f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansNegativeReal) {
  if (SkipLocale()) return;
  Scanner scanner("-3.14159", &string_pool_);

  ASSERT_EQ('-', yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kRealToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(3.14159f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansVeryBigIntegerAsReal) {
  Scanner scanner("2147483647 2147483648", &string_pool_);

  ASSERT_EQ(kIntegerToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(2147483647, token_value_.integer);

  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kRealToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(2147483648, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansInvalidNumber) {
  // As per IEEE 754 the maximum value of |float| has 39 decimal digits.
  // We use 40 decimal digits to cause overflow into positive infinity.
  Scanner scanner("1000000000000000000000000000000000000000", &string_pool_);

  ASSERT_EQ(kInvalidNumberToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("1000000000000000000000000000000000000000", token_value_.string);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansScientificNotationNumberWithNegativeExponent) {
  Scanner scanner("1e-14", &string_pool_);

  ASSERT_EQ(kRealToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(0.00000000000001f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansScientificNotationNumberWithPositiveExponent) {
  if (SkipLocale()) return;
  Scanner scanner("2.5e+6", &string_pool_);

  ASSERT_EQ(kRealToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(2500000, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansScientificNotationNumberWithUnsignedExponent) {
  Scanner scanner("3e5", &string_pool_);

  ASSERT_EQ(kRealToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(300000, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansScientificNotationNumberWithScientificExponent) {
  // This should be parsed as the number "30000" and invalid dimension "e5",
  // not 3*10^400000 nor 30000*10^5.
  Scanner scanner("3e4e5", &string_pool_);

  ASSERT_EQ(kInvalidDimensionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("3e4e5", token_value_.string);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansHexadecimalNumber) {
  // We don't support scanning of hexadecimal numbers. This test is just to
  // confirm that we recover without crashing.
  Scanner scanner("0x0", &string_pool_);

  // "x0" is an invalid dimension.
  ASSERT_EQ(kInvalidDimensionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("0x0", token_value_.string);
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansUnknownDashFunction) {
  Scanner scanner("-cobalt-magic()", &string_pool_);

  ASSERT_EQ(kInvalidFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("-cobalt-magic", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansUnknownDashFunctionWithNumber) {
  Scanner scanner("-cobalt-ma5555gic()", &string_pool_);

  ASSERT_EQ(kInvalidFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("-cobalt-ma5555gic", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansUnknownDashFunctionWithoutClosingAtEnd) {
  Scanner scanner("-cobalt-magic(", &string_pool_);

  ASSERT_EQ(kInvalidFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("-cobalt-magic", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

namespace {
// Array of known dash functions and their associated tokens.
struct {
  const char* text;
  int token;
} known_dash_functions[] = {
  { "-cobalt-mtm", kCobaltMtmFunctionToken },
  { "-cobalt-ui-nav-focus-transform", kCobaltUiNavFocusTransformFunctionToken },
  { "-cobalt-ui-nav-spotlight-transform",
    kCobaltUiNavSpotlightTransformFunctionToken },
};
}  // namespace

TEST_F(ScannerTest, ScansKnownDashFunction) {
  for (size_t i = 0; i < SB_ARRAY_SIZE(known_dash_functions); ++i) {
    std::string text(known_dash_functions[i].text);
    text += "()";

    Scanner scanner(text.c_str(), &string_pool_);
    ASSERT_EQ(known_dash_functions[i].token,
              yylex(&token_value_, &token_location_, &scanner)) << text;
    ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner)) << text;
    ASSERT_EQ(kEndOfFileToken,
              yylex(&token_value_, &token_location_, &scanner)) << text;
  }
}

TEST_F(ScannerTest, ScansKnownDashFunctionWithoutClosingAtEnd) {
  for (size_t i = 0; i < SB_ARRAY_SIZE(known_dash_functions); ++i) {
    std::string text(known_dash_functions[i].text);
    text += "(";

    Scanner scanner(text.c_str(), &string_pool_);
    ASSERT_EQ(known_dash_functions[i].token,
              yylex(&token_value_, &token_location_, &scanner)) << text;
    ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner)) << text;
    ASSERT_EQ(kEndOfFileToken,
              yylex(&token_value_, &token_location_, &scanner)) << text;
  }
}

TEST_F(ScannerTest, ScansMinusNPlusConstant) {
  Scanner scanner("nth-child(-n+2)", &string_pool_);

  ASSERT_EQ(kNthChildFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kNthToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("-n+2", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansDashNIdentifier) {
  Scanner scanner("nth-child(-n)", &string_pool_);

  ASSERT_EQ(kNthChildFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("-n", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansMinusNMinusConstant) {
  Scanner scanner("nth-child(-n-2)", &string_pool_);

  ASSERT_EQ(kNthChildFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kNthToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("-n-2", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansDashNDashIdentifier) {
  Scanner scanner("nth-child(-n-)", &string_pool_);

  ASSERT_EQ(kNthChildFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("-n-", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansEndSgmlCommentDelimiter) {
  Scanner scanner("-->", &string_pool_);

  ASSERT_EQ(kSgmlCommentDelimiterToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansConstantTimesNPlusConstant) {
  Scanner scanner("nth-child(-4n+2)", &string_pool_);

  ASSERT_EQ(kNthChildFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kNthToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("-4n+2", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansMinus) {
  Scanner scanner("-", &string_pool_);

  ASSERT_EQ('-', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansOtherCharacters) {
  Scanner scanner("%&=[]", &string_pool_);

  ASSERT_EQ('%', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ('&', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ('=', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ('[', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(']', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansEmptyInput) {
  Scanner scanner("", &string_pool_);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansWhitespace) {
  Scanner scanner("h1\r\n\t img", &string_pool_);

  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("h1", token_value_.string);
  ASSERT_EQ(1, token_location_.first_line);
  ASSERT_EQ(1, token_location_.first_column);

  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("img", token_value_.string);
  ASSERT_EQ(2, token_location_.first_line);
  ASSERT_EQ(3, token_location_.first_column);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansEndMediaQuery) {
  Scanner scanner("@media;{", &string_pool_);

  ASSERT_EQ(kMediaToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(';', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ('{', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ('}', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansSingleQuotedString) {
  Scanner scanner("'Quantum Paper rocks'", &string_pool_);

  ASSERT_EQ(kStringToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("Quantum Paper rocks", token_value_.string);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansDoubleQuotedString) {
  Scanner scanner("\"Quantum Paper rocks\"", &string_pool_);

  ASSERT_EQ(kStringToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("Quantum Paper rocks", token_value_.string);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansQuote) {
  Scanner scanner("'hello\n", &string_pool_);

  ASSERT_EQ('\'', yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("hello", token_value_.string);

  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansQuoteAtEndOfFile) {
  Scanner scanner("'hello", &string_pool_);

  ASSERT_EQ(kStringToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("hello", token_value_.string);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansImportant) {
  Scanner scanner("! important", &string_pool_);

  ASSERT_EQ(kImportantToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansExclamationMark) {
  Scanner scanner("!", &string_pool_);

  ASSERT_EQ('!', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansObviousHex) {
  Scanner scanner("#1f1f1f", &string_pool_);

  ASSERT_EQ(kHexToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("1f1f1f", token_value_.string);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansIdentifierLikeHex) {
  // Ensure that hex value is not mistakenly recognized as kIdSelectorToken.
  Scanner scanner("#f1f1f1", &string_pool_);

  ASSERT_EQ(kHexToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("f1f1f1", token_value_.string);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansIdSelector) {
  Scanner scanner("#webgl-canvas", &string_pool_);

  ASSERT_EQ(kIdSelectorToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("webgl-canvas", token_value_.string);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansHashmark) {
  Scanner scanner("#", &string_pool_);

  ASSERT_EQ('#', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansComment) {
  Scanner scanner("/* Cobalt\n+ Quantum Paper\n= <3 */", &string_pool_);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(3, token_location_.first_line);
  ASSERT_EQ(8, token_location_.first_column);
}

TEST_F(ScannerTest, ScansUnterminatedComment) {
  Scanner scanner("/* Everything that has a beginning has an end...",
                  // ...except for above comment.
                  &string_pool_);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansSlash) {
  Scanner scanner("/", &string_pool_);

  ASSERT_EQ('/', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansEndsWith) {
  Scanner scanner("$=", &string_pool_);

  ASSERT_EQ(kEndsWithToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansDollar) {
  Scanner scanner("$", &string_pool_);

  ASSERT_EQ('$', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansContains) {
  Scanner scanner("*=", &string_pool_);

  ASSERT_EQ(kContainsToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansAsterisk) {
  Scanner scanner("*", &string_pool_);

  ASSERT_EQ('*', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansPlusNth) {
  Scanner scanner("nth-child(+4n+2)", &string_pool_);

  ASSERT_EQ(kNthChildFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kNthToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("4n+2", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansPlus) {
  Scanner scanner("+4n+2", &string_pool_);

  ASSERT_EQ('+', yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kInvalidDimensionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("4n", token_value_.string);

  ASSERT_EQ('+', yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kIntegerToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(2, token_value_.integer);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansStartSgmlCommentDelimiter) {
  Scanner scanner("<!--", &string_pool_);

  ASSERT_EQ(kSgmlCommentDelimiterToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansLess) {
  Scanner scanner("<", &string_pool_);

  ASSERT_EQ('<', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansInvalidAtRuleBlock) {
  Scanner scanner("@cobalt-magic { foo[()]; bar[{ }] }", &string_pool_);

  ASSERT_EQ(kInvalidAtBlockToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("@cobalt-magic", token_value_.string);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansInvalidAtRuleBlockWithoutClosingBraceAtEndOfFile) {
  Scanner scanner("@cobalt-magic { foo[(", &string_pool_);

  ASSERT_EQ(kInvalidAtBlockToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("@cobalt-magic", token_value_.string);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansInvalidAtRuleBlockEndsWithSemicolon) {
  Scanner scanner("@cobalt-magic;", &string_pool_);

  ASSERT_EQ(kInvalidAtBlockToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("@cobalt-magic", token_value_.string);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansAt) {
  Scanner scanner("@", &string_pool_);

  ASSERT_EQ('@', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansAtWithEscapedIdentifier) {
  Scanner scanner("@\\e9 motion", &string_pool_);

  ASSERT_EQ(kInvalidAtBlockToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("@émotion", token_value_.string);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansEscapedIdentifier) {
  Scanner scanner("\\e9 motion", &string_pool_);

  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("émotion", token_value_.string);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansBackSlash) {
  Scanner scanner("\\", &string_pool_);

  ASSERT_EQ('\\', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansBeginsWith) {
  Scanner scanner("^=", &string_pool_);

  ASSERT_EQ(kBeginsWithToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansXor) {
  Scanner scanner("^", &string_pool_);

  ASSERT_EQ('^', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansDashMatch) {
  Scanner scanner("|=", &string_pool_);

  ASSERT_EQ(kDashMatchToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansVerticalBar) {
  Scanner scanner("|", &string_pool_);

  ASSERT_EQ('|', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansIncludes) {
  Scanner scanner("~=", &string_pool_);

  ASSERT_EQ(kIncludesToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansTilde) {
  Scanner scanner("~", &string_pool_);

  ASSERT_EQ('~', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansUtf8Identifier) {
  // In Russian "огурец" means "cucumber". Why cucumber? It's a long story.
  Scanner scanner("огурец", &string_pool_);

  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("огурец", token_value_.string);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansSupportsOr) {
  Scanner scanner("@supports or", &string_pool_);

  ASSERT_EQ(kSupportsToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kSupportsOrToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansSupportsAnd) {
  Scanner scanner("@supports and", &string_pool_);

  ASSERT_EQ(kSupportsToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kSupportsAndToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansSupportsNot) {
  Scanner scanner("@supports not", &string_pool_);

  ASSERT_EQ(kSupportsToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kSupportsNotToken,
            yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansNotFunction) {
  Scanner scanner("not()", &string_pool_);

  ASSERT_EQ(kNotFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansCalcFunction) {
  Scanner scanner("calc()", &string_pool_);

  ASSERT_EQ(kCalcFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansCueFunction) {
  Scanner scanner("cue()", &string_pool_);

  ASSERT_EQ(kCueFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansFormatFunction) {
  Scanner scanner("format()", &string_pool_);

  ASSERT_EQ(kFormatFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansLocalFunction) {
  Scanner scanner("local()", &string_pool_);

  ASSERT_EQ(kLocalFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansScaleFunction) {
  Scanner scanner("scale()", &string_pool_);

  ASSERT_EQ(kScaleFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

// Test if we add a closing ')' for the function at end of file.
TEST_F(ScannerTest, ScansScaleFunctionWithoutEndBraceAtEndOfFile) {
  Scanner scanner("scale(", &string_pool_);

  ASSERT_EQ(kScaleFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansMapToMeshFunction) {
  Scanner scanner("map-to-mesh()", &string_pool_);

  ASSERT_EQ(kMapToMeshFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansMatrix3dFunction) {
  Scanner scanner("matrix3d()", &string_pool_);

  ASSERT_EQ(kMatrix3dFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansNthOfTypeFunction) {
  Scanner scanner("nth-of-type()", &string_pool_);

  ASSERT_EQ(kNthOfTypeFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansNthLastChildFunction) {
  Scanner scanner("nth-last-child()", &string_pool_);

  ASSERT_EQ(kNthLastChildFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansNthLastOfTypeFunction) {
  Scanner scanner("nth-last-of-type()", &string_pool_);

  ASSERT_EQ(kNthLastOfTypeFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansMediaScreenAnd) {
  Scanner scanner("@media screen and", &string_pool_);

  ASSERT_EQ(kMediaToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kScreenMediaTypeToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kMediaAndToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansMediaNotDeviceAspectRatio) {
  Scanner scanner("@media not (device-aspect-ratio: 2560/1440)", &string_pool_);

  ASSERT_EQ(kMediaToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kMediaNotToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ('(', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kRatioMediaFeatureTypeToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(static_cast<int>(cssom::kDeviceAspectRatioMediaFeature),
            token_value_.integer);
  ASSERT_EQ(':', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kIntegerToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(2560, token_value_.integer);
  ASSERT_EQ('/', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kIntegerToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(1440, token_value_.integer);
  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansMediaOrientationBogus) {
  Scanner scanner("@media (orientation: bogus)", &string_pool_);

  ASSERT_EQ(kMediaToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ('(', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kOrientationMediaFeatureTypeToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(static_cast<int>(cssom::kOrientationMediaFeature),
            token_value_.integer);
  ASSERT_EQ(':', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansMediaOnlyTv) {
  Scanner scanner("@media only tv", &string_pool_);

  ASSERT_EQ(kMediaToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kMediaOnlyToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kTVMediaTypeToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansCentimeters) {
  if (SkipLocale()) return;
  Scanner scanner("8.24cm", &string_pool_);

  ASSERT_EQ(kCentimetersToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansZeroGlyphWidthsAkaCh) {
  if (SkipLocale()) return;
  Scanner scanner("8.24ch", &string_pool_);

  ASSERT_EQ(kZeroGlyphWidthsAkaChToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansDegrees) {
  if (SkipLocale()) return;
  Scanner scanner("8.24deg", &string_pool_);

  ASSERT_EQ(kDegreesToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansDotsPerPixel) {
  if (SkipLocale()) return;
  Scanner scanner("8.24dppx", &string_pool_);

  ASSERT_EQ(kDotsPerPixelToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansDotsPerCentimeter) {
  if (SkipLocale()) return;
  Scanner scanner("8.24dpcm", &string_pool_);

  ASSERT_EQ(kDotsPerCentimeterToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansDotsPerInch) {
  if (SkipLocale()) return;
  Scanner scanner("8.24dpi", &string_pool_);

  ASSERT_EQ(kDotsPerInchToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansFontSizesAkaEm) {
  if (SkipLocale()) return;
  Scanner scanner("8.24em", &string_pool_);

  ASSERT_EQ(kFontSizesAkaEmToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansXHeightsAkaEx) {
  if (SkipLocale()) return;
  Scanner scanner("8.24ex", &string_pool_);

  ASSERT_EQ(kXHeightsAkaExToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansFractions) {
  if (SkipLocale()) return;
  Scanner scanner("8.24fr", &string_pool_);

  ASSERT_EQ(kFractionsToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansGradians) {
  if (SkipLocale()) return;
  Scanner scanner("8.24grad", &string_pool_);

  ASSERT_EQ(kGradiansToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansHertz) {
  if (SkipLocale()) return;
  Scanner scanner("8.24hz", &string_pool_);

  ASSERT_EQ(kHertzToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansInches) {
  if (SkipLocale()) return;
  Scanner scanner("8.24in", &string_pool_);

  ASSERT_EQ(kInchesToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansKilohertz) {
  if (SkipLocale()) return;
  Scanner scanner("8.24khz", &string_pool_);

  ASSERT_EQ(kKilohertzToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansMillimeters) {
  if (SkipLocale()) return;
  Scanner scanner("8.24mm", &string_pool_);

  ASSERT_EQ(kMillimetersToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansMilliseconds) {
  if (SkipLocale()) return;
  Scanner scanner("8.24ms", &string_pool_);

  ASSERT_EQ(kMillisecondsToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansPixels) {
  if (SkipLocale()) return;
  Scanner scanner("8.24px", &string_pool_);

  ASSERT_EQ(kPixelsToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansPoints) {
  if (SkipLocale()) return;
  Scanner scanner("8.24pt", &string_pool_);

  ASSERT_EQ(kPointsToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansPicas) {
  if (SkipLocale()) return;
  Scanner scanner("8.24pc", &string_pool_);

  ASSERT_EQ(kPicasToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansRadians) {
  if (SkipLocale()) return;
  Scanner scanner("8.24rad", &string_pool_);

  ASSERT_EQ(kRadiansToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansRootElementFontSizesAkaRem) {
  if (SkipLocale()) return;
  Scanner scanner("8.24rem", &string_pool_);

  ASSERT_EQ(kRootElementFontSizesAkaRemToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansSeconds) {
  if (SkipLocale()) return;
  Scanner scanner("8.24s", &string_pool_);

  ASSERT_EQ(kSecondsToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansTurns) {
  if (SkipLocale()) return;
  Scanner scanner("8.24turn", &string_pool_);

  ASSERT_EQ(kTurnsToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansViewportWidthPercentsAkaVw) {
  if (SkipLocale()) return;
  Scanner scanner("8.24vw", &string_pool_);

  ASSERT_EQ(kViewportWidthPercentsAkaVwToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansViewportHeightPercentsAkaVh) {
  if (SkipLocale()) return;
  Scanner scanner("8.24vh", &string_pool_);

  ASSERT_EQ(kViewportHeightPercentsAkaVhToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScansViewportSmallerSizePercentsAkaVmin) {
  if (SkipLocale()) return;
  Scanner scanner("8.24vmin", &string_pool_);

  ASSERT_EQ(kViewportSmallerSizePercentsAkaVminToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_P(ScannerTest, ScanskViewportLargerSizePercentsAkaVmax) {
  if (SkipLocale()) return;
  Scanner scanner("8.24vmax", &string_pool_);

  ASSERT_EQ(kViewportLargerSizePercentsAkaVmaxToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_FLOAT_EQ(8.24f, token_value_.real);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansBottomLeft) {
  Scanner scanner("@bottom-left", &string_pool_);

  ASSERT_EQ(kBottomLeftToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansBottomRight) {
  Scanner scanner("@bottom-right", &string_pool_);

  ASSERT_EQ(kBottomRightToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansBottomCenter) {
  Scanner scanner("@bottom-center", &string_pool_);

  ASSERT_EQ(kBottomCenterToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansBottomLeftCorner) {
  Scanner scanner("@bottom-left-corner", &string_pool_);

  ASSERT_EQ(kBottomLeftCornerToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansBottomRightCorner) {
  Scanner scanner("@bottom-right-corner", &string_pool_);

  ASSERT_EQ(kBottomRightCornerToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansCharset) {
  Scanner scanner("@charset", &string_pool_);

  ASSERT_EQ(kCharsetToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansFontFace) {
  Scanner scanner("@font-face", &string_pool_);

  ASSERT_EQ(kFontFaceToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansImport) {
  Scanner scanner("@import", &string_pool_);

  ASSERT_EQ(kImportToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansLeftTop) {
  Scanner scanner("@left-top", &string_pool_);

  ASSERT_EQ(kLeftTopToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansLeftMiddle) {
  Scanner scanner("@left-middle", &string_pool_);

  ASSERT_EQ(kLeftMiddleToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansLeftBottom) {
  Scanner scanner("@left-bottom", &string_pool_);

  ASSERT_EQ(kLeftBottomToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansNamespace) {
  Scanner scanner("@namespace", &string_pool_);

  ASSERT_EQ(kNamespaceToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansPage) {
  Scanner scanner("@page", &string_pool_);

  ASSERT_EQ(kPageToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansRightTop) {
  Scanner scanner("@right-top", &string_pool_);

  ASSERT_EQ(kRightTopToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansRightMiddle) {
  Scanner scanner("@right-middle", &string_pool_);

  ASSERT_EQ(kRightMiddleToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansRightBottom) {
  Scanner scanner("@right-bottom", &string_pool_);

  ASSERT_EQ(kRightBottomToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansTopLeft) {
  Scanner scanner("@top-left", &string_pool_);

  ASSERT_EQ(kTopLeftToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansTopRight) {
  Scanner scanner("@top-right", &string_pool_);

  ASSERT_EQ(kTopRightToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansTopCenter) {
  Scanner scanner("@top-center", &string_pool_);

  ASSERT_EQ(kTopCenterToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansTopLeftCorner) {
  Scanner scanner("@top-left-corner", &string_pool_);

  ASSERT_EQ(kTopLeftCornerToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_ScansTopRightCorner) {
  Scanner scanner("@top-right-corner", &string_pool_);

  ASSERT_EQ(kTopRightCornerToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansWebkitAtRule) {
  Scanner scanner("@-webkit-region foo {foo[ baz( abc; ) ], bar[baz { efg }] }",
                  &string_pool_);

  ASSERT_EQ(kOtherBrowserAtBlockToken,
            yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansColon) {
  Scanner scanner("::", &string_pool_);

  ASSERT_EQ(':', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(':', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, ScansCurlyBrackets) {
  Scanner scanner("{}", &string_pool_);

  ASSERT_EQ('{', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ('}', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, AdvancesLocation) {
  Scanner scanner("body {\n}", &string_pool_);

  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("body", token_value_.string);
  ASSERT_EQ(1, token_location_.first_line);
  ASSERT_EQ(1, token_location_.first_column);

  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(1, token_location_.first_line);
  ASSERT_EQ(5, token_location_.first_column);

  ASSERT_EQ('{', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(1, token_location_.first_line);
  ASSERT_EQ(6, token_location_.first_column);

  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(1, token_location_.first_line);
  ASSERT_EQ(7, token_location_.first_column);

  ASSERT_EQ('}', yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(2, token_location_.first_line);
  ASSERT_EQ(1, token_location_.first_column);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, EntersAndExitsMediaQueryMode) {
  Scanner scanner("@media only; only", &string_pool_);

  // Entered "media query" mode, "only" should be a keyword.
  ASSERT_EQ(kMediaToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kMediaOnlyToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(';', yylex(&token_value_, &token_location_, &scanner));

  // Exited "media query" mode, "only" should be an identifier.
  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("only", token_value_.string);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, DISABLED_EntersAndExitsSupportsMode) {
  Scanner scanner("@supports or; or", &string_pool_);

  // Entered "supports" mode, "or" should be a keyword.
  ASSERT_EQ(kSupportsToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(kSupportsOrToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(';', yylex(&token_value_, &token_location_, &scanner));

  // Exited "supports" mode, "or" should be an identifier.
  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("or", token_value_.string);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

TEST_F(ScannerTest, EntersAndExitsNthChildMode) {
  Scanner scanner("nth-child(n+1) n+1", &string_pool_);

  // Entered "nth-child" mode, "n+1" should be a single token.
  ASSERT_EQ(kNthChildFunctionToken,
            yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kNthToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("n+1", token_value_.string);

  ASSERT_EQ(')', yylex(&token_value_, &token_location_, &scanner));

  // Exited "nth-child" mode, "n+1" should be a sequence of tokens.
  ASSERT_EQ(kWhitespaceToken, yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kIdentifierToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ("n", token_value_.string);

  ASSERT_EQ('+', yylex(&token_value_, &token_location_, &scanner));

  ASSERT_EQ(kIntegerToken, yylex(&token_value_, &token_location_, &scanner));
  ASSERT_EQ(1, token_value_.integer);

  ASSERT_EQ(kEndOfFileToken, yylex(&token_value_, &token_location_, &scanner));
}

}  // namespace css_parser
}  // namespace cobalt
