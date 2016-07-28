// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/message_loop.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/filters/video_decoder_selector.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

static const VideoFrame::Format kVideoFormat = VideoFrame::YV12;
static const gfx::Size kCodedSize(320, 240);
static const gfx::Rect kVisibleRect(320, 240);
static const gfx::Size kNaturalSize(320, 240);

class VideoDecoderSelectorTest : public ::testing::Test {
 public:
  enum DecryptorCapability {
    kNoDecryptor,
    kDecryptOnly,
    kDecryptAndDecode
  };

  VideoDecoderSelectorTest()
      : clear_video_config_(
            kCodecVP8, VIDEO_CODEC_PROFILE_UNKNOWN, kVideoFormat,
            kCodedSize, kVisibleRect, kNaturalSize, NULL, 0, false),
        encrypted_video_config_(
            kCodecVP8, VIDEO_CODEC_PROFILE_UNKNOWN, kVideoFormat,
            kCodedSize, kVisibleRect, kNaturalSize, NULL, 0, true),
        demuxer_stream_(new StrictMock<MockDemuxerStream>()),
        decryptor_(new NiceMock<MockDecryptor>()),
        decoder_1_(new StrictMock<MockVideoDecoder>()),
        decoder_2_(new StrictMock<MockVideoDecoder>()) {
    all_decoders_.push_back(decoder_1_);
    all_decoders_.push_back(decoder_2_);

    EXPECT_CALL(*demuxer_stream_, type())
        .WillRepeatedly(Return(DemuxerStream::VIDEO));
  }

  ~VideoDecoderSelectorTest() {
    EXPECT_CALL(*decoder_1_, Stop(_))
      .WillRepeatedly(RunClosure<0>());
    EXPECT_CALL(*decoder_2_, Stop(_))
      .WillRepeatedly(RunClosure<0>());

    if (selected_decoder_)
      selected_decoder_->Stop(NewExpectedClosure());

    message_loop_.RunUntilIdle();
  }

  MOCK_METHOD1(OnStatistics, void(const PipelineStatistics&));
  MOCK_METHOD1(SetDecryptorReadyCallback, void(const media::DecryptorReadyCB&));
  MOCK_METHOD2(OnDecoderSelected,
               void(const scoped_refptr<VideoDecoder>&,
                    const scoped_refptr<DecryptingDemuxerStream>&));

  void UseClearStream() {
    EXPECT_CALL(*demuxer_stream_, video_decoder_config())
        .WillRepeatedly(ReturnRef(clear_video_config_));
  }

  void UseEncryptedStream() {
    EXPECT_CALL(*demuxer_stream_, video_decoder_config())
        .WillRepeatedly(ReturnRef(encrypted_video_config_));
  }

  void InitializeDecoderSelector(DecryptorCapability decryptor_capability,
                                 int num_decoders) {
    SetDecryptorReadyCB set_decryptor_ready_cb;
    if (decryptor_capability == kDecryptOnly ||
        decryptor_capability == kDecryptAndDecode) {
      set_decryptor_ready_cb = base::Bind(
          &VideoDecoderSelectorTest::SetDecryptorReadyCallback,
          base::Unretained(this));

      EXPECT_CALL(*this, SetDecryptorReadyCallback(_))
          .WillRepeatedly(RunCallback<0>(decryptor_.get()));

      if (decryptor_capability == kDecryptOnly) {
        EXPECT_CALL(*decryptor_, InitializeVideoDecoderMock(_, _))
            .WillRepeatedly(RunCallback<1>(false));
      } else {
        EXPECT_CALL(*decryptor_, InitializeVideoDecoderMock(_, _))
            .WillRepeatedly(RunCallback<1>(true));
      }
    }

    DCHECK_GE(all_decoders_.size(), static_cast<size_t>(num_decoders));
    VideoDecoderSelector::VideoDecoderList decoders(
        all_decoders_.begin(), all_decoders_.begin() + num_decoders);

    decoder_selector_.reset(new VideoDecoderSelector(
        message_loop_.message_loop_proxy(),
        decoders,
        set_decryptor_ready_cb));
  }

