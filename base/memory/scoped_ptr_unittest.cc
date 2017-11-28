// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Used to test depth subtyping.
class ConDecLoggerParent {
 public:
  virtual ~ConDecLoggerParent() {}

  virtual void SetPtr(int* ptr) = 0;

  virtual int SomeMeth(int x) const = 0;
};

class ConDecLogger : public ConDecLoggerParent {
 public:
  ConDecLogger() : ptr_(NULL) { }
  explicit ConDecLogger(int* ptr) { SetPtr(ptr); }
  virtual ~ConDecLogger() { --*ptr_; }

  virtual void SetPtr(int* ptr) override { ptr_ = ptr; ++*ptr_; }

  virtual int SomeMeth(int x) const override { return x; }

 private:
  int* ptr_;

  DISALLOW_COPY_AND_ASSIGN(ConDecLogger);
};

scoped_ptr<ConDecLogger> PassThru(scoped_ptr<ConDecLogger> logger) {
  return logger.Pass();
}

void GrabAndDrop(scoped_ptr<ConDecLogger> logger) {
}

// Do not delete this function!  It's existence is to test that you can
// return a temporarily constructed version of the scoper.
scoped_ptr<ConDecLogger> TestReturnOfType(int* constructed) {
  return scoped_ptr<ConDecLogger>(new ConDecLogger(constructed));
}

scoped_ptr<ConDecLoggerParent> UpcastUsingPassAs(
    scoped_ptr<ConDecLogger> object) {
  return object.PassAs<ConDecLoggerParent>();
}

}  // namespace

TEST(ScopedPtrTest, ScopedPtr) {
  int constructed = 0;

  {
    scoped_ptr<ConDecLogger> scoper(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());

    EXPECT_EQ(10, scoper->SomeMeth(10));
    EXPECT_EQ(10, scoper.get()->SomeMeth(10));
    EXPECT_EQ(10, (*scoper).SomeMeth(10));
  }
  EXPECT_EQ(0, constructed);

  // Test reset() and release()
  {
    scoped_ptr<ConDecLogger> scoper(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());

    scoper.reset(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());

    scoper.reset();
    EXPECT_EQ(0, constructed);
    EXPECT_FALSE(scoper.get());

    scoper.reset(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());

    ConDecLogger* take = scoper.release();
    EXPECT_EQ(1, constructed);
    EXPECT_FALSE(scoper.get());
    delete take;
    EXPECT_EQ(0, constructed);

    scoper.reset(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());
  }
  EXPECT_EQ(0, constructed);

  // Test swap(), == and !=
  {
    scoped_ptr<ConDecLogger> scoper1;
    scoped_ptr<ConDecLogger> scoper2;
    EXPECT_TRUE(scoper1 == scoper2.get());
    EXPECT_FALSE(scoper1 != scoper2.get());

    ConDecLogger* logger = new ConDecLogger(&constructed);
    scoper1.reset(logger);
    EXPECT_EQ(logger, scoper1.get());
    EXPECT_FALSE(scoper2.get());
    EXPECT_FALSE(scoper1 == scoper2.get());
    EXPECT_TRUE(scoper1 != scoper2.get());

    scoper2.swap(scoper1);
    EXPECT_EQ(logger, scoper2.get());
    EXPECT_FALSE(scoper1.get());
    EXPECT_FALSE(scoper1 == scoper2.get());
    EXPECT_TRUE(scoper1 != scoper2.get());
  }
  EXPECT_EQ(0, constructed);
}

TEST(ScopedPtrTest, ScopedPtrDepthSubtyping) {
  int constructed = 0;

#if !defined(COMPILER_GHS)
  // Test construction from a scoped_ptr to a derived class.
  {
    scoped_ptr<ConDecLogger> scoper(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());

    scoped_ptr<ConDecLoggerParent> scoper_parent(scoper.Pass());
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper_parent.get());
    EXPECT_FALSE(scoper.get());

    EXPECT_EQ(10, scoper_parent->SomeMeth(10));
    EXPECT_EQ(10, scoper_parent.get()->SomeMeth(10));
    EXPECT_EQ(10, (*scoper_parent).SomeMeth(10));
  }
  EXPECT_EQ(0, constructed);
