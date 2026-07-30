// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_DICTATION_CONTEXT_H_
#define CHROME_BROWSER_DICTATION_DICTATION_CONTEXT_H_

#include <optional>
#include <string>

#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace dictation {

// Context about the web page that's passed to the speech recognizer to improve
// recognition and command fulfilment.
struct DictationContext {
  DictationContext();
  DictationContext(const DictationContext&) = delete;
  DictationContext& operator=(const DictationContext&) = delete;
  DictationContext(DictationContext&&);
  DictationContext& operator=(DictationContext&&);
  ~DictationContext();

  std::optional<optimization_guide::proto::AnnotatedPageContent>
      annotated_page_content;
  std::optional<std::string> inner_text;
  std::optional<std::string> editable_content;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_DICTATION_CONTEXT_H_
