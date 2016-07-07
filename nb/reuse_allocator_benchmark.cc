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

#if !defined(STARBOARD)
#include <malloc.h>
#endif

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/time.h"
#include "build/build_config.h"
#include "cobalt/base/allocator.h"
#include "cobalt/base/allocator_decorator.h"
#include "cobalt/base/fixed_no_free_allocator.h"
#include "cobalt/base/reuse_allocator.h"
#include "cobalt/base/wrap_main.h"

#if defined(OS_STARBOARD)
#include "starboard/memory.h"
#define free SbMemoryFree
#define malloc SbMemoryAllocate
#define memalign SbMemoryAllocateAligned
#endif

namespace {
inline bool IsAligned(void* ptr, size_t boundary) {
  uintptr_t ptr_as_int = reinterpret_cast<uintptr_t>(ptr);
  return ptr_as_int % boundary == 0;
}

// An allocation or free.
// We use 'id' rather than raw pointer since the pointer values are
// meaningless across runs.
struct AllocationCommand {
  uint32_t id;
  uint32_t size;
  uint32_t alignment;
};

void LoadAllocationPlayback(std::vector<AllocationCommand>* commands,
                            const std::string& filename) {
  FilePath data_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &data_path);
  data_path = data_path.Append("nb").Append("testdata");
  data_path = data_path.Append(filename);
  int64_t data_size = 0;
  file_util::GetFileSize(data_path, &data_size);
  DCHECK_GT(data_size, 0);

  scoped_array<char> buf(new char[data_size + 1]);
  DCHECK_EQ(file_util::ReadFile(data_path, buf.get(), data_size), data_size);
  buf.get()[data_size] = '\0';

  std::map<uint64_t, uint32_t> address_map;
  // Parse each line of this playback file.
  // Each line is a 64-bit hex address, and a 32-bit size and alignment.
  char* line = strtok(buf.get(), "\n");  // NOLINT[runtime/threadsafe_fn]
  int address_id_counter = 0;
  while (line) {
    const size_t line_length = strlen(line);
    // Skip empty lines and comments.
    if (line_length == 0 || line[0] == '#') {
      line = strtok(NULL, "\n");  // NOLINT[runtime/threadsafe_fn]
      continue;
    }

    uint64_t address;
    uint32_t size;
    uint32_t alignment;
    std::stringstream st(line);
    st << std::hex;
    st >> address;
    st << std::dec;
    st >> size >> alignment;
    DCHECK_NE(address, 0);
    // Convert the addresses into unique IDs. We don't
    // really care what the values are, just need to match up
    // allocations with frees.
    int cur_address_id;
    std::map<uint64_t, uint32_t>::iterator it = address_map.find(address);
    if (it != address_map.end()) {
      cur_address_id = (*it).second;
    } else {
      cur_address_id = address_id_counter++;
      address_map[address] = cur_address_id;
    }

    AllocationCommand cmd;
    cmd.id = cur_address_id;
    cmd.size = size;
    cmd.alignment = alignment;
    commands->push_back(cmd);
    line = strtok(NULL, "\n");  // NOLINT[runtime/threadsafe_fn]
  }
}

