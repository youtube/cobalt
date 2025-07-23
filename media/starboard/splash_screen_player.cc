// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "media/starboard/splash_screen_player.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/starboard/sbplayer_interface.h"
#include "media/starboard/sbplayer_set_bounds_helper.h"
#include "starboard/system.h"

namespace media {

namespace {

std::string GetSplashScreenVideoPath() {
  std::vector<char> content_path(kSbFileMaxPath);
  if (!SbSystemGetPath(kSbSystemPathContentDirectory, content_path.data(),
                       content_path.size())) {
    return "";
  }
  base::FilePath path(content_path.data());
  return path.Append("splash.webm").value();
}

// A minimal implementation of SbPlayerBridge::Host for the splash screen.
class SplashScreenHost : public SbPlayerBridge::Host {
 public:
  void OnPlayerStatus(SbPlayerState state) override {}
  void OnPlayerError(SbPlayerError error, const std::string& message) override {
    LOG(ERROR) << "SplashScreenPlayer error: " << message;
  }
  void OnNeedData(DemuxerStream::Type type,
                  int max_number_of_samples_to_write) override {
    // This is where we would be prompted to write more frames.
  }
};

}  // namespace

SplashScreenPlayer::SplashScreenPlayer(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    SbWindow window)
    : task_runner_(task_runner),
      window_(window),
      set_bounds_helper_(new SbPlayerSetBoundsHelper) {}

SplashScreenPlayer::~SplashScreenPlayer() {
  Teardown();
}

void SplashScreenPlayer::Start() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SplashScreenPlayer::Initialize, base::Unretained(this)));
}

void SplashScreenPlayer::Stop() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SplashScreenPlayer::Teardown, base::Unretained(this)));
}

void SplashScreenPlayer::Initialize() {
  std::string video_path = GetSplashScreenVideoPath();
  if (video_path.empty()) {
    LOG(ERROR) << "Failed to get splash screen video path.";
    return;
  }

  reader_ = std::make_unique<mkvparser::MkvReader>();
  if (reader_->Open(video_path.c_str()) != 0) {
    LOG(ERROR) << "Failed to open splash screen video: " << video_path;
    return;
  }

  long long pos = 0;
  mkvparser::EBMLHeader ebml_header;
  if (ebml_header.Parse(reader_.get(), pos) < 0) {
    LOG(ERROR) << "Failed to parse EBML header.";
    reader_->Close();
    return;
  }

  mkvparser::Segment* segment_unowned;
  if (mkvparser::Segment::CreateInstance(reader_.get(), pos, segment_unowned) !=
      0) {
    LOG(ERROR) << "Failed to create segment instance.";
    reader_->Close();
    return;
  }
  segment_.reset(segment_unowned);

  if (segment_->Load() < 0) {
    LOG(ERROR) << "Failed to load segment.";
    reader_->Close();
    return;
  }

  FindVideoTrack();
}

void SplashScreenPlayer::Teardown() {
  if (main_loop_running_) {
    main_loop_running_ = false;
  }
  player_bridge_.reset();
  if (reader_) {
    reader_->Close();
    reader_.reset();
  }
  segment_.reset();
}

void SplashScreenPlayer::FindVideoTrack() {
  const mkvparser::Tracks* tracks = segment_->GetTracks();
  for (unsigned long i = 0; i < tracks->GetTracksCount(); ++i) {
    const mkvparser::Track* track = tracks->GetTrackByIndex(i);
    if (track && track->GetType() == mkvparser::Track::kVideo) {
      video_track_ = static_cast<const mkvparser::VideoTrack*>(track);
      break;
    }
  }

  if (!video_track_) {
    LOG(ERROR) << "No video track found in splash screen video.";
    Teardown();
    return;
  }

  if (strcmp(video_track_->GetCodecId(), "V_VP9") != 0) {
    LOG(ERROR) << "Splash screen video is not VP9.";
    Teardown();
    return;
  }

  CreatePlayer();
}

void SplashScreenPlayer::CreatePlayer() {
  gfx::Size natural_size(video_track_->GetWidth(), video_track_->GetHeight());
  VideoDecoderConfig video_config(VideoCodec::kVP9, VIDEO_CODEC_PROFILE_UNKNOWN,
                                  VideoDecoderConfig::AlphaMode::kIsOpaque,
                                  VideoColorSpace::REC709(), kNoTransformation,
                                  natural_size, natural_size, EmptyExtraData(),
                                  EncryptionScheme::kUnencrypted);

  // This is a simplified setup. A real implementation might need a more
  // sophisticated host and other parameters.
  static SplashScreenHost host;
  player_bridge_ = std::make_unique<SbPlayerBridge>(
      &sbplayer_interface_, task_runner_,
      base::NullCallback(),  // GetDecodeTargetGraphicsContextProviderFunc
      AudioDecoderConfig(), "", video_config, "video/webm", window_,
      kSbDrmSystemInvalid, &host, set_bounds_helper_.get(),
      false,  // allow_resume_after_suspend
      kSbPlayerOutputModePunchOut,
      nullptr,  // decode_target_provider
      "", 0);

  player_created_ = true;
  main_loop_running_ = true;
  current_cluster_ = segment_->GetFirst();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SplashScreenPlayer::MainLoop, base::Unretained(this)));
}

void SplashScreenPlayer::MainLoop() {
  if (!main_loop_running_) {
    return;
  }

  WriteFrame();

  // Post the next iteration of the loop.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SplashScreenPlayer::MainLoop, base::Unretained(this)));
}

void SplashScreenPlayer::WriteFrame() {
  if (!current_cluster_ || current_cluster_->EOS()) {
    main_loop_running_ = false;
    return;
  }

  if (!current_block_entry_) {
    long status = current_cluster_->GetFirst(current_block_entry_);
    if (status < 0) {
      LOG(ERROR) << "Error parsing first block of cluster.";
      main_loop_running_ = false;
      return;
    }
  }

  while (current_block_entry_ && !current_block_entry_->EOS()) {
    const mkvparser::Block* block = current_block_entry_->GetBlock();
    if (block->GetTrackNumber() == video_track_->GetNumber()) {
      for (int i = 0; i < block->GetFrameCount(); ++i) {
        const mkvparser::Block::Frame& frame = block->GetFrame(i);
        auto buffer = base::MakeRefCounted<DecoderBuffer>(frame.len);
        if (frame.Read(reader_.get(), buffer->writable_data()) != 0) {
          LOG(ERROR) << "Failed to read frame data.";
          continue;
        }
        buffer->set_timestamp(
            base::TimeDelta::FromNanoseconds(block->GetTime(current_cluster_)));
        buffer->set_is_key_frame(block->IsKey());

        std::vector<scoped_refptr<DecoderBuffer>> buffers;
        buffers.push_back(buffer);
        player_bridge_->WriteBuffers(DemuxerStream::VIDEO, buffers);
      }
    }

    long status =
        current_cluster_->GetNext(current_block_entry_, current_block_entry_);
    if (status < 0) {
      LOG(ERROR) << "Error parsing next block of cluster.";
      main_loop_running_ = false;
      return;
    }
  }

  current_cluster_ = segment_->GetNext(current_cluster_);
  current_block_entry_ = nullptr;
}

}  // namespace media
