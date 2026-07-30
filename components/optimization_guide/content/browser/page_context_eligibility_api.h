// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTEXT_ELIGIBILITY_API_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTEXT_ELIGIBILITY_API_H_

#include <cstdint>
#include <stddef.h>

#include <string>
#include <string_view>
#include <vector>

extern "C" {

namespace optimization_guide {

// A meta tag represented by its name and content attributes.
struct MetaTag {
  explicit MetaTag(const std::string& name, const std::string& content);
  MetaTag(const MetaTag& other);
  MetaTag& operator=(const MetaTag& other);
  ~MetaTag();

  std::string name;
  std::string content;
};

// A frame URL represented by its host and path.
struct FrameUrl {
  explicit FrameUrl(std::string_view host, std::string_view path);
  FrameUrl(const FrameUrl& other);
  FrameUrl& operator=(const FrameUrl& other);
  ~FrameUrl();

  // The host of the URL of the frame.
  std::string host;
  // The path of the URL of the frame.
  std::string path;
};

// Metadata about a frame.
struct FrameMetadata {
  explicit FrameMetadata(const std::string& host,
                         const std::string& path,
                         std::vector<MetaTag> meta_tags);
  FrameMetadata(const FrameMetadata& other);
  FrameMetadata& operator=(const FrameMetadata& other);
  ~FrameMetadata();

  // The host of the URL of the frame.
  std::string host;
  // The path of the URL of the frame.
  std::string path;
  std::vector<MetaTag> meta_tags;
};

// Represents an span of string views. Used instead of a std::span to avoid ABI
// issues.
struct StringViewSpan {
  const std::string_view* data;
  size_t size;
};

enum class PageEligibility : int32_t {
  // The page is definitively ineligible. Stop processing.
  kIneligible,
  // The page is fully eligible. No further checks needed.
  kEligible,
  // Eligibility depends on meta tags. Extract the tags in
  // `meta_tag_names_affecting_eligibility` and pass them to
  // `IsPageContextEligible`.
  kConditionalOnMetaTags
};

struct PageEligibilityResult {
  PageEligibility status;

  // Only meaningful if status is kConditionalOnMetaTags.
  StringViewSpan meta_tag_names_affecting_eligibility;
};

// Table of C API functions defined within the library.
struct PageContextEligibilityAPI {
  // Whether the page is context eligible.
  bool (*IsPageContextEligible)(
      const std::string& host,
      const std::string& path,
      const std::vector<optimization_guide::FrameMetadata>& frame_metadata);
  // Whether the page is context eligible with account.
  bool (*IsPageContextEligibleWithAccount)(
      const std::string& host,
      const std::string& path,
      const std::string& account,
      const std::vector<optimization_guide::FrameMetadata>& frame_metadata);
  // Whether the page context should be reextracted.
  bool (*ShouldReextractPageContext)(
      const std::string& host,
      const std::string& path,
      const std::vector<std::string>& updated_meta_tags);
  // Returns the meta tag names that, if changed on a frame, could affect the
  // page context eligibility. This could be empty if meta tag changes would
  // not affect eligibility.
  StringViewSpan (*GetMetaTagNamesAffectingEligibility)(
      std::string_view host,
      std::string_view path,
      const std::vector<FrameMetadata>& frame_metadata);
  // Checks the page eligibility based on frame URLs before page metadata extraction.
  // The first entry in `frames` must correspond to the main frame.
  //
  // Caller Flow:
  // 1. Call this method when the set of `frames` or their URLs change.
  // 2. If the returned `status` is PageEligibility::kIneligible:
  //    The page is definitively ineligible; stop processing.
  // 3. If the returned `status` is PageEligibility::kEligible:
  //    The page is unconditionally eligible; no further checks or tag extraction
  //    are needed.
  // 4. If the returned `status` is PageEligibility::kConditionalOnMetaTags:
  //    Page eligibility depends on meta tags. Extract the meta tags whose names
  //    are in `meta_tag_names_affecting_eligibility` for each frame, and pass
  //    them to `IsPageContextEligible` (or `IsPageContextEligibleWithAccount`) to
  //    evaluate final eligibility whenever the tags change.
  PageEligibilityResult (*CheckPageEligibility)(
      const std::vector<FrameUrl>& frames);
};

// Signature of the GetPageContextEligibilityAPI() function which the shared
// library exports.
using PageContextEligibilityAPIGetter = const PageContextEligibilityAPI* (*)();

}  // namespace optimization_guide

}  // extern "C"

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTEXT_ELIGIBILITY_API_H_
