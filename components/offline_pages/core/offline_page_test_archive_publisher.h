// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_TEST_ARCHIVE_PUBLISHER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_TEST_ARCHIVE_PUBLISHER_H_

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/offline_page_archive_publisher.h"
#include "components/offline_pages/core/offline_page_item.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace offline_pages {

class ArchiveManager;

class OfflinePageTestArchivePublisher : public OfflinePageArchivePublisher {
 public:
  OfflinePageTestArchivePublisher(ArchiveManager* archive_manager,
                                  int64_t download_id_to_use);
  ~OfflinePageTestArchivePublisher() override;

  void PublishArchive(
      const OfflinePageItem& offline_page,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      PublishArchiveDoneCallback publish_done_callback) const override;

  void UnpublishArchives(
      const std::vector<PublishedArchiveId>& archive_ids) const override;

  base::WeakPtr<OfflinePageArchivePublisher> GetWeakPtr() override;

  void set_archive_attempt_failure(bool fail) {
    archive_attempt_failure_ = fail;
  }

  void expect_publish_archive_called(bool expect) {
    expect_publish_archive_called_ = expect;
  }

  void use_verbatim_archive_path(bool use) { use_verbatim_archive_path_ = use; }

  PublishedArchiveId last_removed_id() const { return last_removed_id_; }

 private:
  bool expect_publish_archive_called_;
  mutable bool publish_archive_called_;
  bool archive_attempt_failure_;
  bool use_verbatim_archive_path_;
  int64_t download_id_;
  mutable PublishedArchiveId last_removed_id_;

  raw_ptr<ArchiveManager> archive_manager_;
  base::WeakPtrFactory<OfflinePageArchivePublisher> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_TEST_ARCHIVE_PUBLISHER_H_
