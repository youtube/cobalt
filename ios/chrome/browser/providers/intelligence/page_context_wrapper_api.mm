// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/intelligence/page_context_wrapper_api.h"

#import <string>

#import "url/gurl.h"

namespace {
// Script to check whether PageContext should be detached from the request.
constexpr const char16_t* kShouldDetachPageContextScript = u"return false;";
}  // namespace

namespace ios::provider {

bool IsProtectedUrlV2(const GURL& url) {
  return false;
}

const std::u16string GetPageContextShouldDetachScriptV2() {
  return kShouldDetachPageContextScript;
}

}  // namespace ios::provider
