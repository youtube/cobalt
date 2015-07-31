// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/filter_collection.h"
#include "media/base/mock_filters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class FilterCollectionTest : public ::testing::Test {
 public:
  FilterCollectionTest() {}
  virtual ~FilterCollectionTest() {}

 protected:
  FilterCollection collection_;
  MockFilterCollection mock_filters_;

  DISALLOW_COPY_AND_ASSIGN(FilterCollectionTest);
};

TEST_F(FilterCollectionTest, SelectXXXMethods) {
  scoped_refptr<AudioRenderer> audio_renderer;

  collection_.SelectAudioRenderer(&audio_renderer);
  EXPECT_FALSE(audio_renderer);

  // Add an audio decoder.
  collection_.AddAudioRenderer(mock_filters_.audio_renderer());

  // Verify that we can select the audio decoder.
  collection_.SelectAudioRenderer(&audio_renderer);
  EXPECT_TRUE(audio_renderer);

  // Verify that we can't select it again since only one has been added.
  collection_.SelectAudioRenderer(&audio_renderer);
  EXPECT_FALSE(audio_renderer);
}

TEST_F(FilterCollectionTest, MultipleFiltersOfSameType) {
  scoped_refptr<AudioRenderer> audio_renderer_a(new MockAudioRenderer());
  scoped_refptr<AudioRenderer> audio_renderer_b(new MockAudioRenderer());

  scoped_refptr<AudioRenderer> audio_renderer;

  collection_.AddAudioRenderer(audio_renderer_a.get());
  collection_.AddAudioRenderer(audio_renderer_b.get());

  // Verify that first SelectAudioRenderer() returns audio_renderer_a.
  collection_.SelectAudioRenderer(&audio_renderer);
  EXPECT_TRUE(audio_renderer);
  EXPECT_EQ(audio_renderer, audio_renderer_a);

  // Verify that second SelectAudioRenderer() returns audio_renderer_b.
  collection_.SelectAudioRenderer(&audio_renderer);
  EXPECT_TRUE(audio_renderer);
  EXPECT_EQ(audio_renderer, audio_renderer_b);

  // Verify that third SelectAudioRenderer() returns nothing.
  collection_.SelectAudioRenderer(&audio_renderer);
  EXPECT_FALSE(audio_renderer);
}

}  // namespace media
