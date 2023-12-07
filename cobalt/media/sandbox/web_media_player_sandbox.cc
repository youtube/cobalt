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

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/threading/platform_thread.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/media/sandbox/format_guesstimator.h"
#include "cobalt/media/sandbox/media_sandbox.h"
#include "cobalt/media/sandbox/web_media_player_helper.h"
#include "cobalt/render_tree/image.h"
#include "starboard/common/file.h"
#include "starboard/event.h"
#include "starboard/log.h"
#include "starboard/system.h"
#include "third_party/chromium/media/base/timestamp_constants.h"
#include "third_party/chromium/media/filters/chunk_demuxer.h"

namespace cobalt {
namespace media {
namespace sandbox {
namespace {

using base::TimeDelta;
using render_tree::Image;
using starboard::ScopedFile;

void PrintUsage(const char* executable_path_name) {
  std::string executable_file_name =
      base::FilePath(executable_path_name).BaseName().value();
  const char kExampleAdaptiveAudioPathName[] =
      "demos/media-element-demo/public/assets/dash-audio.mp4";
  const char kExampleAdaptiveVideoPathName[] =
      "demos/media-element-demo/public/assets/dash-video-1080p.mp4";
  const char kExampleProgressiveUrl[] =
      "https://storage.googleapis.com/yt-cobalt-media-element-demo/"
      "progressive.mp4";
  std::stringstream ss;
  // Head
  ss << "\n\n"
     << "======================== " << executable_file_name
     << " ========================\n";

  // Basic usage
  ss << "Usage: " << executable_file_name
     << " [OPTIONS] <adaptive audio file path>\n"
     << "   or: " << executable_file_name
     << " [OPTIONS] <adaptive video file path>\n"
     << "   or: " << executable_file_name
     << " [OPTIONS] <adaptive audio file path> "
     << " <adaptive video file path>\n"
     << "   or: " << executable_file_name
     << " [OPTIONS] <progressive video path or url>\n"
     << "Play adaptive audio/video or progressive video\n\n";

  // Options
  ss << "OPTIONS:\n"
     << "  --dump_video_data: Dump video data into .dmp files\n"
     << "  --use_stub_audio_decoder: Use stub audio decoder to play the video\n"
     << "  --use_stub_audio_sink: Use stub audio sink to play the video\n"
     << "  --use_stub_video_decoder: Use stub video decoder to play the video\n"
     << "\n";

  // Usage examples
  ss << "For example:\n  " << executable_file_name << " --dump_video_data "
     << kExampleAdaptiveAudioPathName << "\n  " << executable_file_name << " "
     << kExampleAdaptiveVideoPathName << "\n  " << executable_file_name << " "
     << kExampleAdaptiveAudioPathName << " " << kExampleAdaptiveVideoPathName
     << "\n  " << executable_file_name << " " << kExampleProgressiveUrl
     << "\n\n";
  SbLogRaw(ss.str().c_str());
}

void OnInitSegmentReceived(std::unique_ptr<::media::MediaTracks> tracks) {}

class InitCobaltHelper {
 public:
  InitCobaltHelper(int argc, char* argv[]) {
    cobalt::InitCobalt(argc, argv, NULL);
  }

