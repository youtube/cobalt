// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/char_iterator.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_regex_provider.h"
#include "components/autofill/core/browser/data_model/borrowed_transliterator.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

SortedTokenComparisonResult::SortedTokenComparisonResult(
    SortedTokenComparisonStatus status,
    std::vector<AddressToken> additional_tokens)
    : status(status), additional_tokens(additional_tokens) {}

SortedTokenComparisonResult::SortedTokenComparisonResult(
    SortedTokenComparisonResult&& other) = default;

SortedTokenComparisonResult& SortedTokenComparisonResult::operator=(
    SortedTokenComparisonResult&& other) = default;

SortedTokenComparisonResult::~SortedTokenComparisonResult() = default;

bool SortedTokenComparisonResult::IsSingleTokenSubset() const {
  return status == SUBSET && additional_tokens.size() == 1;
}

bool SortedTokenComparisonResult::IsSingleTokenSuperset() const {
  return status == SUPERSET && additional_tokens.size() == 1;
}

bool SortedTokenComparisonResult::OneIsSubset() const {
  return status == SUBSET || status == SUPERSET;
}

bool SortedTokenComparisonResult::ContainEachOther() const {
  return status != DISTINCT;
}

bool SortedTokenComparisonResult::TokensMatch() const {
  return status == MATCH;
}

bool HonorificPrefixEnabled() {
  return base::FeatureList::IsEnabled(
      features::kAutofillEnableSupportForHonorificPrefixes);
}

Re2RegExCache::Re2RegExCache() = default;

// static
Re2RegExCache* Re2RegExCache::Instance() {
  static base::NoDestructor<Re2RegExCache> g_re2regex_cache;
  return g_re2regex_cache.get();
}

const RE2* Re2RegExCache::GetRegEx(const std::string& pattern) {
  // For thread safety, acquire a lock to prevent concurrent access.
  base::AutoLock lock(lock_);

  auto it = regex_map_.find(pattern);
  if (it != regex_map_.end()) {
    const RE2* regex = it->second.get();
    return regex;
  }

  // Build the expression and verify it is correct.
  auto regex_ptr = BuildRegExFromPattern(pattern);

  // Insert the expression into the map, check the success and return the
  // pointer.
  auto result = regex_map_.emplace(pattern, std::move(regex_ptr));
  DCHECK(result.second);
  return result.first->second.get();
}

RewriterCache::RewriterCache() = default;

// static
RewriterCache* RewriterCache::GetInstance() {
  static base::NoDestructor<RewriterCache> g_rewriter_cache;
  return g_rewriter_cache.get();
}

// static
std::u16string RewriterCache::Rewrite(const std::u16string& country_code,
                                      const std::u16string& text) {
  return GetInstance()->GetRewriter(country_code).Rewrite(NormalizeValue(text));
}

const AddressRewriter& RewriterCache::GetRewriter(
    const std::u16string& country_code) {
  // For thread safety, acquire a lock to prevent concurrent access.
  base::AutoLock lock(lock_);

  auto it = rewriter_map_.find(country_code);
  if (it != rewriter_map_.end()) {
    const AddressRewriter& rewriter = it->second;
    return rewriter;
  }

  // Insert the expression into the map, check the success and return the
  // pointer.
  auto result = rewriter_map_.emplace(
      country_code, AddressRewriter::ForCountryCode(country_code));
  DCHECK(result.second);
  return result.first->second;
}

std::unique_ptr<const RE2> BuildRegExFromPattern(const std::string& pattern) {
  RE2::Options opt;
  // By default, patters are case sensitive.
  // Note that, the named-capture-group patterns build with
  // |CaptureTypeWithPattern()| apply a flag to make the matching case
  // insensitive.
  opt.set_case_sensitive(true);

  auto regex = std::make_unique<const RE2>(pattern, opt);

  if (!regex->ok()) {
    DEBUG_ALIAS_FOR_CSTR(pattern_copy, pattern.c_str(), 128);
    base::debug::DumpWithoutCrashing();
  }

  return regex;
}

bool HasCjkNameCharacteristics(const std::string& name) {
  return IsPartialMatch(name, RegEx::kMatchCjkNameCharacteristics);
}

bool HasMiddleNameInitialsCharacteristics(const std::string& middle_name) {
  return IsPartialMatch(middle_name,
                        RegEx::kMatchMiddleNameInitialsCharacteristics);
}

bool HasHispanicLatinxNameCharaceristics(const std::string& name) {
  // Check if the name contains one of the most common Hispanic/Latinx
  // last names.
  if (IsPartialMatch(name, RegEx::kMatchHispanicCommonNameCharacteristics))
    return true;

  // Check if it contains a last name conjunction.
  if (IsPartialMatch(name,
                     RegEx::kMatchHispanicLastNameConjuctionCharacteristics))
    return true;

  // If none of the above, there is not sufficient reason to assume this is a
  // Hispanic/Latinx name.
  return false;
}

