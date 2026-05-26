// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_OP_PUFFIN_H_
#define COMPONENTS_UPDATE_CLIENT_OP_PUFFIN_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/patcher.h"

#if BUILDFLAG(IS_STARBOARD)
#include "components/update_client/pipeline.h"
#endif

namespace base {
class FilePath;
}

namespace update_client {

struct CategorizedError;

// Apply a puffin patch. `callback` is posted to the sequence PuffOperation was
// called on, with a file path containing the result of the patch, if
// successful. If unsuccessful, `callback` is posted with an error. In either
#if BUILDFLAG(IS_STARBOARD)
// case, `patch_operation_result.response` is deleted. Returns a cancellation callback.
#else
// case, `patch_file` is deleted. Returns a cancellation callback.
#endif
base::OnceClosure PuffOperation(
    scoped_refptr<CrxCache> crx_cache,
    scoped_refptr<Patcher> patcher,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    const std::string& old_hash,
    const std::string& output_hash,
#if BUILDFLAG(IS_STARBOARD)
    const OperationResult& patch_operation_result,
    base::OnceCallback<void(base::expected<OperationResult, CategorizedError>)>
#else
    const base::FilePath& patch_file,
    base::OnceCallback<void(base::expected<base::FilePath, CategorizedError>)>
#endif
        callback);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_OP_PUFFIN_H_