void ReplayCommands(base::Allocator* allocator,
                    const std::vector<AllocationCommand>& commands,
                    base::TimeDelta* elapsed_time,
                    size_t* actual_bytes_used,
                    size_t* actual_high_water_mark,
                    size_t* allocator_bytes_used) {
  base::Time start = base::Time::Now();
  // Execute each command in commands vector.
  // Record total bytes allocated and report.

  // Map from allocation IDs to pointers returned by the allocator.
  std::map<uint32_t, void*> address_table;
  // Map from allocation IDs to size requested.
  std::map<uint32_t, size_t> size_table;
  // How many bytes have we actually requested.
  size_t cur_bytes_allocated = 0;

  for (std::vector<AllocationCommand>::const_iterator iter = commands.begin();
       iter != commands.end(); ++iter) {
    const AllocationCommand& cmd = *iter;
    bool is_free = cmd.size == 0 && cmd.alignment == 0;
    if (!is_free) {
      void* ptr = allocator->Allocate(cmd.size, cmd.alignment);
      DCHECK(IsAligned(ptr, cmd.alignment));
      cur_bytes_allocated += cmd.size;
      *actual_high_water_mark =
          std::max(*actual_high_water_mark, cur_bytes_allocated);
      size_table[cmd.id] = cmd.size;
      DCHECK(address_table.find(cmd.id) == address_table.end());
      address_table[cmd.id] = ptr;
    } else {
      std::map<uint32_t, void*>::iterator it = address_table.find(cmd.id);
      DCHECK(it != address_table.end());
      void* ptr = it->second;
      allocator->Free(ptr);
      cur_bytes_allocated -= size_table[cmd.id];
      address_table.erase(it);
    }
  }
  base::Time end = base::Time::Now();
  *elapsed_time = end - start;
  *actual_bytes_used = cur_bytes_allocated;
  *allocator_bytes_used = allocator->GetAllocated();

  // Free any remaining allocations.
  for (std::map<uint32_t, void*>::iterator it = address_table.begin();
       it != address_table.end(); ++it) {
    allocator->Free(it->second);
  }
}

class DefaultAllocator : public base::Allocator {
 public:
  void* Allocate(size_t size) { return Allocate(size, 0); }
  void* Allocate(size_t size, size_t alignment) {
    return memalign(alignment, size);
  }
  void* AllocateForAlignment(size_t size, size_t alignment) {
    return memalign(alignment, size);
  }
  void Free(void* memory) { free(memory); }
  size_t GetCapacity() const { return 0; }
  size_t GetAllocated() const { return 0; }
};

void MemoryPlaybackTest(const std::string& filename) {
  const size_t kFixedNoFreeMemorySize = 512 * 1024 * 1024;
  void* fixed_no_free_memory = malloc(kFixedNoFreeMemorySize);
  base::FixedNoFreeAllocator fallback_allocator(fixed_no_free_memory,
                                                kFixedNoFreeMemorySize);
  base::ReuseAllocator reuse_allocator(&fallback_allocator);

  std::vector<AllocationCommand> commands;
  LoadAllocationPlayback(&commands, filename);

  size_t actual_bytes_used = 0;
  size_t actual_high_water_mark = 0;
  size_t allocator_bytes_used = 0;
  base::TimeDelta elapsed_time;
  ReplayCommands(&reuse_allocator, commands, &elapsed_time, &actual_bytes_used,
                 &actual_high_water_mark, &allocator_bytes_used);
  size_t allocator_high_water_mark = reuse_allocator.GetCapacity();
  DLOG(INFO) << "Actual used: " << actual_bytes_used;
  DLOG(INFO) << "Actual high water mark: " << actual_high_water_mark;
  DLOG(INFO) << "Allocator used: " << allocator_bytes_used;
  DLOG(INFO) << "Allocator high water mark: " << allocator_high_water_mark;
  DLOG(INFO) << "Elapsed (ms): " << elapsed_time.InMillisecondsF();

  free(fixed_no_free_memory);

  // Test again using the system allocator, to compare performance.
  DefaultAllocator default_allocator;
  ReplayCommands(&default_allocator, commands, &elapsed_time,
                 &actual_bytes_used, &actual_high_water_mark,
                 &allocator_bytes_used);
  DLOG(INFO) << "Default allocator elapsed (ms): "
             << elapsed_time.InMillisecondsF();
}

int BenchmarkMain(int argc, char** argv) {
  const char* kBenchmarks[] = {
      "mem_history_onion.txt", "mem_history_garlic.txt",
      "mem_history_cobalt_ps3.txt",
  };

  for (size_t i = 0; i < arraysize(kBenchmarks); ++i) {
    DLOG(INFO) << "Memory playback test: " << kBenchmarks[i];
    MemoryPlaybackTest(kBenchmarks[i]);
  }

  return 0;
}

}  // namespace

COBALT_WRAP_SIMPLE_MAIN(BenchmarkMain);
