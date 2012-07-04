// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"

#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

template <class T>
class OffThreadObjectCreator {
 public:
  static T* NewObject() {
    T* result;
    {
      Thread creator_thread("creator_thread");
      creator_thread.Start();
      creator_thread.message_loop()->PostTask(
          FROM_HERE,
          base::Bind(OffThreadObjectCreator::CreateObject, &result));
    }
    DCHECK(result);  // We synchronized on thread destruction above.
    return result;
  }
 private:
  static void CreateObject(T** result) {
    *result = new T;
  }
};

struct Base { std::string member; };
struct Derived : Base {};

struct Target : SupportsWeakPtr<Target> {};
struct DerivedTarget : Target {};
struct Arrow { WeakPtr<Target> target; };

// Helper class to create and destroy weak pointer copies
// and delete objects on a background thread.
class BackgroundThread : public Thread {
 public:
  BackgroundThread() : Thread("owner_thread") {}

  virtual ~BackgroundThread() {
    Stop();
  }

  void CreateArrowFromTarget(Arrow** arrow, Target* target) {
    WaitableEvent completion(true, false);
    message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&BackgroundThread::DoCreateFromTarget, arrow, target,
                   &completion));
    completion.Wait();
  }

  void CreateArrowFromArrow(Arrow** arrow, const Arrow* other) {
    WaitableEvent completion(true, false);
    message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&BackgroundThread::DoCreateFromArrow, arrow, other,
                   &completion));
    completion.Wait();
  }

  void DeleteTarget(Target* object) {
    WaitableEvent completion(true, false);
    message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&BackgroundThread::DoDeleteTarget, object, &completion));
    completion.Wait();
  }

  void DeleteArrow(Arrow* object) {
    WaitableEvent completion(true, false);
    message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&BackgroundThread::DoDeleteArrow, object, &completion));
    completion.Wait();
  }

  Target* DeRef(const Arrow* arrow) {
    WaitableEvent completion(true, false);
    Target* result = NULL;
    message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&BackgroundThread::DoDeRef, arrow, &result, &completion));
    completion.Wait();
    return result;
  }

 protected:
  static void DoCreateFromArrow(Arrow** arrow,
                                const Arrow* other,
                                WaitableEvent* completion) {
    *arrow = new Arrow;
    **arrow = *other;
    completion->Signal();
  }

  static void DoCreateFromTarget(Arrow** arrow,
                                 Target* target,
                                 WaitableEvent* completion) {
    *arrow = new Arrow;
    (*arrow)->target = target->AsWeakPtr();
    completion->Signal();
  }

  static void DoDeRef(const Arrow* arrow,
                      Target** result,
                      WaitableEvent* completion) {
    *result = arrow->target.get();
    completion->Signal();
  }

  static void DoDeleteTarget(Target* object, WaitableEvent* completion) {
    delete object;
    completion->Signal();
  }

  static void DoDeleteArrow(Arrow* object, WaitableEvent* completion) {
    delete object;
    completion->Signal();
  }
};

}  // namespace

TEST(WeakPtrFactoryTest, Basic) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  EXPECT_EQ(&data, ptr.get());
}

TEST(WeakPtrFactoryTest, Comparison) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  WeakPtr<int> ptr2 = ptr;
  EXPECT_TRUE(ptr == ptr2);
}

TEST(WeakPtrFactoryTest, OutOfScope) {
  WeakPtr<int> ptr;
  EXPECT_TRUE(ptr.get() == NULL);
  {
    int data;
    WeakPtrFactory<int> factory(&data);
    ptr = factory.GetWeakPtr();
  }
  EXPECT_TRUE(ptr.get() == NULL);
}

TEST(WeakPtrFactoryTest, Multiple) {
  WeakPtr<int> a, b;
  {
    int data;
    WeakPtrFactory<int> factory(&data);
    a = factory.GetWeakPtr();
    b = factory.GetWeakPtr();
    EXPECT_EQ(&data, a.get());
    EXPECT_EQ(&data, b.get());
  }
  EXPECT_TRUE(a.get() == NULL);
  EXPECT_TRUE(b.get() == NULL);
}

TEST(WeakPtrFactoryTest, MultipleStaged) {
  WeakPtr<int> a;
  {
    int data;
    WeakPtrFactory<int> factory(&data);
    a = factory.GetWeakPtr();
    {
      WeakPtr<int> b = factory.GetWeakPtr();
    }
    EXPECT_TRUE(a.get() != NULL);
  }
  EXPECT_TRUE(a.get() == NULL);
}