#endif

  // Test assignment from a scoped_ptr to a derived class.
  {
    scoped_ptr<ConDecLogger> scoper(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());

    scoped_ptr<ConDecLoggerParent> scoper_parent;
    scoper_parent = scoper.Pass();
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper_parent.get());
    EXPECT_FALSE(scoper.get());
  }
  EXPECT_EQ(0, constructed);

#if !defined(COMPILER_GHS)
  // For the GHS compiler we have had to disable depth subtyping constructor,
  // since that interferes with the move constructor. Luckily, we don't use
  // the subtyping constructor anywhere in chromium, yet.
  // Test construction of a scoped_ptr with an additional const annotation.
  {
    scoped_ptr<ConDecLogger> scoper(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());

    scoped_ptr<const ConDecLogger> scoper_const(scoper.Pass());
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper_const.get());
    EXPECT_FALSE(scoper.get());

    EXPECT_EQ(10, scoper_const->SomeMeth(10));
    EXPECT_EQ(10, scoper_const.get()->SomeMeth(10));
    EXPECT_EQ(10, (*scoper_const).SomeMeth(10));
  }
  EXPECT_EQ(0, constructed);
#endif

  // Test assignment to a scoped_ptr with an additional const annotation.
  {
    scoped_ptr<ConDecLogger> scoper(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());

    scoped_ptr<const ConDecLogger> scoper_const;
    scoper_const = scoper.Pass();
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper_const.get());
    EXPECT_FALSE(scoper.get());
  }
  EXPECT_EQ(0, constructed);
}

TEST(ScopedPtrTest, ScopedArray) {
  static const int kNumLoggers = 12;

  int constructed = 0;

  {
    scoped_array<ConDecLogger> scoper(new ConDecLogger[kNumLoggers]);
    EXPECT_TRUE(scoper.get());
    EXPECT_EQ(&scoper[0], scoper.get());
    for (int i = 0; i < kNumLoggers; ++i) {
      scoper[i].SetPtr(&constructed);
    }
    EXPECT_EQ(12, constructed);

    EXPECT_EQ(10, scoper.get()->SomeMeth(10));
    EXPECT_EQ(10, scoper[2].SomeMeth(10));
  }
  EXPECT_EQ(0, constructed);

  // Test reset() and release()
  {
    scoped_array<ConDecLogger> scoper;
    EXPECT_FALSE(scoper.get());
    EXPECT_FALSE(scoper.release());
    EXPECT_FALSE(scoper.get());
    scoper.reset();
    EXPECT_FALSE(scoper.get());

    scoper.reset(new ConDecLogger[kNumLoggers]);
    for (int i = 0; i < kNumLoggers; ++i) {
      scoper[i].SetPtr(&constructed);
    }
    EXPECT_EQ(12, constructed);
    scoper.reset();
    EXPECT_EQ(0, constructed);

    scoper.reset(new ConDecLogger[kNumLoggers]);
    for (int i = 0; i < kNumLoggers; ++i) {
      scoper[i].SetPtr(&constructed);
    }
    EXPECT_EQ(12, constructed);
    ConDecLogger* ptr = scoper.release();
    EXPECT_EQ(12, constructed);
    delete[] ptr;
    EXPECT_EQ(0, constructed);
  }
  EXPECT_EQ(0, constructed);

  // Test swap(), == and !=
  {
    scoped_array<ConDecLogger> scoper1;
    scoped_array<ConDecLogger> scoper2;
    EXPECT_TRUE(scoper1 == scoper2.get());
    EXPECT_FALSE(scoper1 != scoper2.get());

    ConDecLogger* loggers = new ConDecLogger[kNumLoggers];
    for (int i = 0; i < kNumLoggers; ++i) {
      loggers[i].SetPtr(&constructed);
    }
    scoper1.reset(loggers);
    EXPECT_EQ(loggers, scoper1.get());
    EXPECT_FALSE(scoper2.get());
    EXPECT_FALSE(scoper1 == scoper2.get());
    EXPECT_TRUE(scoper1 != scoper2.get());

    scoper2.swap(scoper1);
    EXPECT_EQ(loggers, scoper2.get());
    EXPECT_FALSE(scoper1.get());
    EXPECT_FALSE(scoper1 == scoper2.get());
    EXPECT_TRUE(scoper1 != scoper2.get());
  }
  EXPECT_EQ(0, constructed);
}

