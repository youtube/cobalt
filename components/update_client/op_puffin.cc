// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/op_puffin.h"

#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/patcher.h"
#include "components/update_client/pipeline_util.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "third_party/puffin/src/include/puffin/puffpatch.h"

namespace update_client {

namespace {

// The sequence of calls is:
//
// [Original Sequence]    [Blocking Pool]
//
// PuffOperation
// CacheLookupDone
//                        Patch
//                        VerifyAndCleanUp
// PatchDone
// [original callback]
//
// All errors shortcut to PatchDone.

// Runs on the original sequence. Adds events and calls the original callback.
void PatchDone(
    base::OnceCallback<void(base::expected<base::FilePath, CategorizedError>)>
        callback,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    base::expected<base::FilePath, CategorizedError> result) {
  event_adder.Run(
      MakeSimpleOperationEvent(result, protocol_request::kEventPuff));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

// Runs in the blocking pool. Deletes any files that are no longer needed.
void VerifyAndCleanUp(
    base::OnceCallback<void(base::expected<base::FilePath, CategorizedError>)>
        callback,
    const base::FilePath& patch_file,
    const base::FilePath& new_file,
    const std::string& output_hash,
    int result) {
  base::DeleteFile(patch_file);
  if (result != puffin::P_OK) {
    base::DeleteFile(new_file);
    std::move(callback).Run(base::unexpected<CategorizedError>(
        {.category = ErrorCategory::kUnpack,
         .code = static_cast<int>(UnpackerError::kDeltaOperationFailure),
         .extra = result}));
    return;
  }

  if (!VerifyFileHash256(new_file, output_hash)) {
    base::DeleteFile(new_file);
    std::move(callback).Run(base::unexpected<CategorizedError>(
        {.category = ErrorCategory::kUnpack,
         .code = static_cast<int>(UnpackerError::kPatchOutHashMismatch)}));
    return;
  }

  std::move(callback).Run(new_file);
}

// Runs in the blocking pool. Opens file handles and applies the patch.
void Patch(
    scoped_refptr<Patcher> patcher,
    const base::FilePath& old_file,
    const base::FilePath& patch_file,
    const base::FilePath& temp_dir,
    const std::string& output_hash,
    base::OnceCallback<void(base::expected<base::FilePath, CategorizedError>)>
        callback) {
  base::FilePath new_file = temp_dir.Append(FILE_PATH_LITERAL("puffpatch_out"));
  patcher->PatchPuffPatch(
      base::File(old_file, base::File::FLAG_OPEN | base::File::FLAG_READ),
      base::File(patch_file, base::File::FLAG_OPEN | base::File::FLAG_READ),
      base::File(new_file, base::File::FLAG_CREATE_ALWAYS |
                               base::File::FLAG_WRITE |
                               base::File::FLAG_WIN_EXCLUSIVE_WRITE),
      base::BindOnce(&VerifyAndCleanUp, std::move(callback), patch_file,
                     new_file, output_hash));
}

// Runs on the original sequence.
void CacheLookupDone(
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    scoped_refptr<Patcher> patcher,
    const base::FilePath& patch_file,
    const base::FilePath& temp_dir,
    const std::string& output_hash,
    base::OnceCallback<void(base::expected<base::FilePath, CategorizedError>)>
        callback,
    base::expected<base::FilePath, UnpackerError> cache_result) {
  if (!cache_result.has_value()) {
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, kTaskTraits,
        base::BindOnce(IgnoreResult(&base::DeleteFile), patch_file),
        base::BindOnce(&PatchDone, std::move(callback), event_adder,
                       base::unexpected<CategorizedError>(
                           {.category = ErrorCategory::kUnpack,
                            .code = static_cast<int>(cache_result.error())})));
    return;
  }
  base::ThreadPool::CreateSequencedTaskRunner(kTaskTraits)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&Patch, patcher, cache_result.value(), patch_file,
                         temp_dir, output_hash,
                         base::BindPostTaskToCurrentDefault(base::BindOnce(
                             &PatchDone, std::move(callback), event_adder))));
}

}  // namespace

base::OnceClosure PuffOperation(
    scoped_refptr<CrxCache> crx_cache,
    scoped_refptr<Patcher> patcher,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    const std::string& old_hash,
    const std::string& output_hash,
    const base::FilePath& patch_file,
    base::OnceCallback<void(base::expected<base::FilePath, CategorizedError>)>
        callback) {
  crx_cache->GetByHash(
      old_hash,
      base::BindOnce(&CacheLookupDone, event_adder, patcher, patch_file,
                     patch_file.DirName(), output_hash, std::move(callback)));
  return base::DoNothing();
}

}  // namespace update_client
