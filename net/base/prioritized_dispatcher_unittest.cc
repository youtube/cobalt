// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ctype.h>
#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "net/base/prioritized_dispatcher.h"
#include "net/base/request_priority.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// We rely on the priority enum values being sequential having starting at 0,
// and increasing for higher priorities.
COMPILE_ASSERT(MINIMUM_PRIORITY == 0u &&
               MINIMUM_PRIORITY == IDLE &&
               IDLE < LOWEST &&
               LOWEST < HIGHEST &&
               HIGHEST < NUM_PRIORITIES,
               priority_indexes_incompatible);

class PrioritizedDispatcherTest : public testing::Test {
 public:
  typedef PrioritizedDispatcher::Priority Priority;
  // A job that appends |tag| to |log| when started and '.' when finished.
  // This is intended to confirm the execution order of a sequence of jobs added
  // to the dispatcher. Note that finishing order of jobs does not matter.
  class TestJob : public PrioritizedDispatcher::Job {
   public:
    TestJob(PrioritizedDispatcher* dispatcher,
            char tag,
            Priority priority,
            std::string* log)
        : dispatcher_(dispatcher),
          tag_(tag),
          priority_(priority),
          running_(false),
          log_(log) {}

    bool running() const {
      return running_;
    }

    const PrioritizedDispatcher::Handle handle() const {
      return handle_;
    }

    void Add() {
      CHECK(handle_.is_null());
      CHECK(!running_);
      size_t num_queued = dispatcher_->num_queued_jobs();
      size_t num_running = dispatcher_->num_running_jobs();

      handle_ = dispatcher_->Add(this, priority_);

      if (handle_.is_null()) {
        EXPECT_EQ(num_queued, dispatcher_->num_queued_jobs());
        EXPECT_TRUE(running_);
        EXPECT_EQ(num_running + 1, dispatcher_->num_running_jobs());
      } else {
        EXPECT_FALSE(running_);
        EXPECT_EQ(priority_, handle_.priority());
        EXPECT_EQ(tag_, reinterpret_cast<TestJob*>(handle_.value())->tag_);
        EXPECT_EQ(num_running, dispatcher_->num_running_jobs());
      }
    }

    void ChangePriority(Priority priority) {
      CHECK(!handle_.is_null());
      CHECK(!running_);
      size_t num_queued = dispatcher_->num_queued_jobs();
      size_t num_running = dispatcher_->num_running_jobs();

      handle_ = dispatcher_->ChangePriority(handle_, priority);

      if (handle_.is_null()) {
        EXPECT_TRUE(running_);
        EXPECT_EQ(num_queued - 1, dispatcher_->num_queued_jobs());
        EXPECT_EQ(num_running + 1, dispatcher_->num_running_jobs());
      } else {
        EXPECT_FALSE(running_);
        EXPECT_EQ(priority, handle_.priority());
        EXPECT_EQ(tag_, reinterpret_cast<TestJob*>(handle_.value())->tag_);
        EXPECT_EQ(num_queued, dispatcher_->num_queued_jobs());
        EXPECT_EQ(num_running, dispatcher_->num_running_jobs());
      }
    }

    void Cancel() {
      CHECK(!handle_.is_null());
      CHECK(!running_);
      size_t num_queued = dispatcher_->num_queued_jobs();

      dispatcher_->Cancel(handle_);

      EXPECT_EQ(num_queued - 1, dispatcher_->num_queued_jobs());
      handle_ = PrioritizedDispatcher::Handle();
    }

    void Finish() {
      CHECK(running_);
      running_ = false;
      log_->append(1u, '.');

      dispatcher_->OnJobFinished();
    }

    // PriorityDispatch::Job interface
    virtual void Start() override {
      EXPECT_FALSE(running_);
      handle_ = PrioritizedDispatcher::Handle();
      running_ = true;
      log_->append(1u, tag_);
    }

   private:
    PrioritizedDispatcher* dispatcher_;

    char tag_;
    Priority priority_;

    PrioritizedDispatcher::Handle handle_;
    bool running_;

    std::string* log_;
  };

 protected:
  void Prepare(const PrioritizedDispatcher::Limits& limits) {
    dispatcher_.reset(new PrioritizedDispatcher(limits));
  }

  TestJob* AddJob(char data, Priority priority) {
    TestJob* job = new TestJob(dispatcher_.get(), data, priority, &log_);
    jobs_.push_back(job);
    job->Add();
    return job;
  }

