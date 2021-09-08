// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/font_cache.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/dom/font_face.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/loader/font/remote_typeface_cache.h"
#include "cobalt/loader/font/typeface_decoder.h"
#include "cobalt/loader/loader.h"
#include "cobalt/loader/mock_loader_factory.h"
#include "cobalt/render_tree/mock_resource_provider.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/render_tree/resource_provider_stub.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {
namespace dom {

using ::testing::_;
using ::testing::Return;

const render_tree::FontStyle kNormalUpright(
    render_tree::FontStyle::kNormalWeight,
    render_tree::FontStyle::kUprightSlant);

std::unique_ptr<FontCache::FontFaceMap> CreateFontFaceMapHelper(
    const std::string& family_name, const base::StringPiece local_font_name) {
  std::unique_ptr<FontCache::FontFaceMap> ffm(new FontCache::FontFaceMap());
  dom::FontFaceStyleSet ffss;
  scoped_refptr<dom::FontFaceStyleSet::Entry> entry =
      base::MakeRefCounted<dom::FontFaceStyleSet::Entry>();
  entry->sources.push_back(
      FontFaceSource(local_font_name.as_string()));  // local()
  entry->sources.push_back(FontFaceSource(
      GURL("https://example.com/Dancing-Regular.woff")));  // url()
  ffss.AddEntry(entry);
  ffm->insert(FontCache::FontFaceMap::value_type(family_name, ffss));
  return ffm;
}

class FontCacheTest : public ::testing::Test {
 public:
  FontCacheTest();
  ~FontCacheTest() override {
    EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
  }

  void DummyOnTypefaceLoadEvent() {}

 protected:
  base::NullDebuggerHooks debugger_hooks_;
  ::testing::StrictMock<loader::MockLoaderFactory> loader_factory_;
  scoped_refptr<cobalt::render_tree::Typeface> sample_typeface_;
  ::testing::StrictMock<cobalt::render_tree::MockResourceProvider>
      mock_resource_provider_;
  cobalt::render_tree::ResourceProvider* mrp;
  std::unique_ptr<loader::font::RemoteTypefaceCache> rtc;
  std::unique_ptr<dom::FontCache> font_cache_;

  base::MessageLoop message_loop_;
};

FontCacheTest::FontCacheTest()
    : sample_typeface_(new render_tree::TypefaceStub(NULL)),
      mrp(dynamic_cast<cobalt::render_tree::ResourceProvider*>(
          &mock_resource_provider_)),
      rtc(new loader::font::RemoteTypefaceCache(
          "test_cache", debugger_hooks_, 32 * 1024 /* 32 KB */,
          true /*are_loading_retries_enabled*/,
          base::Bind(&loader::MockLoaderFactory::CreateTypefaceLoader,
                     base::Unretained(&loader_factory_)))),
      font_cache_(
          new dom::FontCache(&mrp,       // resource_provider
                             rtc.get(),  // remotetypefacecache
                             ALLOW_THIS_IN_INITIALIZER_LIST(base::Bind(
                                 &FontCacheTest::DummyOnTypefaceLoadEvent,
                                 base::Unretained(this))),
                             "en-US", NULL)) {}

TEST_F(FontCacheTest, FindPostscriptFont) {
  const std::string family_name("Dancing Script");
  const std::string postscript_font_name("DancingScript");
  std::unique_ptr<FontCache::FontFaceMap> ffm =
      CreateFontFaceMapHelper(family_name, postscript_font_name);

  const FontFaceStyleSet::Entry* entry = nullptr;
  FontCache::FontFaceMap::iterator ffm_iterator = ffm->find(family_name);
  if (ffm_iterator != ffm->end()) {
    entry = ffm_iterator->second.GetEntriesThatMatchStyle(kNormalUpright)[0];
  }
  EXPECT_TRUE(entry);
  font_cache_->SetFontFaceMap(std::move(ffm));

  EXPECT_CALL(loader_factory_, CreateTypefaceLoaderMock(_, _, _, _, _))
      .Times(0);

  EXPECT_CALL(mock_resource_provider_,
              GetLocalTypefaceIfAvailableMock(postscript_font_name))
      .WillOnce(Return(sample_typeface_));

  FontFace::State state;
  scoped_refptr<render_tree::Font> f =
      font_cache_->TryGetFont(family_name, kNormalUpright, 12.0, &state, entry);

  EXPECT_TRUE(f);
}

TEST_F(FontCacheTest, UseRemote) {
  std::string invalid_postscript_font_name = "DancingScriptInvalidName";
  const std::string family_name("Dancing Script");
  std::unique_ptr<FontCache::FontFaceMap> ffm =
      CreateFontFaceMapHelper(family_name, invalid_postscript_font_name);

  const FontFaceStyleSet::Entry* entry = nullptr;
  FontCache::FontFaceMap::iterator ffm_iterator = ffm->find(family_name);
  if (ffm_iterator != ffm->end()) {
    entry = ffm_iterator->second.GetEntriesThatMatchStyle(kNormalUpright)[0];
  }
  EXPECT_TRUE(entry);
  font_cache_->SetFontFaceMap(std::move(ffm));

  EXPECT_CALL(mock_resource_provider_,
              GetLocalTypefaceIfAvailableMock(invalid_postscript_font_name))
      .Times(1);
  EXPECT_CALL(loader_factory_, CreateTypefaceLoaderMock(_, _, _, _, _));

  FontFace::State state;
  scoped_refptr<render_tree::Font> f =
      font_cache_->TryGetFont(family_name, kNormalUpright, 12.0, &state, entry);
  EXPECT_FALSE(f);
}

}  // namespace dom
}  // namespace cobalt
