// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDERER_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDERER_H_

#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {

// Interface for speculation-rules based prerenderer.
class Prerenderer {
 public:
  using PrerenderCancellationCallback =
      base::RepeatingCallback<void(const GURL&)>;

  virtual ~Prerenderer() = default;

  virtual void ProcessCandidatesForPrerender(
      const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) = 0;

  virtual bool MaybePrerender(
      const blink::mojom::SpeculationCandidatePtr& candidate) = 0;

  virtual bool ShouldWaitForPrerenderResult(const GURL& url) = 0;

  virtual void SetPrerenderCancellationCallback(
      PrerenderCancellationCallback callback) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDERER_H_