  void Expect(std::string log) {
    EXPECT_EQ(0u, dispatcher_->num_queued_jobs());
    EXPECT_EQ(0u, dispatcher_->num_running_jobs());
    EXPECT_EQ(log, log_);
    log_.clear();
  }

  std::string log_;
  scoped_ptr<PrioritizedDispatcher> dispatcher_;
  ScopedVector<TestJob> jobs_;
};

TEST_F(PrioritizedDispatcherTest, AddAFIFO) {
  // Allow only one running job.
  PrioritizedDispatcher::Limits limits(NUM_PRIORITIES, 1);
  Prepare(limits);

  TestJob* job_a = AddJob('a', IDLE);
  TestJob* job_b = AddJob('b', IDLE);
  TestJob* job_c = AddJob('c', IDLE);
  TestJob* job_d = AddJob('d', IDLE);

  ASSERT_TRUE(job_a->running());
  job_a->Finish();
  ASSERT_TRUE(job_b->running());
  job_b->Finish();
  ASSERT_TRUE(job_c->running());
  job_c->Finish();
  ASSERT_TRUE(job_d->running());
  job_d->Finish();

  Expect("a.b.c.d.");
}

TEST_F(PrioritizedDispatcherTest, AddPriority) {
  PrioritizedDispatcher::Limits limits(NUM_PRIORITIES, 1);
  Prepare(limits);

  TestJob* job_a = AddJob('a', IDLE);
  TestJob* job_b = AddJob('b', MEDIUM);
  TestJob* job_c = AddJob('c', HIGHEST);
  TestJob* job_d = AddJob('d', HIGHEST);
  TestJob* job_e = AddJob('e', MEDIUM);

  ASSERT_TRUE(job_a->running());
  job_a->Finish();
  ASSERT_TRUE(job_c->running());
  job_c->Finish();
  ASSERT_TRUE(job_d->running());
  job_d->Finish();
  ASSERT_TRUE(job_b->running());
  job_b->Finish();
  ASSERT_TRUE(job_e->running());
  job_e->Finish();

  Expect("a.c.d.b.e.");
}

TEST_F(PrioritizedDispatcherTest, EnforceLimits) {
  // Reserve 2 for HIGHEST and 1 for LOW or higher.
  // This leaves 2 for LOWEST or lower.
  PrioritizedDispatcher::Limits limits(NUM_PRIORITIES, 5);
  limits.reserved_slots[HIGHEST] = 2;
  limits.reserved_slots[LOW] = 1;
  Prepare(limits);

  TestJob* job_a = AddJob('a', IDLE);    // Uses unreserved slot.
  TestJob* job_b = AddJob('b', IDLE);    // Uses unreserved slot.
  TestJob* job_c = AddJob('c', LOWEST);  // Must wait.
  TestJob* job_d = AddJob('d', LOW);     // Uses reserved slot.
  TestJob* job_e = AddJob('e', MEDIUM);  // Must wait.
  TestJob* job_f = AddJob('f', HIGHEST); // Uses reserved slot.
  TestJob* job_g = AddJob('g', HIGHEST); // Uses reserved slot.
  TestJob* job_h = AddJob('h', HIGHEST); // Must wait.

  EXPECT_EQ(5u, dispatcher_->num_running_jobs());
  EXPECT_EQ(3u, dispatcher_->num_queued_jobs());

  ASSERT_TRUE(job_a->running());
  ASSERT_TRUE(job_b->running());
  ASSERT_TRUE(job_d->running());
  ASSERT_TRUE(job_f->running());
  ASSERT_TRUE(job_g->running());
  // a, b, d, f, g are running. Finish them in any order.
  job_b->Finish();  // Releases h.
  job_f->Finish();
  job_a->Finish();
  job_g->Finish();  // Releases e.
  job_d->Finish();
  ASSERT_TRUE(job_e->running());
  ASSERT_TRUE(job_h->running());
  // h, e are running.
  job_e->Finish();  // Releases c.
  ASSERT_TRUE(job_c->running());
  job_c->Finish();
  job_h->Finish();

  Expect("abdfg.h...e..c..");
}

