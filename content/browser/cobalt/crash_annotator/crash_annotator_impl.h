// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef CONTENT_BROWSER_CRASH_ANNOTATOR_IMPL_H_
#define CONTENT_BROWSER_CRASH_ANNOTATOR_IMPL_H_

#include <string>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/cobalt/crash_annotator/crash_annotator.mojom.h"

namespace content {

// TODO(cobalt, b/383301493): consider another location for Cobalt's Mojo
// implementations, since they are not "core" services. mcasas@ suggested
// //components or //services as possible destinations.
class CrashAnnotatorImpl : public blink::mojom::CrashAnnotator {
 public:
  explicit CrashAnnotatorImpl(
      mojo::PendingReceiver<blink::mojom::CrashAnnotator> receiver)
      : receiver_(this, std::move(receiver)) {}
  CrashAnnotatorImpl(const CrashAnnotatorImpl&) = delete;
  CrashAnnotatorImpl& operator=(const CrashAnnotatorImpl&) = delete;

  void SetString(const std::string& key,
                 const std::string& value,
                 SetStringCallback callback) override;

 private:
  mojo::Receiver<blink::mojom::CrashAnnotator> receiver_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CRASH_ANNOTATOR_IMPL_H_