TEST(WeakPtrFactoryTest, Dereference) {
  Base data;
  data.member = "123456";
  WeakPtrFactory<Base> factory(&data);
  WeakPtr<Base> ptr = factory.GetWeakPtr();
  EXPECT_EQ(&data, ptr.get());
  EXPECT_EQ(data.member, (*ptr).member);
  EXPECT_EQ(data.member, ptr->member);
}

TEST(WeakPtrFactoryTest, UpCast) {
  Derived data;
  WeakPtrFactory<Derived> factory(&data);
  WeakPtr<Base> ptr = factory.GetWeakPtr();
  ptr = factory.GetWeakPtr();
  EXPECT_EQ(ptr.get(), &data);
}

TEST(WeakPtrTest, SupportsWeakPtr) {
  Target f;
  WeakPtr<Target> ptr = f.AsWeakPtr();
  EXPECT_EQ(&f, ptr.get());
}

TEST(WeakPtrTest, DerivedTarget) {
  DerivedTarget f;
  WeakPtr<DerivedTarget> ptr = AsWeakPtr(&f);
  EXPECT_EQ(&f, ptr.get());
}

TEST(WeakPtrTest, InvalidateWeakPtrs) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  EXPECT_EQ(&data, ptr.get());
  EXPECT_TRUE(factory.HasWeakPtrs());
  factory.InvalidateWeakPtrs();
  EXPECT_TRUE(ptr.get() == NULL);
  EXPECT_FALSE(factory.HasWeakPtrs());
}

TEST(WeakPtrTest, HasWeakPtrs) {
  int data;
  WeakPtrFactory<int> factory(&data);
  {
    WeakPtr<int> ptr = factory.GetWeakPtr();
    EXPECT_TRUE(factory.HasWeakPtrs());
  }
  EXPECT_FALSE(factory.HasWeakPtrs());
}

TEST(WeakPtrTest, ObjectAndWeakPtrOnDifferentThreads) {
  // Test that it is OK to create an object that supports WeakPtr on one thread,
  // but uses it on another.  This tests that we do not trip runtime checks that
  // ensure that a WeakPtr is not used by multiple threads.
  scoped_ptr<Target> target(OffThreadObjectCreator<Target>::NewObject());
  WeakPtr<Target> weak_ptr = target->AsWeakPtr();
  EXPECT_EQ(target.get(), weak_ptr.get());
}

TEST(WeakPtrTest, WeakPtrInitiateAndUseOnDifferentThreads) {
  // Test that it is OK to create an object that has a WeakPtr member on one
  // thread, but use it on another.  This tests that we do not trip runtime
  // checks that ensure that a WeakPtr is not used by multiple threads.
  scoped_ptr<Arrow> arrow(OffThreadObjectCreator<Arrow>::NewObject());
  Target target;
  arrow->target = target.AsWeakPtr();
  EXPECT_EQ(&target, arrow->target.get());
}

TEST(WeakPtrTest, MoveOwnershipImplicitly) {
  // Move object ownership to other thread by releasing all weak pointers
  // on the original thread first. Establishing weak pointers on a different
  // thread after previous pointers have been destroyed implicitly reattaches
  // the thread checks.
  // - Main thread creates object and weak pointer
  // - Main thread deletes the weak pointer, then the thread ownership of the
  //   object can be implicitly moved.
  // - Background thread creates weak pointer(and implicitly owns the object)
  // - Background thread derefs weak pointer
  // - Background thread deletes object
  // - Background thread deletes weak pointer
  BackgroundThread background;
  background.Start();
  Target* target = new Target();
  {
    WeakPtr<Target> weak_ptr = target->AsWeakPtr();
  }
  Arrow* arrow;
  background.CreateArrowFromTarget(&arrow, target);
  EXPECT_EQ(background.DeRef(arrow), target);
  background.DeleteTarget(target);
  background.DeleteArrow(arrow);
}

TEST(WeakPtrTest, MoveOwnershipExplicitlyObjectNotReferenced) {
  // Case 1: The target is not bound to any thread yet. So calling
  // DetachFromThread() is a no-op.
  Target target;
  target.DetachFromThread();

  // Case 2: The target is bound to main thread but no WeakPtr is pointing to
  // it. In this case, it will be re-bound to any thread trying to get a
  // WeakPtr pointing to it. So detach function call is again no-op.
  {
    WeakPtr<Target> weak_ptr = target.AsWeakPtr();
  }
  target.DetachFromThread();
}