  void SelectDecoder() {
    decoder_selector_->SelectVideoDecoder(
        demuxer_stream_,
        base::Bind(&VideoDecoderSelectorTest::OnStatistics,
                   base::Unretained(this)),
        base::Bind(&VideoDecoderSelectorTest::OnDecoderSelected,
                   base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  // Fixture members.
  scoped_ptr<VideoDecoderSelector> decoder_selector_;
  VideoDecoderConfig clear_video_config_;
  VideoDecoderConfig encrypted_video_config_;
  scoped_refptr<StrictMock<MockDemuxerStream> > demuxer_stream_;
  // Use NiceMock since we don't care about most of calls on the decryptor, e.g.
  // RegisterKeyAddedCB().
  scoped_ptr<NiceMock<MockDecryptor> > decryptor_;
  scoped_refptr<StrictMock<MockVideoDecoder> > decoder_1_;
  scoped_refptr<StrictMock<MockVideoDecoder> > decoder_2_;
  std::vector<scoped_refptr<VideoDecoder> > all_decoders_;

  scoped_refptr<VideoDecoder> selected_decoder_;

  MessageLoop message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoDecoderSelectorTest);
};

// The stream is not encrypted but we have no clear decoder. No decoder can be
// selected.
TEST_F(VideoDecoderSelectorTest, ClearStream_NoDecryptor_NoClearDecoder) {
  UseClearStream();
  InitializeDecoderSelector(kNoDecryptor, 0);

  EXPECT_CALL(*this, OnDecoderSelected(IsNull(), IsNull()));

  SelectDecoder();
}

// The stream is not encrypted and we have one clear decoder. The decoder
// will be selected.
TEST_F(VideoDecoderSelectorTest, ClearStream_NoDecryptor_OneClearDecoder) {
  UseClearStream();
  InitializeDecoderSelector(kNoDecryptor, 1);

  EXPECT_CALL(*decoder_1_, Initialize(_, _, _))
      .WillOnce(RunCallback<1>(PIPELINE_OK));
  EXPECT_CALL(*this, OnDecoderSelected(scoped_refptr<VideoDecoder>(decoder_1_),
                                       IsNull()))
      .WillOnce(SaveArg<0>(&selected_decoder_));

  SelectDecoder();
}

// The stream is not encrypted and we have multiple clear decoders. The first
// decoder that can decode the input stream will be selected.
TEST_F(VideoDecoderSelectorTest, ClearStream_NoDecryptor_MultipleClearDecoder) {
  UseClearStream();
  InitializeDecoderSelector(kNoDecryptor, 2);

  EXPECT_CALL(*decoder_1_, Initialize(_, _, _))
      .WillOnce(RunCallback<1>(DECODER_ERROR_NOT_SUPPORTED));
  EXPECT_CALL(*decoder_2_, Initialize(_, _, _))
      .WillOnce(RunCallback<1>(PIPELINE_OK));
  EXPECT_CALL(*this, OnDecoderSelected(scoped_refptr<VideoDecoder>(decoder_2_),
                                       IsNull()))
      .WillOnce(SaveArg<0>(&selected_decoder_));

  SelectDecoder();
}

// There is a decryptor but the stream is not encrypted. The decoder will be
// selected.
TEST_F(VideoDecoderSelectorTest, ClearStream_HasDecryptor) {
  UseClearStream();
  InitializeDecoderSelector(kDecryptOnly, 1);

  EXPECT_CALL(*decoder_1_, Initialize(_, _, _))
      .WillOnce(RunCallback<1>(PIPELINE_OK));
  EXPECT_CALL(*this, OnDecoderSelected(scoped_refptr<VideoDecoder>(decoder_1_),
                                       IsNull()))
      .WillOnce(SaveArg<0>(&selected_decoder_));

  SelectDecoder();
}

// The stream is encrypted and there's no decryptor. No decoder can be selected.
TEST_F(VideoDecoderSelectorTest, EncryptedStream_NoDecryptor) {
  UseEncryptedStream();
  InitializeDecoderSelector(kNoDecryptor, 1);

  EXPECT_CALL(*this, OnDecoderSelected(IsNull(), IsNull()));

  SelectDecoder();
}

// Decryptor can only do decryption and there's no decoder available. No decoder
// can be selected.
TEST_F(VideoDecoderSelectorTest, EncryptedStream_DecryptOnly_NoClearDecoder) {
  UseEncryptedStream();
  InitializeDecoderSelector(kDecryptOnly, 0);

  EXPECT_CALL(*this, OnDecoderSelected(IsNull(), IsNull()));

  SelectDecoder();
}

// Decryptor can do decryption-only and there's a decoder available. The decoder
// will be selected and a DecryptingDemuxerStream will be created.
TEST_F(VideoDecoderSelectorTest, EncryptedStream_DecryptOnly_OneClearDecoder) {
  UseEncryptedStream();
  InitializeDecoderSelector(kDecryptOnly, 1);

  EXPECT_CALL(*decoder_1_, Initialize(_, _, _))
      .WillOnce(RunCallback<1>(PIPELINE_OK));
  EXPECT_CALL(*this, OnDecoderSelected(scoped_refptr<VideoDecoder>(decoder_1_),
                                       NotNull()))
      .WillOnce(SaveArg<0>(&selected_decoder_));

  SelectDecoder();
}

// Decryptor can only do decryption and there are multiple decoders available.
// The first decoder that can decode the input stream will be selected and
// a DecryptingDemuxerStream will be created.
TEST_F(VideoDecoderSelectorTest,
       EncryptedStream_DecryptOnly_MultipleClearDecoder) {
  UseEncryptedStream();
  InitializeDecoderSelector(kDecryptOnly, 2);

  EXPECT_CALL(*decoder_1_, Initialize(_, _, _))
      .WillOnce(RunCallback<1>(DECODER_ERROR_NOT_SUPPORTED));
  EXPECT_CALL(*decoder_2_, Initialize(_, _, _))
      .WillOnce(RunCallback<1>(PIPELINE_OK));
  EXPECT_CALL(*this, OnDecoderSelected(scoped_refptr<VideoDecoder>(decoder_2_),
                                       NotNull()))
      .WillOnce(SaveArg<0>(&selected_decoder_));

  SelectDecoder();
}

// Decryptor can do decryption and decoding. A DecryptingVideoDecoder will be
// created and selected. The clear decoders should not be touched at all.
// No DecryptingDemuxerStream should to be created.
TEST_F(VideoDecoderSelectorTest, EncryptedStream_DecryptAndDecode) {
  UseEncryptedStream();
  InitializeDecoderSelector(kDecryptAndDecode, 1);

  EXPECT_CALL(*this, OnDecoderSelected(NotNull(), IsNull()))
      .WillOnce(SaveArg<0>(&selected_decoder_));

  SelectDecoder();
}

}  // namespace media
