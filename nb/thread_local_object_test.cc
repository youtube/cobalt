// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <map>
#include <string>

#include "nb/thread_local_object.h"
#include "nb/scoped_ptr.h"
#include "starboard/mutex.h"
#include "starboard/thread.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

// Similar to C++11 std::atomic<T>.
// Atomic<T> may be instantiated with any TriviallyCopyable type T.
// Atomic<T> is neither copyable nor movable.
// TODO: Lift this class out into the library.
template <typename T>
class Atomic {
 public:
  // C++11 forbids a copy constructor for std::atomic<T>, it also forbids
  // a move operation.
  Atomic() : value_() {}
  explicit Atomic(T v) : value_(v) {}

  // Checks whether the atomic operations on all objects of this type
  // are lock-free.
  // Returns true if the atomic operations on the objects of this type
  // are lock-free, false otherwise.
  //
  // All atomic types may be implemented using mutexes or other locking
  // operations, rather than using the lock-free atomic CPU instructions.
  // Atomic types are also allowed to be sometimes lock-free, e.g. if only
  // aligned memory accesses are naturally atomic on a given architecture,
  // misaligned objects of the same type have to use locks.
  bool is_lock_free() const { return false; }
  bool is_lock_free() const volatile { return false; }

  // Atomically replaces the value of the atomic object
  // and returns the value held previously.
  T Swap(T new_val) {
    int old_value = -1;
    {
      starboard::ScopedLock lock(mutex_);
      old_value = value_;
      value_ = new_val;
    }
    return old_value;
  }

  // Atomically obtains the value of the atomic object.
  T Get() const {
    starboard::ScopedLock lock(mutex_);
    return value_;
  }

  // Returns the new updated value after the operation has been applied.
  T Add(T val) {
    starboard::ScopedLock lock(mutex_);
    value_ += val;
    return value_;
  }

  // TrySwap(...) sets the new value if and only if "expected_old_value"
  // matches the actual value during the atomic assignment operation. If this
  // succeeds then true is returned. If there is a mismatch then the value is
  // left unchanged and false is returned.
  // Inputs:
  //  new_value: Attempt to set the value to this new value.
  //  expected_old_value: A test condition for success. If the actual value
  //    matches the expected_old_value then the swap will succeed.
  //  optional_actual_value: If non-null, then the actual value at the time
  //    of the attempted operation is set to this value.
  bool TrySwap(T new_value, T expected_old_value,
               T* optional_actual_value) {
    starboard::ScopedLock lock(mutex_);
    if (optional_actual_value) {
      *optional_actual_value = value_;
    }
    if (expected_old_value == value_) {
      value_ = new_value;
      return true;
    }
    return false;
  }

 private:
  T value_;
  starboard::Mutex mutex_;
};

// Simple atomic int class. This could be optimized for speed using
// compiler intrinsics for concurrent integer modification.
class AtomicInt : public Atomic<int> {
 public:
  AtomicInt() : Atomic<int>(0) {}
  explicit AtomicInt(int initial_val) : Atomic<int>(initial_val) {}
  void Increment() { Add(1); }
  void Decrement() { Add(-1); }
};

// Simple atomic bool class. This could be optimized for speed using
// compiler intrinsics for concurrent integer modification.
class AtomicBool : public Atomic<bool>  {
 public:
  AtomicBool() : Atomic<bool>(false) {}
  explicit AtomicBool(bool initial_val) : Atomic<bool>(initial_val) {}
};

// AbstractTestThread that is a bare bones class wrapper around Starboard
// thread. Subclasses must override Run().
// TODO: Move this to nplb/thread_helpers.h
class AbstractTestThread {
 public:
  explicit AbstractTestThread() : thread_(kSbThreadInvalid) {}
  virtual ~AbstractTestThread() {}

  // Subclasses should override the Run method.
  virtual void Run() = 0;

