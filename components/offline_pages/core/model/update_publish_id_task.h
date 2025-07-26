// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_UPDATE_PUBLISH_ID_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_UPDATE_PUBLISH_ID_TASK_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/model/get_pages_task.h"
#include "components/offline_pages/core/offline_page_archive_publisher.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {

using ReadResult = GetPagesTask::ReadResult;

class OfflinePageMetadataStore;

// Task that updates the file path in the metadata store. It takes the offline
// ID of the page accessed, the new file path, and the completion callback.
class UpdatePublishIdTask : public Task {
 public:
  UpdatePublishIdTask(OfflinePageMetadataStore* store,
                      int64_t offline_id,
                      const PublishedArchiveId& publish_id,
                      base::OnceCallback<void(bool)> callback);

  UpdatePublishIdTask(const UpdatePublishIdTask&) = delete;
  UpdatePublishIdTask& operator=(const UpdatePublishIdTask&) = delete;

  ~UpdatePublishIdTask() override;

 private:
  // Task implementation.
  void Run() override;

  void OnUpdatePublishIdDone(bool result);

  // The metadata store used to update the page. Not owned.
  raw_ptr<OfflinePageMetadataStore> store_;

  int64_t offline_id_;
  PublishedArchiveId publish_id_;
  base::OnceCallback<void(bool)> callback_;

  base::WeakPtrFactory<UpdatePublishIdTask> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_UPDATE_PUBLISH_ID_TASK_H_
