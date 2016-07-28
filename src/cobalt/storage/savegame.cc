/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/storage/savegame.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"

namespace cobalt {
namespace storage {

Savegame::Savegame(const Options& options) : options_(options) {}

Savegame::~Savegame() {}

bool Savegame::Read(std::vector<uint8>* bytes) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("cobalt::storage", "Savegame::Read()");
  bool ret = PlatformRead(bytes);
  return ret;
}

bool Savegame::Write(const std::vector<uint8>& bytes) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("cobalt::storage", "Savegame::Write()");
  bool ret = PlatformWrite(bytes);
  return ret;
}

bool Savegame::Delete() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return PlatformDelete();
}

}  // namespace storage
}  // namespace cobalt
