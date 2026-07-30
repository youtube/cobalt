// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/extras/sqlite/sqlite_persistent_store_backend_base.h"

#include <memory>
#include <optional>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "net/test/test_with_task_environment.h"
#include "sql/database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class TestBackend : public SQLitePersistentStoreBackendBase {
 public:
  TestBackend(const base::FilePath& path,
              scoped_refptr<base::SequencedTaskRunner> background_task_runner,
              scoped_refptr<base::SequencedTaskRunner> client_task_runner)
      : SQLitePersistentStoreBackendBase(
            path,
            /* histogram_tag = */ "Test",
            /* current_version_number = */ 1,
            /* compatible_version_number = */ 1,
            background_task_runner,
            client_task_runner,
            /* enable_exclusive_access = */ false) {}

  bool InitializeDatabaseForTesting() { return InitializeDatabase(); }

 private:
  ~TestBackend() override = default;

  std::optional<int> DoMigrateDatabaseSchema() override { return 1; }
  bool CreateDatabaseSchema() override { return true; }
  void DoCommit() override {}
};

class SQLitePersistentStoreBackendBaseTest : public TestWithTaskEnvironment {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

 protected:
  base::ScopedTempDir temp_dir_;
  const scoped_refptr<base::SequencedTaskRunner> background_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
};

TEST_F(SQLitePersistentStoreBackendBaseTest, InitializeAfterClose) {
  base::FilePath db_path = temp_dir_.GetPath().AppendASCII("TestVal.db");
  auto backend = base::MakeRefCounted<TestBackend>(
      db_path, background_task_runner_,
      base::SequencedTaskRunner::GetCurrentDefault());

  EXPECT_FALSE(backend->IsClosedForTesting());

  // Close the backend. This posts DoCloseInBackground to the background runner.
  backend->Close();

  // Wait until the backend is closed. We post a dummy task to the background
  // runner and wait for the reply on the main thread to ensure the background
  // close task has completed.
  base::test::TestFuture<void> close_future;
  background_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                            close_future.GetCallback());
  ASSERT_TRUE(close_future.Wait());
  EXPECT_TRUE(backend->IsClosedForTesting());

  // Now try to initialize the database on the background runner.
  // It should return false because the backend is closed.
  {
    base::test::TestFuture<bool> init_future;
    background_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&TestBackend::InitializeDatabaseForTesting, backend),
        init_future.GetCallback());
    EXPECT_FALSE(init_future.Get());
  }
}

}  // namespace

}  // namespace net
