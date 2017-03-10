// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BLINK_CDM_RESULT_PROMISE_HELPER_H_
#define COBALT_MEDIA_BLINK_CDM_RESULT_PROMISE_HELPER_H_

#include <string>

#include "cobalt/media/base/media_keys.h"
#include "cobalt/media/blink/media_blink_export.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModuleResult.h"

namespace cobalt {
namespace media {

// A superset of media::MediaKeys::Exception for UMA reporting. These values
// should never be changed as it will affect existing reporting, and must match
// the values for CdmPromiseResult in tools/metrics/histograms/histograms.xml.
enum CdmResultForUMA {
  SUCCESS = 0,
  NOT_SUPPORTED_ERROR = 1,
  INVALID_STATE_ERROR = 2,
  INVALID_ACCESS_ERROR = 3,
  QUOTA_EXCEEDED_ERROR = 4,
  UNKNOWN_ERROR = 5,
  CLIENT_ERROR = 6,
  OUTPUT_ERROR = 7,
  NUM_RESULT_CODES
};

MEDIA_BLINK_EXPORT CdmResultForUMA
ConvertCdmExceptionToResultForUMA(MediaKeys::Exception exception_code);

MEDIA_BLINK_EXPORT blink::WebContentDecryptionModuleException
ConvertCdmException(MediaKeys::Exception exception_code);

MEDIA_BLINK_EXPORT void ReportCdmResultUMA(const std::string& uma_name,
                                           CdmResultForUMA result);

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BLINK_CDM_RESULT_PROMISE_HELPER_H_
