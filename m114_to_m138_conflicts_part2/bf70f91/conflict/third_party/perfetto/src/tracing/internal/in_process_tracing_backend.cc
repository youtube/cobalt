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

#include "perfetto/tracing/internal/in_process_tracing_backend.h"

#include "perfetto/base/logging.h"
#include "perfetto/base/task_runner.h"
#include "perfetto/ext/base/paged_memory.h"
<<<<<<< HEAD
#include "perfetto/ext/tracing/core/client_identity.h"
#include "perfetto/ext/tracing/core/shared_memory.h"
#include "perfetto/ext/tracing/core/tracing_service.h"

#include "src/tracing/core/in_process_shared_memory.h"

=======
#include "perfetto/ext/tracing/core/shared_memory.h"
#include "perfetto/ext/tracing/core/tracing_service.h"

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
// TODO(primiano): When the in-process backend is used, we should never end up
// in a situation where the thread where the TracingService and Producer live
// writes a packet and hence can get into the GetNewChunk() stall.
// This would happen only if the API client code calls Trace() from one of the
// callbacks it receives (e.g. OnStart(), OnStop()). We should either cause a
// hard crash or ignore traces from that thread if that happens, because it
// will deadlock (the Service will never free up the SMB because won't ever get
// to run the task).

namespace perfetto {
namespace internal {

<<<<<<< HEAD
=======
namespace {

class InProcessShm : public SharedMemory {
 public:
  explicit InProcessShm(size_t size);
  ~InProcessShm() override;
  void* start() const override;
  size_t size() const override;

 private:
  base::PagedMemory mem_;
};

class InProcessShmFactory : public SharedMemory::Factory {
 public:
  ~InProcessShmFactory() override;
  std::unique_ptr<SharedMemory> CreateSharedMemory(size_t) override;
};

InProcessShm::~InProcessShm() = default;

InProcessShm::InProcessShm(size_t size)
    : mem_(base::PagedMemory::Allocate(size)) {}

void* InProcessShm::start() const {
  return mem_.Get();
}

size_t InProcessShm::size() const {
  return mem_.size();
}

InProcessShmFactory::~InProcessShmFactory() = default;
std::unique_ptr<SharedMemory> InProcessShmFactory::CreateSharedMemory(
    size_t size) {
  return std::unique_ptr<SharedMemory>(new InProcessShm(size));
}

}  // namespace

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
// static
TracingBackend* InProcessTracingBackend::GetInstance() {
  static auto* instance = new InProcessTracingBackend();
  return instance;
}

<<<<<<< HEAD
InProcessTracingBackend::InProcessTracingBackend() = default;
InProcessTracingBackend::~InProcessTracingBackend() = default;
=======
InProcessTracingBackend::InProcessTracingBackend() {}
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

std::unique_ptr<ProducerEndpoint> InProcessTracingBackend::ConnectProducer(
    const ConnectProducerArgs& args) {
  PERFETTO_DCHECK(args.task_runner->RunsTasksOnCurrentThread());
  return GetOrCreateService(args.task_runner)
<<<<<<< HEAD
      ->ConnectProducer(args.producer, ClientIdentity(/*uid=*/0, /*pid=*/0),
                        args.producer_name, args.shmem_size_hint_bytes,
=======
      ->ConnectProducer(args.producer, /*uid=*/0, /*pid=*/0, args.producer_name,
                        args.shmem_size_hint_bytes,
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
                        /*in_process=*/true,
                        TracingService::ProducerSMBScrapingMode::kEnabled,
                        args.shmem_page_size_hint_bytes);
}

std::unique_ptr<ConsumerEndpoint> InProcessTracingBackend::ConnectConsumer(
    const ConnectConsumerArgs& args) {
  return GetOrCreateService(args.task_runner)
      ->ConnectConsumer(args.consumer, /*uid=*/0);
}

TracingService* InProcessTracingBackend::GetOrCreateService(
    base::TaskRunner* task_runner) {
  if (!service_) {
<<<<<<< HEAD
    std::unique_ptr<InProcessSharedMemory::Factory> shm(
        new InProcessSharedMemory::Factory());
=======
    std::unique_ptr<InProcessShmFactory> shm(new InProcessShmFactory());
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    service_ = TracingService::CreateInstance(std::move(shm), task_runner);
    service_->SetSMBScrapingEnabled(true);
  }
  return service_.get();
}

}  // namespace internal
}  // namespace perfetto