 private:
  base::AtExitManager at_exit_manager_;
};

class Application {
 public:
  Application(int argc, char* argv[])
      : init_cobalt_helper_(argc, argv),
        media_sandbox_(argc, argv,
                       base::FilePath(FILE_PATH_LITERAL(
                           "media_source_sandbox_trace.json"))) {
    if (argc > 1) {
      FormatGuesstimator guesstimator1(argv[argc - 1],
                                       media_sandbox_.GetMediaModule());
      FormatGuesstimator guesstimator2(argv[argc - 2],
                                       media_sandbox_.GetMediaModule());

      if (!guesstimator1.is_valid()) {
        SB_LOG(ERROR) << "Invalid path or url: " << argv[argc - 1];
        // Fall off to PrintUsage() and terminate.
      } else if (guesstimator1.is_progressive()) {
        InitializeProgressivePlayback(guesstimator1);
        return;
      } else if (!guesstimator2.is_adaptive()) {
        InitializeAdaptivePlayback(guesstimator1);
        return;
      } else if (guesstimator1.is_audio() && guesstimator2.is_audio()) {
        SB_LOG(ERROR) << "Failed to play because both " << argv[argc - 1]
                      << " and " << argv[argc - 2]
                      << " are audio streams, check usage for more details.";
        // Fall off to PrintUsage() and terminate.
      } else if (!guesstimator1.is_audio() && !guesstimator2.is_audio()) {
        SB_LOG(ERROR) << "Failed to play because both " << argv[argc - 1]
                      << " and " << argv[argc - 2]
                      << " are video streams, check usage for more details.";
        // Fall off to PrintUsage() and terminate.
      } else if (guesstimator1.is_audio()) {
        InitializeAdaptivePlayback(guesstimator1, guesstimator2);
        return;
      } else {
        InitializeAdaptivePlayback(guesstimator2, guesstimator1);
        return;
      }
    }

    PrintUsage(argv[0]);
    SbSystemRequestStop(0);
  }
  ~Application() { media_sandbox_.RegisterFrameCB(MediaSandbox::FrameCB()); }

 private:
  typedef ::media::ChunkDemuxer ChunkDemuxer;

  void InitializeAdaptivePlayback(const FormatGuesstimator& guesstimator) {
    is_adaptive_playback_ = true;

    std::unique_ptr<ScopedFile>& file =
        guesstimator.is_audio() ? audio_file_ : video_file_;
    file.reset(new ScopedFile(guesstimator.adaptive_path().c_str(),
                              kSbFileOpenOnly | kSbFileRead));

    if (!file->IsValid()) {
      LOG(ERROR) << "Failed to open file: " << guesstimator.adaptive_path();
      SbSystemRequestStop(0);
      return;
    }

    player_helper_.reset(new WebMediaPlayerHelper(
        media_sandbox_.GetMediaModule(),
        base::Bind(&Application::OnChunkDemuxerOpened, base::Unretained(this)),
        media_sandbox_.GetViewportSize()));

    // |chunk_demuxer_| will be set inside OnChunkDemuxerOpened()
    // asynchronously during initialization of |player_helper_|.  Wait until
    // it is set before proceed.
    while (!chunk_demuxer_) {
      base::RunLoop().RunUntilIdle();
    }

    LOG(INFO) << "Playing " << guesstimator.adaptive_path();

    std::string id = guesstimator.is_audio() ? kAudioId : kVideoId;
    auto status = chunk_demuxer_->AddId(id, guesstimator.mime_type());
    CHECK_EQ(status, ChunkDemuxer::kOk);

    chunk_demuxer_->SetTracksWatcher(id, base::Bind(OnInitSegmentReceived));
    chunk_demuxer_->SetParseWarningCallback(
        id, base::BindRepeating([](::media::SourceBufferParseWarning warning) {
          LOG(WARNING) << "Encountered SourceBufferParseWarning "
                       << static_cast<int>(warning);
        }));
    player_ = player_helper_->player();

    media_sandbox_.RegisterFrameCB(
        base::Bind(&Application::FrameCB, base::Unretained(this)));

    timer_event_id_ =
        SbEventSchedule(Application::OnTimer, this, kSbTimeSecond / 10);
  }

