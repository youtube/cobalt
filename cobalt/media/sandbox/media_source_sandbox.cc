// Copyright 2016 Google Inc. All Rights Reserved.
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
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/threading/platform_thread.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/media/sandbox/media_sandbox.h"
#include "cobalt/media/sandbox/web_media_player_helper.h"
#include "cobalt/render_tree/image.h"
#if defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/media/base/shell_video_frame_provider.h"
#else  // defined(COBALT_MEDIA_SOURCE_2016)
#include "media/base/video_frame.h"
#endif  // defined(COBALT_MEDIA_SOURCE_2016)
#include "starboard/event.h"
#include "starboard/file.h"
#include "starboard/system.h"

namespace cobalt {
namespace media {
namespace sandbox {
namespace {

#if !defined(COBALT_MEDIA_SOURCE_2016)
using ::media::ChunkDemuxer;
using ::media::PIPELINE_OK;
using ::media::Ranges;
using ::media::VideoFrame;
using ::media::WebMediaPlayer;
#endif  // !defined(COBALT_MEDIA_SOURCE_2016)

using base::TimeDelta;
using render_tree::Image;
using starboard::ScopedFile;

std::string ResolvePath(const std::string& path) {
  FilePath result(path);
  if (!result.IsAbsolute()) {
    FilePath content_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &content_path);
    CHECK(content_path.IsAbsolute());
    result = content_path.Append(result);
  }
  return result.value();
}

#if defined(COBALT_MEDIA_SOURCE_2016)

std::string MakeCodecParameter(const char* string) { return string; }
void OnInitSegmentReceived(scoped_ptr<MediaTracks> tracks) {
  UNREFERENCED_PARAMETER(tracks);
}

#else  // defined(COBALT_MEDIA_SOURCE_2016)

std::vector<std::string> MakeCodecParameter(const char* string) {
  std::vector<std::string> result;
  result.push_back(string);
  return result;
}

#endif  // defined(COBALT_MEDIA_SOURCE_2016)

bool IsWebM(const std::string& path) {
  return path.size() >= 5 && path.substr(path.size() - 5) == ".webm";
}

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
        media_sandbox_(
            argc, argv,
            FilePath(FILE_PATH_LITERAL("media_source_sandbox_trace.json"))),
        player_helper_(media_sandbox_.GetMediaModule(),
                       base::Bind(&Application::OnChunkDemuxerOpened,
                                  base::Unretained(this))) {
    DCHECK_EQ(argc, 3);

    // |chunk_demuxer_| will be set inside OnChunkDemuxerOpened() asynchronously
    // during initialization of |player_helper_|.  Wait until it is set before
    // proceed.
    while (!chunk_demuxer_) {
      MessageLoop::current()->RunUntilIdle();
    }

    std::string audio_path = ResolvePath(argv[1]);
    std::string video_path = ResolvePath(argv[2]);

    audio_file_.reset(
        new ScopedFile(audio_path.c_str(), kSbFileOpenOnly | kSbFileRead));
    video_file_.reset(
        new ScopedFile(video_path.c_str(), kSbFileOpenOnly | kSbFileRead));

    if (!audio_file_->IsValid()) {
      LOG(ERROR) << "Failed to open audio file: " << audio_path;
      SbSystemRequestStop(0);
      return;
    }

    if (!video_file_->IsValid()) {
      LOG(ERROR) << "Failed to open video file: " << video_path;
      SbSystemRequestStop(0);
      return;
    }

    LOG(INFO) << "Playing " << audio_path << " and " << video_path;

    player_ = player_helper_.player();

    AddSourceBuffers(IsWebM(video_path));

#if defined(COBALT_MEDIA_SOURCE_2016)
    chunk_demuxer_->SetTracksWatcher(kAudioId,
                                     base::Bind(OnInitSegmentReceived));
    chunk_demuxer_->SetTracksWatcher(kVideoId,
                                     base::Bind(OnInitSegmentReceived));
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

    media_sandbox_.RegisterFrameCB(
        base::Bind(&Application::FrameCB, base::Unretained(this)));

    timer_event_id_ =
        SbEventSchedule(Application::OnTimer, this, kSbTimeSecond / 10);
  }

