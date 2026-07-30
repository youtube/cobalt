// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/bcp47_extensions.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/i18n/language_tag.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/types/pass_key.h"

namespace base::i18n_extensions {
namespace {

bool IsAllAlphaNumeric(std::string_view s) {
  return std::ranges::all_of(
      s, [](char c) { return base::IsAsciiAlphaNumeric(c); });
}

size_t FindNextKeyIndex(base::span<const std::string_view> subtags) {
  auto it = std::ranges::find_if(subtags, [](const std::string_view& subtag) {
    return subtag.size() == 2u;
  });
  return std::distance(subtags.begin(), it);
}

// Returns true if all items in subtags match the rule for either an attribute
// or a type: three to eight alphanumeric ascii chars.
bool VerifyTypeOrAttributeSubtags(base::span<const std::string_view> subtags) {
  return std::ranges::all_of(subtags, [](std::string_view subtag) {
    return subtag.size() >= 3 && subtag.size() <= 8 &&
           IsAllAlphaNumeric(subtag);
  });
}

// Returns true if subtag matches the rule for a "key": exactly two alphanumeric
// ascii chars.
bool VerifyKeySubtag(std::string_view subtag) {
  return subtag.size() == 2 && IsAllAlphaNumeric(subtag);
}

std::optional<base::flat_set<std::string, std::less<>>>
GetUnicodeAttributesIfValid(
    base::span<const std::string_view> attributes_subspan) {
  // Attributes follow the same format as type subtags.
  if (!VerifyTypeOrAttributeSubtags(attributes_subspan)) {
    return std::nullopt;
  }
  // This constructor will drop duplicates according to rfc6067.
  return base::flat_set<std::string, std::less<>>(attributes_subspan.begin(),
                                                  attributes_subspan.end());
}

std::optional<base::flat_map<std::string, std::string, std::less<>>>
GetUnicodeKeywords(base::span<const std::string_view> keywords_span) {
  base::flat_map<std::string, std::string, std::less<>> keywords;
  while (!keywords_span.empty()) {
    std::string_view key = keywords_span.take_first_elem();
    if (!VerifyKeySubtag(key)) {
      return std::nullopt;
    }
    size_t next_key_index = FindNextKeyIndex(keywords_span);
    base::span<const std::string_view> types_subspan =
        keywords_span.take_first(next_key_index);
    if (!VerifyTypeOrAttributeSubtags(types_subspan)) {
      return std::nullopt;
    }
    auto [it, inserted] =
        keywords.try_emplace(key, base::JoinString(types_subspan, "-"));
    // If it was not possible to insert a new keyword, the input is not in the
    // format defined by the spec which states that a unicode extension key MUST
    // NOT appear more than once.
    if (!inserted) {
      return std::nullopt;
    }
  }

  return keywords;
}

}  // namespace

Extension::Extension(base::PassKey<base::LanguageTag>,
                     std::string_view extension)
    : extension_(extension) {
  CHECK_GE(extension_.size(), 4u);
  CHECK_EQ(extension_[1], '-');
}

UnicodeExtension::~UnicodeExtension() = default;
UnicodeExtension::UnicodeExtension(const UnicodeExtension&) = default;
UnicodeExtension& UnicodeExtension::operator=(const UnicodeExtension&) =
    default;
UnicodeExtension::UnicodeExtension(UnicodeExtension&&) = default;
UnicodeExtension& UnicodeExtension::operator=(UnicodeExtension&&) = default;

UnicodeExtension::UnicodeExtension(
    base::flat_set<std::string, std::less<>> attributes,
    base::flat_map<std::string, std::string, std::less<>> keywords)
    : attributes_(std::move(attributes)), keywords_(std::move(keywords)) {}

// static
std::optional<UnicodeExtension> UnicodeExtension::FromString(
    std::string_view extension) {
  if (extension.size() < 4u) {
    return std::nullopt;
  }
  if (extension[0] != 'u' || extension[1] != '-') {
    return std::nullopt;
  }

  std::vector<std::string_view> subtags = base::SplitStringPiece(
      extension.substr(2), "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  size_t keyword_start = FindNextKeyIndex(subtags);
  base::span<const std::string_view> subtags_span(subtags);
  std::optional<base::flat_set<std::string, std::less<>>> attributes =
      GetUnicodeAttributesIfValid(subtags_span.take_first(keyword_start));
  if (!attributes) {
    return std::nullopt;
  }

  std::optional<base::flat_map<std::string, std::string, std::less<>>>
      keywords = GetUnicodeKeywords(subtags_span);
  if (!keywords) {
    return std::nullopt;
  }

  return UnicodeExtension(*std::move(attributes), *std::move(keywords));
}

bool UnicodeExtension::AddAttribute(std::string_view attribute) {
  if (!VerifyTypeOrAttributeSubtags({attribute})) {
    return false;
  }
  attributes_.emplace(attribute);
  return true;
}

bool UnicodeExtension::SetKeyword(std::string_view key,
                                  std::string_view type_subtags) {
  if (!VerifyKeySubtag(key)) {
    return false;
  }
  std::vector<std::string_view> types = base::SplitStringPiece(
      type_subtags, "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (!VerifyTypeOrAttributeSubtags(types)) {
    return false;
  }
  keywords_.insert_or_assign(key, type_subtags);
  return true;
}

std::string UnicodeExtension::ToString() const {
  std::string subtags = base::JoinString(attributes_, "-");
  // Iterates sorted by key.
  for (auto& [key, value] : keywords_) {
    if (!subtags.empty()) {
      subtags.append("-");
    }
    subtags.append(key);
    if (!value.empty()) {
      base::StrAppend(&subtags, {"-", value});
    }
  }
  return subtags;
}

std::optional<std::string_view> UnicodeExtension::GetKeywordValue(
    std::string_view key) const {
  if (key.size() != 2) {
    return std::nullopt;
  }
  auto it = keywords_.find(key);
  if (it == keywords_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::vector<std::string> UnicodeExtension::GetKeywordKeys() const {
  std::vector<std::string> keys;
  keys.reserve(keywords_.size());
  std::ranges::transform(keywords_, std::back_inserter(keys),
                         [](const auto& pair) { return pair.first; });
  return keys;
}

PrivateUseSubtags::PrivateUseSubtags(base::PassKey<LanguageTag>,
                                     std::string_view private_use)
    : subtags_(std::string(private_use.substr(2))) {
  CHECK_GE(private_use.size(), 3u);
  CHECK_EQ(private_use[0], 'x');
  CHECK_EQ(private_use[1], '-');
}

}  // namespace base::i18n_extensions
