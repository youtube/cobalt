// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_DEBUG_UKM_DEBUG_DATA_EXTRACTOR_H_
#define COMPONENTS_UKM_DEBUG_UKM_DEBUG_DATA_EXTRACTOR_H_

#include <string>

#include "base/macros.h"
#include "base/values.h"
#include "components/ukm/ukm_service.h"

namespace ukm {

class UkmService;

namespace debug {

// Extracts UKM data as an HTML page for debugging purposes.
class UkmDebugDataExtractor {
 public:
  UkmDebugDataExtractor();
  ~UkmDebugDataExtractor();

  // Returns UKM data structured in a DictionaryValue.
  static base::Value GetStructuredData(const UkmService* ukm_service);

 private:
  DISALLOW_COPY_AND_ASSIGN(UkmDebugDataExtractor);
};

}  // namespace debug
}  // namespace ukm

#endif  // COMPONENTS_UKM_DEBUG_UKM_DEBUG_DATA_EXTRACTOR_H_
