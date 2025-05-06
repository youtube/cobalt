// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/storage_partition/storage_partition_utils.h"

#include "base/check.h"

namespace cobalt {
namespace browser {

// static
StoragePartitionUtils* StoragePartitionUtils::GetInstance() {
  static base::NoDestructor<StoragePartitionUtils> provider;
  return provider.get();
}

void StoragePartitionUtils::Flush() {
  CHECK(!flush_closure_.is_null());
  flush_closure_.Run();
}

void StoragePartitionUtils::set_flush(base::RepeatingClosure closure) {
  flush_closure_ = closure;
}

}  // namespace browser
}  // namespace cobalt
