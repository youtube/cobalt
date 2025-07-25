// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_PATCH_PUBLIC_CPP_PATCH_H_
#define COMPONENTS_SERVICES_PATCH_PUBLIC_CPP_PATCH_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "components/services/patch/public/mojom/file_patcher.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace base {
class FilePath;
}

namespace patch {

// Patches |input_abs_path| with |patch_abs_path| using the |operation|
// algorithm and place the output in |output_abs_path|.
using PatchCallback = base::OnceCallback<void(int result)>;
void Patch(mojo::PendingRemote<mojom::FilePatcher> file_patcher,
           const std::string& operation,
           const base::FilePath& input_abs_path,
           const base::FilePath& patch_abs_path,
           const base::FilePath& output_abs_path,
           PatchCallback callback);

// Patches |input_abs_path| with |patch_abs_path| using the |operation|
// algorithm and place the output in |output_abs_path|.
void PuffPatch(mojo::PendingRemote<mojom::FilePatcher> file_patcher,
               base::File input_abs_path,
               base::File patch_abs_path,
               base::File output_abs_path,
               PatchCallback callback);

}  // namespace patch

#endif  // COMPONENTS_SERVICES_PATCH_PUBLIC_CPP_PATCH_H_
