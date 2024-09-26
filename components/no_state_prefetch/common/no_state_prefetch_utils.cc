// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/common/no_state_prefetch_utils.h"

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
// GN doesn't understand conditional includes, so we need nogncheck here.
#include "extensions/common/constants.h"  // nogncheck
#endif

namespace prerender {

namespace {

// Valid HTTP methods for NoStatePrefetch.
const char* const kValidHttpMethods[] = {
    "GET",
    "HEAD",
};

// This enum is used to define the buckets for the
// "Prerender.NoStatePrefetchResourceCount" histogram family.
// Hence, existing enumerated constants should never be deleted or reordered,
// and new constants should only be appended at the end of the enumeration.
enum NoStatePrefetchResponseType {
  NO_STORE = 1 << 0,
  REDIRECT = 1 << 1,
  MAIN_RESOURCE = 1 << 2,
  NO_STATE_PREFETCH_RESPONSE_TYPE_COUNT = 1 << 3
};

int GetResourceType(bool is_main_resource, bool is_redirect, bool is_no_store) {
  return (is_no_store * NO_STORE) + (is_redirect * REDIRECT) +
         (is_main_resource * MAIN_RESOURCE);
}

}  // namespace

const char kFollowOnlyWhenPrerenderShown[] = "follow-only-when-prerender-shown";

bool DoesURLHaveValidScheme(const GURL& url) {
  return (url.SchemeIsHTTPOrHTTPS() ||
#if BUILDFLAG(ENABLE_EXTENSIONS)
          url.SchemeIs(extensions::kExtensionScheme) ||
#endif
          url.SchemeIs(url::kDataScheme));
}

bool DoesSubresourceURLHaveValidScheme(const GURL& url) {
  return DoesURLHaveValidScheme(url) || url == url::kAboutBlankURL;
}

bool IsValidHttpMethod(const std::string& method) {
  // |method| has been canonicalized to upper case at this point so we can just
  // compare them.
  DCHECK_EQ(method, base::ToUpperASCII(method));
  for (auto* valid_method : kValidHttpMethods) {
    if (method == valid_method)
      return true;
  }
  return false;
}

std::string ComposeHistogramName(const std::string& prefix_type,
                                 const std::string& name) {
  if (prefix_type.empty())
    return std::string("Prerender.") + name;
  return std::string("Prerender.") + prefix_type + std::string("_") + name;
}

void RecordPrefetchResponseReceived(const std::string& histogram_prefix,
                                    bool is_main_resource,
                                    bool is_redirect,
                                    bool is_no_store) {
  int sample = GetResourceType(is_main_resource, is_redirect, is_no_store);
  std::string histogram_name =
      ComposeHistogramName(histogram_prefix, "NoStatePrefetchResponseTypes");
  base::UmaHistogramExactLinear(histogram_name, sample,
                                NO_STATE_PREFETCH_RESPONSE_TYPE_COUNT);
}

void RecordPrefetchRedirectCount(const std::string& histogram_prefix,
                                 bool is_main_resource,
                                 int redirect_count) {
  const int kMaxRedirectCount = 10;
  std::string histogram_base_name = base::StringPrintf(
      "NoStatePrefetch%sResourceRedirects", is_main_resource ? "Main" : "Sub");
  std::string histogram_name =
      ComposeHistogramName(histogram_prefix, histogram_base_name);
  base::UmaHistogramExactLinear(histogram_name, redirect_count,
                                kMaxRedirectCount);
}

}  // namespace prerender
