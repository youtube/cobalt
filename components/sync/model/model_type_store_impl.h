// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_MODEL_TYPE_STORE_IMPL_H_
#define COMPONENTS_SYNC_MODEL_MODEL_TYPE_STORE_IMPL_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/storage_type.h"
#include "components/sync/model/model_type_store.h"

namespace syncer {

class BlockingModelTypeStoreImpl;

// ModelTypeStoreImpl handles details of store initialization and threading.
// Actual leveldb IO calls are performed in BlockingModelTypeStoreImpl (in the
// underlying ModelTypeStoreBackend).
class ModelTypeStoreImpl : public ModelTypeStore {
 public:
  // |backend_store| must not be null and must have been created in
  // |backend_task_runner|.
  ModelTypeStoreImpl(
      ModelType model_type,
      StorageType storage_type,
      std::unique_ptr<BlockingModelTypeStoreImpl, base::OnTaskRunnerDeleter>
          backend_store,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner);

  ModelTypeStoreImpl(const ModelTypeStoreImpl&) = delete;
  ModelTypeStoreImpl& operator=(const ModelTypeStoreImpl&) = delete;

  ~ModelTypeStoreImpl() override;

  // ModelTypeStore implementation.
  void ReadData(const IdList& id_list, ReadDataCallback callback) override;
  void ReadAllData(ReadAllDataCallback callback) override;
  void ReadAllMetadata(ReadMetadataCallback callback) override;
  void ReadAllDataAndPreprocess(
      PreprocessCallback preprocess_on_backend_sequence_callback,
      CallbackWithResult completion_on_frontend_sequence_callback) override;
  std::unique_ptr<WriteBatch> CreateWriteBatch() override;
  void CommitWriteBatch(std::unique_ptr<WriteBatch> write_batch,
                        CallbackWithResult callback) override;
  void DeleteAllDataAndMetadata(CallbackWithResult callback) override;

 private:
  // Callbacks for different calls to ModelTypeStoreBackend.
  void ReadDataDone(ReadDataCallback callback,
                    std::unique_ptr<RecordList> record_list,
                    std::unique_ptr<IdList> missing_id_list,
                    const absl::optional<ModelError>& error);
  void ReadAllDataDone(ReadAllDataCallback callback,
                       std::unique_ptr<RecordList> record_list,
                       const absl::optional<ModelError>& error);
  void ReadAllMetadataDone(ReadMetadataCallback callback,
                           std::unique_ptr<MetadataBatch> metadata_batch,
                           const absl::optional<ModelError>& error);
  void ReadAllDataAndPreprocessDone(CallbackWithResult callback,
                                    const absl::optional<ModelError>& error);
  void WriteModificationsDone(CallbackWithResult callback,
                              const absl::optional<ModelError>& error);

  const ModelType model_type_;
  const StorageType storage_type_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  // |backend_store_| should be deleted on backend thread.
  std::unique_ptr<BlockingModelTypeStoreImpl, base::OnTaskRunnerDeleter>
      backend_store_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ModelTypeStoreImpl> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_MODEL_TYPE_STORE_IMPL_H_
