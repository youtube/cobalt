// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "base/guid.h"
#include "base/logging.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/cross_thread_media_source_attachment.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/media_source.h"
#include "cobalt/dom/media_source_attachment.h"
#include "cobalt/dom/same_thread_media_source_attachment.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/url.h"
#include "cobalt/web/window_or_worker_global_scope.h"
#include "cobalt/worker/worker_settings.h"
#include "url/gurl.h"

namespace cobalt {
namespace dom {

// extern
void RegisterMediaSourceObjectURL(
    script::EnvironmentSettings* environment_settings,
    const std::string& blob_url,
    const scoped_refptr<dom::MediaSource>& media_source) {
  web::EnvironmentSettings* web_settings =
      base::polymorphic_downcast<web::EnvironmentSettings*>(
          environment_settings);
  DCHECK(web_settings);
  DCHECK(web_settings->context());
  web::WindowOrWorkerGlobalScope* global_scope =
      web_settings->context()->GetWindowOrWorkerGlobalScope();

  if (global_scope->IsWorker()) {
    worker::WorkerSettings* worker_settings =
        base::polymorphic_downcast<worker::WorkerSettings*>(
            environment_settings);
    DCHECK(worker_settings);
    DCHECK(worker_settings->media_source_registry());

    scoped_refptr<MediaSourceAttachment> attachment =
        base::MakeRefCounted<CrossThreadMediaSourceAttachment>(media_source);

    worker_settings->media_source_registry()->Register(blob_url, attachment);
  } else {
    dom::DOMSettings* dom_settings =
        base::polymorphic_downcast<dom::DOMSettings*>(environment_settings);
    DCHECK(dom_settings);
    DCHECK(dom_settings->media_source_registry());

    scoped_refptr<MediaSourceAttachment> attachment =
        base::MakeRefCounted<SameThreadMediaSourceAttachment>(media_source);

    dom_settings->media_source_registry()->Register(blob_url, attachment);
  }
}

// extern
bool UnregisterMediaSourceObjectURL(
    script::EnvironmentSettings* environment_settings, const std::string& url) {
  web::EnvironmentSettings* web_environment_settings =
      base::polymorphic_downcast<web::EnvironmentSettings*>(
          environment_settings);
  DCHECK(web_environment_settings);
  DCHECK(web_environment_settings->context());
  web::WindowOrWorkerGlobalScope* global_scope =
      web_environment_settings->context()->GetWindowOrWorkerGlobalScope();

  if (global_scope->IsWorker()) {
    worker::WorkerSettings* worker_settings =
        base::polymorphic_downcast<worker::WorkerSettings*>(
            environment_settings);
    DCHECK(worker_settings);
    DCHECK(worker_settings->media_source_registry());
    return worker_settings->media_source_registry()->Unregister(url);
  } else {
    dom::DOMSettings* dom_settings =
        base::polymorphic_downcast<dom::DOMSettings*>(environment_settings);
    DCHECK(dom_settings);
    DCHECK(dom_settings->media_source_registry());
    return dom_settings->media_source_registry()->Unregister(url);
  }
}

}  // namespace dom
}  // namespace cobalt
