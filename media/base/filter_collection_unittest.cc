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
  scoped_refptr<AudioDecoder> audio_decoder;

  collection_.SelectAudioDecoder(&audio_decoder);
  EXPECT_FALSE(audio_decoder);

  // Add an audio decoder.
  collection_.AddAudioDecoder(mock_filters_.audio_decoder());

  // Verify that we can select the audio decoder.
  collection_.SelectAudioDecoder(&audio_decoder);
  EXPECT_TRUE(audio_decoder);

  // Verify that we can't select it again since only one has been added.
  collection_.SelectAudioDecoder(&audio_decoder);
  EXPECT_FALSE(audio_decoder);
}

TEST_F(FilterCollectionTest, MultipleFiltersOfSameType) {
  scoped_refptr<AudioDecoder> audio_decoder_a(new MockAudioDecoder());
  scoped_refptr<AudioDecoder> audio_decoder_b(new MockAudioDecoder());

  scoped_refptr<AudioDecoder> audio_decoder;

  collection_.AddAudioDecoder(audio_decoder_a.get());
  collection_.AddAudioDecoder(audio_decoder_b.get());

  // Verify that first SelectAudioDecoder() returns audio_decoder_a.
  collection_.SelectAudioDecoder(&audio_decoder);
  EXPECT_TRUE(audio_decoder);
  EXPECT_EQ(audio_decoder, audio_decoder_a);

  // Verify that second SelectAudioDecoder() returns audio_decoder_b.
  collection_.SelectAudioDecoder(&audio_decoder);
  EXPECT_TRUE(audio_decoder);
  EXPECT_EQ(audio_decoder, audio_decoder_b);

  // Verify that third SelectAudioDecoder() returns nothing.
  collection_.SelectAudioDecoder(&audio_decoder);
  EXPECT_FALSE(audio_decoder);
}

}  // namespace media