absl::optional<base::flat_map<std::string, std::string>>
ParseValueByRegularExpression(const std::string& value,
                              const std::string& pattern) {
  const RE2* regex = Re2RegExCache::Instance()->GetRegEx(pattern);
  return ParseValueByRegularExpression(value, regex);
}

absl::optional<base::flat_map<std::string, std::string>>
ParseValueByRegularExpression(const std::string& value, const RE2* regex) {
  if (!regex || !regex->ok())
    return absl::nullopt;

  // Get the number of capturing groups in the expression.
  // Note, the capturing group for the full match is not counted.
  size_t number_of_capturing_groups = regex->NumberOfCapturingGroups() + 1;

  // Create result vectors to get the matches for the capturing groups.
  std::vector<std::string> results(number_of_capturing_groups);
  std::vector<RE2::Arg> match_results(number_of_capturing_groups);
  std::vector<RE2::Arg*> match_results_ptr(number_of_capturing_groups);

  // Note, the capturing group for the full match is not counted by
  // |NumberOfCapturingGroups|.
  for (size_t i = 0; i < number_of_capturing_groups; i++) {
    match_results[i] = &results[i];
    match_results_ptr[i] = &match_results[i];
  }

  // One capturing group is not counted since it holds the full match.
  if (!RE2::FullMatchN(value, *regex, match_results_ptr.data(),
                       number_of_capturing_groups - 1))
    return absl::nullopt;

  // If successful, write the values into the results map.
  // Note, the capturing group for the full match creates an off-by-one scenario
  // in the indexing.
  return base::MakeFlatMap<std::string, std::string>(
      regex->NamedCapturingGroups(), {}, [&results](const auto& group) mutable {
        const auto& [name, index] = group;
        return std::make_pair(name, std::move(results[index - 1]));
      });
}

bool IsPartialMatch(const std::string& value, RegEx regex) {
  return IsPartialMatch(
      value, StructuredAddressesRegExProvider::Instance()->GetRegEx(regex));
}

bool IsPartialMatch(const std::string& value, const std::string& pattern) {
  return IsPartialMatch(value, Re2RegExCache::Instance()->GetRegEx(pattern));
}

bool IsPartialMatch(const std::string& value, const RE2* expression) {
  return RE2::PartialMatch(value, *expression);
}

std::vector<std::string> GetAllPartialMatches(const std::string& value,
                                              const std::string& pattern) {
  const RE2* regex = Re2RegExCache::Instance()->GetRegEx(pattern);
  if (!regex || !regex->ok())
    return {};
  re2::StringPiece input(value);
  std::string match;
  std::vector<std::string> matches;
  while (re2::RE2::FindAndConsume(&input, *regex, &match)) {
    matches.emplace_back(match);
  }
  return matches;
}

std::vector<std::string> ExtractAllPlaceholders(const std::string& value) {
  return GetAllPartialMatches(value, "\\${([\\w]+)}");
}

std::string GetPlaceholderToken(const std::string& value) {
  return base::StrCat({"${", value, "}"});
}

std::string CaptureTypeWithPattern(
    const ServerFieldType& type,
    std::initializer_list<base::StringPiece> pattern_span_initializer_list) {
  return CaptureTypeWithPattern(type, pattern_span_initializer_list,
                                CaptureOptions());
}

std::string CaptureTypeWithPattern(
    const ServerFieldType& type,
    std::initializer_list<base::StringPiece> pattern_span_initializer_list,
    const CaptureOptions& options) {
  return CaptureTypeWithPattern(
      type, base::StrCat(base::make_span(pattern_span_initializer_list)),
      options);
}

std::string NoCapturePattern(const std::string& pattern,
                             const CaptureOptions& options) {
  std::string quantifier;
  switch (options.quantifier) {
    // Makes the match optional.
    case MATCH_OPTIONAL:
      quantifier = "?";
      break;
    // Makes the match lazy meaning that it is avoided if possible.
    case MATCH_LAZY_OPTIONAL:
      quantifier = "??";
      break;
    // Makes the match required.
    case MATCH_REQUIRED:
      quantifier = "";
  }

  // By adding an "i" in the first group, the capturing is case insensitive.
  // Allow multiple separators to support the ", " case.
  return base::StrCat(
      {"(?i:", pattern, "(?:", options.separator, ")+)", quantifier});
}

std::string CaptureTypeWithAffixedPattern(const ServerFieldType& type,
                                          const std::string& prefix,
                                          const std::string& pattern,
                                          const std::string& suffix,
                                          const CaptureOptions& options) {
  std::string quantifier;
  switch (options.quantifier) {
    // Makes the match optional.
    case MATCH_OPTIONAL:
      quantifier = "?";
      break;
    // Makes the match lazy meaning that it is avoided if possible.
    case MATCH_LAZY_OPTIONAL:
      quantifier = "??";
      break;
    // Makes the match required.
    case MATCH_REQUIRED:
      quantifier = "";
  }

  // By adding an "i" in the first group, the capturing is case insensitive.
  // Allow multiple separators to support the ", " case.
  return base::StrCat(
      {"(?i:", prefix, "(?P<", AutofillType::ServerFieldTypeToString(type), ">",
       pattern, ")", suffix, "(?:", options.separator, ")+)", quantifier});
}

