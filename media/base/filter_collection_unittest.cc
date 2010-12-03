// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

  collection_.AddDataSource(mock_filters_.data_source());

  EXPECT_FALSE(collection_.IsEmpty());

  collection_.Clear();

  EXPECT_TRUE(collection_.IsEmpty());
}

TEST_F(FilterCollectionTest, SelectXXXMethods) {
  scoped_refptr<AudioDecoder> audio_decoder;
  scoped_refptr<DataSource> data_source;

  collection_.AddDataSource(mock_filters_.data_source());
  EXPECT_FALSE(collection_.IsEmpty());

  // Verify that the data source will not be returned if we
  // ask for a different type.
  collection_.SelectAudioDecoder(&audio_decoder);
  EXPECT_FALSE(audio_decoder);
  EXPECT_FALSE(collection_.IsEmpty());

  // Verify that we can actually retrieve the data source
  // and that it is removed from the collection.
  collection_.SelectDataSource(&data_source);
  EXPECT_TRUE(data_source);
  EXPECT_TRUE(collection_.IsEmpty());

  // Add a data source and audio decoder.
  collection_.AddDataSource(mock_filters_.data_source());
  collection_.AddAudioDecoder(mock_filters_.audio_decoder());

  // Verify that we can select the audio decoder.
  collection_.SelectAudioDecoder(&audio_decoder);
  EXPECT_TRUE(audio_decoder);
  EXPECT_FALSE(collection_.IsEmpty());

  // Verify that we can't select it again since only one has been added.
  collection_.SelectAudioDecoder(&audio_decoder);
  EXPECT_FALSE(audio_decoder);

  // Verify that we can select the data source and that doing so will
  // empty the collection again.
  collection_.SelectDataSource(&data_source);
  EXPECT_TRUE(collection_.IsEmpty());
}

TEST_F(FilterCollectionTest, MultipleFiltersOfSameType) {
  scoped_refptr<DataSource> data_source_a(new MockDataSource());
  scoped_refptr<DataSource> data_source_b(new MockDataSource());

  scoped_refptr<DataSource> data_source;

  collection_.AddDataSource(data_source_a.get());
  collection_.AddDataSource(data_source_b.get());

  // Verify that first SelectDataSource() returns data_source_a.
  collection_.SelectDataSource(&data_source);
  EXPECT_TRUE(data_source);
  EXPECT_EQ(data_source, data_source_a);
  EXPECT_FALSE(collection_.IsEmpty());

  // Verify that second SelectDataSource() returns data_source_b.
  collection_.SelectDataSource(&data_source);
  EXPECT_TRUE(data_source);
  EXPECT_EQ(data_source, data_source_b);
  EXPECT_TRUE(collection_.IsEmpty());

  // Verify that third SelectDataSource() returns nothing.
  collection_.SelectDataSource(&data_source);
  EXPECT_FALSE(data_source);
}

}  // namespace media