TEST(ScopedPtrTest, PassBehavior) {
  int constructed = 0;
  {
    ConDecLogger* logger = new ConDecLogger(&constructed);
    scoped_ptr<ConDecLogger> scoper(logger);
    EXPECT_EQ(1, constructed);

    // Test Pass() with constructor;
    scoped_ptr<ConDecLogger> scoper2(scoper.Pass());
    EXPECT_EQ(1, constructed);

    // Test Pass() with assignment;
    scoped_ptr<ConDecLogger> scoper3;
    scoper3 = scoper2.Pass();
    EXPECT_EQ(1, constructed);
    EXPECT_FALSE(scoper.get());
    EXPECT_FALSE(scoper2.get());
    EXPECT_TRUE(scoper3.get());
  }

  // Test uncaught Pass() does not leak.
  {
    ConDecLogger* logger = new ConDecLogger(&constructed);
    scoped_ptr<ConDecLogger> scoper(logger);
    EXPECT_EQ(1, constructed);

    // Should auto-destruct logger by end of scope.
    scoper.Pass();
    EXPECT_FALSE(scoper.get());
  }
  EXPECT_EQ(0, constructed);

  // Test that passing to function which does nothing does not leak.
  {
    ConDecLogger* logger = new ConDecLogger(&constructed);
    scoped_ptr<ConDecLogger> scoper(logger);
    EXPECT_EQ(1, constructed);

    // Should auto-destruct logger by end of scope.
    GrabAndDrop(scoper.Pass());
    EXPECT_FALSE(scoper.get());
  }
  EXPECT_EQ(0, constructed);
}

TEST(ScopedPtrTest, ReturnTypeBehavior) {
  int constructed = 0;

  // Test that we can return a scoped_ptr.
  {
    ConDecLogger* logger = new ConDecLogger(&constructed);
    scoped_ptr<ConDecLogger> scoper(logger);
    EXPECT_EQ(1, constructed);

    PassThru(scoper.Pass());
    EXPECT_FALSE(scoper.get());
  }
  EXPECT_EQ(0, constructed);

  // Test uncaught return type not leak.
  {
    ConDecLogger* logger = new ConDecLogger(&constructed);
    scoped_ptr<ConDecLogger> scoper(logger);
    EXPECT_EQ(1, constructed);

    // Should auto-destruct logger by end of scope.
    PassThru(scoper.Pass());
    EXPECT_FALSE(scoper.get());
  }
  EXPECT_EQ(0, constructed);

  // Call TestReturnOfType() so the compiler doesn't warn for an unused
  // function.
  {
    TestReturnOfType(&constructed);
  }
  EXPECT_EQ(0, constructed);
}

TEST(ScopedPtrTest, PassAs) {
  int constructed = 0;
  {
    scoped_ptr<ConDecLogger> scoper(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());

    scoped_ptr<ConDecLoggerParent> scoper_parent;
    scoper_parent = UpcastUsingPassAs(scoper.Pass());
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper_parent.get());
    EXPECT_FALSE(scoper.get());
  }
  EXPECT_EQ(0, constructed);
}

// TODO scoped_ptr_malloc
