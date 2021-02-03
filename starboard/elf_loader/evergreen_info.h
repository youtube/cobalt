// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

// An implementation of storage that just uses the filesystem. The User
// implementation must implement a way to get the user's home directory.

#ifndef STARBOARD_ELF_LOADER_EVERGREEN_INFO_H_
#define STARBOARD_ELF_LOADER_EVERGREEN_INFO_H_

#include <stddef.h>
#include <stdint.h>

// This is duplicate constant for use from signal-safe code in
// the starboard implementation.
#define EVERGREEN_FILE_PATH_MAX_SIZE 4096
#define EVERGREEN_BUILD_ID_MAX_SIZE 128

#define IS_EVERGREEN_ADDRESS(address, evergreen_info)                    \
  (evergreen_info.base_address != 0 &&                                   \
   reinterpret_cast<uint64_t>(address) >= evergreen_info.base_address && \
   (reinterpret_cast<uint64_t>(address) - evergreen_info.base_address) < \
       evergreen_info.load_size)

#ifdef __cplusplus
extern "C" {
#endif

// Evergreen shared library memory mapping information used for
// stack unwinding and symbolizing.
typedef struct EvergreenInfo {
  // File path of the shared library file.
  char file_path_buf[EVERGREEN_FILE_PATH_MAX_SIZE];

  // Base memory address of the mapped library.
  uint64_t base_address;

  // Size of the mapped memory.
  size_t load_size;

  // Address of the Program Header Table for the library.
  uint64_t phdr_table;

  // Number of items in the Program Header Table.
  size_t phdr_table_num;

  // Contents of the build id.
  char build_id[EVERGREEN_BUILD_ID_MAX_SIZE];

  // Length of the build id.
  size_t build_id_length;
} EvergreenInfo;

// Set the Evergreen information. Should be called only from the
// elf_loader module. Passing NULL clears the currently stored
// information.
// Returns true if if successful.
bool SetEvergreenInfo(const EvergreenInfo* evergreen_info);

// Retrieve the Evergreen information. The implementation must be
// signal-safe. The main clients for this call are stack unwinding
// libraries and symbolization routines.
// Returns true if the Evergreen info is available.
bool GetEvergreenInfo(EvergreenInfo* evergreen_info);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_ELF_LOADER_EVERGREEN_INFO_H_
