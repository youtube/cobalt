// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/worker/worker_global_scope.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/loader/net_fetcher.h"
#include "cobalt/loader/origin.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/web/context.h"
#include "cobalt/web/dom_exception.h"
#include "cobalt/web/user_agent_platform_info.h"
#include "cobalt/web/window_or_worker_global_scope.h"
#include "cobalt/web/window_timers.h"
#include "cobalt/worker/service_worker_object.h"
#include "cobalt/worker/worker_consts.h"
#include "cobalt/worker/worker_location.h"
#include "cobalt/worker/worker_navigator.h"
#include "net/base/mime_util.h"
#include "starboard/atomic.h"
#include "url/gurl.h"

namespace cobalt {
namespace worker {

namespace {

class ScriptLoader : public base::MessageLoop::DestructionObserver {
 public:
  explicit ScriptLoader(web::Context* context) : context_(context) {
    thread_.reset(new base::Thread("ImportScriptsLoader"));
    thread_->Start();

    // Register as a destruction observer to shut down the Loaders once all
    // pending tasks have been executed and the message loop is about to be
    // destroyed. This allows us to safely stop the thread, drain the task
    // queue, then destroy the internal components before the message loop is
    // reset. No posted tasks will be executed once the thread is stopped.
    thread_->message_loop()->task_runner()->PostTask(
        FROM_HERE, base::Bind(&base::MessageLoop::AddDestructionObserver,
                              base::Unretained(thread_->message_loop()),
                              base::Unretained(this)));

    thread_->message_loop()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](loader::FetcherFactory* fetcher_factory,
               std::unique_ptr<loader::ScriptLoaderFactory>*
                   script_loader_factory_) {
              TRACE_EVENT0("cobalt::worker",
                           "ScriptLoader::ScriptLoaderFactory Task");
              script_loader_factory_->reset(new loader::ScriptLoaderFactory(
                  "ImportScriptsLoader", fetcher_factory));
            },
            context_->fetcher_factory(), &script_loader_factory_));
  }


  ~ScriptLoader() {
    if (thread_) {
      // Stop the thread. This will cause the destruction observer to be
      // notified.
      thread_->Stop();
    }
  }

  void WillDestroyCurrentMessageLoop() {
    // Destroy members that were constructed in the worker thread.
    errors_.clear();
    errors_.shrink_to_fit();
    contents_.clear();
    contents_.shrink_to_fit();
    loaders_.clear();
    loaders_.shrink_to_fit();
    script_loader_factory_.reset();
  }

