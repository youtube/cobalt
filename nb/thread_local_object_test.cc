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

#include "nb/test_thread.h"
#include "nb/thread_local_object.h"
#include "nb/scoped_ptr.h"
#include "starboard/atomic.h"
#include "starboard/mutex.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nb {
namespace {

// Simple class that counts the number of instances alive.
struct CountsInstances {
  CountsInstances() { s_instances_.increment(); }
  ~CountsInstances() { s_instances_.decrement(); }
  static starboard::atomic_int32_t s_instances_;
  static int NumInstances() { return s_instances_.load(); }
  static void ResetNumInstances() { s_instances_.exchange(0); }
};
starboard::atomic_int32_t CountsInstances::s_instances_(0);

// A simple thread that just creates the an object from the supplied
// ThreadLocalObject<T> and then exits.
template <typename TYPE>
class CreateThreadLocalObjectThenExit : public TestThread {
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
class DestroyTypeOnThread : public TestThread {
 public:
  explicit DestroyTypeOnThread(TYPE* ptr)
      : ptr_(ptr) {}
  virtual void Run() {
    ptr_.reset(NULL);  // Destroys the object.
  }
 private:
  nb::scoped_ptr<TYPE> ptr_;
};

// A simple struct that wraps an integer, and cannot be default constructed.
struct IntegerWrapper {
  explicit IntegerWrapper(int value) : value(value) {}
  int value;
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

// The same as |MainThread|, except with |tlo_bool| initialized to |true|
// rather than |false|, via passing |true| to |GetOrCreate|.
TEST(ThreadLocalObject, MainThreadWithArg) {
  ThreadLocalObject<bool> tlo_bool;
  EXPECT_TRUE(NULL == tlo_bool.GetIfExists());
  bool* the_bool = tlo_bool.GetOrCreate(true);
  EXPECT_TRUE(the_bool != NULL);
  EXPECT_TRUE(*the_bool);
  *the_bool = false;
  EXPECT_FALSE(*(tlo_bool.GetIfExists()));
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

// The complex object analog of |MainThreadWithArg|.
TEST(ThreadLocalObject, MainThreadComplexObjectWithArg) {
  ThreadLocalObject<IntegerWrapper> integer_wrapper_tlo;
  EXPECT_FALSE(integer_wrapper_tlo.GetIfExists());
  ASSERT_TRUE(integer_wrapper_tlo.GetOrCreate(14));
  IntegerWrapper* integer_wrapper = integer_wrapper_tlo.GetIfExists();
  const IntegerWrapper* const_integer_wrapper =
      integer_wrapper_tlo.GetIfExists();
  ASSERT_TRUE(integer_wrapper);
  ASSERT_TRUE(const_integer_wrapper);
  integer_wrapper->value = 15;
  ASSERT_EQ(15, integer_wrapper->value);
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
    TestThread* thread =
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
  nb::scoped_ptr<TestThread> thread_ptr(
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
}  // nb namespace