TEST(WeakPtrTest, MoveOwnershipExplicitly) {
  // Test that we do not trip any checks if we establish WeakPtr on one thread
  // and delete the object on another thread after explicit detachment.
  // - Main thread creates object
  // - Background thread creates weak pointer(and implicitly owns the object)
  // - Object detach from background thread
  // - Main thread destroys object
  BackgroundThread background;
  background.Start();
  Target target;
  Arrow* arrow;

  background.CreateArrowFromTarget(&arrow, &target);
  EXPECT_EQ(background.DeRef(arrow), &target);
  target.DetachFromThread();
  // Detached target getting destructed will not cause thread ownership
  // violation.
}

TEST(WeakPtrTest, ThreadARefOutlivesThreadBRef) {
  // Originating thread has a WeakPtr that outlives others.
  // - Main thread creates a WeakPtr
  // - Background thread creates a WeakPtr copy from the one in main thread
  // - Destruct the WeakPtr on background thread
  // - Destruct the WeakPtr on main thread

  BackgroundThread background;
  background.Start();

  Target target;
  Arrow arrow;
  arrow.target = target.AsWeakPtr();

  Arrow* arrow_copy;
  background.CreateArrowFromArrow(&arrow_copy, &arrow);
  EXPECT_EQ(arrow_copy->target, &target);
  background.DeleteArrow(arrow_copy);
}

TEST(WeakPtrTest, ThreadBRefOutlivesThreadARef) {
  // Originating thread drops all references before another thread.
  // - Main thread creates a WeakPtr and passes copy to background thread
  // - Destruct the pointer on main thread
  // - Destruct the pointer on background thread
  BackgroundThread background;
  background.Start();
  Target target;
  Arrow* arrow_copy;
  {
    Arrow arrow;
    arrow.target = target.AsWeakPtr();
    background.CreateArrowFromArrow(&arrow_copy, &arrow);
  }
  EXPECT_EQ(arrow_copy->target, &target);
  background.DeleteArrow(arrow_copy);
}

TEST(WeakPtrTest, OwnerThreadDeletesObject) {
  // Originating thread invalidates WeakPtrs while its held by other thread.
  // - Main thread creates WeakPtr and passes Copy to background thread
  // - Object gets destroyed on main thread
  //   (invalidates WeakPtr on background thread)
  // - WeakPtr gets destroyed on Thread B
  BackgroundThread background;
  background.Start();
  Arrow* arrow_copy;
  {
    Target target;
    Arrow arrow;
    arrow.target = target.AsWeakPtr();
    background.CreateArrowFromArrow(&arrow_copy, &arrow);
  }
  EXPECT_TRUE(arrow_copy->target == NULL);
  background.DeleteArrow(arrow_copy);
}

TEST(WeakPtrTest, NonOwnerThreadCanDeleteWeakPtr) {
  // Main thread creates a Target object.
  Target target;
  // Main thread creates a arrow referencing the Target.
  Arrow* arrow = new Arrow();
  arrow->target = target.AsWeakPtr();

  // Background can delete arrow (as well as the WeakPtr inside).
  BackgroundThread background;
  background.Start();
  background.DeleteArrow(arrow);
}

#ifndef NDEBUG  // Thread ownership is only checked in debug version.

TEST(WeakPtrDeathTest, NonOwnerThreadDereferencesWeakPtr) {
  // Main thread creates a Target object.
  Target target;
  // Main thread creates a arrow referencing the Target (so target's
  // thread ownership can not be implicitly moved).
  Arrow arrow;
  arrow.target = target.AsWeakPtr();

  // Background thread tries to delete target, which violates thread
  // ownership.
  BackgroundThread background;
  background.Start();
  ASSERT_DEATH(background.DeRef(&arrow), "");
}

TEST(WeakPtrDeathTest, NonOwnerThreadDeletesObject) {
  // Main thread creates a Target object.
  Target* target = new Target();
  // Main thread creates a arrow referencing the Target (so target's thread
  // ownership can not be implicitly moved).
  Arrow arrow;
  arrow.target = target->AsWeakPtr();

  // Background thread tries to deref target, which violates thread ownership.
  BackgroundThread background;
  background.Start();
  ASSERT_DEATH(background.DeleteTarget(target), "");
}

#endif

}  // namespace base
