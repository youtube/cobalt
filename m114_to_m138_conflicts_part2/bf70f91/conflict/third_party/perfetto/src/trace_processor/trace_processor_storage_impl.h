/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_TRACE_PROCESSOR_STORAGE_IMPL_H_
#define SRC_TRACE_PROCESSOR_TRACE_PROCESSOR_STORAGE_IMPL_H_

<<<<<<< HEAD
#include "perfetto/ext/base/hash.h"
#include "perfetto/trace_processor/basic_types.h"
#include "perfetto/trace_processor/trace_processor_storage.h"
#include "src/trace_processor/importers/common/trace_file_tracker.h"
=======
#include <memory>

#include "perfetto/ext/base/hash.h"
#include "perfetto/trace_processor/basic_types.h"
#include "perfetto/trace_processor/status.h"
#include "perfetto/trace_processor/trace_processor_storage.h"
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
#include "src/trace_processor/types/trace_processor_context.h"

namespace perfetto {
namespace trace_processor {

<<<<<<< HEAD
class ForwardingTraceParser;

=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
class TraceProcessorStorageImpl : public TraceProcessorStorage {
 public:
  explicit TraceProcessorStorageImpl(const Config&);
  ~TraceProcessorStorageImpl() override;

<<<<<<< HEAD
  base::Status Parse(TraceBlobView) override;
  void Flush() override;
  base::Status NotifyEndOfFile() override;
=======
  util::Status Parse(TraceBlobView) override;
  void Flush() override;
  void NotifyEndOfFile() override;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  void DestroyContext();

  TraceProcessorContext* context() { return &context_; }

 protected:
  base::Hasher trace_hash_;
  TraceProcessorContext context_;
  bool unrecoverable_parse_error_ = false;
<<<<<<< HEAD
  bool eof_ = false;
  size_t hash_input_size_remaining_ = 4096;
  ForwardingTraceParser* parser_ = nullptr;
=======
  size_t hash_input_size_remaining_ = 4096;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TRACE_PROCESSOR_STORAGE_IMPL_H_
