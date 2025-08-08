// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATOR_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/core/page_content_annotations_common.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

using BatchAnnotationCallback =
    base::OnceCallback<void(const std::vector<BatchAnnotationResult>&)>;

// A virtual interface that is called to run a page content annotation. This
// interface can be implemented by testing mocks.
class PageContentAnnotator {
 public:
  virtual ~PageContentAnnotator() = default;

  // Annotates all |inputs| according to the |annotation_type| and returns the
  // result to the given |callback|. The vector size passed to the callback will
  // always match the size and ordering of |inputs|.
  virtual void Annotate(BatchAnnotationCallback callback,
                        const std::vector<std::string>& inputs,
                        AnnotationType annotation_type) = 0;

  // Returns the model info associated with the given AnnotationType, if it is
  // available and loaded.
  virtual absl::optional<ModelInfo> GetModelInfoForType(
      AnnotationType annotation_type) const = 0;

  // Requests that the given model for |type| be loaded in the background and
  // then runs |callback| with true when the model is ready to execute. If the
  // model is ready now, the callback is run immediately. If the model file will
  // never be available, the callback is run with false.
  virtual void RequestAndNotifyWhenModelAvailable(
      AnnotationType type,
      base::OnceCallback<void(bool)> callback) = 0;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATOR_H_
