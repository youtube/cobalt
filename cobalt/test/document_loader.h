// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_TEST_DOCUMENT_LOADER_H_
#define COBALT_TEST_DOCUMENT_LOADER_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "cobalt/base/clock.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/cssom/viewport_size.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_parser.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/testing/stub_environment_settings.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/image/image_cache.h"
#include "cobalt/loader/loader.h"
#include "cobalt/render_tree/resource_provider_stub.h"
#include "cobalt/script/fake_script_runner.h"

namespace cobalt {
namespace test {

// This is a helper class for tests that want to load a .html file from disk
// and interact with it on a single thread.
// Loading a Document from a url involves a lot of boilerplate and a number of
// dependencies. Furthermore, loading a document in Cobalt requires that there
// be a message loop that is pumped until the document finishes loading.
class DocumentLoader : public dom::DocumentObserver {
 public:
  DocumentLoader()
      : fetcher_factory_(NULL /* network_module */),
        css_parser_(css_parser::Parser::Create()),
        dom_parser_(new dom_parser::Parser()),
        resource_provider_stub_(new render_tree::ResourceProviderStub()),
        loader_factory_(new loader::LoaderFactory(
            "Test" /* name */, &fetcher_factory_, resource_provider_stub_.get(),
            debugger_hooks_, 0 /* encoded_image_cache_capacity */,
            base::ThreadPriority::BACKGROUND)),
        image_cache_(loader::image::CreateImageCache(
            "Test.ImageCache", debugger_hooks_, 32U * 1024 * 1024,
            loader_factory_.get())),
        dom_stat_tracker_(new dom::DomStatTracker("IsDisplayedTest")),
        resource_provider_(resource_provider_stub_.get()),
        html_element_context_(
            &environment_settings_, &fetcher_factory_, loader_factory_.get(),
            css_parser_.get(), dom_parser_.get(),
            NULL /* can_play_type_handler  */,
            NULL /* web_media_player_factory */, &script_runner_,
            NULL /* script_value_factory */, NULL /* media_source_registry */,
            &resource_provider_, NULL /* animated_image_tracker */,
            image_cache_.get(), NULL /* reduced_image_cache_capacity_manager */,
            NULL /* remote_font_cache */, NULL /* mesh_cache */,
            dom_stat_tracker_.get(), "" /* language */,
            base::kApplicationStateStarted,
            NULL /* synchronous_loader_interrupt */,
            NULL /* performance */) {}
  void Load(const GURL& url) {
    // Load the document in a nested message loop.
    dom::Document::Options options(url);
    options.navigation_start_clock = new base::SystemMonotonicClock();
    options.viewport_size = cssom::ViewportSize(1920, 1080);
    document_ = new dom::Document(&html_element_context_, options);
    document_->AddObserver(this);
    document_loader_.reset(new loader::Loader(
        base::Bind(&loader::FetcherFactory::CreateFetcher,
                   base::Unretained(&fetcher_factory_), url),
        base::Bind(&dom_parser::Parser::ParseDocumentAsync,
                   base::Unretained(dom_parser_.get()), document_,
                   base::SourceLocation(url.spec(), 1, 1)),
        base::Bind(&OnLoadComplete)));

    nested_loop_.Run();
  }
  dom::Document* document() { return document_.get(); }

 private:
  static void OnLoadComplete(const base::Optional<std::string>& error) {
    if (error) DLOG(ERROR) << *error;
  }

  // dom::DocumentObserver functions
  void OnLoad() override { nested_loop_.Quit(); }
  void OnMutation() override {}
  void OnFocusChanged() override {}

  // A nested message loop needs a non-nested message loop to exist.
  base::MessageLoop message_loop_;

  // Nested message loop on which the document loading will occur.
  base::RunLoop nested_loop_;

  dom::testing::StubEnvironmentSettings environment_settings_;
  base::NullDebuggerHooks debugger_hooks_;
  script::FakeScriptRunner script_runner_;
  loader::FetcherFactory fetcher_factory_;
  std::unique_ptr<css_parser::Parser> css_parser_;
  std::unique_ptr<dom_parser::Parser> dom_parser_;
  std::unique_ptr<render_tree::ResourceProviderStub> resource_provider_stub_;
  std::unique_ptr<loader::LoaderFactory> loader_factory_;
  std::unique_ptr<loader::image::ImageCache> image_cache_;
  std::unique_ptr<dom::DomStatTracker> dom_stat_tracker_;
  render_tree::ResourceProvider* resource_provider_;
  dom::HTMLElementContext html_element_context_;
  scoped_refptr<dom::Document> document_;
  std::unique_ptr<loader::Loader> document_loader_;
};
}  // namespace test
}  // namespace cobalt
#endif  // COBALT_TEST_DOCUMENT_LOADER_H_
