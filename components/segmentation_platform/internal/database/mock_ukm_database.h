// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_MOCK_UKM_DATABASE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_MOCK_UKM_DATABASE_H_

#include "components/segmentation_platform/internal/database/ukm_database.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace segmentation_platform {

class MockUkmDatabase : public UkmDatabase {
 public:
  MockUkmDatabase();
  ~MockUkmDatabase() override;

  MOCK_METHOD1(InitDatabase, void(SuccessCallback callback));

  MOCK_METHOD1(StoreUkmEntry, void(ukm::mojom::UkmEntryPtr ukm_entry));

  MOCK_METHOD3(UpdateUrlForUkmSource,
               void(ukm::SourceId source_id,
                    const GURL& url,
                    bool is_validated));

  MOCK_METHOD1(OnUrlValidated, void(const GURL& url));

  MOCK_METHOD2(RemoveUrls, void(const std::vector<GURL>& urls, bool));

  MOCK_METHOD2(RunReadonlyQueries,
               void(QueryList&& queries, QueryCallback callback));

  MOCK_METHOD1(DeleteEntriesOlderThan, void(base::Time time));
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_MOCK_UKM_DATABASE_H_
