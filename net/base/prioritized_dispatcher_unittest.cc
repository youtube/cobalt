// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ctype.h>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "net/base/prioritized_dispatcher.h"
#include "net/base/request_priority.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// We rely on the priority enum values being sequential having starting at 0,
// and increasing for lower priorities.
COMPILE_ASSERT(HIGHEST == 0u &&
               LOWEST > HIGHEST &&
               IDLE > LOWEST &&
               NUM_PRIORITIES > IDLE,
               priority_indexes_incompatible);

class PrioritizedDispatcherTest : public testing::Test {
 public:
  typedef PrioritizedDispatcher::Priority Priority;
  // A job that appends |data| to |log_| when started and '.' when finished.
  // This is intended to confirm the execution order of a sequence of jobs added
  // to the dispatcher.
  class TestJob : public PrioritizedDispatcher::Job {
   public:
    TestJob(PrioritizedDispatcherTest* test, char data, Priority priority)
        : test_(test), data_(data), priority_(priority), running_(false) {}

    // MSVS does not accept EXPECT_EQ(this, ...) so wrap it up.
    PrioritizedDispatcher::Job* this_job() {
      return this;
    }

    void Add() {
      EXPECT_TRUE(handle_.is_null());
      EXPECT_FALSE(running_);
      size_t num_queued = dispatch().num_queued_jobs();
      size_t num_running = dispatch().num_running_jobs();

      handle_ = dispatch().Add(this, priority_);

      if (handle_.is_null()) {
        EXPECT_EQ(num_queued, dispatch().num_queued_jobs());
        EXPECT_TRUE(running_);
        EXPECT_EQ(num_running + 1, dispatch().num_running_jobs());
      } else {
        EXPECT_FALSE(running_);
        EXPECT_EQ(priority_, handle_.priority());
        EXPECT_EQ(this_job(), handle_.value());
        EXPECT_EQ(num_running, dispatch().num_running_jobs());
      }
    }

    void ChangePriority(Priority priority) {
      EXPECT_FALSE(running_);
      ASSERT_FALSE(handle_.is_null());
      size_t num_queued = dispatch().num_queued_jobs();
      size_t num_running = dispatch().num_running_jobs();

      handle_ = dispatch().ChangePriority(handle_, priority);

      if (handle_.is_null()) {
        EXPECT_TRUE(running_);
        EXPECT_EQ(num_queued - 1, dispatch().num_queued_jobs());
        EXPECT_EQ(num_running + 1, dispatch().num_running_jobs());
      } else {
        EXPECT_FALSE(running_);
        EXPECT_EQ(priority, handle_.priority());
        EXPECT_EQ(this_job(), handle_.value());
        EXPECT_EQ(num_queued, dispatch().num_queued_jobs());
        EXPECT_EQ(num_running, dispatch().num_running_jobs());
      }
    }

    void Cancel() {
      EXPECT_FALSE(running_);
      ASSERT_FALSE(handle_.is_null());
      size_t num_queued = dispatch().num_queued_jobs();

      dispatch().Cancel(handle_);

      EXPECT_EQ(num_queued - 1, dispatch().num_queued_jobs());
      handle_ = PrioritizedDispatcher::Handle();
    }

    void Finish() {
      EXPECT_TRUE(running_);
      running_ = false;
      test_->log_.append(1u, '.');

      dispatch().OnJobFinished();
    }

    // PriorityDispatch::Job interface
    virtual void Start() OVERRIDE {
      EXPECT_FALSE(running_);
      handle_ = PrioritizedDispatcher::Handle();
      running_ = true;
      test_->log_.append(1u, data_);
    }

   private:
    PrioritizedDispatcher& dispatch() { return *(test_->dispatch_); }

    PrioritizedDispatcherTest* test_;

    char data_;
    Priority priority_;

    PrioritizedDispatcher::Handle handle_;
    bool running_;
  };

 protected:
  void Prepare(const PrioritizedDispatcher::Limits& limits) {
    dispatch_.reset(new PrioritizedDispatcher(limits));
  }

  TestJob* AddJob(char data, Priority priority) {
    TestJob* job = new TestJob(this, data, priority);
    jobs_.push_back(job);
    job->Add();
    return job;
  }

  void Expect(std::string log) {
    EXPECT_EQ(0u, dispatch_->num_queued_jobs());
    EXPECT_EQ(0u, dispatch_->num_running_jobs());
    EXPECT_EQ(log, log_);
    log_.clear();
  }

  std::string log_;
  scoped_ptr<PrioritizedDispatcher> dispatch_;
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

  job_a->Finish();
  job_b->Finish();
  job_c->Finish();
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

  job_a->Finish();
  job_c->Finish();
  job_d->Finish();
  job_b->Finish();
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

  EXPECT_EQ(5u, dispatch_->num_running_jobs());
  EXPECT_EQ(3u, dispatch_->num_queued_jobs());

  job_a->Finish();  // Releases h.
  job_b->Finish();
  job_d->Finish();
  job_f->Finish();  // Releases e.
  job_g->Finish();
  job_h->Finish();  // Releases c.
  job_e->Finish();
  job_c->Finish();

  Expect("abdfg.h...e..c..");
}

TEST_F(PrioritizedDispatcherTest, ChangePriority) {
  PrioritizedDispatcher::Limits limits(NUM_PRIORITIES, 1);
  Prepare(limits);

  TestJob* job_a = AddJob('a', IDLE);
  TestJob* job_b = AddJob('b', MEDIUM);
  TestJob* job_c = AddJob('c', HIGHEST);
  TestJob* job_d = AddJob('d', HIGHEST);

  job_b->ChangePriority(HIGHEST);
  job_c->ChangePriority(MEDIUM);

  job_a->Finish();
  job_d->Finish();
  job_b->Finish();
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

  job_b->Cancel();
  job_d->Cancel();

  job_a->Finish();
  job_c->Finish();
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

  EXPECT_EQ(job_b, dispatch_->EvictOldestLowest());
  EXPECT_EQ(job_d, dispatch_->EvictOldestLowest());

  job_a->Finish();
  job_c->Finish();
  job_e->Finish();

  Expect("a.c.e.");
}

}  // namespace

}  // namespace net

