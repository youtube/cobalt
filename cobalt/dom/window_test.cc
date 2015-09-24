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

#include "cobalt/dom/window.h"

#include "base/message_loop.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/dom/local_storage_database.h"
#include "cobalt/dom/screen.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/media/media_module_stub.h"
#include "cobalt/network/network_module.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

class MockErrorCallback : public base::Callback<void(const std::string&)> {
 public:
  MOCK_METHOD1(Run, void(const std::string&));
};

TEST(WindowTest, ViewportSize) {
  GURL url("about:blank");
  std::string user_agent;
  MockErrorCallback mock_error_callback;
  MessageLoop message_loop(MessageLoop::TYPE_DEFAULT);
  network::NetworkModule network_module(NULL);
  scoped_ptr<media::MediaModule> stub_media_module(
      new media::MediaModuleStub());
  scoped_ptr<css_parser::Parser> css_parser(css_parser::Parser::Create());
  scoped_ptr<dom_parser::Parser> dom_parser(
      new dom_parser::Parser(mock_error_callback));
  scoped_ptr<loader::FetcherFactory> fetcher_factory(
      new loader::FetcherFactory(&network_module));
  dom::LocalStorageDatabase local_storage_database(NULL);

  scoped_refptr<Window> window = new Window(
      1920, 1080, css_parser.get(), dom_parser.get(), fetcher_factory.get(),
      NULL, &local_storage_database, stub_media_module.get(), NULL, NULL, url,
      user_agent, base::Bind(&MockErrorCallback::Run,
                             base::Unretained(&mock_error_callback)));

  EXPECT_FLOAT_EQ(window->inner_width(), 1920.0f);
  EXPECT_FLOAT_EQ(window->inner_height(), 1080.0f);
  EXPECT_FLOAT_EQ(window->screen_x(), 0.0f);
  EXPECT_FLOAT_EQ(window->screen_y(), 0.0f);
  EXPECT_FLOAT_EQ(window->outer_width(), 1920.0f);
  EXPECT_FLOAT_EQ(window->outer_height(), 1080.0f);
  EXPECT_FLOAT_EQ(window->device_pixel_ratio(), 1.0f);
  EXPECT_FLOAT_EQ(window->screen()->width(), 1920.0f);
  EXPECT_FLOAT_EQ(window->screen()->height(), 1080.0f);
  EXPECT_FLOAT_EQ(window->screen()->avail_width(), 1920.0f);
  EXPECT_FLOAT_EQ(window->screen()->avail_height(), 1080.0f);
}

}  // namespace dom
}  // namespace cobalt
