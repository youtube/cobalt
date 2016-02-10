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

#ifndef COBALT_STORAGE_SAVEGAME_H_
#define COBALT_STORAGE_SAVEGAME_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "sql/connection.h"

namespace cobalt {
namespace storage {

// Savegame controls loading and saving of a save file for a given platform.
// Read and Write operations are synchronous. These functions should
// be called from a worker thread. Normally this will be the StorageManager's
// internal thread.
class Savegame {
 public:
  typedef std::vector<uint8> ByteVector;
  struct Options;

  typedef scoped_ptr<Savegame>(*Factory)(const Options& options);

  struct Options {
    Options() : factory(&Create), delete_on_destruction(false) {}
    scoped_ptr<Savegame> CreateSavegame() { return factory(*this); }

    // Factory method for constructing a Savegame instance.
    // Defaults to Savegame::Create() but can be overriden for tests.
    Savegame::Factory factory;
    // File path the Savegame should read/write from, rather than its default.
    std::string path_override;
    // Delete the savegame file when the Savegame object goes out of scope.
    // This should only be used by tests.
    bool delete_on_destruction;
  };

  static scoped_ptr<Savegame> Create(const Options& options);
  virtual ~Savegame();

  // Load savegame and return its contents as a vector of bytes.
  // Return true on success, false on failure.
  bool Read(ByteVector* bytes);

  // Write the given blob to the savegame.
  // Return true on success, false on failure.
  bool Write(const ByteVector& bytes);

  // Delete the savegame from the disk. Mostly useful for testing.
  bool Delete();

  Options& options() { return options_; }

 protected:
  explicit Savegame(const Options& options);
  virtual bool PlatformRead(ByteVector* bytes) = 0;
  virtual bool PlatformWrite(const ByteVector& bytes) = 0;
  virtual bool PlatformDelete() = 0;
  Options options_;

 private:
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(Savegame);
};


}  // namespace storage
}  // namespace cobalt

#endif  // COBALT_STORAGE_SAVEGAME_H_