  void Load(web::CspDelegate* csp_delegate, const loader::Origin& origin,
            const std::vector<GURL>& resolved_urls) {
    TRACE_EVENT0("cobalt::worker", "ScriptLoader::Load()");
    number_of_loads_ = resolved_urls.size();
    contents_.resize(resolved_urls.size());
    loaders_.resize(resolved_urls.size());
    errors_.resize(resolved_urls.size());
    if (resolved_urls.empty()) return;

    for (int i = 0; i < resolved_urls.size(); ++i) {
      const GURL& url = resolved_urls[i];
      thread_->message_loop()->task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(&ScriptLoader::LoaderTask, base::Unretained(this),
                         csp_delegate, &loaders_[i], origin, url, &contents_[i],
                         &errors_[i]));
    }
    load_finished_.Wait();
  }

  void LoaderTask(web::CspDelegate* csp_delegate,
                  std::unique_ptr<loader::Loader>* loader,
                  const loader::Origin& origin, const GURL& url,
                  std::unique_ptr<std::string>* content,
                  std::unique_ptr<std::string>* error) {
    TRACE_EVENT0("cobalt::worker", "ScriptLoader::LoaderTask()");
    csp::SecurityCallback csp_callback =
        base::Bind(&web::CspDelegate::CanLoad, base::Unretained(csp_delegate),
                   web::CspDelegate::kWorker);

    bool skip_fetch_intercept =
        context_->GetWindowOrWorkerGlobalScope()->IsServiceWorker();
    // If there is a request callback, call it to possibly retrieve previously
    // requested content.
    *loader = script_loader_factory_->CreateScriptLoader(
        url, origin, csp_callback,
        base::Bind(
            [](std::unique_ptr<std::string>* output_content,
               const loader::Origin& last_url_origin,
               std::unique_ptr<std::string> content) {
              *output_content = std::move(content);
            },
            content),
        base::Bind(&ScriptLoader::UpdateOnResponseStarted,
                   base::Unretained(this), error),
        base::Bind(&ScriptLoader::LoadingCompleteCallback,
                   base::Unretained(this), loader, error),
        net::HttpRequestHeaders(), skip_fetch_intercept);
  }

  void LoadingCompleteCallback(std::unique_ptr<loader::Loader>* loader,
                               std::unique_ptr<std::string>* output_error,
                               const base::Optional<std::string>& error) {
    TRACE_EVENT0("cobalt::worker", "ScriptLoader::LoadingCompleteCallback()");
    if (error) {
      output_error->reset(new std::string(std::move(error.value())));
    }
    if (!SbAtomicNoBarrier_Increment(&number_of_loads_, -1)) {
      // Clear the loader factory after this callback
      // completes.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(
                         [](base::WaitableEvent* load_finished_) {
                           load_finished_->Signal();
                         },
                         &load_finished_));
    }
  }

  bool UpdateOnResponseStarted(
      std::unique_ptr<std::string>* error, loader::Fetcher* fetcher,
      const scoped_refptr<net::HttpResponseHeaders>& headers) {
    std::string content_type;
    bool mime_type_is_javascript = false;
    if (headers->GetNormalizedHeader("Content-type", &content_type)) {
      for (auto mime_type : WorkerConsts::kJavaScriptMimeTypes) {
        if (net::MatchesMimeType(mime_type, content_type)) {
          mime_type_is_javascript = true;
          break;
        }
      }
    }
    if (content_type.empty()) {
      error->reset(new std::string(
          base::StringPrintf(WorkerConsts::kServiceWorkerRegisterNoMIMEError)));
    } else if (!mime_type_is_javascript) {
      error->reset(new std::string(
          base::StringPrintf(WorkerConsts::kServiceWorkerRegisterBadMIMEError,
                             content_type.c_str())));
    }
    return true;
  }

  std::unique_ptr<std::string>& GetContents(int index) {
    return contents_[index];
  }

  const std::unique_ptr<std::string>& GetError(int index) {
    return errors_[index];
  }

 private:
  web::Context* context_;
  base::WaitableEvent load_finished_ = {
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED};

  std::unique_ptr<base::Thread> thread_;

  std::unique_ptr<loader::ScriptLoaderFactory> script_loader_factory_;

  volatile SbAtomic32 number_of_loads_;

  std::vector<std::unique_ptr<std::string>> contents_;
  std::vector<std::unique_ptr<std::string>> errors_;
  std::vector<std::unique_ptr<loader::Loader>> loaders_;

  base::Optional<std::string> error_;
};

}  // namespace

WorkerGlobalScope::WorkerGlobalScope(
    script::EnvironmentSettings* settings,
    const web::WindowOrWorkerGlobalScope::Options& options)
    : web::WindowOrWorkerGlobalScope(settings, options),
      location_(new WorkerLocation(settings->creation_url())),
      navigator_(new WorkerNavigator(settings)) {
  set_navigator_base(navigator_);
}