TEST_F(PrioritizedDispatcherTest, ChangePriority) {
  PrioritizedDispatcher::Limits limits(NUM_PRIORITIES, 1);
  Prepare(limits);

  TestJob* job_a = AddJob('a', IDLE);
  TestJob* job_b = AddJob('b', MEDIUM);
  TestJob* job_c = AddJob('c', HIGHEST);
  TestJob* job_d = AddJob('d', HIGHEST);

  ASSERT_FALSE(job_b->running());
  ASSERT_FALSE(job_c->running());
  job_b->ChangePriority(HIGHEST);
  job_c->ChangePriority(MEDIUM);

  ASSERT_TRUE(job_a->running());
  job_a->Finish();
  ASSERT_TRUE(job_d->running());
  job_d->Finish();
  ASSERT_TRUE(job_b->running());
  job_b->Finish();
  ASSERT_TRUE(job_c->running());
  job_c->Finish();

  Expect("a.d.b.c.");
}

TEST_F(PrioritizedDispatcherTest, Cancel) {
  PrioritizedDispatcher::Limits limits(NUM_PRIORITIES, 1);
  Prepare(limits);

  TestJob* job_a = AddJob('a', IDLE);
  TestJob* job_b = AddJob('b', IDLE);
  TestJob* job_c = AddJob('c', IDLE);
  TestJob* job_d = AddJob('d', IDLE);
  TestJob* job_e = AddJob('e', IDLE);

  ASSERT_FALSE(job_b->running());
  ASSERT_FALSE(job_d->running());
  job_b->Cancel();
  job_d->Cancel();

  ASSERT_TRUE(job_a->running());
  job_a->Finish();
  ASSERT_TRUE(job_c->running());
  job_c->Finish();
  ASSERT_TRUE(job_e->running());
  job_e->Finish();

  Expect("a.c.e.");
}

TEST_F(PrioritizedDispatcherTest, Evict) {
  PrioritizedDispatcher::Limits limits(NUM_PRIORITIES, 1);
  Prepare(limits);

  TestJob* job_a = AddJob('a', IDLE);
  TestJob* job_b = AddJob('b', LOW);
  TestJob* job_c = AddJob('c', HIGHEST);
  TestJob* job_d = AddJob('d', LOW);
  TestJob* job_e = AddJob('e', HIGHEST);

  EXPECT_EQ(job_b, dispatcher_->EvictOldestLowest());
  EXPECT_EQ(job_d, dispatcher_->EvictOldestLowest());

  ASSERT_TRUE(job_a->running());
  job_a->Finish();
  ASSERT_TRUE(job_c->running());
  job_c->Finish();
  ASSERT_TRUE(job_e->running());
  job_e->Finish();

  Expect("a.c.e.");
}

TEST_F(PrioritizedDispatcherTest, EvictFromEmpty) {
  PrioritizedDispatcher::Limits limits(NUM_PRIORITIES, 1);
  Prepare(limits);
  EXPECT_TRUE(dispatcher_->EvictOldestLowest() == NULL);
}

#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
TEST_F(PrioritizedDispatcherTest, CancelNull) {
  PrioritizedDispatcher::Limits limits(NUM_PRIORITIES, 1);
  Prepare(limits);
  EXPECT_DEBUG_DEATH(dispatcher_->Cancel(PrioritizedDispatcher::Handle()), "");
}

TEST_F(PrioritizedDispatcherTest, CancelMissing) {
  PrioritizedDispatcher::Limits limits(NUM_PRIORITIES, 1);
  Prepare(limits);
  AddJob('a', IDLE);
  TestJob* job_b = AddJob('b', IDLE);
  PrioritizedDispatcher::Handle handle = job_b->handle();
  ASSERT_FALSE(handle.is_null());
  dispatcher_->Cancel(handle);
  EXPECT_DEBUG_DEATH(dispatcher_->Cancel(handle), "");
}

// TODO(szym): Fix the PriorityQueue::Pointer check to die here.
// http://crbug.com/130846
TEST_F(PrioritizedDispatcherTest, DISABLED_CancelIncompatible) {
  PrioritizedDispatcher::Limits limits(NUM_PRIORITIES, 1);
  Prepare(limits);
  AddJob('a', IDLE);
  TestJob* job_b = AddJob('b', IDLE);
  PrioritizedDispatcher::Handle handle = job_b->handle();
  ASSERT_FALSE(handle.is_null());

  // New dispatcher.
  Prepare(limits);
  AddJob('a', IDLE);
  AddJob('b', IDLE);
  EXPECT_DEBUG_DEATH(dispatcher_->Cancel(handle), "");
}
#endif  // GTEST_HAS_DEATH_TEST && !defined(NDEBUG)

}  // namespace

}  // namespace net
