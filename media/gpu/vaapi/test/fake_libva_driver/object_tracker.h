// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_OBJECT_TRACKER_H_
#define MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_OBJECT_TRACKER_H_

#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/synchronization/lock.h"

namespace media::internal {

// Class that manages and stores the objects that FakeDriver needs to keep
// track of. All public methods are thread-safe.
template <class T>
class ObjectTracker {
  static_assert(std::is_integral<typename T::IdType>::value);

 public:
  ObjectTracker() = default;
  ObjectTracker(const ObjectTracker&) = delete;
  ObjectTracker& operator=(const ObjectTracker&) = delete;
  ~ObjectTracker() = default;

  template <class... Args>
  typename T::IdType CreateObject(Args... args) {
    base::AutoLock lock(lock_);

    objects_.push_back(std::make_unique<T>(next_id_, args...));

    do {
      CHECK_LT(next_id_, std::numeric_limits<typename T::IdType>::max());
      next_id_++;
    } while (std::find_if(objects_.begin(), objects_.end(),
                          [this](const std::unique_ptr<T>& it)
                              EXCLUSIVE_LOCKS_REQUIRED(lock_) {
                                return it->GetID() == next_id_;
                              }) != objects_.end());

    return objects_.back()->GetID();
  }

  bool ObjectExists(typename T::IdType id) {
    base::AutoLock lock(lock_);

    return std::find_if(objects_.begin(), objects_.end(),
                        [id](const std::unique_ptr<T>& it) {
                          return it->GetID() == id;
                        }) != objects_.end();
  }

  const T& GetObject(typename T::IdType id) {
    base::AutoLock lock(lock_);

    auto object = std::find_if(
        objects_.begin(), objects_.end(),
        [id](const std::unique_ptr<T>& it) { return it->GetID() == id; });

    CHECK(object != objects_.end());

    return **object;
  }

  void DestroyObject(typename T::IdType id) {
    base::AutoLock lock(lock_);
    auto object = std::find_if(
        objects_.begin(), objects_.end(),
        [id](const std::unique_ptr<T>& it) { return it->GetID() == id; });

    CHECK(object != objects_.end());

    objects_.erase(object);

    if (id < next_id_) {
      next_id_ = id;
    }
  }

 private:
  base::Lock lock_;
  std::vector<std::unique_ptr<T>> GUARDED_BY(lock_) objects_;
  typename T::IdType GUARDED_BY(lock_) next_id_ =
      std::numeric_limits<typename T::IdType>::min();
};

}  // namespace media::internal

#endif  // MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_OBJECT_TRACKER_H_