bool WorkerGlobalScope::InitializePolicyContainerCallback(
    loader::Fetcher* fetcher,
    const scoped_refptr<net::HttpResponseHeaders>& headers) {
  DCHECK(headers);
  // https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#initialize-worker-policy-container
  // 1. If workerGlobalScope's url is local but its scheme is not "blob":
  //   1. Assert: workerGlobalScope's owner set's size is 1.
  //   2. Set workerGlobalScope's policy container to a clone of
  //      workerGlobalScope's owner set[0]'s relevant settings object's policy
  //      container.
  // 2. Otherwise, set workerGlobalScope's policy container to the result of
  //    creating a policy container from a fetch response given response and
  //    environment.
  // Steps from create a policy container from a fetch response:
  // https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#creating-a-policy-container-from-a-fetch-response
  // 1. If response's URL's scheme is "blob", then return a clone of response's
  //    URL's blob URL entry's environment's policy container.
  // 2. Let result be a new policy container.
  // 3. Set result's CSP list to the result of parsing a response's Content
  //    Security Policies given response.
  // 4. If environment is non-null, then set result's embedder policy to the
  //    result of obtaining an embedder policy given response and environment.
  //    Otherwise, set it to "unsafe-none".
  // 5. Set result's referrer policy to the result of parsing the
  //    `Referrer-Policy` header given response. [REFERRERPOLICY]
  // 6. Return result.

  // Steps 3-6. Since csp_delegate doesn't fully mirror PolicyContainer, we
  // don't create a new one here and return it. Instead we update the existing
  // one for this worker and return true for success and false for failure.
  csp::ResponseHeaders csp_headers(headers);
  if (csp_delegate()->OnReceiveHeaders(csp_headers)) {
    return true;
  }
  // Only NetFetchers are expected to call this, since only they have the
  // response headers.
  loader::NetFetcher* net_fetcher =
      fetcher ? base::polymorphic_downcast<loader::NetFetcher*>(fetcher)
              : nullptr;
  net::URLFetcher* url_fetcher =
      net_fetcher ? net_fetcher->url_fetcher() : nullptr;
  const GURL& url = url_fetcher ? url_fetcher->GetURL()
                                : environment_settings()->creation_url();
  LOG(INFO) << "Failure receiving Content Security Policy headers for URL: "
            << url << ".";
  // Return true regardless of CSP headers being received to continue loading
  // the response.
  return true;
}

void WorkerGlobalScope::ImportScripts(const std::vector<std::string>& urls,
                                      script::ExceptionState* exception_state) {
  ImportScriptsInternal(urls, exception_state, URLLookupCallback(),
                        ResponseCallback());
}

bool WorkerGlobalScope::LoadImportsAndReturnIfUpdated(
    const ScriptResourceMap& previous_resource_map,
    ScriptResourceMap* new_resource_map) {
  bool has_updated_resources = false;
  // Steps from Algorithm for Update:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#update-algorithm
  //   8.21.1. For each importUrl -> storedResponse of newestWorker’s script
  //           resource map:
  std::vector<GURL> request_urls;
  for (const auto& resource_map_entry : previous_resource_map) {
    //   8.21.1.1. If importUrl is url, then continue.
    if (new_resource_map->find(resource_map_entry.first) !=
        new_resource_map->end()) {
      continue;
    }
    request_urls.push_back(resource_map_entry.first);
  }

  //   8.21.1.2. Let importRequest be a new request whose url is importUrl,
  //             client is job’s client, destination is "script", parser
  //             metadata is "not parser-inserted", synchronous flag is set,
  //             and whose use-URL-credentials flag is set.
  //   8.21.1.3. Set importRequest’s cache mode to "no-cache" if any of the
  //             following are true:
  //               - registration’s update via cache mode is "none".
  //               - job’s force bypass cache flag is set.
  //               - registration is stale.
  //   8.21.1.4. Let fetchedResponse be the result of fetching importRequest.
  web::EnvironmentSettings* settings = environment_settings();
  const GURL& base_url = settings->base_url();
  loader::Origin origin = loader::Origin(base_url.DeprecatedGetOriginAsURL());
  ScriptLoader script_loader(settings->context());
  script_loader.Load(csp_delegate(), origin, request_urls);

  for (int index = 0; index < request_urls.size(); ++index) {
    const auto& error = script_loader.GetError(index);
    if (error) continue;
    const GURL& url = request_urls[index];
    //   8.21.1.5. Set updatedResourceMap[importRequest’s url] to
    //             fetchedResponse.
    // Note: The headers of imported scripts aren't used anywhere.
    auto result = new_resource_map->insert(std::make_pair(
        url, ScriptResource(std::move(script_loader.GetContents(index)))));
    // Assert that the insert was successful.
    DCHECK(result.second);
    //   8.21.1.6. Set fetchedResponse to fetchedResponse’s unsafe response.
    //   8.21.1.7. If fetchedResponse’s cache state is not
    //             "local", set registration’s last update check time to the
    //             current time.
    //   8.21.1.8. If fetchedResponse is a bad import script response, continue.
    //   8.21.1.9. If fetchedResponse’s body is not byte-for-byte identical with
    //             storedResponse’s unsafe response's body, set
    //             hasUpdatedResources to true.
    DCHECK(previous_resource_map.find(url) != previous_resource_map.end());
    if (*result.first->second.content !=
        *(previous_resource_map.find(url)->second.content)) {
      has_updated_resources = true;
    }
  }
  return has_updated_resources;
}

