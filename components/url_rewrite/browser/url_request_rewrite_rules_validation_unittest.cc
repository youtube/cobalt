// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_rewrite/browser/url_request_rewrite_rules_validation.h"

#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace url_rewrite {
namespace {

mojom::UrlRequestActionPtr CreateRewriteAddHeaders(
    base::StringPiece header_name,
    base::StringPiece header_value) {
  auto add_headers = mojom::UrlRequestRewriteAddHeaders::New();
  add_headers->headers.push_back(mojom::UrlHeader::New(
      std::string(header_name), std::string(header_value)));
  return mojom::UrlRequestAction::NewAddHeaders(std::move(add_headers));
}

mojom::UrlRequestActionPtr CreateRewriteRemoveHeader(
    absl::optional<base::StringPiece> query_pattern,
    base::StringPiece header_name) {
  auto remove_header = mojom::UrlRequestRewriteRemoveHeader::New();
  if (query_pattern)
    remove_header->query_pattern.emplace(std::string(*query_pattern));
  remove_header->header_name = std::string(header_name);
  return mojom::UrlRequestAction::NewRemoveHeader(std::move(remove_header));
}

mojom::UrlRequestActionPtr CreateRewriteSubstituteQueryPattern(
    base::StringPiece pattern,
    base::StringPiece substitution) {
  auto substitute_query_pattern =
      mojom::UrlRequestRewriteSubstituteQueryPattern::New();
  substitute_query_pattern->pattern = std::string(pattern);
  substitute_query_pattern->substitution = std::string(substitution);
  return mojom::UrlRequestAction::NewSubstituteQueryPattern(
      std::move(substitute_query_pattern));
}

mojom::UrlRequestActionPtr CreateRewriteReplaceUrl(
    base::StringPiece url_ends_with,
    base::StringPiece new_url) {
  auto replace_url = mojom::UrlRequestRewriteReplaceUrl::New();
  replace_url->url_ends_with = std::string(url_ends_with);
  replace_url->new_url = GURL(new_url);
  return mojom::UrlRequestAction::NewReplaceUrl(std::move(replace_url));
}

mojom::UrlRequestActionPtr CreateRewriteAppendToQuery(base::StringPiece query) {
  auto append_to_query = mojom::UrlRequestRewriteAppendToQuery::New();
  append_to_query->query = std::string(query);
  return mojom::UrlRequestAction::NewAppendToQuery(std::move(append_to_query));
}

bool ValidateRulesFromAction(mojom::UrlRequestActionPtr action) {
  auto rule = mojom::UrlRequestRule::New();
  rule->actions.emplace_back(std::move(action));

  mojom::UrlRequestRewriteRulesPtr rules = mojom::UrlRequestRewriteRules::New();
  rules->rules.push_back(std::move(rule));
  return ValidateRules(rules.get());
}

}  // namespace

// Tests AddHeaders rewrites are properly converted to their Mojo equivalent.
TEST(UrlRequestRewriteRulesValidationTest, ValidateAddHeaders) {
  EXPECT_TRUE(
      ValidateRulesFromAction(CreateRewriteAddHeaders("Test", "Value")));

  // Invalid AddHeaders header name.
  EXPECT_FALSE(
      ValidateRulesFromAction(CreateRewriteAddHeaders("Te\nst1", "Value")));

  // Invalid AddHeaders header value.
  EXPECT_FALSE(
      ValidateRulesFromAction(CreateRewriteAddHeaders("Test1", "Val\nue")));

  // Empty AddHeaders.
  EXPECT_FALSE(ValidateRulesFromAction(mojom::UrlRequestAction::NewAddHeaders(
      mojom::UrlRequestRewriteAddHeaders::New())));
}

// Tests RemoveHeader rewrites are properly converted to their Mojo equivalent.
TEST(UrlRequestRewriteRulesValidationTest, ValidateRemoveHeader) {
  EXPECT_TRUE(ValidateRulesFromAction(
      CreateRewriteRemoveHeader(absl::make_optional("Test"), "Header")));

  // Create a RemoveHeader action with no pattern.
  EXPECT_TRUE(ValidateRulesFromAction(
      CreateRewriteRemoveHeader(absl::nullopt, "Header")));

  // Invalid RemoveHeader header name.
  EXPECT_FALSE(
      ValidateRulesFromAction(CreateRewriteRemoveHeader("Query", "Head\ner")));

  // Empty RemoveHeader.
  EXPECT_FALSE(ValidateRulesFromAction(mojom::UrlRequestAction::NewRemoveHeader(
      mojom::UrlRequestRewriteRemoveHeader::New())));
}

// Tests SubstituteQueryPattern rewrites are properly converted to their Mojo
// equivalent.
TEST(UrlRequestRewriteRulesValidationTest, ValidateSubstituteQueryPattern) {
  EXPECT_TRUE(ValidateRulesFromAction(
      CreateRewriteSubstituteQueryPattern("Pattern", "Substitution")));

  EXPECT_FALSE(ValidateRulesFromAction(
      mojom::UrlRequestAction::NewSubstituteQueryPattern(
          mojom::UrlRequestRewriteSubstituteQueryPattern::New())));
}

// Tests ReplaceUrl rewrites are properly converted to their Mojo equivalent.
TEST(UrlRequestRewriteRulesValidationTest, ValidateReplaceUrl) {
  GURL url("http://site.xyz");
  EXPECT_TRUE(ValidateRulesFromAction(
      CreateRewriteReplaceUrl("/something", url.spec())));

  // Invalid ReplaceUrl url_ends_with.
  EXPECT_FALSE(ValidateRulesFromAction(
      CreateRewriteReplaceUrl("some%00thing", GURL("http://site.xyz").spec())));

  // Invalid ReplaceUrl new_url.
  EXPECT_FALSE(ValidateRulesFromAction(
      CreateRewriteReplaceUrl("/something", "http:site:xyz")));

  // Empty ReplaceUrl.
  EXPECT_FALSE(ValidateRulesFromAction(mojom::UrlRequestAction::NewReplaceUrl(
      mojom::UrlRequestRewriteReplaceUrl::New())));
}

// Tests AppendToQuery rewrites are properly converted to their Mojo equivalent.
TEST(UrlRequestRewriteRulesValidationTest, ValidateAppendToQuery) {
  EXPECT_TRUE(
      ValidateRulesFromAction(CreateRewriteAppendToQuery("foo=bar&foo")));

  EXPECT_FALSE(
      ValidateRulesFromAction(mojom::UrlRequestAction::NewAppendToQuery(
          mojom::UrlRequestRewriteAppendToQuery::New())));
}

// Tests validation is working as expected.
TEST(UrlRequestRewriteRulesValidationTest, ValidateNullAction) {
  // Empty action.
  EXPECT_FALSE(ValidateRulesFromAction(nullptr));
}

TEST(UrlRequestRewriteRulesValidationTest, ValidateNullRules) {
  // Empty action.
  EXPECT_FALSE(ValidateRules(nullptr));
}

}  // namespace url_rewrite
