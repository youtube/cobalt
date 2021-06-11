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

#include "cobalt/browser/memory_tracker/tool/buffered_file_writer.h"

#include "base/basictypes.h"
#include "base/logging.h"

#include "starboard/common/condition_variable.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/file.h"
#include "starboard/thread.h"
#include "starboard/types.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

BufferedFileWriter::BufferedFileWriter(const std::string& file_path)
    : current_log_buffer_(0),
      log_buffer_to_flush_(NULL),
      log_file_(kSbFileInvalid),
      diskwrite_mutex_(),
      diskwrite_cond_(diskwrite_mutex_),
      quit_thread_(false),
      file_path_(file_path) {
  memset(log_buffers_, 0, sizeof(log_buffers_));
  StartThread();
}

BufferedFileWriter::~BufferedFileWriter() { QuitThread(); }

void BufferedFileWriter::QuitThread() {
  quit_thread_ = true;
  SwapBuffers();
  diskwrite_cond_.Signal();

  SbThreadJoin(flush_thread_, NULL);

  SbFileClose(log_file_);
  log_file_ = kSbFileInvalid;
}

void BufferedFileWriter::StartThread() {
  // Do not reset the LogBuffers here, as they may have been written to
  // before the log thread was started.
  int flags = kSbFileCreateAlways | kSbFileWrite;
  bool created_ok = false;
  SbFileError error_code = kSbFileOk;

  log_file_ = SbFileOpen(file_path_.c_str(), flags, &created_ok, &error_code);

  if (log_file_ == kSbFileInvalid || !created_ok) {
    SbLogRaw("Error creating memory log file\n");
    return;
  }

  flush_thread_ = SbThreadCreate(0,  // default stack size.
                                 kSbThreadPriorityHigh, kSbThreadNoAffinity,
                                 true,  // true - joinable.
                                 "AllocationLoggerWriter", ThreadEntryFunc,
                                 static_cast<void*>(this));
}

void BufferedFileWriter::Append(const char* data, size_t num_bytes) {
  if (num_bytes > kBufferSize) {
    // We can never log this, and it's probably an error, but let's not write
    // over the end of the buffer.
    DCHECK(false) << "Log data is larger than the full buffer size. "
                     "Dropping log data\n";
    return;
  }

  starboard::ScopedLock lock(buffer_write_mutex_);

  // This function may be called before memory_log_writer_start.
  if (log_buffers_[current_log_buffer_].num_bytes + num_bytes > kBufferSize) {
    if (!SwapBuffers()) {
      // Failed to swap the buffer, so we will have to drop this log
      DCHECK(false) << "Dropping log data. Try increasing buffer size.";
      return;
    }
  }

  LogBuffer& current_buffer = log_buffers_[current_log_buffer_];
  memcpy(current_buffer.buffer + current_buffer.num_bytes, data,
               num_bytes);
  current_buffer.num_bytes += num_bytes;
  return;
}

void BufferedFileWriter::Run() {
  starboard::ScopedLock lock(diskwrite_mutex_);
  while (!quit_thread_) {
    if (log_buffer_to_flush_ != NULL) {
      size_t bytes_written =
          SbFileWrite(log_file_, log_buffer_to_flush_->buffer,
                      static_cast<int>(log_buffer_to_flush_->num_bytes));
      if (bytes_written != log_buffer_to_flush_->num_bytes) {
        SbLogRaw("Error writing to memory log. Aborting logging\n");
        break;
      }
      log_buffer_to_flush_->num_bytes = 0;
      log_buffer_to_flush_ = NULL;
    }
    diskwrite_cond_.Wait();  // Unlocks diskwrite_mutex_.
  }
}

void* BufferedFileWriter::ThreadEntryFunc(void* context) {
  BufferedFileWriter* self = static_cast<BufferedFileWriter*>(context);
  self->Run();
  return NULL;
}

bool BufferedFileWriter::SwapBuffers() {
  // If the non-logging threads block on this for too long, try increasing
  // the size of the buffers.
  diskwrite_mutex_.Acquire();

  bool can_swap = (log_buffer_to_flush_ == NULL);
  if (!can_swap) {
    SbLogRaw("Cannot swap buffer while a flush is pending.\n");
  } else {
    log_buffer_to_flush_ = &(log_buffers_[current_log_buffer_]);
    current_log_buffer_ = (current_log_buffer_ + 1) % kNumBuffers;
    SB_DCHECK(log_buffers_[current_log_buffer_].num_bytes == 0);
  }

  diskwrite_cond_.Signal();

  diskwrite_mutex_.Release();
  SbThreadYield();

  return can_swap;
}

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt
