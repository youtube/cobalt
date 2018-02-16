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

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "nb/allocator.h"
#include "nb/first_fit_reuse_allocator.h"
#include "nb/fixed_no_free_allocator.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"
#include "starboard/event.h"
#include "starboard/file.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/system.h"
#include "starboard/time.h"
#include "starboard/types.h"

namespace {

inline bool IsAligned(void* ptr, std::size_t boundary) {
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

std::string ReadFileContent(const std::string& pathname) {
  SB_DLOG(ERROR) << pathname;
  SbFile file =
      SbFileOpen(pathname.c_str(), kSbFileOpenOnly | kSbFileRead, NULL, NULL);
  SB_DCHECK(SbFileIsValid(file));
  SbFileInfo file_info;
  bool result = SbFileGetInfo(file, &file_info);
  SB_DCHECK(result);

  std::vector<char> buffer(file_info.size);
  int bytes_read = SbFileRead(file, &buffer[0], buffer.size());
  SB_DCHECK(bytes_read == file_info.size);
  SbFileClose(file);

  return std::string(buffer.begin(), buffer.end());
}

void LoadAllocationPlayback(std::vector<AllocationCommand>* commands,
                            const std::string& filename) {
  char buffer[SB_FILE_MAX_NAME * 16];
  bool result = SbSystemGetPath(kSbSystemPathContentDirectory, buffer,
                                SB_ARRAY_SIZE(buffer));
  SB_DCHECK(result);
  std::string path_name = buffer;
  path_name += SB_FILE_SEP_CHAR;
  path_name += "test";
  path_name += SB_FILE_SEP_CHAR;
  path_name += "nb";
  path_name += SB_FILE_SEP_CHAR;
  path_name += "testdata";
  path_name += SB_FILE_SEP_CHAR;
  path_name += filename;

  std::map<uint64_t, uint32_t> address_map;
  // Parse each line of this playback file.
  // Each line is a 64-bit hex address, and a 32-bit size and alignment.
  std::stringstream file_stream(ReadFileContent(path_name));

  std::string line;
  int address_id_counter = 0;
  while (std::getline(file_stream, line)) {
    if (!line.empty() && *line.rbegin() == '\n') {
      line.resize(line.size() - 1);
    }
    // Skip empty lines and comments.
    if (line.empty() || line[0] == '#') {
      continue;
    }

    uint64_t address;
    uint32_t size;
    uint32_t alignment;
    std::stringstream line_stream(line);
    line_stream << std::hex;
    line_stream >> address;
    line_stream << std::dec;
    line_stream >> size >> alignment;
    SB_DCHECK(address != 0);
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
  }
}

void ReplayCommands(nb::Allocator* allocator,
                    const std::vector<AllocationCommand>& commands,
                    SbTimeMonotonic* elapsed_time,
                    std::size_t* actual_bytes_used,
                    std::size_t* actual_high_water_mark,
                    std::size_t* allocator_bytes_used) {
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  // Execute each command in commands vector.
  // Record total bytes allocated and report.

  // Map from allocation IDs to pointers returned by the allocator.
  std::map<uint32_t, void*> address_table;
  // Map from allocation IDs to size requested.
  std::map<uint32_t, std::size_t> size_table;
  // How many bytes have we actually requested.
  std::size_t cur_bytes_allocated = 0;

  for (std::vector<AllocationCommand>::const_iterator iter = commands.begin();
       iter != commands.end(); ++iter) {
    const AllocationCommand& cmd = *iter;
    bool is_free = cmd.size == 0 && cmd.alignment == 0;
    if (!is_free) {
      void* ptr = allocator->Allocate(cmd.size, cmd.alignment);
      SB_DCHECK(IsAligned(ptr, cmd.alignment));
      cur_bytes_allocated += cmd.size;
      *actual_high_water_mark =
          std::max(*actual_high_water_mark, cur_bytes_allocated);
      size_table[cmd.id] = cmd.size;
      SB_DCHECK(address_table.find(cmd.id) == address_table.end());
      address_table[cmd.id] = ptr;
    } else {
      std::map<uint32_t, void*>::iterator it = address_table.find(cmd.id);
      SB_DCHECK(it != address_table.end());
      void* ptr = it->second;
      allocator->Free(ptr);
      cur_bytes_allocated -= size_table[cmd.id];
      address_table.erase(it);
    }
  }
  SbTimeMonotonic end = SbTimeGetMonotonicNow();
  *elapsed_time = end - start;
  *actual_bytes_used = cur_bytes_allocated;
  *allocator_bytes_used = allocator->GetAllocated();

  // Free any remaining allocations.
  for (std::map<uint32_t, void*>::iterator it = address_table.begin();
       it != address_table.end(); ++it) {
    allocator->Free(it->second);
  }
}

class DefaultAllocator : public nb::Allocator {
 public:
  void* Allocate(std::size_t size) override { return Allocate(size, 0); }
  void* Allocate(std::size_t size, std::size_t alignment) override {
    return SbMemoryAllocateAligned(alignment, size);
  }
  void Free(void* memory) override { SbMemoryDeallocate(memory); }
  std::size_t GetCapacity() const override { return 0; }
  std::size_t GetAllocated() const override { return 0; }
  void PrintAllocations() const override {}
};

// TODO: Make this work with other ReuseAllocator types.
void MemoryPlaybackTest(const std::string& filename) {
  const std::size_t kFixedNoFreeMemorySize = 512 * 1024 * 1024;
  void* fixed_no_free_memory = SbMemoryAllocate(kFixedNoFreeMemorySize);
  nb::FixedNoFreeAllocator fallback_allocator(fixed_no_free_memory,
                                              kFixedNoFreeMemorySize);
  nb::FirstFitReuseAllocator reuse_allocator(&fallback_allocator, 0);

  std::vector<AllocationCommand> commands;
  LoadAllocationPlayback(&commands, filename);

  std::size_t actual_bytes_used = 0;
  std::size_t actual_high_water_mark = 0;
  std::size_t allocator_bytes_used = 0;
  SbTimeMonotonic elapsed_time;
  ReplayCommands(&reuse_allocator, commands, &elapsed_time, &actual_bytes_used,
                 &actual_high_water_mark, &allocator_bytes_used);
  std::size_t allocator_high_water_mark = reuse_allocator.GetCapacity();
  SB_DLOG(INFO) << "Actual used: " << actual_bytes_used;
  SB_DLOG(INFO) << "Actual high water mark: " << actual_high_water_mark;
  SB_DLOG(INFO) << "Allocator used: " << allocator_bytes_used;
  SB_DLOG(INFO) << "Allocator high water mark: " << allocator_high_water_mark;
  SB_DLOG(INFO) << "Elapsed (ms): " << elapsed_time / kSbTimeMillisecond;

  SbMemoryDeallocate(fixed_no_free_memory);

  // Test again using the system allocator, to compare performance.
  DefaultAllocator default_allocator;
  ReplayCommands(&default_allocator, commands, &elapsed_time,
                 &actual_bytes_used, &actual_high_water_mark,
                 &allocator_bytes_used);
  SB_DLOG(INFO) << "Default allocator elapsed (ms): "
                << elapsed_time / kSbTimeMillisecond;
}

int BenchmarkMain(int argc, char** argv) {
  const char* kBenchmarks[] = {
      "mem_history_cobalt.txt", "mem_history_gpu.txt", "mem_history_main.txt",
  };

  for (std::size_t i = 0; i < SB_ARRAY_SIZE(kBenchmarks); ++i) {
    SB_DLOG(INFO) << "Memory playback test: " << kBenchmarks[i];
    MemoryPlaybackTest(kBenchmarks[i]);
  }

  return 0;
}

}  // namespace

STARBOARD_WRAP_SIMPLE_MAIN(BenchmarkMain);
