/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "cobalt/base/clock.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_parser.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/html_body_element.h"
#include "cobalt/dom/html_collection.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/image/image_cache.h"
#include "cobalt/loader/loader.h"
#include "cobalt/render_tree/resource_provider_stub.h"
#include "cobalt/script/fake_script_runner.h"
#include "cobalt/webdriver/algorithms.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace webdriver {
namespace {

const uint32 kImageCacheCapacity = 32U * 1024 * 1024;

void OnError(const std::string& error) { DLOG(ERROR) << error; }

class IsDisplayedTest : public ::testing::Test, public dom::DocumentObserver {
 protected:
  IsDisplayedTest()
      : fetcher_factory_(NULL /* network_module */),
        css_parser_(css_parser::Parser::Create()),
        dom_parser_(new dom_parser::Parser()),
        resource_provider_stub_(new render_tree::ResourceProviderStub()),
        loader_factory_(new loader::LoaderFactory(
            &fetcher_factory_, resource_provider_stub_.get())),
        image_cache_(loader::image::CreateImageCache("WebdriverTest.ImageCache",
                                                     kImageCacheCapacity,
                                                     loader_factory_.get())),
        dom_stat_tracker_(new dom::DomStatTracker("IsDisplayedTest")),
        resource_provider_(resource_provider_stub_.get()),
        html_element_context_(
            &fetcher_factory_, css_parser_.get(), dom_parser_.get(),
            NULL /* can_play_type_handler  */,
            NULL /* web_media_player_factory */, &script_runner_,
            NULL /* media_source_registry */, &resource_provider_,
            image_cache_.get(), NULL /* reduced_image_cache_capacity_manager */,
            NULL /* remote_font_cache */, dom_stat_tracker_.get(),
            "" /* language */) {}

  void SetUp() OVERRIDE {
    // Load the document in a nested message loop.
    GURL url("file:///cobalt/webdriver_test/displayed_test.html");
    dom::Document::Options options(url);
    options.navigation_start_clock = new base::SystemMonotonicClock();
    options.viewport_size = math::Size(1920, 1080);

    document_ = new dom::Document(&html_element_context_, options);
    document_->AddObserver(this);
    document_loader_.reset(new loader::Loader(
        base::Bind(&loader::FetcherFactory::CreateFetcher,
                   base::Unretained(&fetcher_factory_), url),
        dom_parser_->ParseDocumentAsync(document_,
                                        base::SourceLocation(url.spec(), 1, 1)),
        base::Bind(&OnError)));

    nested_loop_.Run();
  }

  // dom::DocumentObserver functions
  void OnLoad() OVERRIDE { nested_loop_.Quit(); }
  void OnMutation() OVERRIDE {}

  script::FakeScriptRunner script_runner_;
  loader::FetcherFactory fetcher_factory_;
  scoped_ptr<css_parser::Parser> css_parser_;
  scoped_ptr<dom_parser::Parser> dom_parser_;
  scoped_ptr<render_tree::ResourceProviderStub> resource_provider_stub_;
  scoped_ptr<loader::LoaderFactory> loader_factory_;
  scoped_ptr<loader::image::ImageCache> image_cache_;
  scoped_ptr<dom::DomStatTracker> dom_stat_tracker_;
  render_tree::ResourceProvider* resource_provider_;
  dom::HTMLElementContext html_element_context_;
  scoped_refptr<dom::Document> document_;
  scoped_ptr<loader::Loader> document_loader_;

  // A nested message loop needs a non-nested message loop to exist.
  MessageLoop message_loop_;

  // Nested message loop on which the document loading will occur.
  base::RunLoop nested_loop_;
};

}  // namespace

TEST_F(IsDisplayedTest, BodyIsDisplayed) {
  EXPECT_TRUE(algorithms::IsDisplayed(document_->body().get()));
  document_->body()->style()->set_display("none", NULL);
  EXPECT_TRUE(algorithms::IsDisplayed(document_->body().get()));
}

TEST_F(IsDisplayedTest, ElementIsShown) {
  // No style set.
  EXPECT_TRUE(
      algorithms::IsDisplayed(document_->GetElementById("displayed").get()));
  // display: none
  EXPECT_FALSE(
      algorithms::IsDisplayed(document_->GetElementById("none").get()));
  // visiblity: hidden
  EXPECT_FALSE(
      algorithms::IsDisplayed(document_->GetElementById("hidden").get()));
}

TEST_F(IsDisplayedTest, HiddenByParentDisplayStyle) {
  // display: none
  EXPECT_FALSE(
      algorithms::IsDisplayed(document_->GetElementById("hiddenparent").get()));
  // No style set, but is a child of hiddenparent.
  EXPECT_FALSE(
      algorithms::IsDisplayed(document_->GetElementById("hiddenchild").get()));
  // No style set, but is a child of hiddenchild.
  EXPECT_FALSE(
      algorithms::IsDisplayed(document_->GetElementById("hiddenlink").get()));
}

TEST_F(IsDisplayedTest, ZeroHeightOrWidthIsNotDisplayed) {
  EXPECT_FALSE(
      algorithms::IsDisplayed(document_->GetElementById("zeroheight").get()));
  EXPECT_FALSE(
      algorithms::IsDisplayed(document_->GetElementById("zerowidth").get()));
}

TEST_F(IsDisplayedTest, ElementWithNestedBlockLevelElementIsDisplayed) {
  EXPECT_TRUE(algorithms::IsDisplayed(
      document_->GetElementById("containsNestedBlock").get()));
}

TEST_F(IsDisplayedTest, ZeroOpacityIsNotDisplayed) {
  EXPECT_FALSE(
      algorithms::IsDisplayed(document_->GetElementById("transparent").get()));
}

TEST_F(IsDisplayedTest, ZeroSizeDivHasDescendentWithSize) {
  EXPECT_TRUE(
      algorithms::IsDisplayed(document_->GetElementById("zerosizeddiv").get()));
}

TEST_F(IsDisplayedTest, ElementHiddenByOverflow) {
  // Div with zero size width and height and overflow: hidden.
  EXPECT_FALSE(
      algorithms::IsDisplayed(document_->GetElementById("bug5022").get()));
}

}  // namespace webdriver
}  // namespace cobalt
