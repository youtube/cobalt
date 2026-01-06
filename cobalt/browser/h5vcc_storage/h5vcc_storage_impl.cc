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

#include "cobalt/browser/h5vcc_storage/h5vcc_storage_impl.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#if BUILDFLAG(USE_EVERGREEN)
#include "starboard/configuration_constants.h"  // nogncheck
#endif

namespace h5vcc_storage {

namespace {

#if BUILDFLAG(USE_EVERGREEN)
const char kCrashpadDBName[] = "crashpad_database";
#endif

}  // namespace

H5vccStorageImpl::H5vccStorageImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccStorage> receiver)
    : content::DocumentService<mojom::H5vccStorage>(render_frame_host,
                                                    std::move(receiver)) {
  DETACH_FROM_THREAD(thread_checker_);
}

H5vccStorageImpl::~H5vccStorageImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void H5vccStorageImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccStorage> receiver) {
  new H5vccStorageImpl(*render_frame_host, std::move(receiver));
}

void H5vccStorageImpl::ClearCrashpadDatabase(
    ClearCrashpadDatabaseCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(USE_EVERGREEN)
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  sequenced_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<base::SingleThreadTaskRunner> task_runner,
             ClearCrashpadDatabaseCallback callback) {
            std::vector<char> cache_dir(kSbFileMaxPath + 1, 0);
            if (SbSystemGetPath(kSbSystemPathCacheDirectory, cache_dir.data(),
                                kSbFileMaxPath)) {
              base::FilePath crashpad_db_dir =
                  base::FilePath(cache_dir.data()).Append(kCrashpadDBName);
              if (!base::DeletePathRecursively(crashpad_db_dir)) {
                DLOG(ERROR) << "Failed to delete crashpad directory: "
                            << crashpad_db_dir.value();
              }
              if (!base::CreateDirectory(crashpad_db_dir)) {
                DLOG(ERROR) << "Failed to recreate crashpad directory: "
                            << crashpad_db_dir.value();
              }
            } else {
              DLOG(ERROR) << "Failed to get cache directory path.";
            }
            task_runner->PostTask(FROM_HERE, std::move(callback));
          },
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          std::move(callback)));
#else
  std::move(callback).Run();
#endif  // BUILDFLAG(USE_EVERGREEN)
}

}  // namespace h5vcc_storage
