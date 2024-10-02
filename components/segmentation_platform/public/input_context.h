// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_INPUT_CONTEXT_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_INPUT_CONTEXT_H_

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "components/segmentation_platform/public/types/processed_value.h"

namespace segmentation_platform {

// Input provided for segment selection, based on the current state of the
// browser.
struct InputContext : base::RefCounted<InputContext> {
 public:
  InputContext();

  InputContext(const InputContext&) = delete;
  InputContext& operator=(const InputContext&) = delete;

  // A list of params that can be used as input either directly to the model, or
  // to SQL queries, or custom input delegates. The exact mechanism and
  // semantics is still under construction.
  base::flat_map<std::string, processing::ProcessedValue> metadata_args;

 private:
  friend class base::RefCounted<InputContext>;

  ~InputContext();
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_INPUT_CONTEXT_H_
