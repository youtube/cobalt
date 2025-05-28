// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_LEVELDB_CLEANUP_SCHEDULER_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_LEVELDB_CLEANUP_SCHEDULER_H_

#include <memory>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "content/browser/indexed_db/status.h"
#include "content/common/content_export.h"

namespace blink {
struct IndexedDBDatabaseMetadata;
}  // namespace blink

namespace leveldb {
class DB;
}  // namespace leveldb

namespace content::indexed_db {

CONTENT_EXPORT BASE_DECLARE_FEATURE(kIdbInSessionDbCleanup);

class LevelDbTombstoneSweeper;
class BackingStore;

// Sweeps the IndexedDB LevelDB database looking for index tombstones, followed
// by a round of DB compaction. Sweeping is broken into phases so as to not
// impact ongoing transactions. Please check `Phase` description for more
// information. Also sets the last run time of the tombstone sweeper after a
// successful run. Ref: crbug.com/374691835
class CONTENT_EXPORT LevelDBCleanupScheduler {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // `kRunScheduled` is the state when the `running_state_` has been set. When
  // `RunCleanupTask` is called, the scheduler will start the tombstone sweeper.
  // `kTombstoneSweeper` is the state when the tombstone sweeper is running.
  // The tombstone sweeper runs in rounds so this state can last multiple task
  // rounds.
  // `kDatabaseCompaction` is set when the tombstone sweeper completes and the
  // next time the `RunCleanupTask` gets called, the DB compaction task is run.
  // `kLoggingAndCleanup` is set when the DB compaction task completes and the
  // next time logging operations are carried followed by resetting the state.
  // LINT.IfChange(Phase)
  enum class Phase {
    kRunScheduled = 0,
    kTombstoneSweeper = 1,
    kDatabaseCompaction = 2,
    kLoggingAndCleanup = 3,
    kMaxValue = kLoggingAndCleanup,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/storage/enums.xml:LevelDBCleanupSchedulerPhase)

  // Abstraction of backing store calls which are required
  // by the scheduler.
  class Delegate {
   public:
    // This function updates the next run timestamp for the
    // tombstone sweeper in the database metadata.
    // Virtual for testing.
    // Returns `true`  if the update was successful.
    virtual bool UpdateEarliestSweepTime() = 0;
    // This function updates the next run timestamp for the
    // level db compaction in the database metadata.
    // Virtual for testing.
    // Returns `true` if the update was successful.
    virtual bool UpdateEarliestCompactionTime() = 0;
    // This method is also used by `BucketContext` where it
    // is defined as a friend class of `BackingStore`.
    virtual Status GetCompleteMetadata(
        std::vector<blink::IndexedDBDatabaseMetadata>* output) = 0;
  };

  struct RunningState {
    RunningState();
    RunningState(const RunningState&) = delete;
    RunningState& operator=(const RunningState&) = delete;
    ~RunningState();

    std::vector<blink::IndexedDBDatabaseMetadata> metadata_vector;
    std::unique_ptr<LevelDbTombstoneSweeper> tombstone_sweeper;

    base::TimeDelta tombstone_sweeper_duration;
    base::TimeDelta db_compaction_duration;

    // The timer for running the cleanup tasks. Can be scheduled for either
    // `kDeferTimeOnNoTransactions` when there are not active transactions
    // or `kDeferTimeAfterLastTransaction` when there are active
    // transactions.
    base::RetainingOneShotTimer clean_up_scheduling_timer_;

    Phase cleanup_phase = Phase::kRunScheduled;
    int postpone_count = 0;
  };

  // `database` and `backing_store` must outlive this instance.
  LevelDBCleanupScheduler(leveldb::DB* database, Delegate* backing_store);

  LevelDBCleanupScheduler(const LevelDBCleanupScheduler&) = delete;
  LevelDBCleanupScheduler& operator=(const LevelDBCleanupScheduler&) = delete;

  virtual ~LevelDBCleanupScheduler();

  // Initializes the running state if a certain amount of time has passed
  // since the last run. This is invoked during transaction completion when
  // enough number of tombstones are encountered by the cursor.
  void Initialize();

  const std::optional<RunningState>& GetRunningStateForTesting() const {
    return running_state_;
  }

  // Postpones any scheduled task unless `kMaximumTimeBetweenRuns` has passed.
  void OnTransactionStart();

  // Schedules a cleanup task if `running_state_` is set and there are no other
  // active transactions.
  void OnTransactionComplete();

  // To avoid interrupting active usage, defer runs until there has been no
  // activity for a few seconds.
  static constexpr base::TimeDelta kDeferTimeAfterLastTransaction =
      base::Seconds(4);
  static constexpr base::TimeDelta kDeferTimeOnNoTransactions =
      base::Seconds(0.4);

 private:
  // Starts the timer for the cleanup tasks.
  void ScheduleNextCleanupTask(const base::TimeDelta& defer_time);

  // Logs the histograms for the current clean up tasks, resets the running
  // state and marks the last run time.
  void LogAndResetState();

  // Orchestrates the clean up tasks. Decides if we need to schedule a sweeper
  // run or DB compaction run or log and reset the scheduler state.
  void RunCleanupTask();

  bool RunTombstoneSweeper();

  // The actual DB reference inside `TransactionalLevelDBDatabase` owned by
  // `BackingStore`. It's instantiated before the scheduler and hence also
  // destroyed after the scheduler.
  const raw_ptr<leveldb::DB> database_;

  const raw_ptr<Delegate> delegate_;

  base::TimeTicks last_run_;
  std::optional<RunningState> running_state_;
  int active_transactions_count_ = 0;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_LEVELDB_CLEANUP_SCHEDULER_H_
