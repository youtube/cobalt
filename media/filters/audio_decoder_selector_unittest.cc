// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/message_loop.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/mock_filters.h"
#include "media/filters/audio_decoder_selector.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;

namespace media {

class AudioDecoderSelectorTest : public ::testing::Test {
 public:
  enum DecryptorCapability {
    kNoDecryptor,
    kDecryptOnly,
    kDecryptAndDecode
  };

  AudioDecoderSelectorTest()
      : clear_audio_config_(
            kCodecVorbis, 16, CHANNEL_LAYOUT_STEREO, 44100, NULL, 0, false),
        encrypted_audio_config_(
            kCodecVorbis, 16, CHANNEL_LAYOUT_STEREO, 44100, NULL, 0, true),
        demuxer_stream_(new StrictMock<MockDemuxerStream>()),
        decryptor_(new NiceMock<MockDecryptor>()),
        decoder_1_(new StrictMock<MockAudioDecoder>()),
        decoder_2_(new StrictMock<MockAudioDecoder>()) {
    all_decoders_.push_back(decoder_1_);
    all_decoders_.push_back(decoder_2_);

    EXPECT_CALL(*demuxer_stream_, type())
        .WillRepeatedly(Return(DemuxerStream::AUDIO));
  }

  MOCK_METHOD1(OnStatistics, void(const PipelineStatistics&));
  MOCK_METHOD1(SetDecryptorReadyCallback, void(const media::DecryptorReadyCB&));
  MOCK_METHOD2(OnDecoderSelected,
               void(const scoped_refptr<AudioDecoder>&,
                    const scoped_refptr<DecryptingDemuxerStream>&));

  void UseClearStream() {
    EXPECT_CALL(*demuxer_stream_, audio_decoder_config())
        .WillRepeatedly(ReturnRef(clear_audio_config_));
  }

  void UseEncryptedStream() {
    EXPECT_CALL(*demuxer_stream_, audio_decoder_config())
        .WillRepeatedly(ReturnRef(encrypted_audio_config_));
  }

  void InitializeDecoderSelector(DecryptorCapability decryptor_capability,
                                 int num_decoders) {
    SetDecryptorReadyCB set_decryptor_ready_cb;
    if (decryptor_capability == kDecryptOnly ||
        decryptor_capability == kDecryptAndDecode) {
      set_decryptor_ready_cb = base::Bind(
          &AudioDecoderSelectorTest::SetDecryptorReadyCallback,
          base::Unretained(this));

      EXPECT_CALL(*this, SetDecryptorReadyCallback(_))
          .WillRepeatedly(RunCallback<0>(decryptor_.get()));

      if (decryptor_capability == kDecryptOnly) {
        EXPECT_CALL(*decryptor_, InitializeAudioDecoderMock(_, _))
            .WillRepeatedly(RunCallback<1>(false));
      } else {
        EXPECT_CALL(*decryptor_, InitializeAudioDecoderMock(_, _))
            .WillRepeatedly(RunCallback<1>(true));
      }
    }

    DCHECK_GE(all_decoders_.size(), static_cast<size_t>(num_decoders));
    AudioDecoderSelector::AudioDecoderList decoders(
        all_decoders_.begin(), all_decoders_.begin() + num_decoders);

    decoder_selector_.reset(new AudioDecoderSelector(
        message_loop_.message_loop_proxy(),
        decoders,
        set_decryptor_ready_cb));
  }

