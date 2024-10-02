// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_DATABASE_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_DATABASE_IMPL_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/ukm_database.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "url/gurl.h"

namespace segmentation_platform {

class UkmDatabaseBackend;

class UkmDatabaseImpl : public UkmDatabase {
 public:
  explicit UkmDatabaseImpl(const base::FilePath& database_path);
  ~UkmDatabaseImpl() override;

  UkmDatabaseImpl(const UkmDatabaseImpl&) = delete;
  UkmDatabaseImpl& operator=(const UkmDatabaseImpl&) = delete;

  void InitDatabase(SuccessCallback callback) override;
  void StoreUkmEntry(ukm::mojom::UkmEntryPtr ukm_entry) override;
  void UpdateUrlForUkmSource(ukm::SourceId source_id,
                             const GURL& url,
                             bool is_validated) override;
  void OnUrlValidated(const GURL& url) override;
  void RemoveUrls(const std::vector<GURL>& urls, bool all_urls) override;
  void RunReadonlyQueries(QueryList&& queries, QueryCallback callback) override;
  void DeleteEntriesOlderThan(base::Time time) override;

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  std::unique_ptr<UkmDatabaseBackend> backend_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_DATABASE_IMPL_H_
