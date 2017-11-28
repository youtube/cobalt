// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cross_process_notification.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/platform_thread.h"
#include "base/win/scoped_handle.h"

CrossProcessNotification::~CrossProcessNotification() {}

CrossProcessNotification::CrossProcessNotification(IPCHandle handle_1,
                                                   IPCHandle handle_2)
    : mine_(handle_1), other_(handle_2) {
  DCHECK(IsValid());
}

void CrossProcessNotification::Signal() {
  DCHECK(IsValid());
  DCHECK_EQ(::WaitForSingleObject(mine_, 0), static_cast<DWORD>(WAIT_TIMEOUT))
      << "Are you calling Signal() without calling Wait() first?";
  BOOL ok = ::SetEvent(mine_);
  CHECK(ok);
}

void CrossProcessNotification::Wait() {
  DCHECK(IsValid());
  DWORD wait = ::WaitForSingleObject(other_, INFINITE);
  DCHECK_EQ(wait, WAIT_OBJECT_0);
  BOOL ok = ::ResetEvent(other_);
  CHECK(ok);
}

bool CrossProcessNotification::IsValid() const {
  return mine_.IsValid() && other_.IsValid();
}

bool CrossProcessNotification::ShareToProcess(base::ProcessHandle process,
                                              IPCHandle* handle_1,
                                              IPCHandle* handle_2) {
  DCHECK(IsValid());
  HANDLE our_process = ::GetCurrentProcess();
  if (!::DuplicateHandle(our_process, mine_, process, handle_1, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
    return false;
  }

  if (!::DuplicateHandle(our_process, other_, process, handle_2, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
    // In case we're sharing to ourselves, we can close the handle, but
    // if the target process is a different process, we do nothing.
    if (process == our_process)
      ::CloseHandle(*handle_1);
    *handle_1 = NULL;
    return false;
  }

  return true;
}

// static
bool CrossProcessNotification::InitializePair(CrossProcessNotification* a,
                                              CrossProcessNotification* b) {
  DCHECK(!a->IsValid());
  DCHECK(!b->IsValid());

  bool success = false;

  // Create two manually resettable events and give each party a handle
  // to both events.
  HANDLE event_a = ::CreateEvent(NULL, TRUE, FALSE, NULL);
  HANDLE event_b = ::CreateEvent(NULL, TRUE, FALSE, NULL);
  if (event_a && event_b) {
    a->mine_.Set(event_a);
    a->other_.Set(event_b);
    success = a->ShareToProcess(GetCurrentProcess(), &event_a, &event_b);
    if (success) {
      b->mine_.Set(event_b);
      b->other_.Set(event_a);
    } else {
      a->mine_.Close();
      a->other_.Close();
    }
  } else {
    if (event_a)
      ::CloseHandle(event_a);
    if (event_b)
      ::CloseHandle(event_b);
  }

  DCHECK(!success || a->IsValid());
  DCHECK(!success || b->IsValid());

  return success;
}

namespace {
class ExtraWaitThread : public base::PlatformThread::Delegate {
 public:
  ExtraWaitThread(HANDLE stop, HANDLE* events, size_t count,
                  int* signaled_event)
      : stop_(stop), events_(events), count_(count),
        signaled_event_(signaled_event) {
    *signaled_event_ = -1;
  }
  virtual ~ExtraWaitThread() {}

  virtual void ThreadMain() override {
    // Store the |stop_| event as the first event.
    HANDLE events[MAXIMUM_WAIT_OBJECTS] = { stop_ };
    HANDLE next_thread = NULL;
    DWORD event_count = MAXIMUM_WAIT_OBJECTS;
    int thread_signaled_event = -1;
    scoped_ptr<ExtraWaitThread> extra_wait_thread;
    if (count_ > (MAXIMUM_WAIT_OBJECTS - 1)) {
      std::copy(&events_[0], &events_[MAXIMUM_WAIT_OBJECTS - 2], &events[1]);

      extra_wait_thread.reset(new ExtraWaitThread(stop_,
          &events_[MAXIMUM_WAIT_OBJECTS - 2],
          count_ - (MAXIMUM_WAIT_OBJECTS - 2),
          &thread_signaled_event));
      base::PlatformThread::Create(0, extra_wait_thread.get(), &next_thread);

      event_count = MAXIMUM_WAIT_OBJECTS;
      events[MAXIMUM_WAIT_OBJECTS - 1] = next_thread;
    } else {
      std::copy(&events_[0], &events_[count_], &events[1]);
      event_count = count_ + 1;
    }

    DWORD wait = ::WaitForMultipleObjects(event_count, &events[0], FALSE,
                                          INFINITE);
    if (wait >= WAIT_OBJECT_0 && wait < (WAIT_OBJECT_0 + event_count)) {
      wait -= WAIT_OBJECT_0;
      if (wait == 0) {
        // The stop event was signaled.  Check if it was signaled by a
        // sub thread.  In case our sub thread had to spin another thread (and
        // so on), we must wait for ours to exit before we can check the
        // propagated event offset.
        if (next_thread) {
          base::PlatformThread::Join(next_thread);
          next_thread = NULL;
        }
        if (thread_signaled_event != -1)
          *signaled_event_ = thread_signaled_event + (MAXIMUM_WAIT_OBJECTS - 2);
      } else if (events[wait] == next_thread) {
        NOTREACHED();
      } else {
        *signaled_event_ = static_cast<int>(wait);
        SetEvent(stop_);
      }
    } else {
      NOTREACHED();
    }

    if (next_thread)
      base::PlatformThread::Join(next_thread);
  }

 private:
  HANDLE stop_;
  HANDLE* events_;
  size_t count_;
  int* signaled_event_;
  DISALLOW_COPY_AND_ASSIGN(ExtraWaitThread);
};
}  // end namespace

// static
int CrossProcessNotification::WaitMultiple(const Notifications& notifications,
                                           size_t wait_offset) {
  DCHECK_LT(wait_offset, notifications.size());

  for (size_t i = 0; i < notifications.size(); ++i) {
    DCHECK(notifications[i]->IsValid());
  }

  // TODO(tommi): Should we wait in an alertable state so that we can be
  // canceled via an APC?
  scoped_array<HANDLE> handles(new HANDLE[notifications.size()]);

  // Because of the way WaitForMultipleObjects works, we do a little trick here.
  // When multiple events are signaled, WaitForMultipleObjects will return the
  // index of the first signaled item (lowest).  This means that if we always
  // pass the array the same way to WaitForMultipleObjects, the objects that
  // come first, have higher priority.  In times of heavy load, this will cause
  // elements at the back to become DOS-ed.
  // So, we store the location of the item that was last signaled. Then we split
  // up the array and move everything higher than the last signaled index to the
  // front and the rest to the back (meaning that the last signaled item will
  // become the last element in the list).
  // Assuming equally busy events, this approach distributes the priority
  // evenly.

  size_t index = 0;
  for (size_t i = wait_offset; i < notifications.size(); ++i)
    handles[index++] = notifications[i]->other_;

  for (size_t i = 0; i < wait_offset; ++i)
    handles[index++] = notifications[i]->other_;
  DCHECK_EQ(index, notifications.size());

  DWORD wait = WAIT_FAILED;
  bool wait_failed = false;
  if (notifications.size() <= MAXIMUM_WAIT_OBJECTS) {
    wait = ::WaitForMultipleObjects(notifications.size(), &handles[0], FALSE,
                                    INFINITE);
    wait_failed = wait < WAIT_OBJECT_0 ||
                  wait >= (WAIT_OBJECT_0 + MAXIMUM_WAIT_OBJECTS);
  } else {
    // Used to stop the other wait threads when an event has been signaled.
    base::win::ScopedHandle stop(::CreateEvent(NULL, TRUE, FALSE, NULL));

    // Create the first thread and pass a pointer to all handles >63
    // to the thread + 'stop'.  Then implement the thread so that it checks
    // if the number of handles is > 63.  If so, spawns a new thread and
    // passes >62 handles to that thread and waits for the 62 handles + stop +
    // next thread.  etc etc.

    // Create a list of threads so that each thread waits on at most 62 events
    // including one event for when a child thread signals completion and one
    // event for when all of the threads must be stopped (due to some event
    // being signaled).

    int thread_signaled_event = -1;
    ExtraWaitThread wait_thread(stop, &handles[MAXIMUM_WAIT_OBJECTS - 1],
        notifications.size() - (MAXIMUM_WAIT_OBJECTS - 1),
        &thread_signaled_event);
    base::PlatformThreadHandle thread;
    base::PlatformThread::Create(0, &wait_thread, &thread);
    HANDLE events[MAXIMUM_WAIT_OBJECTS];
    std::copy(&handles[0], &handles[MAXIMUM_WAIT_OBJECTS - 1], &events[0]);
    events[MAXIMUM_WAIT_OBJECTS - 1] = thread;
    wait = ::WaitForMultipleObjects(MAXIMUM_WAIT_OBJECTS, &events[0], FALSE,
                                    INFINITE);
    wait_failed = wait < WAIT_OBJECT_0 ||
                  wait >= (WAIT_OBJECT_0 + MAXIMUM_WAIT_OBJECTS);
    if (wait == WAIT_OBJECT_0 + (MAXIMUM_WAIT_OBJECTS - 1)) {
      if (thread_signaled_event < 0) {
        wait_failed = true;
        NOTREACHED();
      } else {
        wait = WAIT_OBJECT_0 + (MAXIMUM_WAIT_OBJECTS - 2) +
               thread_signaled_event;
      }
    } else {
      ::SetEvent(stop);
    }
    base::PlatformThread::Join(thread);
  }

  int ret = -1;
  if (!wait_failed) {
    // Subtract to be politically correct (WAIT_OBJECT_0 is actually 0).
    wait -= WAIT_OBJECT_0;
    BOOL ok = ::ResetEvent(handles[wait]);
    CHECK(ok);
    ret = (wait + wait_offset) % notifications.size();
    DCHECK_EQ(handles[wait], notifications[ret]->other_.Get());
  } else {
    NOTREACHED();
  }

  CHECK_NE(ret, -1);
  return ret;
}
