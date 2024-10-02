// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/common/test/url_request_rewrite_test_util.h"

#include "base/strings/string_piece.h"
#include "fuchsia_web/common/string_util.h"

fuchsia::web::UrlRequestRewrite CreateRewriteAddHeaders(
    base::StringPiece header_name,
    base::StringPiece header_value) {
  fuchsia::net::http::Header header;
  header.name = StringToBytes(header_name);
  header.value = StringToBytes(header_value);

  std::vector<fuchsia::net::http::Header> response_headers;
  response_headers.push_back(std::move(header));

  fuchsia::web::UrlRequestRewriteAddHeaders add_headers;
  add_headers.set_headers(std::move(response_headers));
  fuchsia::web::UrlRequestRewrite rewrite;
  rewrite.set_add_headers(std::move(add_headers));

  return rewrite;
}

fuchsia::web::UrlRequestRewrite CreateRewriteRemoveHeader(
    absl::optional<base::StringPiece> query_pattern,
    base::StringPiece header_name) {
  fuchsia::web::UrlRequestRewriteRemoveHeader remove_header;
  if (query_pattern)
    remove_header.set_query_pattern(std::string(query_pattern.value()));
  remove_header.set_header_name(StringToBytes(header_name));
  fuchsia::web::UrlRequestRewrite rewrite;
  rewrite.set_remove_header(std::move(remove_header));

  return rewrite;
}

fuchsia::web::UrlRequestRewrite CreateRewriteSubstituteQueryPattern(
    base::StringPiece pattern,
    base::StringPiece substitution) {
  fuchsia::web::UrlRequestRewriteSubstituteQueryPattern
      substitute_query_pattern;
  substitute_query_pattern.set_pattern(std::string(pattern));
  substitute_query_pattern.set_substitution(std::string(substitution));
  fuchsia::web::UrlRequestRewrite rewrite;
  rewrite.set_substitute_query_pattern(std::move(substitute_query_pattern));

  return rewrite;
}

fuchsia::web::UrlRequestRewrite CreateRewriteReplaceUrl(
    base::StringPiece url_ends_with,
    base::StringPiece new_url) {
  fuchsia::web::UrlRequestRewriteReplaceUrl replace_url;
  replace_url.set_url_ends_with(std::string(url_ends_with));
  replace_url.set_new_url(std::string(new_url));
  fuchsia::web::UrlRequestRewrite rewrite;
  rewrite.set_replace_url(std::move(replace_url));

  return rewrite;
}

fuchsia::web::UrlRequestRewrite CreateRewriteAppendToQuery(
    base::StringPiece query) {
  fuchsia::web::UrlRequestRewriteAppendToQuery append_to_query;
  append_to_query.set_query(std::string(query));
  fuchsia::web::UrlRequestRewrite rewrite;
  rewrite.set_append_to_query(std::move(append_to_query));

  return rewrite;
}