  void SelectDecoder() {
    decoder_selector_->SelectAudioDecoder(
        demuxer_stream_,
        base::Bind(&AudioDecoderSelectorTest::OnStatistics,
                   base::Unretained(this)),
        base::Bind(&AudioDecoderSelectorTest::OnDecoderSelected,
                   base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  // Fixture members.
  scoped_ptr<AudioDecoderSelector> decoder_selector_;
  AudioDecoderConfig clear_audio_config_;
  AudioDecoderConfig encrypted_audio_config_;
  scoped_refptr<StrictMock<MockDemuxerStream> > demuxer_stream_;
  // Use NiceMock since we don't care about most of calls on the decryptor, e.g.
  // RegisterKeyAddedCB().
  scoped_ptr<NiceMock<MockDecryptor> > decryptor_;
  scoped_refptr<StrictMock<MockAudioDecoder> > decoder_1_;
  scoped_refptr<StrictMock<MockAudioDecoder> > decoder_2_;
  std::vector<scoped_refptr<AudioDecoder> > all_decoders_;
  MessageLoop message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioDecoderSelectorTest);
};

// The stream is not encrypted but we have no clear decoder. No decoder can be
// selected.
TEST_F(AudioDecoderSelectorTest, ClearStream_NoDecryptor_NoClearDecoder) {
  UseClearStream();
  InitializeDecoderSelector(kNoDecryptor, 0);

  EXPECT_CALL(*this, OnDecoderSelected(IsNull(), IsNull()));

  SelectDecoder();
}

// The stream is not encrypted and we have one clear decoder. The decoder
// will be selected.
TEST_F(AudioDecoderSelectorTest, ClearStream_NoDecryptor_OneClearDecoder) {
  UseClearStream();
  InitializeDecoderSelector(kNoDecryptor, 1);

  EXPECT_CALL(*decoder_1_, Initialize(_, _, _))
      .WillOnce(RunCallback<1>(PIPELINE_OK));
  EXPECT_CALL(*this, OnDecoderSelected(scoped_refptr<AudioDecoder>(decoder_1_),
                                       IsNull()));

  SelectDecoder();
}

// The stream is not encrypted and we have multiple clear decoders. The first
// decoder that can decode the input stream will be selected.
TEST_F(AudioDecoderSelectorTest, ClearStream_NoDecryptor_MultipleClearDecoder) {
  UseClearStream();
  InitializeDecoderSelector(kNoDecryptor, 2);

  EXPECT_CALL(*decoder_1_, Initialize(_, _, _))
      .WillOnce(RunCallback<1>(DECODER_ERROR_NOT_SUPPORTED));
  EXPECT_CALL(*decoder_2_, Initialize(_, _, _))
      .WillOnce(RunCallback<1>(PIPELINE_OK));
  EXPECT_CALL(*this, OnDecoderSelected(scoped_refptr<AudioDecoder>(decoder_2_),
                                       IsNull()));

  SelectDecoder();
}

// There is a decryptor but the stream is not encrypted. The decoder will be
// selected.
TEST_F(AudioDecoderSelectorTest, ClearStream_HasDecryptor) {
  UseClearStream();
  InitializeDecoderSelector(kDecryptOnly, 1);

  EXPECT_CALL(*decoder_1_, Initialize(_, _, _))
      .WillOnce(RunCallback<1>(PIPELINE_OK));
  EXPECT_CALL(*this, OnDecoderSelected(scoped_refptr<AudioDecoder>(decoder_1_),
                                       IsNull()));

  SelectDecoder();
}

// The stream is encrypted and there's no decryptor. No decoder can be selected.
TEST_F(AudioDecoderSelectorTest, EncryptedStream_NoDecryptor) {
  UseEncryptedStream();
  InitializeDecoderSelector(kNoDecryptor, 1);

  EXPECT_CALL(*this, OnDecoderSelected(IsNull(), IsNull()));

  SelectDecoder();
}

// Decryptor can only do decryption and there's no decoder available. No decoder
// can be selected.
TEST_F(AudioDecoderSelectorTest, EncryptedStream_DecryptOnly_NoClearDecoder) {
  UseEncryptedStream();
  InitializeDecoderSelector(kDecryptOnly, 0);

  EXPECT_CALL(*this, OnDecoderSelected(IsNull(), IsNull()));

  SelectDecoder();
}

// Decryptor can do decryption-only and there's a decoder available. The decoder
// will be selected and a DecryptingDemuxerStream will be created.
TEST_F(AudioDecoderSelectorTest, EncryptedStream_DecryptOnly_OneClearDecoder) {
  UseEncryptedStream();
  InitializeDecoderSelector(kDecryptOnly, 1);

  EXPECT_CALL(*decoder_1_, Initialize(_, _, _))
      .WillOnce(RunCallback<1>(PIPELINE_OK));
  EXPECT_CALL(*this, OnDecoderSelected(scoped_refptr<AudioDecoder>(decoder_1_),
                                       NotNull()));

  SelectDecoder();
}

// Decryptor can only do decryption and there are multiple decoders available.
// The first decoder that can decode the input stream will be selected and
// a DecryptingDemuxerStream will be created.
TEST_F(AudioDecoderSelectorTest,
       EncryptedStream_DecryptOnly_MultipleClearDecoder) {
  UseEncryptedStream();
  InitializeDecoderSelector(kDecryptOnly, 2);

  EXPECT_CALL(*decoder_1_, Initialize(_, _, _))
      .WillOnce(RunCallback<1>(DECODER_ERROR_NOT_SUPPORTED));
  EXPECT_CALL(*decoder_2_, Initialize(_, _, _))
      .WillOnce(RunCallback<1>(PIPELINE_OK));
  EXPECT_CALL(*this, OnDecoderSelected(scoped_refptr<AudioDecoder>(decoder_2_),
                                       NotNull()));

  SelectDecoder();
}

// Decryptor can do decryption and decoding. A DecryptingAudioDecoder will be
// created and selected. The clear decoders should not be touched at all.
// No DecryptingDemuxerStream should to be created.
TEST_F(AudioDecoderSelectorTest, EncryptedStream_DecryptAndDecode) {
  UseEncryptedStream();
  InitializeDecoderSelector(kDecryptAndDecode, 1);

  EXPECT_CALL(*this, OnDecoderSelected(NotNull(), IsNull()));

  SelectDecoder();
}

}  // namespace media