std::string CaptureTypeWithSuffixedPattern(const ServerFieldType& type,
                                           const std::string& pattern,
                                           const std::string& suffix_pattern,
                                           const CaptureOptions& options) {
  return CaptureTypeWithAffixedPattern(type, std::string(), pattern,
                                       suffix_pattern, options);
}

std::string CaptureTypeWithPrefixedPattern(const ServerFieldType& type,
                                           const std::string& prefix_pattern,
                                           const std::string& pattern,
                                           const CaptureOptions& options) {
  return CaptureTypeWithAffixedPattern(type, prefix_pattern, pattern,
                                       std::string(), options);
}

std::string CaptureTypeWithPattern(const ServerFieldType& type,
                                   const std::string& pattern,
                                   CaptureOptions options) {
  return CaptureTypeWithAffixedPattern(type, std::string(), pattern,
                                       std::string(), options);
}

std::u16string NormalizeValue(base::StringPiece16 value,
                              bool keep_white_space) {
  return AutofillProfileComparator::NormalizeForComparison(
      value, keep_white_space ? AutofillProfileComparator::RETAIN_WHITESPACE
                              : AutofillProfileComparator::DISCARD_WHITESPACE);
}

bool AreStringTokenEquivalent(const std::u16string& one,
                              const std::u16string& other) {
  return AreSortedTokensEqual(TokenizeValue(one), TokenizeValue(other));
}

SortedTokenComparisonResult CompareSortedTokens(
    const std::vector<AddressToken>& first,
    const std::vector<AddressToken>& second) {
  // Lambda to compare the normalized values of two AddressTokens.
  auto cmp_normalized = [](const auto& a, const auto& b) {
    return a.normalized_value < b.normalized_value;
  };

  // Verify that the two multi sets are sorted.
  DCHECK(std::is_sorted(first.begin(), first.end(), cmp_normalized) &&
         std::is_sorted(second.begin(), second.end(), cmp_normalized));

  bool is_supserset = std::includes(first.begin(), first.end(), second.begin(),
                                    second.end(), cmp_normalized);
  bool is_subset = std::includes(second.begin(), second.end(), first.begin(),
                                 first.end(), cmp_normalized);

  // If first is both a superset and a subset it is the same.
  if (is_supserset && is_subset)
    return SortedTokenComparisonResult(MATCH);

  // If it is neither, both are distinct.
  if (!is_supserset && !is_subset)
    return SortedTokenComparisonResult(DISTINCT);

  std::vector<AddressToken> additional_tokens;

  // Collect the additional tokens from the superset.
  // Note, that the superset property is already assured.
  std::set_symmetric_difference(
      first.begin(), first.end(), second.begin(), second.end(),
      std::back_inserter(additional_tokens), cmp_normalized);

  if (is_supserset) {
    return SortedTokenComparisonResult(SUPERSET, additional_tokens);
  }

  return SortedTokenComparisonResult(SUBSET, additional_tokens);
}

SortedTokenComparisonResult CompareSortedTokens(const std::u16string& first,
                                                const std::u16string& second) {
  return CompareSortedTokens(TokenizeValue(first), TokenizeValue(second));
}

bool AreSortedTokensEqual(const std::vector<AddressToken>& first,
                          const std::vector<AddressToken>& second) {
  return CompareSortedTokens(first, second).status == MATCH;
}

std::vector<AddressToken> TokenizeValue(const std::u16string value) {
  std::vector<AddressToken> tokens;
  int index = 0;

  // CJK names are a special case and are tokenized by character without the
  // separators.
  if (HasCjkNameCharacteristics(base::UTF16ToUTF8(value))) {
    tokens.reserve(value.size());
    for (size_t i = 0; i < value.size(); i++) {
      std::u16string cjk_separators = u"・·　 ";
      if (cjk_separators.find(value.substr(i, 1)) == std::u16string::npos) {
        tokens.emplace_back(AddressToken{.value = value.substr(i, 1),
                                         .normalized_value = value.substr(i, 1),
                                         .position = index++});
      }
    }
  } else {
    // Split it by white spaces and commas into non-empty values.
    for (const auto& token :
         base::SplitString(value, u", \n", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY)) {
      tokens.emplace_back(
          AddressToken{.value = token,
                       .normalized_value = NormalizeValue(token),
                       .position = index++});
    }
  }
  // Sort the tokens lexicographically by their normalized value.
  std::sort(tokens.begin(), tokens.end(), [](const auto& a, const auto& b) {
    return a.normalized_value < b.normalized_value;
  });

  return tokens;
}

}  // namespace autofill