  void InitializeAdaptivePlayback(
      const FormatGuesstimator& audio_guesstimator,
      const FormatGuesstimator& video_guesstimator) {
    is_adaptive_playback_ = true;
    audio_file_.reset(new ScopedFile(audio_guesstimator.adaptive_path().c_str(),
                                     kSbFileOpenOnly | kSbFileRead));
    video_file_.reset(new ScopedFile(video_guesstimator.adaptive_path().c_str(),
                                     kSbFileOpenOnly | kSbFileRead));

    if (!audio_file_->IsValid()) {
      LOG(ERROR) << "Failed to open audio file: "
                 << audio_guesstimator.adaptive_path();
      SbSystemRequestStop(0);
      return;
    }

    if (!video_file_->IsValid()) {
      LOG(ERROR) << "Failed to open video file: "
                 << video_guesstimator.adaptive_path();
      SbSystemRequestStop(0);
      return;
    }

    player_helper_.reset(new WebMediaPlayerHelper(
        media_sandbox_.GetMediaModule(),
        base::Bind(&Application::OnChunkDemuxerOpened, base::Unretained(this)),
        media_sandbox_.GetViewportSize()));

    // |chunk_demuxer_| will be set inside OnChunkDemuxerOpened()
    // asynchronously during initialization of |player_helper_|.  Wait until
    // it is set before proceed.
    while (!chunk_demuxer_) {
      base::RunLoop().RunUntilIdle();
    }

    LOG(INFO) << "Playing " << audio_guesstimator.adaptive_path() << " and "
              << video_guesstimator.adaptive_path();

    auto status =
        chunk_demuxer_->AddId(kAudioId, audio_guesstimator.mime_type());
    CHECK_EQ(status, ChunkDemuxer::kOk);

    status = chunk_demuxer_->AddId(kVideoId, video_guesstimator.mime_type());
    CHECK_EQ(status, ChunkDemuxer::kOk);

    chunk_demuxer_->SetTracksWatcher(kAudioId,
                                     base::Bind(OnInitSegmentReceived));
    chunk_demuxer_->SetParseWarningCallback(
        kAudioId,
        base::BindRepeating([](::media::SourceBufferParseWarning warning) {
          LOG(WARNING) << "Encountered SourceBufferParseWarning "
                       << static_cast<int>(warning);
        }));
    chunk_demuxer_->SetTracksWatcher(kVideoId,
                                     base::Bind(OnInitSegmentReceived));
    chunk_demuxer_->SetParseWarningCallback(
        kVideoId,
        base::BindRepeating([](::media::SourceBufferParseWarning warning) {
          LOG(WARNING) << "Encountered SourceBufferParseWarning "
                       << static_cast<int>(warning);
        }));
    player_ = player_helper_->player();

    media_sandbox_.RegisterFrameCB(
        base::Bind(&Application::FrameCB, base::Unretained(this)));

    timer_event_id_ =
        SbEventSchedule(Application::OnTimer, this, kSbTimeSecond / 10);
  }

  void InitializeProgressivePlayback(const FormatGuesstimator& guesstimator) {
    LOG(INFO) << "Playing " << guesstimator.progressive_url();

    is_adaptive_playback_ = false;

    player_helper_.reset(new WebMediaPlayerHelper(
        media_sandbox_.GetMediaModule(), media_sandbox_.GetFetcherFactory(),
        guesstimator.progressive_url(), media_sandbox_.GetViewportSize()));
    player_ = player_helper_->player();

    media_sandbox_.RegisterFrameCB(
        base::Bind(&Application::FrameCB, base::Unretained(this)));

    timer_event_id_ =
        SbEventSchedule(Application::OnTimer, this, kSbTimeSecond / 10);
  }

  static void OnTimer(void* context) {
    Application* application = static_cast<Application*>(context);
    DCHECK(application);
    application->Tick();
  }

  void Tick() {
    if (player_helper_->IsPlaybackFinished()) {
      media_sandbox_.RegisterFrameCB(MediaSandbox::FrameCB());
      LOG(INFO) << "Playback finished.";
      SbEventCancel(timer_event_id_);
      SbSystemRequestStop(0);
      return;
    }
    if (is_adaptive_playback_ && !eos_appended_) {
      if (audio_file_) {
        AppendData(kAudioId, audio_file_.get(), &audio_offset_);
      }
      if (video_file_) {
        AppendData(kVideoId, video_file_.get(), &video_offset_);
      }
      bool audio_eos = !audio_file_ || audio_offset_ == audio_file_->GetSize();
      bool video_eos = !video_file_ || video_offset_ == video_file_->GetSize();
      if (audio_eos && video_eos) {
        chunk_demuxer_->MarkEndOfStream(::media::PIPELINE_OK);
        eos_appended_ = true;
      }
    }

    base::RunLoop().RunUntilIdle();
    timer_event_id_ =
        SbEventSchedule(Application::OnTimer, this, kSbTimeSecond / 10);
  }

