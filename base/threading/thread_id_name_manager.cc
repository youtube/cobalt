// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_id_name_manager.h"

#include <stdlib.h>
#include <string.h>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/memory/singleton.h"
#include "base/strings/string_util.h"
#include "base/trace_event/heap_profiler_allocation_context_tracker.h"  // no-presubmit-check
#include "third_party/abseil-cpp/absl/base/attributes.h"

#if defined(STARBOARD)
#include "base/check_op.h"
#include "starboard/once.h"
#include "starboard/thread.h"
#endif

namespace base {
namespace {

static const char kDefaultName[] = "";
static std::string* g_default_name;

#if defined(STARBOARD)
ABSL_CONST_INIT SbOnceControl s_once_flag = SB_ONCE_INITIALIZER;
ABSL_CONST_INIT SbThreadLocalKey s_thread_local_key = kSbThreadLocalKeyInvalid;

void InitThreadLocalKey() {
  s_thread_local_key = SbThreadCreateLocalKey(NULL);
  DCHECK(SbThreadIsValidLocalKey(s_thread_local_key));
}

void EnsureThreadLocalKeyInited() {
  SbOnce(&s_once_flag, InitThreadLocalKey);
  DCHECK(SbThreadIsValidLocalKey(s_thread_local_key));
}

const char* GetThreadName() {
  const char* thread_name = static_cast<const char*>(
      SbThreadGetLocalValue(s_thread_local_key));
  return !!thread_name ? thread_name : kDefaultName;
}
#else
ABSL_CONST_INIT thread_local const char* thread_name = kDefaultName;
#endif
}

ThreadIdNameManager::Observer::~Observer() = default;

ThreadIdNameManager::ThreadIdNameManager()
    : main_process_name_(nullptr), main_process_id_(kInvalidThreadId) {
  g_default_name = new std::string(kDefaultName);

  AutoLock locked(lock_);
  name_to_interned_name_[kDefaultName] = g_default_name;
}

ThreadIdNameManager::~ThreadIdNameManager() = default;

ThreadIdNameManager* ThreadIdNameManager::GetInstance() {
  return Singleton<ThreadIdNameManager,
      LeakySingletonTraits<ThreadIdNameManager> >::get();
}

const char* ThreadIdNameManager::GetDefaultInternedString() {
  return g_default_name->c_str();
}

void ThreadIdNameManager::RegisterThread(PlatformThreadHandle::Handle handle,
                                         PlatformThreadId id) {
  AutoLock locked(lock_);
  thread_id_to_handle_[id] = handle;
  thread_handle_to_interned_name_[handle] =
      name_to_interned_name_[kDefaultName];
}

void ThreadIdNameManager::AddObserver(Observer* obs) {
  AutoLock locked(lock_);
  DCHECK(!base::Contains(observers_, obs));
  observers_.push_back(obs);
}

void ThreadIdNameManager::RemoveObserver(Observer* obs) {
  AutoLock locked(lock_);
  DCHECK(base::Contains(observers_, obs));
  base::Erase(observers_, obs);
}

void ThreadIdNameManager::SetName(const std::string& name) {
  PlatformThreadId id = PlatformThread::CurrentId();
  std::string* leaked_str = nullptr;
  {
    AutoLock locked(lock_);
    auto iter = name_to_interned_name_.find(name);
    if (iter != name_to_interned_name_.end()) {
      leaked_str = iter->second;
    } else {
      leaked_str = new std::string(name);
      name_to_interned_name_[name] = leaked_str;
    }

    auto id_to_handle_iter = thread_id_to_handle_.find(id);

#if defined(STARBOARD)
    EnsureThreadLocalKeyInited();
    SbThreadSetLocalValue(s_thread_local_key, const_cast<char*>(leaked_str->c_str()));
#else
    thread_name = leaked_str->c_str();
#endif
    for (Observer* obs : observers_)
      obs->OnThreadNameChanged(leaked_str->c_str());

    // The main thread of a process will not be created as a Thread object which
    // means there is no PlatformThreadHandler registered.
    if (id_to_handle_iter == thread_id_to_handle_.end()) {
      main_process_name_ = leaked_str;
      main_process_id_ = id;
      return;
    }
    thread_handle_to_interned_name_[id_to_handle_iter->second] = leaked_str;
  }

  // Add the leaked thread name to heap profiler context tracker. The name added
  // is valid for the lifetime of the process. AllocationContextTracker cannot
  // call GetName(which holds a lock) during the first allocation because it can
  // cause a deadlock when the first allocation happens in the
  // ThreadIdNameManager itself when holding the lock.
  trace_event::AllocationContextTracker::SetCurrentThreadName(
      leaked_str->c_str());
}

const char* ThreadIdNameManager::GetName(PlatformThreadId id) {
  AutoLock locked(lock_);

  if (id == main_process_id_)
    return main_process_name_->c_str();

  auto id_to_handle_iter = thread_id_to_handle_.find(id);
  if (id_to_handle_iter == thread_id_to_handle_.end())
    return name_to_interned_name_[kDefaultName]->c_str();

  auto handle_to_name_iter =
      thread_handle_to_interned_name_.find(id_to_handle_iter->second);
  return handle_to_name_iter->second->c_str();
}

const char* ThreadIdNameManager::GetNameForCurrentThread() {
#if defined(STARBOARD)
  return GetThreadName();
#else
  return thread_name;
#endif
}

void ThreadIdNameManager::RemoveName(PlatformThreadHandle::Handle handle,
                                     PlatformThreadId id) {
  AutoLock locked(lock_);
  auto handle_to_name_iter = thread_handle_to_interned_name_.find(handle);

  DCHECK(handle_to_name_iter != thread_handle_to_interned_name_.end());
  thread_handle_to_interned_name_.erase(handle_to_name_iter);

  auto id_to_handle_iter = thread_id_to_handle_.find(id);
  DCHECK((id_to_handle_iter!= thread_id_to_handle_.end()));
  // The given |id| may have been re-used by the system. Make sure the
  // mapping points to the provided |handle| before removal.
  if (id_to_handle_iter->second != handle)
    return;

  thread_id_to_handle_.erase(id_to_handle_iter);
}

std::vector<PlatformThreadId> ThreadIdNameManager::GetIds() {
  AutoLock locked(lock_);

  std::vector<PlatformThreadId> ids;
  ids.reserve(thread_id_to_handle_.size());
  for (const auto& iter : thread_id_to_handle_)
    ids.push_back(iter.first);

  return ids;
}

}  // namespace base
