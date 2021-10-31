// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_MEMORY_TRACKER_TOOL_BUFFERED_FILE_WRITER_H_
#define COBALT_BROWSER_MEMORY_TRACKER_TOOL_BUFFERED_FILE_WRITER_H_

#include <string>

#include "starboard/common/condition_variable.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/file.h"
#include "starboard/memory.h"
#include "starboard/thread.h"
#include "starboard/types.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

// This is a heavy weight file writing class. It's "heavy weight" because
// 1) It uses its own thread.
// 2) It uses a 4mb inlined buffer for file writes. This class guarantees
// that it will never allocate memory and therefore can be used in high
// performance, low-level data writes.
class BufferedFileWriter {
 public:
  // Constructs the BufferedFileWriter with the path to write to. A thread is
  // started which will begin writing to the file object.
  explicit BufferedFileWriter(const std::string& file_path);

  // Implicitly calls JoinThread(). This is safe if JoinThread() has already
  // been called.
  ~BufferedFileWriter();

  // Can be called by any thread. Pushes data to the buffer to be written out
  // by the disk writing thread as is.
  // Note that this append is raw and will NOT add any new line or other
  // formatting characters.
  void Append(const char* data, size_t num_bytes);

 private:
  static void* ThreadEntryFunc(void* context);  // Delegates to Run().
  void Run();
  bool SwapBuffers();
  void StartThread();
  void QuitThread();

  static const int kBufferSize = 1 << 20;
  static const int kNumBuffers = 2;
  struct LogBuffer {
    char buffer[kBufferSize];
    size_t num_bytes;
  };
  LogBuffer log_buffers_[kNumBuffers];
  int current_log_buffer_;
  LogBuffer* log_buffer_to_flush_;
  SbFile log_file_;

  starboard::Mutex diskwrite_mutex_;
  starboard::Mutex buffer_write_mutex_;
  starboard::ConditionVariable diskwrite_cond_;
  SbThread flush_thread_;
  bool quit_thread_;
  std::string file_path_;
};

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_TRACKER_TOOL_BUFFERED_FILE_WRITER_H_
