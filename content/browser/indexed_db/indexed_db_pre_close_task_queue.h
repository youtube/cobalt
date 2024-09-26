// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_PRE_CLOSE_TASK_QUEUE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_PRE_CLOSE_TASK_QUEUE_H_

#include <list>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace blink {
struct IndexedDBDatabaseMetadata;
}

namespace leveldb {
class DB;
}

namespace content {

// Holds a queue of PreCloseTask's to be run after an IndexedDBBackingStore no
// longer has any connections.
//
// There is a special IndexedDBMetadata fetcher task that runs before all the
// other tasks, and whose output is passed to each task before they start.
//
// Owned by IndexedDBBackingStore.
class CONTENT_EXPORT IndexedDBPreCloseTaskQueue {
 public:
  // This function should fetch all database metadata for the origin. The
  // returned status signifies if the metadata was read successfully.
  using MetadataFetcher = base::OnceCallback<leveldb::Status(
      std::vector<blink::IndexedDBDatabaseMetadata>*)>;

  enum class StopReason {
    // A new connection was made to the closing backing store.
    NEW_CONNECTION,
    // The maximum time for all tasks to complete as passed.
    TIMEOUT,
    // There was an error reading the database metadata.
    METADATA_ERROR,
    // Force closed due to shutdown.
    FORCE_CLOSE,
  };

  // Defines a task that will be run after closing an IndexedDB backing store
  // instance. The task can be destructed at any time if the browser process is
  // shutting down, otherwise Stop(...) will be called.
  // Instances of this class are sequence-hostile. Each instance must only be
  // used on the same SequencedTaskRunner.
  class CONTENT_EXPORT PreCloseTask {
   public:
    explicit PreCloseTask(leveldb::DB* database);
    virtual ~PreCloseTask();

    leveldb::DB* database() { return database_; }

    // Implementations should override if they need database metadata. If
    // overridden to return true, the metadata will be loaded and SetMetadata()
    // will be called once before the first call to RunRound().
    virtual bool RequiresMetadata() const;

    // Called before RunRound. |metadata| is guaranteed to outlive this task.
    virtual void SetMetadata(
        const std::vector<blink::IndexedDBDatabaseMetadata>* metadata);

    // Tells the task to stop before completion. It will be destroyed after this
    // call. Can be called at any time.
    virtual void Stop(StopReason reason) = 0;

    // Runs a round of work. Tasks are expected to keep round execution time
    // small. Returns if the task is complete and can be destroyed.
    virtual bool RunRound() = 0;

   private:
    friend class IndexedDBPreCloseTaskQueue;

    bool set_metadata_was_called_ = false;
    // Raw pointer is safe because `database_` is owned by the
    // IndexedDBBucketState.
    const raw_ptr<leveldb::DB> database_;
  };

  // |on_complete| must not contain a refptr to the IndexedDBBackingStore, as
  // this would create a cycle.
  IndexedDBPreCloseTaskQueue(std::list<std::unique_ptr<PreCloseTask>> tasks,
                             base::OnceClosure on_complete,
                             base::TimeDelta max_run_time,
                             std::unique_ptr<base::OneShotTimer> timer);

  IndexedDBPreCloseTaskQueue(const IndexedDBPreCloseTaskQueue&) = delete;
  IndexedDBPreCloseTaskQueue& operator=(const IndexedDBPreCloseTaskQueue&) =
      delete;

  ~IndexedDBPreCloseTaskQueue();

  bool started() const { return started_; }

  // Tasks are all complete or they have been stopped.
  bool done() const { return done_; }

  // Stops all tasks and destroys them. The |on_complete| callback will be
  // immediately called.
  void Stop(StopReason reason);

  // Starts running tasks. Can only be called once. MetadataFetcher is expected
  // to load the metadata from the database on disk.
  void Start(MetadataFetcher metadata_fetcher);

 private:
  void OnComplete();

  void StopForTimout();
  void StopForMetadataError(const leveldb::Status& status);

  void RunLoop();

  bool has_metadata_ = false;
  // This callback is populated when |Start| is called, and executed if a
  // pre-close task requires metadata (see RequiresMetadata). This happens
  // before that task is run.
  MetadataFetcher metadata_fetcher_;
  std::vector<blink::IndexedDBDatabaseMetadata> metadata_;

  bool started_ = false;
  bool done_ = false;
  std::list<std::unique_ptr<PreCloseTask>> tasks_;
  base::OnceClosure on_done_;

  base::TimeDelta timeout_time_;
  std::unique_ptr<base::OneShotTimer> timeout_timer_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<IndexedDBPreCloseTaskQueue> ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_PRE_CLOSE_TASK_QUEUE_H_
