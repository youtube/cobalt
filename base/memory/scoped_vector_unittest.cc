// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// The LifeCycleObject notifies its Observer upon construction & destruction.
class LifeCycleObject {
 public:
  class Observer {
   public:
    virtual void OnLifeCycleConstruct(LifeCycleObject* o) = 0;
    virtual void OnLifeCycleDestroy(LifeCycleObject* o) = 0;

   protected:
    virtual ~Observer() {}
  };

  explicit LifeCycleObject(Observer* observer)
      : observer_(observer) {
    observer_->OnLifeCycleConstruct(this);
  }

  ~LifeCycleObject() {
    observer_->OnLifeCycleDestroy(this);
  }

 private:
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(LifeCycleObject);
};

// The life cycle states we care about for the purposes of testing ScopedVector
// against objects.
enum LifeCycleState {
  LC_INITIAL,
  LC_CONSTRUCTED,
  LC_DESTROYED,
};

// Because we wish to watch the life cycle of an object being constructed and
// destroyed, and further wish to test expectations against the state of that
// object, we cannot save state in that object itself. Instead, we use this
// pairing of the watcher, which observes the object and notifies of
// construction & destruction. Since we also may be testing assumptions about
// things not getting freed, this class also acts like a scoping object and
// deletes the |constructed_life_cycle_object_|, if any when the
// LifeCycleWatcher is destroyed. To keep this simple, the only expected state
// changes are:
//   INITIAL -> CONSTRUCTED -> DESTROYED.
// Anything more complicated than that should start another test.
class LifeCycleWatcher : public LifeCycleObject::Observer {
 public:
  LifeCycleWatcher()
      : life_cycle_state_(LC_INITIAL),
        constructed_life_cycle_object_(NULL) {}
  ~LifeCycleWatcher() {
  }

  // Assert INITIAL -> CONSTRUCTED and no LifeCycleObject associated with this
  // LifeCycleWatcher.
  virtual void OnLifeCycleConstruct(LifeCycleObject* object) {
    ASSERT_EQ(LC_INITIAL, life_cycle_state_);
    ASSERT_EQ(NULL, constructed_life_cycle_object_.get());
    life_cycle_state_ = LC_CONSTRUCTED;
    constructed_life_cycle_object_.reset(object);
  }

  // Assert CONSTRUCTED -> DESTROYED and the |object| being destroyed is the
  // same one we saw constructed.
  virtual void OnLifeCycleDestroy(LifeCycleObject* object) {
    ASSERT_EQ(LC_CONSTRUCTED, life_cycle_state_);
    LifeCycleObject* constructed_life_cycle_object =
        constructed_life_cycle_object_.release();
    ASSERT_EQ(constructed_life_cycle_object, object);
    life_cycle_state_ = LC_DESTROYED;
  }

  LifeCycleState life_cycle_state() const { return life_cycle_state_; }

  // Factory method for creating a new LifeCycleObject tied to this
  // LifeCycleWatcher.
  LifeCycleObject* NewLifeCycleObject() {
    return new LifeCycleObject(this);
  }

 private:
  LifeCycleState life_cycle_state_;
  scoped_ptr<LifeCycleObject> constructed_life_cycle_object_;

  DISALLOW_COPY_AND_ASSIGN(LifeCycleWatcher);
};

TEST(ScopedVectorTest, LifeCycleWatcher) {
  LifeCycleWatcher watcher;
  EXPECT_EQ(LC_INITIAL, watcher.life_cycle_state());
  LifeCycleObject* object = watcher.NewLifeCycleObject();
  EXPECT_EQ(LC_CONSTRUCTED, watcher.life_cycle_state());
  delete object;
  EXPECT_EQ(LC_DESTROYED, watcher.life_cycle_state());
}

TEST(ScopedVectorTest, Reset) {
  LifeCycleWatcher watcher;
  EXPECT_EQ(LC_INITIAL, watcher.life_cycle_state());
  ScopedVector<LifeCycleObject> scoped_vector;
  scoped_vector.push_back(watcher.NewLifeCycleObject());
  EXPECT_EQ(LC_CONSTRUCTED, watcher.life_cycle_state());
  scoped_vector.reset();
  EXPECT_EQ(LC_DESTROYED, watcher.life_cycle_state());
}

TEST(ScopedVectorTest, Scope) {
  LifeCycleWatcher watcher;
  EXPECT_EQ(LC_INITIAL, watcher.life_cycle_state());
  {
    ScopedVector<LifeCycleObject> scoped_vector;
    scoped_vector.push_back(watcher.NewLifeCycleObject());
    EXPECT_EQ(LC_CONSTRUCTED, watcher.life_cycle_state());
  }
  EXPECT_EQ(LC_DESTROYED, watcher.life_cycle_state());
}

TEST(ScopedVectorTest, InsertRange) {
  LifeCycleWatcher watchers[5];

  std::vector<LifeCycleObject*> vec;
  for(LifeCycleWatcher* it = watchers; it != watchers + arraysize(watchers);
      ++it) {
    EXPECT_EQ(LC_INITIAL, it->life_cycle_state());
    vec.push_back(it->NewLifeCycleObject());
    EXPECT_EQ(LC_CONSTRUCTED, it->life_cycle_state());
  }
  // Start scope for ScopedVector.
  {
    ScopedVector<LifeCycleObject> scoped_vector;
    scoped_vector.insert(scoped_vector.end(), vec.begin() + 1, vec.begin() + 3);
    for(LifeCycleWatcher* it = watchers; it != watchers + arraysize(watchers);
        ++it)
      EXPECT_EQ(LC_CONSTRUCTED, it->life_cycle_state());
  }
  for(LifeCycleWatcher* it = watchers; it != watchers + 1; ++it)
    EXPECT_EQ(LC_CONSTRUCTED, it->life_cycle_state());
  for(LifeCycleWatcher* it = watchers + 1; it != watchers + 3; ++it)
    EXPECT_EQ(LC_DESTROYED, it->life_cycle_state());
  for(LifeCycleWatcher* it = watchers + 3; it != watchers + arraysize(watchers);
      ++it)
    EXPECT_EQ(LC_CONSTRUCTED, it->life_cycle_state());
}

}  // namespace
