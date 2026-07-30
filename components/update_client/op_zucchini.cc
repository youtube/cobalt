// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/op_zucchini.h"

#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/delta_patch_operation.h"
#include "components/update_client/patcher.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"

#if defined(IN_MEMORY_UPDATES)
#include "base/logging.h"
#endif

namespace update_client {

base::OnceClosure ZucchiniOperation(
    scoped_refptr<CrxCache> crx_cache,
    scoped_refptr<Patcher> patcher,
    base::RepeatingCallback<void(base::DictValue)> event_adder,
    base::RepeatingCallback<void(ComponentState)> state_tracker,
    const std::string& previous_hash,
    const std::string& output_hash,
#if BUILDFLAG(IS_STARBOARD)
    const OperationResult& patch_operation_result,
    base::OnceCallback<void(base::expected<OperationResult, CategorizedError>)>
#else
    const base::FilePath& patch_file,
    base::OnceCallback<void(base::expected<base::FilePath, CategorizedError>)>
#endif
        callback) {
#if defined(IN_MEMORY_UPDATES)
  LOG(ERROR) << "Zucchini delta patching Operation not supported with Cobalt "
                "IN_MEMORY_UPDATES";
  std::move(callback).Run(base::unexpected<CategorizedError>(
      {.category = ErrorCategory::kUnpack,
       .code = static_cast<int>(UnpackerError::kDeltaOperationFailure)}));
#else
#if BUILDFLAG(IS_STARBOARD)
  const base::FilePath& patch_file = patch_operation_result.response;
#endif  // BUILDFLAG(IS_STARBOARD)
  base::MakeRefCounted<DeltaPatchOperation>(
      crx_cache, event_adder, state_tracker, previous_hash,
      base::File::FLAG_CREATE | base::File::FLAG_READ | base::File::FLAG_WRITE |
          base::File::FLAG_WIN_EXCLUSIVE_WRITE |
          base::File::FLAG_WIN_SHARE_DELETE |
          base::File::FLAG_CAN_DELETE_ON_CLOSE,
      output_hash, 0, patch_file, protocol_request::kEventZucchini,
#if BUILDFLAG(IS_STARBOARD)
      patch_operation_result,
#endif  // BUILDFLAG(IS_STARBOARD)
      std::move(callback))
      ->Operation(base::BindOnce(&Patcher::PatchZucchini, patcher));
#endif  // defined(IN_MEMORY_UPDATES)
  return base::DoNothing();
}

}  // namespace update_client