  void OnChunkDemuxerOpened(ChunkDemuxer* chunk_demuxer) {
    CHECK(chunk_demuxer);
    CHECK(!chunk_demuxer_);

    chunk_demuxer_ = chunk_demuxer;
  }

  void AppendData(const std::string& id, ScopedFile* file, int64* offset) {
    const double kLowWaterMarkInSeconds = 5.0;
    const int64 kMaxBytesToAppend = 1024 * 1024;
    std::vector<uint8_t> buffer(kMaxBytesToAppend);

    while (*offset < file->GetSize()) {
      ::media::Ranges<TimeDelta> ranges = chunk_demuxer_->GetBufferedRanges(id);
      const double end_of_buffer =
          ranges.size() == 0 ? 0.0 : ranges.end(ranges.size() - 1).InSecondsF();
      if (end_of_buffer - player_->GetCurrentTime() > kLowWaterMarkInSeconds) {
        break;
      }
      int64 bytes_to_append =
          std::min(kMaxBytesToAppend, file->GetSize() - *offset);

      const double current_time = player_ ? player_->GetCurrentTime() : 0.0;
      auto evicted = chunk_demuxer_->EvictCodedFrames(
          id, base::TimeDelta::FromSecondsD(current_time), bytes_to_append);
      SB_DCHECK(evicted);

      file->Read(reinterpret_cast<char*>(buffer.data()), bytes_to_append);
      base::TimeDelta timestamp_offset;
      auto appended = chunk_demuxer_->AppendData(
          id, buffer.data(), bytes_to_append, base::TimeDelta(),
          ::media::kInfiniteDuration, &timestamp_offset);
      SB_DCHECK(appended);

      *offset += bytes_to_append;
    }
  }

  scoped_refptr<Image> FrameCB(const base::TimeDelta& time) {
    SbDecodeTarget decode_target = player_helper_->GetCurrentDecodeTarget();

    if (SbDecodeTargetIsValid(decode_target)) {
      return media_sandbox_.resource_provider()->CreateImageFromSbDecodeTarget(
          decode_target);
    }
    return NULL;
  }

  const std::string kAudioId = "audio";
  const std::string kVideoId = "video";

  bool is_adaptive_playback_;
  InitCobaltHelper init_cobalt_helper_;
  MediaSandbox media_sandbox_;
  std::unique_ptr<WebMediaPlayerHelper> player_helper_;
  ChunkDemuxer* chunk_demuxer_ = NULL;
  WebMediaPlayer* player_ = NULL;
  std::unique_ptr<ScopedFile> audio_file_;
  std::unique_ptr<ScopedFile> video_file_;
  int64 audio_offset_ = 0;
  int64 video_offset_ = 0;
  bool eos_appended_ = false;
  SbEventId timer_event_id_ = kSbEventIdInvalid;
};

}  // namespace
}  // namespace sandbox
}  // namespace media
}  // namespace cobalt

void SbEventHandle(const SbEvent* event) {
  using cobalt::media::sandbox::Application;

  static Application* s_application;

  switch (event->type) {
    case kSbEventTypeStart: {
      SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
      DCHECK(!s_application);
      if (data->argument_count == 1) {
        cobalt::media::sandbox::PrintUsage(data->argument_values[0]);
        SbSystemRequestStop(0);
        return;
      }
      s_application =
          new Application(data->argument_count, data->argument_values);
      break;
    }
    case kSbEventTypeStop: {
      delete s_application;
      s_application = NULL;
      break;
    }

    default: {
      break;
    }
  }
}
