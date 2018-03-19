// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_LAYOUT_TESTS_LAYOUT_SNAPSHOT_H_
#define COBALT_LAYOUT_TESTS_LAYOUT_SNAPSHOT_H_

#include "cobalt/browser/web_module.h"
#include "cobalt/dom/screenshot_manager.h"
#include "cobalt/render_tree/resource_provider.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace layout_tests {

// This function will internally create a WebModule object which opens the
// specified URL, waits for it to load and perform a layout (via the
// kTestRunnerMode layout trigger), and returns the output layout results.
// At encapsulates core functionality used by both layout benchmarks and layout
// tests.
browser::WebModule::LayoutResults SnapshotURL(
    const GURL& url, const math::Size& viewport_size,
    render_tree::ResourceProvider* resource_provider,
    const dom::ScreenshotManager::ProvideScreenshotFunctionCallback&
        screenshot_provider);

}  // namespace layout_tests
}  // namespace cobalt

#endif  // COBALT_LAYOUT_TESTS_LAYOUT_SNAPSHOT_H_
