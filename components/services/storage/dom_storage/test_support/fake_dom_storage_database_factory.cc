// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/test_support/fake_dom_storage_database_factory.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/dom_storage/db_status.h"
#include "components/services/storage/dom_storage/dom_storage_histogram_helper.h"
#include "components/services/storage/dom_storage/test_support/fake_dom_storage_database.h"

namespace storage {

FakeDomStorageDatabaseFactory::FakeDomStorageDatabaseFactory(
    int num_open_failures,
    int num_destroy_failures)
    : num_open_failures_(num_open_failures),
      num_destroy_failures_(num_destroy_failures),
      // base::Unretained is safe because `this` owns `scoped_factory_` and
      // destructs it first.
      scoped_factory_(
          base::BindRepeating(&FakeDomStorageDatabaseFactory::Open,
                              base::Unretained(this)),
          base::BindRepeating(&FakeDomStorageDatabaseFactory::Destroy,
                              base::Unretained(this))) {}

FakeDomStorageDatabaseFactory::FakeDomStorageDatabaseFactory(
    int num_open_failures,
    DomStorageDatabaseFactory::DestroyCallback custom_destroy_callback)
    : num_open_failures_(num_open_failures),
      num_destroy_failures_(0),
      // base::Unretained is safe because `this` owns `scoped_factory_` and
      // destructs it first.
      scoped_factory_(base::BindRepeating(&FakeDomStorageDatabaseFactory::Open,
                                          base::Unretained(this)),
                      std::move(custom_destroy_callback)) {}

FakeDomStorageDatabaseFactory::~FakeDomStorageDatabaseFactory() = default;

void FakeDomStorageDatabaseFactory::Open(
    StorageType,
    const base::FilePath& storage_partition_dir,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&,
    DomStorageDatabaseFactory::OpenResultCallback callback) {
  bool is_in_memory = storage_partition_dir.empty();
  DbStatus open_status = open_count_++ < num_open_failures_
                             ? DbStatus::Corruption("test")
                             : DbStatus::OK();
  DomStorageDatabaseFactory::OpenResult result;
  result.SetDatabase(GetTaskRunnerForDb(storage_partition_dir),
                     std::make_unique<FakeDomStorageDatabase>(open_status));
  result.metrics_type = is_in_memory ? DatabaseMetricsType::kInMemory
                                     : DatabaseMetricsType::kOnDisk;
  result.open_status = open_status;
  std::move(callback).Run(std::move(result));
}

void FakeDomStorageDatabaseFactory::Destroy(
    const base::FilePath&,
    bool /*is_sqlite*/,
    DomStorageDatabaseFactory::StatusCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                destroy_count_++ < num_destroy_failures_
                                    ? DbStatus::IOError("test")
                                    : DbStatus::OK()));
}

}  // namespace storage