  // Calls SbThreadCreate() with default parameters.
  void Start() {
    SbThreadEntryPoint entry_point = ThreadEntryPoint;

    thread_ = SbThreadCreate(
        0,                     // default stack_size.
        kSbThreadNoPriority,   // default priority.
        kSbThreadNoAffinity,   // default affinity.
        true,                  // joinable.
        "AbstractTestThread",
        entry_point,
        this);

    if (kSbThreadInvalid == thread_) {
      ADD_FAILURE_AT(__FILE__, __LINE__) << "Invalid thread.";
    }
    return;
  }

  void Join() {
    if (!SbThreadJoin(thread_, NULL)) {
      ADD_FAILURE_AT(__FILE__, __LINE__) << "Could not join thread.";
    }
  }

 private:
  static void* ThreadEntryPoint(void* ptr) {
    AbstractTestThread* this_ptr = static_cast<AbstractTestThread*>(ptr);
    this_ptr->Run();
    return NULL;
  }

  SbThread thread_;
};

// Simple class that counts the number of instances alive.
struct CountsInstances {
  CountsInstances() { s_instances_.Increment(); }
  ~CountsInstances() { s_instances_.Decrement(); }
  static AtomicInt s_instances_;
  static int NumInstances() { return s_instances_.Get(); }
  static void ResetNumInstances() { s_instances_.Swap(0); }
};
AtomicInt CountsInstances::s_instances_(0);

// A simple thread that just creates the an object from the supplied
// ThreadLocalObject<T> and then exits.
template <typename TYPE>
class CreateThreadLocalObjectThenExit : public AbstractTestThread {
 public:
  explicit CreateThreadLocalObjectThenExit(
      ThreadLocalObject<TYPE>* tlo) : tlo_(tlo) {}

  virtual void Run() {
    // volatile as a defensive measure to prevent compiler from optimizing this
    // statement out.
    volatile TYPE* val = tlo_->GetOrCreate();
  }
  ThreadLocalObject<TYPE>* tlo_;
};

// A simple thread that just deletes the object supplied on a thread and then
// exists.
template <typename TYPE>
class DestroyTypeOnThread : public AbstractTestThread {
 public:
  explicit DestroyTypeOnThread(TYPE* ptr)
      : ptr_(ptr) {}
  virtual void Run() {
    ptr_.reset(NULL);  // Destroys the object.
  }
 private:
  nb::scoped_ptr<TYPE> ptr_;
};

// Tests the expectation that a ThreadLocalObject can be simply used by
// the main thread.
TEST(ThreadLocalObject, MainThread) {
  ThreadLocalObject<bool> tlo_bool;
  EXPECT_TRUE(NULL == tlo_bool.GetIfExists());
  bool* the_bool = tlo_bool.GetOrCreate();
  EXPECT_TRUE(the_bool != NULL);
  EXPECT_FALSE(*the_bool);
  *the_bool = true;
  EXPECT_TRUE(*(tlo_bool.GetIfExists()));
}

// Tests the expectation that a ThreadLocalObject can be used on
// complex objects type (i.e. non pod types).
TEST(ThreadLocalObject, MainThreadComplexObject) {
  typedef std::map<std::string, int> Map;
  ThreadLocalObject<Map> map_tlo;
  EXPECT_FALSE(map_tlo.GetIfExists());
  ASSERT_TRUE(map_tlo.GetOrCreate());
  Map* map = map_tlo.GetIfExists();
  const Map* const_map = map_tlo.GetIfExists();
  ASSERT_TRUE(map);
  ASSERT_TRUE(const_map);
  // If the object is properly constructed then this find operation
  // should succeed.
  (*map)["my string"] = 15;
  ASSERT_EQ(15, (*map)["my string"]);
}

// Tests that when a ThreadLocalObject is destroyed on the main thread that
// the pointers it contained are also destroyed.
TEST(ThreadLocalObject, DestroysObjectOnTLODestruction) {
  CountsInstances::ResetNumInstances();
  typedef ThreadLocalObject<CountsInstances> TLO;

  // Create the TLO object and then immediately destroy it.
  nb::scoped_ptr<TLO> tlo_ptr(new TLO);
  tlo_ptr->GetOrCreate(); // Instantiate the internal object.
  EXPECT_EQ(1, CountsInstances::NumInstances());
  tlo_ptr.reset(NULL);    // Should destroy all outstanding allocs.
  // Now the TLO is destroyed and therefore the destructor should run on the
  // internal object.
  EXPECT_EQ(0, CountsInstances::NumInstances());

  CountsInstances::ResetNumInstances();
}