 private:
  static void OnTimer(void* context) {
    Application* application = static_cast<Application*>(context);
    DCHECK(application);
    application->Tick();
  }
  void Tick() {
    if (player_helper_.IsPlaybackFinished()) {
      media_sandbox_.RegisterFrameCB(MediaSandbox::FrameCB());
      LOG(INFO) << "Playback finished.";
      SbEventCancel(timer_event_id_);
      SbSystemRequestStop(0);
      return;
    }
    if (!eos_appended_) {
      AppendData(kAudioId, audio_file_.get(), &audio_offset_);
      AppendData(kVideoId, video_file_.get(), &video_offset_);
      if (audio_offset_ == audio_file_->GetSize() &&
          video_offset_ == video_file_->GetSize()) {
#if defined(COBALT_MEDIA_SOURCE_2016)
        chunk_demuxer_->MarkEndOfStream(PIPELINE_OK);
#else   // defined(COBALT_MEDIA_SOURCE_2016)
        chunk_demuxer_->EndOfStream(PIPELINE_OK);
#endif  // defined(COBALT_MEDIA_SOURCE_2016)
        eos_appended_ = true;
      }
    }

    MessageLoop::current()->RunUntilIdle();
    timer_event_id_ =
        SbEventSchedule(Application::OnTimer, this, kSbTimeSecond / 10);
  }

  void OnChunkDemuxerOpened(ChunkDemuxer* chunk_demuxer) {
    CHECK(chunk_demuxer);
    CHECK(!chunk_demuxer_);

    chunk_demuxer_ = chunk_demuxer;
  }

  void AddSourceBuffers(bool is_webm) {
    const char kAACMime[] = "audio/mp4";
    const char kAACCodecs[] = "mp4a.40.2";
    const char kAVCMime[] = "video/mp4";
    const char kAVCCodecs[] = "avc1.640028";
    const char kWebMMime[] = "video/webm";
    const char kVp9Codecs[] = "vp9";

    auto audio_codecs = MakeCodecParameter(kAACCodecs);
    auto status = chunk_demuxer_->AddId(kAudioId, kAACMime, audio_codecs);
    CHECK_EQ(status, ChunkDemuxer::kOk);
    auto video_codecs = MakeCodecParameter(is_webm ? kVp9Codecs : kAVCCodecs);
    status = chunk_demuxer_->AddId(kVideoId, is_webm ? kWebMMime : kAVCMime,
                                   video_codecs);
    CHECK_EQ(status, ChunkDemuxer::kOk);
  }

  void AppendData(const std::string& id, ScopedFile* file, int64* offset) {
    const float kLowWaterMarkInSeconds = 5.f;
    const int64 kMaxBytesToAppend = 1024 * 1024;
    char buffer[kMaxBytesToAppend];

    while (*offset < file->GetSize()) {
      Ranges<TimeDelta> ranges = chunk_demuxer_->GetBufferedRanges(id);
      float end_of_buffer =
          ranges.size() == 0 ? 0.f : ranges.end(ranges.size() - 1).InSecondsF();
      if (end_of_buffer - player_->GetCurrentTime() > kLowWaterMarkInSeconds) {
        break;
      }
      int64 bytes_to_append =
          std::min(kMaxBytesToAppend, file->GetSize() - *offset);
      file->Read(buffer, bytes_to_append);
#if defined(COBALT_MEDIA_SOURCE_2016)
      base::TimeDelta timestamp_offset;
      chunk_demuxer_->AppendData(id, reinterpret_cast<uint8*>(buffer),
                                 bytes_to_append, base::TimeDelta(),
                                 media::kInfiniteDuration, &timestamp_offset);
#else   //  defined(COBALT_MEDIA_SOURCE_2016)
      chunk_demuxer_->AppendData(id, reinterpret_cast<uint8*>(buffer),
                                 bytes_to_append);
#endif  //  defined(COBALT_MEDIA_SOURCE_2016)

      *offset += bytes_to_append;
    }
  }

  scoped_refptr<Image> FrameCB(const base::TimeDelta& time) {
    UNREFERENCED_PARAMETER(time);

#if SB_HAS(GRAPHICS)
    SbDecodeTarget decode_target = player_helper_.GetCurrentDecodeTarget();

    if (SbDecodeTargetIsValid(decode_target)) {
      return media_sandbox_.resource_provider()->CreateImageFromSbDecodeTarget(
          decode_target);
    }

    scoped_refptr<VideoFrame> frame = player_helper_.GetCurrentFrame();
    return frame ? reinterpret_cast<Image*>(frame->texture_id()) : NULL;
#else   // SB_HAS(GRAPHICS)
    return NULL;
#endif  // SB_HAS(GRAPHICS)
  }

  const std::string kAudioId = "audio";
  const std::string kVideoId = "video";

  InitCobaltHelper init_cobalt_helper_;
  MediaSandbox media_sandbox_;
  WebMediaPlayerHelper player_helper_;
  ChunkDemuxer* chunk_demuxer_ = NULL;
  WebMediaPlayer* player_ = NULL;
  scoped_ptr<ScopedFile> audio_file_;
  scoped_ptr<ScopedFile> video_file_;
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
      if (data->argument_count != 3) {
        LOG(ERROR) << "Usage: " << data->argument_values[0]
                   << " <audio file path> "
                   << "<video file path>";
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

    default: { break; }
  }
}
