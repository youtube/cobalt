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

TEST_F(FilterCollectionTest, TestIsEmptyAndClear) {
  EXPECT_TRUE(collection_.IsEmpty());

  collection_.AddAudioDecoder(mock_filters_.audio_decoder());

  EXPECT_FALSE(collection_.IsEmpty());

  collection_.Clear();

  EXPECT_TRUE(collection_.IsEmpty());
}

TEST_F(FilterCollectionTest, SelectXXXMethods) {
  scoped_refptr<AudioDecoder> audio_decoder;
  scoped_refptr<VideoDecoder> video_decoder;

  collection_.AddVideoDecoder(mock_filters_.video_decoder());
  EXPECT_FALSE(collection_.IsEmpty());

  // Verify that the video decoder will not be returned if we
  // ask for a different type.
  collection_.SelectAudioDecoder(&audio_decoder);
  EXPECT_FALSE(audio_decoder);
  EXPECT_FALSE(collection_.IsEmpty());

  // Verify that we can actually retrieve the video decoder
  // and that it is removed from the collection.
  collection_.SelectVideoDecoder(&video_decoder);
  EXPECT_TRUE(video_decoder);
  EXPECT_TRUE(collection_.IsEmpty());

  // Add a video decoder and audio decoder.
  collection_.AddVideoDecoder(mock_filters_.video_decoder());
  collection_.AddAudioDecoder(mock_filters_.audio_decoder());

  // Verify that we can select the audio decoder.
  collection_.SelectAudioDecoder(&audio_decoder);
  EXPECT_TRUE(audio_decoder);
  EXPECT_FALSE(collection_.IsEmpty());

  // Verify that we can't select it again since only one has been added.
  collection_.SelectAudioDecoder(&audio_decoder);
  EXPECT_FALSE(audio_decoder);

  // Verify that we can select the video decoder and that doing so will
  // empty the collection again.
  collection_.SelectVideoDecoder(&video_decoder);
  EXPECT_TRUE(collection_.IsEmpty());
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
  EXPECT_FALSE(collection_.IsEmpty());

  // Verify that second SelectAudioDecoder() returns audio_decoder_b.
  collection_.SelectAudioDecoder(&audio_decoder);
  EXPECT_TRUE(audio_decoder);
  EXPECT_EQ(audio_decoder, audio_decoder_b);
  EXPECT_TRUE(collection_.IsEmpty());

  // Verify that third SelectAudioDecoder() returns nothing.
  collection_.SelectAudioDecoder(&audio_decoder);
  EXPECT_FALSE(audio_decoder);
}

}  // namespace media
