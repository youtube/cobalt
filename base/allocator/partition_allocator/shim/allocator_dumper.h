// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SHIM_ALLOCATOR_DUMPER_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SHIM_ALLOCATOR_DUMPER_H_

#include <sys/types.h>

namespace base {

// If there exists a file named "alloc.log", this function generates
// "alloc_symbols.log" containing symbols of all addresses.
void TryToDumpSymbolsAndExit();

void DumpOperation(char type, void* pointer, ssize_t size);

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SHIM_ALLOCATOR_DUMPER_H_
