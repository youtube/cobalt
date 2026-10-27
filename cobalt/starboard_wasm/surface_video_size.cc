// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/starboard_wasm/surface_video_size.h"

#include <cstdint>
#include <map>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"

namespace cobalt {
namespace starboard_wasm {

namespace {

struct Watcher {
  scoped_refptr<base::SequencedTaskRunner> task_runner;
  SurfaceVideoSize::Callback callback;
  uint64_t id;

  void Notify(const gfx::Size& size) const {
    task_runner->PostTask(FROM_HERE, base::BindOnce(callback, size));
  }
};

struct Entry {
  gfx::Size size;
  std::optional<Watcher> watcher;
};

class Registry {
 public:
  static Registry& Get() {
    static base::NoDestructor<Registry> registry;
    return *registry;
  }

  void Set(const base::UnguessableToken& surface, const gfx::Size& size) {
    base::AutoLock lock(lock_);
    auto it = entries_.try_emplace(surface).first;
    if (it->second.size != size) {
      it->second.size = size;
      if (it->second.watcher && !size.IsEmpty()) {
        it->second.watcher->Notify(size);
      }
    }
    EraseIfUnused(it);
  }

  uint64_t Watch(const base::UnguessableToken& surface,
                 SurfaceVideoSize::Callback callback) {
    base::AutoLock lock(lock_);
    Entry& entry = entries_[surface];
    const uint64_t id = next_id_++;
    entry.watcher = Watcher{base::SequencedTaskRunner::GetCurrentDefault(),
                            std::move(callback), id};
    if (!entry.size.IsEmpty()) {
      entry.watcher->Notify(entry.size);
    }
    return id;
  }

  void Unwatch(const base::UnguessableToken& surface, uint64_t id) {
    base::AutoLock lock(lock_);
    auto it = entries_.find(surface);
    if (it == entries_.end() || !it->second.watcher ||
        it->second.watcher->id != id) {
      return;
    }
    it->second.watcher.reset();
    EraseIfUnused(it);
  }

 private:
  using Entries = std::map<base::UnguessableToken, Entry>;

  void EraseIfUnused(Entries::iterator it) EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    if (it->second.size.IsEmpty() && !it->second.watcher) {
      entries_.erase(it);
    }
  }

  base::Lock lock_;
  Entries entries_ GUARDED_BY(lock_);
  uint64_t next_id_ GUARDED_BY(lock_) = 1;
};

}  // namespace

// static
void SurfaceVideoSize::Set(const base::UnguessableToken& surface,
                           const gfx::Size& size) {
  Registry::Get().Set(surface, size);
}

// static
base::ScopedClosureRunner SurfaceVideoSize::Watch(
    const base::UnguessableToken& surface,
    Callback callback) {
  const uint64_t id = Registry::Get().Watch(surface, std::move(callback));
  return base::ScopedClosureRunner(
      base::BindOnce([](const base::UnguessableToken& surface,
                        uint64_t id) { Registry::Get().Unwatch(surface, id); },
                     surface, id));
}

}  // namespace starboard_wasm
}  // namespace cobalt
