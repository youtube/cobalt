// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_MATCHER_URL_UTIL_H_
#define COMPONENTS_URL_MATCHER_URL_UTIL_H_

#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_matcher_export.h"

class GURL;

namespace url_matcher {
namespace util {

// Maximum filters allowed. Filters over this index are ignored.
inline constexpr size_t kMaxFiltersAllowed = 1000;

// Converts a ValueList `value` of strings into a vector. Returns true if
// successful.
bool GetAsStringVector(const base::Value* value, std::vector<std::string>* out);

// Normalizes a URL for matching purposes.
URL_MATCHER_EXPORT GURL Normalize(const GURL& url);

// Helper function to extract the underlying URL wrapped by services such as
// Google AMP or Google Translate. Returns an empty GURL if `url` doesn't match
// a known format.
URL_MATCHER_EXPORT GURL GetEmbeddedURL(const GURL& url);

// Helper function to extract the underlying URL wrapped by Google AMP viewer.
// Returns an empty GURL if `url` doesn't match a known format.
URL_MATCHER_EXPORT GURL GetGoogleAmpViewerEmbeddedURL(const GURL& url);

// Utility struct used to represent a url filter scheme into its components.
struct URL_MATCHER_EXPORT FilterComponents {
  FilterComponents();
  FilterComponents(const FilterComponents&) = delete;
  FilterComponents(FilterComponents&&);
  FilterComponents& operator=(const FilterComponents&) = delete;
  FilterComponents& operator=(FilterComponents&&) = default;

  ~FilterComponents();

  // Returns true if `this` represents the "*" filter.
  bool IsWildcard() const;
  std::string scheme;
  std::string host;
  uint16_t port = 0;
  std::string path;
  std::string query;
  // Number of conditions that a url needs to match it to be considered a match
  // for this filter.
  int number_of_url_matching_conditions = 0;
  bool match_subdomains = true;
  bool allow = true;
};

// Creates a condition set that can be used with the `url_matcher`. `id` needs
// to be a unique number that will be returned by the `url_matcher` if the URL
// matches that condition set. `allow` indicates if it is an allow-list (true)
// or block-list (false) filter.
URL_MATCHER_EXPORT scoped_refptr<url_matcher::URLMatcherConditionSet>
CreateConditionSet(url_matcher::URLMatcher* url_matcher,
                   base::MatcherStringPattern::ID id,
                   const std::string& scheme,
                   const std::string& host,
                   bool match_subdomains,
                   uint16_t port,
                   const std::string& path,
                   const std::string& query,
                   bool allow);

// Splits a URL filter into its components. A GURL isn't used because these
// can be invalid URLs e.g. "google.com".
// Returns false if the URL couldn't be parsed. In case false is returned,
// the values of output parameters are undefined.
// The `filter` should have the format described at
// http://www.chromium.org/administrators/url-blocklist-filter-format and
// accepts wildcards. The `host` is preprocessed so it can be passed to
// URLMatcher for the appropriate condition. The optional username and password
// are ignored. `match_subdomains` specifies whether the filter should include
// subdomains of the hostname (if it is one.) `port` is 0 if none is explicitly
// defined. `path` does not include query parameters. `query` contains the query
// parameters ('?' not included). All arguments are mandatory.
URL_MATCHER_EXPORT bool FilterToComponents(const std::string& filter,
                                           std::string* scheme,
                                           std::string* host,
                                           bool* match_subdomains,
                                           uint16_t* port,
                                           std::string* path,
                                           std::string* query);

// Adds a limited number of URL filters `patterns` to a URLMatcher
// `matcher`. The `max_filters` parameter specifies the maximum number of
// filters added.
//
// If `allow` is true, the filters will allow matching URLs; otherwise, they
// block them. The `id` parameter provides a pointer to the ID assigned to the
// filters, incremented for each filter added.
//
// `patterns` should be a list of URL patterns (see format description at
// http://www.chromium.org/administrators/url-blocklist-filter-format).
//
// An optional map to store the generated FilterComponents can be provided
// via |filters|.
URL_MATCHER_EXPORT void AddFiltersWithLimit(
    url_matcher::URLMatcher* matcher,
    bool allow,
    base::MatcherStringPattern::ID* id,
    const base::Value::List& patterns,
    std::map<base::MatcherStringPattern::ID, FilterComponents>* filters =
        nullptr,
    size_t max_filters = kMaxFiltersAllowed);

// Adds a limited number of URL filters `patterns` to a URLMatcher
// `matcher`. The `max_filters` parameter specifies the maximum number of
// filters added.
//
// If `allow` is true, the filters will allow matching URLs; otherwise, they
// block them. The `id` parameter provides a pointer to the ID assigned to the
// filters, incremented for each filter added.
//
// `patterns` should be a list of URL patterns (see format description at
// http://www.chromium.org/administrators/url-blocklist-filter-format).
//
// An optional map to store the generated FilterComponents can be provided
// via |filters|.
URL_MATCHER_EXPORT void AddFiltersWithLimit(
    url_matcher::URLMatcher* matcher,
    bool allow,
    base::MatcherStringPattern::ID* id,
    const std::vector<std::string>& patterns,
    std::map<base::MatcherStringPattern::ID, FilterComponents>* filters =
        nullptr,
    size_t max_filters = kMaxFiltersAllowed);

URL_MATCHER_EXPORT void AddAllowFiltersWithLimit(
    url_matcher::URLMatcher* matcher,
    const base::Value::List& patterns,
    size_t max_filters = kMaxFiltersAllowed);

URL_MATCHER_EXPORT void AddAllowFiltersWithLimit(
    url_matcher::URLMatcher* matcher,
    const std::vector<std::string>& patterns,
    size_t max_filters = kMaxFiltersAllowed);

}  // namespace util
}  // namespace url_matcher

#endif  // COMPONENTS_URL_MATCHER_URL_UTIL_H_
