// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_COMMON_SOURCE_INFO_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_COMMON_SOURCE_INFO_H_

#include "base/time/time.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/common/content_export.h"
#include "net/base/schemeful_site.h"

namespace content {

// Contains common attributes of `StorableSource` and `StoredSource`.
class CONTENT_EXPORT CommonSourceInfo {
 public:
  CommonSourceInfo(attribution_reporting::SuitableOrigin source_origin,
                   attribution_reporting::SuitableOrigin reporting_origin,
                   base::Time source_time,
                   attribution_reporting::mojom::SourceType);

  ~CommonSourceInfo();

  CommonSourceInfo(const CommonSourceInfo&);
  CommonSourceInfo(CommonSourceInfo&&);

  CommonSourceInfo& operator=(const CommonSourceInfo&);
  CommonSourceInfo& operator=(CommonSourceInfo&&);

  const attribution_reporting::SuitableOrigin& source_origin() const {
    return source_origin_;
  }

  const attribution_reporting::SuitableOrigin& reporting_origin() const {
    return reporting_origin_;
  }

  base::Time source_time() const { return source_time_; }

  attribution_reporting::mojom::SourceType source_type() const {
    return source_type_;
  }

  const net::SchemefulSite& source_site() const { return source_site_; }

 private:
  net::SchemefulSite source_site_;
  attribution_reporting::SuitableOrigin source_origin_;
  attribution_reporting::SuitableOrigin reporting_origin_;
  base::Time source_time_;
  attribution_reporting::mojom::SourceType source_type_;

  // When adding new members, the corresponding `operator==()` definition in
  // `attribution_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_COMMON_SOURCE_INFO_H_
