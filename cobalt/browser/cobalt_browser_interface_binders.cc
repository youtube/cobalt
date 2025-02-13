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

#include "cobalt/browser/cobalt_browser_interface_binders.h"

#include "base/functional/bind.h"
#include "cobalt/browser/crash_annotator/crash_annotator_impl.h"
#include "cobalt/browser/crash_annotator/public/mojom/crash_annotator.mojom.h"
#include "cobalt/browser/h5vcc_runtime/h5vcc_runtime_impl.h"
#include "cobalt/browser/h5vcc_runtime/public/mojom/h5vcc_runtime.mojom.h"
#include "cobalt/browser/h5vcc_system/h5vcc_system_impl.h"
#include "cobalt/browser/h5vcc_system/public/mojom/h5vcc_system.mojom.h"

namespace cobalt {

void PopulateCobaltFrameBinders(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map) {
  binder_map->Add<crash_annotator::mojom::CrashAnnotator>(
      base::BindRepeating(&crash_annotator::CrashAnnotatorImpl::Create));
  binder_map->Add<h5vcc_system::mojom::H5vccSystem>(
      base::BindRepeating(&h5vcc_system::H5vccSystemImpl::Create));
  binder_map->Add<h5vcc_runtime::mojom::H5vccRuntime>(
      base::BindRepeating(&h5vcc_runtime::H5vccRuntimeImpl::Create));
}

}  // namespace cobalt
