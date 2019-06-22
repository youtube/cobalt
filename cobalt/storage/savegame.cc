// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/storage/savegame.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"

namespace cobalt {
namespace storage {

// An arbitrary max size for |last_bytes_| to prevent it from growing larger
// than desired.
size_t kMaxLastBytesSize = 128 * 1024;

Savegame::Savegame(const Options& options) : options_(options) {}

Savegame::~Savegame() {}

bool Savegame::Read(std::vector<uint8>* bytes, size_t max_to_read) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0("cobalt::storage", "Savegame::Read()");
  bool ret = PlatformRead(bytes, max_to_read);
  return ret;
}

bool Savegame::Write(const std::vector<uint8>& bytes) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0("cobalt::storage", "Savegame::Write()");
  if (bytes == last_bytes_) {
    return true;
  }

  bool ret = PlatformWrite(bytes);

  if (bytes.size() <= kMaxLastBytesSize) {
    last_bytes_ = bytes;
  } else {
    DLOG(WARNING) << "Unable to cache last savegame, which is used to prevent "
                     "duplicate saves, because it is too large: "
                  << bytes.size() << " bytes";
    last_bytes_.clear();
  }

  return ret;
}

bool Savegame::Delete() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return PlatformDelete();
}

}  // namespace storage
}  // namespace cobalt
