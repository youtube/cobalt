// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_STORAGE_SAVEGAME_FAKE_H_
#define COBALT_STORAGE_SAVEGAME_FAKE_H_

#if defined(COBALT_BUILD_TYPE_GOLD)
#error Only for use in non production builds
#endif

#include "cobalt/storage/savegame.h"

namespace cobalt {
namespace storage {

// We pretend to provide persistent storage for a test savegame.
// We only support a single "file".
class SavegameFake : public Savegame {
 public:
  explicit SavegameFake(const Options& options);

  ~SavegameFake() override {
    if (options_.delete_on_destruction) {
      Delete();
    }
  }

  bool PlatformRead(ByteVector* bytes, size_t max_to_read) override;
  bool PlatformWrite(const ByteVector& bytes) override;
  bool PlatformDelete() override;
  static scoped_ptr<Savegame> Create(const Options& options);

 private:
  static ByteVector* s_persistent_data_;
};

}  // namespace storage
}  // namespace cobalt

#endif  // COBALT_STORAGE_SAVEGAME_FAKE_H_