void WorkerGlobalScope::ImportScriptsInternal(
    const std::vector<std::string>& urls,
    script::ExceptionState* exception_state,
    URLLookupCallback url_lookup_callback, ResponseCallback response_callback) {
  // Algorithm for import scripts into worker global scope:
  //   https://html.spec.whatwg.org/multipage/workers.html#import-scripts-into-worker-global-scope

  // 1. If worker global scope's type is "module", throw a TypeError exception.
  // Cobalt does not support "module" type scripts.

  // 2. Let settings object be the current settings object.
  web::EnvironmentSettings* settings = environment_settings();
  DCHECK(settings->context()
             ->message_loop()
             ->task_runner()
             ->BelongsToCurrentThread());

  // 3. If urls is empty, return.
  if (urls.empty()) return;

  // 4. Parse each value in urls relative to settings object. If any fail, throw
  //    a "SyntaxError" DOMException.
  std::vector<GURL> request_urls;
  std::vector<std::string*> looked_up_content;
  request_urls.reserve(urls.size());
  std::vector<GURL> resolved_urls(urls.size());
  std::vector<int> request_url_indexes(urls.size());

  const GURL& base_url = settings->base_url();
  for (int index = 0; index < urls.size(); ++index) {
    const std::string& url = urls[index];
    resolved_urls[index] = base_url.Resolve(url);
    if (resolved_urls[index].is_empty()) {
      web::DOMException::Raise(web::DOMException::kSyntaxErr, exception_state);
      return;
    }
    std::string* content =
        url_lookup_callback
            ? url_lookup_callback.Run(resolved_urls[index], exception_state)
            : nullptr;
    // Return if the url lookup callback has set the exception state.
    if (exception_state->is_exception_set()) return;
    if (content) {
      // Store the result of the url lookup callback.
      request_url_indexes[index] = -1;
      looked_up_content.push_back(content);
    } else {
      // Add the url to the list to pass to ScriptLoader for loading.
      request_url_indexes[index] = request_urls.size();
      request_urls.push_back(resolved_urls[index]);
    }
  }

  loader::Origin origin = loader::Origin(base_url.DeprecatedGetOriginAsURL());

  // 5. For each url in the resulting URL records, run these substeps:
  // 5.1. Fetch a classic worker-imported script given url and settings
  //      object, passing along any custom perform the fetch steps provided.
  //      If this succeeds, let script be the result. Otherwise, rethrow the
  //      exception.
  ScriptLoader script_loader(settings->context());
  script_loader.Load(csp_delegate(), origin, request_urls);

  // 5. For each url in the resulting URL records, run these substeps:
  int content_lookup_index = 0;
  for (int index = 0; index < resolved_urls.size(); ++index) {
    int request_index = request_url_indexes[index];
    std::string* script = nullptr;

    if (request_index == -1) {
      // The content at this index was received from the url lookup callback.
      script = looked_up_content[content_lookup_index++];
    } else {
      const auto& error = script_loader.GetError(request_index);
      if (error) {
        LOG(WARNING) << "Script Loading Failed : " << *error;
        web::DOMException::Raise(web::DOMException::kNetworkErr, *error,
                                 exception_state);
        break;
      }
      script = script_loader.GetContents(request_index).get();
    }
    // 5.2. Run the classic script script, with the rethrow errors argument
    //      set to true.
    if (script) {
      if (response_callback) {
        script = response_callback.Run(resolved_urls[index], script);
      }
      bool succeeded = false;
      std::string retval;
      if (script) {
        bool mute_errors = false;
        const base::SourceLocation script_location(resolved_urls[index].spec(),
                                                   1, 1);
        retval = settings->context()->script_runner()->Execute(
            *script, script_location, mute_errors, &succeeded);
      }
      if (!succeeded) {
        // TODO(): Handle script execution errors.
        web::DOMException::Raise(web::DOMException::kSyntaxErr,
                                 exception_state);
        return;
      }
    }
  }
  // If an exception was thrown or if the script was prematurely aborted, then
  // abort all these steps, letting the exception or aborting continue to be
  // processed by the calling script.
}

}  // namespace worker
}  // namespace cobalt
