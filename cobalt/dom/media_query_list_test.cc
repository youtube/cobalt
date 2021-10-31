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

#include "cobalt/dom/media_query_list.h"

#include <string>

#include "cobalt/cssom/media_list.h"
#include "cobalt/cssom/media_query.h"
#include "cobalt/cssom/viewport_size.h"
#include "cobalt/dom/screen.h"
#include "testing/gtest/include/gtest/gtest.h"

using cobalt::cssom::ViewportSize;

namespace cobalt {
namespace dom {
namespace {
const ViewportSize kViewSize(1920, 1080);
}  // namespace

TEST(MediaQueryListTest, MediaGetter) {
  scoped_refptr<Screen> screen(new Screen(kViewSize));
  scoped_refptr<cssom::MediaList> media_list(new cssom::MediaList());
  scoped_refptr<cssom::MediaQuery> media_query_1(new cssom::MediaQuery(true));
  scoped_refptr<cssom::MediaQuery> media_query_2(new cssom::MediaQuery(false));
  media_list->Append(media_query_1);
  media_list->Append(media_query_2);

  scoped_refptr<MediaQueryList> media_query_list(
      new MediaQueryList(media_list, screen));
  std::string media = media_query_list->media();
  // The returned string is nearly empty, because MediaQuery serialization is
  // not implemented yet.
  EXPECT_EQ(media, ", ");
}

TEST(MediaQueryListTest, MatchesFalse) {
  scoped_refptr<Screen> screen(new Screen(kViewSize));
  scoped_refptr<cssom::MediaList> media_list(new cssom::MediaList());
  scoped_refptr<cssom::MediaQuery> media_query(new cssom::MediaQuery(false));
  media_list->Append(media_query);

  scoped_refptr<MediaQueryList> media_query_list(
      new MediaQueryList(media_list, screen));
  EXPECT_FALSE(media_query_list->matches());
}

TEST(MediaQueryListTest, MatchesTrue) {
  scoped_refptr<Screen> screen(new Screen(kViewSize));
  scoped_refptr<cssom::MediaList> media_list(new cssom::MediaList());
  scoped_refptr<cssom::MediaQuery> media_query(new cssom::MediaQuery(true));
  media_list->Append(media_query);

  scoped_refptr<MediaQueryList> media_query_list(
      new MediaQueryList(media_list, screen));
  EXPECT_TRUE(media_query_list->matches());
}

}  // namespace dom
}  // namespace cobalt
