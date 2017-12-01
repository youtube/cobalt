/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NB_THREAD_LOCAL_OBJECT_H_
#define NB_THREAD_LOCAL_OBJECT_H_

#include <set>

#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/mutex.h"
#include "starboard/thread.h"

namespace nb {

// Like base::ThreadLocalPointer<T> but destroys objects. This is important
// for using ThreadLocalObjects that aren't tied to a singleton, or for
// access by threads which will call join().
//
// FEATURE COMPARISON TABLE:
//                         |  Thread Join       | Container Destroyed
//   ------------------------------------------------------------------
//   ThreadLocalPointer<T> | LEAKS              | LEAKS
//   ThreadLocalObject<T>  | Object Destroyed   | Objects Destroyed
//
// EXAMPLE:
//  ThreadLocalObject<std::map<std::string, int> > map_tls;
//  Map* map = map_tls->GetOrCreate();
//  (*map)["my string"] = 15;
//  Thread t = new Thread(&map_tls);
//  t->start();
//  // t creates it's own thread local map.
//  t->join();  // Make sure that thread joins before map_tls is destroyed!
//
// OBJECT DESTRUCTION:
//   There are two ways for an object to be destroyed by the ThreadLocalObject.
//   The first way is via a thread join. In this case only the object
//   associated with the thread is deleted.
//   The second way an object is destroyed is by the ThreadLocalObject
//   container to be destroyed, in this case ALL thread local objects are
//   destroyed.
//
// PERFORMANCE:
//   ThreadLocalObject is fast for the Get() function if the object has
//   has already been created, requiring one extra pointer dereference
//   over ThreadLocalPointer<T>.
template <typename Type>
class ThreadLocalObject {
 public:
  ThreadLocalObject() : slot_() {
    slot_ = SbThreadCreateLocalKey(DeleteEntry);
    SB_DCHECK(kSbThreadLocalKeyInvalid != slot_);
    constructing_thread_id_ = SbThreadGetId();
  }

  // Enables destruction by any other thread. Otherwise, the class instance
  // will warn when a different thread than the constructing destroys this.
  void EnableDestructionByAnyThread() {
    constructing_thread_id_ = kSbThreadInvalidId;
  }

  // Thread Local Objects are destroyed after this call.
  ~ThreadLocalObject() {
    CheckCurrentThreadAllowedToDestruct();
    if (SB_DLOG_IS_ON(FATAL)) {
      SB_DCHECK(entry_set_.size() < 2)
        << "Logic error: Some threads may still be accessing the objects that "
        << "are about to be destroyed. Only one object is expected and that "
        << "should be for the main thread. The caller should ensure that "
        << "other threads that access this object are externally "
        << "synchronized.";
    }
    // No locking is done because the entries should not be accessed by
    // different threads while this object is shutting down. If access is
    // occuring then the caller has a race condition, external to this class.
    typedef typename Set::iterator Iter;
    for (Iter it = entry_set_.begin(); it != entry_set_.end(); ++it) {
      Entry* entry = *it;
      SB_DCHECK(entry->owner_ == this);
      delete entry->ptr_;
      delete entry;
    }

    // Cleanup the thread local key.
    SbThreadDestroyLocalKey(slot_);
  }

  // Warns if there is a misuse of this object.
  void CheckCurrentThreadAllowedToDestruct() const {
    if (kSbThreadInvalidId == constructing_thread_id_) {
      return;   // EnableDestructionByAnyThread() called.
    }
    const SbThreadId curr_thread_id = SbThreadGetId();
    if (curr_thread_id == constructing_thread_id_) {
      return;   // Same thread that constructed this.
    }

    if (SB_DLOG_IS_ON(FATAL)) {
      SB_DCHECK(false)
          << "ThreadLocalObject<T> was created in thread "
          << constructing_thread_id_ << "\nbut was destroyed by "
          << curr_thread_id << ". If this is intentional then call "
          << "EnableDestructionByAnyThread() to silence this "
          << "warning.";
    }
  }

  // Either returns the created pointer for the current thread, or otherwise
  // constructs the object using the default constructor and returns it.
  Type* GetOrCreate() {
    Type* object = GetIfExists();
    if (!object) {  // create object.
      object = new Type();
      Entry* entry = new Entry(this, object);
      // Insert into the set of tls entries.
      // Performance: Its assumed that creation of objects is much less
      // frequent than getting an object.
      {
        starboard::ScopedLock lock(entry_set_mutex_);
        entry_set_.insert(entry);
      }
      SbThreadSetLocalValue(slot_, entry);
    }
    return object;
  }

  template <typename ConstructorArg>
  Type* GetOrCreate(const ConstructorArg& arg) {
    Type* object = GetIfExists();
    if (!object) {  // create object.
      object = new Type(arg);
      Entry* entry = new Entry(this, object);
      // Insert into the set of tls entries.
      // Performance: Its assumed that creation of objects is much less
      // frequent than getting an object.
      {
        starboard::ScopedLock lock(entry_set_mutex_);
        entry_set_.insert(entry);
      }
      SbThreadSetLocalValue(slot_, entry);
    }
    return object;
  }

  // Returns the pointer if it exists in the current thread, otherwise NULL.
  Type* GetIfExists() const {
    Entry* entry = GetEntryIfExists();
    if (!entry) { return NULL; }
    return entry->ptr_;
  }

  // Releases ownership of the pointer FROM THE CURRENT THREAD.
  // The caller has responsibility to make sure that the pointer is destroyed.
  Type* Release() {
    if (Entry* entry = GetEntryIfExists()) {
      // The entry will no longer run it's destructor on thread join.
      SbThreadSetLocalValue(slot_, NULL);  // NULL out pointer for TLS.
      Type* object = entry->ptr_;
      RemoveEntry(entry);
      return object;
    } else {
      return NULL;
    }
  }

 private:
  struct Entry {
    Entry(ThreadLocalObject* own, Type* ptr) : owner_(own), ptr_(ptr) {
    }
    ~Entry() {
      ptr_ = NULL;
      owner_ = NULL;
    }
    ThreadLocalObject* owner_;
    Type* ptr_;
  };

  // Deletes the TLSEntry.
  static void DeleteEntry(void* ptr) {
    if (!ptr) {
      SB_NOTREACHED();
      return;
    }
    Entry* entry = reinterpret_cast<Entry*>(ptr);
    ThreadLocalObject* tls = entry->owner_;
    Type* object = entry->ptr_;
    if (tls) {
      tls->RemoveEntry(entry);
    }
    delete object;
  }

  void RemoveEntry(Entry* entry) {
    {
      starboard::ScopedLock lock(entry_set_mutex_);
      entry_set_.erase(entry);
    }
    delete entry;
  }

  Entry* GetEntryIfExists() const {
    void* ptr = SbThreadGetLocalValue(slot_);
    Entry* entry = static_cast<Entry*>(ptr);
    return entry;
  }

  typedef std::set<Entry*> Set;
  // Allows GetIfExists() to be const.
  mutable SbThreadLocalKey slot_;
  // entry_set_ contains all the outstanding entries for the thread local
  // objects that have been created.
  Set entry_set_;
  mutable starboard::Mutex entry_set_mutex_;
  // Used to warn when there is a mismatch between thread that constructed and
  // thread that destroyed this object.
  SbThreadId constructing_thread_id_;

  SB_DISALLOW_COPY_AND_ASSIGN(ThreadLocalObject<Type>);
};

}  // namespace nb

#endif  // NB_THREAD_LOCAL_OBJECT_H_