// Tests the expectation that the object can be released and that the pointer
// won't be deleted when the ThreadLocalObject that created it is destroyed.
TEST(ThreadLocalObject, ReleasesObject) {
  CountsInstances::ResetNumInstances();
  typedef ThreadLocalObject<CountsInstances> TLO;

  nb::scoped_ptr<TLO> tlo_ptr(new TLO);
  // Instantiate the internal object.
  tlo_ptr->GetOrCreate();
  // Now release the pointer into the container.
  nb::scoped_ptr<CountsInstances> last_ref(tlo_ptr->Release());
  // Destroying the TLO should not trigger the destruction of the object,
  // because it was released.
  tlo_ptr.reset(NULL);
  // 1 instance left, which is held in last_ref.
  EXPECT_EQ(1, CountsInstances::NumInstances());
  last_ref.reset(NULL);   // Now the object should be destroyed and the
                          // instance count drops to 0.
  EXPECT_EQ(0, CountsInstances::NumInstances());
  CountsInstances::ResetNumInstances();
}

// Tests the expectation that a thread that creates an object from
// the ThreadLocalObject store will automatically be destroyed by the
// thread joining.
TEST(ThreadLocalObject, ThreadJoinDestroysObject) {
  CountsInstances::ResetNumInstances();
  typedef ThreadLocalObject<CountsInstances> TLO;

  nb::scoped_ptr<TLO> tlo(new TLO);
  {
    AbstractTestThread* thread =
        new CreateThreadLocalObjectThenExit<CountsInstances>(tlo.get());
    thread->Start();
    thread->Join();
    // Once the thread joins, the object should be deleted and the instance
    // counter falls to 0.
    EXPECT_EQ(0, CountsInstances::NumInstances());
    delete thread;
  }

  tlo.reset(NULL);  // Now TLO destructor runs.
  EXPECT_EQ(0, CountsInstances::NumInstances());
  CountsInstances::ResetNumInstances();
}

// Tests the expectation that objects created on the main thread are not
// leaked.
TEST(ThreadLocalObject, NoLeaksOnMainThread) {
  CountsInstances::ResetNumInstances();

  ThreadLocalObject<CountsInstances>* tlo =
      new ThreadLocalObject<CountsInstances>;
  tlo->EnableDestructionByAnyThread();

  // Creates the object on the main thread. This is important because the
  // main thread will never join and therefore at-exit functions won't get
  // run.
  CountsInstances* main_thread_object = tlo->GetOrCreate();

  EXPECT_EQ(1, CountsInstances::NumInstances());

  // Thread will simply create the thread local object (CountsInstances)
  // and then return.
  nb::scoped_ptr<AbstractTestThread> thread_ptr(
        new CreateThreadLocalObjectThenExit<CountsInstances>(tlo));
  thread_ptr->Start();  // Object is now created.
  thread_ptr->Join();   // ...then destroyed.
  thread_ptr.reset(NULL);

  // Only main_thread_object should be alive now, therefore the count is 1.
  EXPECT_EQ(1, CountsInstances::NumInstances());

  // We COULD destroy the TLO on the main thread, but to be even fancier lets
  // create a thread that will destroy the object on a back ground thread.
  // The end result is that the TLO entry should be cleared out.
  thread_ptr.reset(
      new DestroyTypeOnThread<ThreadLocalObject<CountsInstances> >(tlo));
  thread_ptr->Start();
  thread_ptr->Join();
  thread_ptr.reset(NULL);

  // Now we expect that number of instances to be 0.
  EXPECT_EQ(0, CountsInstances::NumInstances());
  CountsInstances::ResetNumInstances();
}

}  // anonymous namespace
}  // namespace base

