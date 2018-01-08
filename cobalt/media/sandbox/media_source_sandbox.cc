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
#include "net/base/net_util.h"
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

FilePath ResolvePath(const char* path) {
  FilePath result(path);
  if (!result.IsAbsolute()) {
    FilePath content_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &content_path);
    CHECK(content_path.IsAbsolute());
    result = content_path.Append(result);
  }
  if (SbFileCanOpen(result.value().c_str(), kSbFileOpenOnly | kSbFileRead)) {
    return result;
  }
  return FilePath();
}

GURL ResolveUrl(const char* url) {
  GURL gurl(url);
  if (gurl.is_valid()) {
    return gurl;
  }

  // Assume the input is a path.  Try to figure out the path to this file and
  // convert it to a URL.
  FilePath path = ResolvePath(url);
  if (path.empty()) {
    return GURL();
  }

  return net::FilePathToFileURL(path);
}

void PrintUsage(const char* executable_path_name) {
  std::string executable_file_name =
      FilePath(executable_path_name).BaseName().value();
  const char kExampleAdaptiveAudioPathName[] =
      "cobalt/browser/testdata/media-element-demo/dash-audio.mp4";
  const char kExampleAdaptiveVideoPathName[] =
      "cobalt/browser/testdata/media-element-demo/dash-video-1080p.mp4";
  const char kExampleProgressiveUrl[] =
      "https://storage.googleapis.com/yt-cobalt-media-element-demo/"
      "progressive.mp4";
  LOG(ERROR) << "\n\n\n"  // Extra empty lines to separate from other messages
             << "Usage: " << executable_file_name
             << " [OPTIONS] <adaptive audio file path> "
             << " <adaptive video file path>\n"
             << "   or: " << executable_file_name
             << " [OPTIONS] <progressive video path or url>\n"
             << "Play adaptive video or progressive video\n\n"
             << "For example:\n"
             << executable_file_name << " " << kExampleAdaptiveAudioPathName
             << " " << kExampleAdaptiveVideoPathName << "\n"
             << executable_file_name << " " << kExampleProgressiveUrl << "\n\n";
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
            FilePath(FILE_PATH_LITERAL("media_source_sandbox_trace.json"))) {
    if (argc > 2) {
      FilePath audio_path = ResolvePath(argv[argc - 2]);
      FilePath video_path = ResolvePath(argv[argc - 1]);
      if (!audio_path.empty() && !video_path.empty()) {
        is_adaptive_playback_ = true;
        InitializeAdaptivePlayback(audio_path.value(), video_path.value());
        return;
      }
    }

    if (argc > 1) {
      GURL video_url = ResolveUrl(argv[argc - 1]);
      if (video_url.is_valid()) {
        is_adaptive_playback_ = false;
        InitializeProgressivePlayback(video_url);
        return;
      }
    }

    PrintUsage(argv[0]);
    SbSystemRequestStop(0);
  }

 private:
  void InitializeAdaptivePlayback(const std::string& audio_path,
                                  const std::string& video_path) {
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

    player_helper_.reset(
        new WebMediaPlayerHelper(media_sandbox_.GetMediaModule(),
                                 base::Bind(&Application::OnChunkDemuxerOpened,
                                            base::Unretained(this))));

    // |chunk_demuxer_| will be set inside OnChunkDemuxerOpened()
    // asynchronously during initialization of |player_helper_|.  Wait until
    // it is set before proceed.
    while (!chunk_demuxer_) {
      MessageLoop::current()->RunUntilIdle();
    }

    LOG(INFO) << "Playing " << audio_path << " and " << video_path;

    AddSourceBuffers(IsWebM(audio_path), IsWebM(video_path));

#if defined(COBALT_MEDIA_SOURCE_2016)
    chunk_demuxer_->SetTracksWatcher(kAudioId,
                                     base::Bind(OnInitSegmentReceived));
    chunk_demuxer_->SetTracksWatcher(kVideoId,
                                     base::Bind(OnInitSegmentReceived));
#endif  // defined(COBALT_MEDIA_SOURCE_2016)
    player_ = player_helper_->player();

    media_sandbox_.RegisterFrameCB(
        base::Bind(&Application::FrameCB, base::Unretained(this)));

    timer_event_id_ =
        SbEventSchedule(Application::OnTimer, this, kSbTimeSecond / 10);
  }

  void InitializeProgressivePlayback(const GURL& video_url) {
    LOG(INFO) << "Playing " << video_url;

    player_helper_.reset(new WebMediaPlayerHelper(
        media_sandbox_.GetMediaModule(), media_sandbox_.GetFetcherFactory(),
        video_url));
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

  void AddSourceBuffers(bool is_audio_webm, bool is_video_webm) {
    const char kAACMime[] = "audio/mp4";
    const char kAACCodecs[] = "mp4a.40.2";
    const char kAVCMime[] = "video/mp4";
    const char kAVCCodecs[] = "avc1.640028";

    const char kOpusMime[] = "audio/webm";
    const char kOpusCodecs[] = "opus";
    const char kVp9Mime[] = "video/webm";
    const char kVp9Codecs[] = "vp9";

    auto audio_codecs =
        MakeCodecParameter(is_audio_webm ? kOpusCodecs : kAACCodecs);
    auto status = chunk_demuxer_->AddId(
        kAudioId, is_audio_webm ? kOpusMime : kAACMime, audio_codecs);
    CHECK_EQ(status, ChunkDemuxer::kOk);

    auto video_codecs =
        MakeCodecParameter(is_video_webm ? kVp9Codecs : kAVCCodecs);
    status = chunk_demuxer_->AddId(
        kVideoId, is_video_webm ? kVp9Mime : kAVCMime, video_codecs);
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
    SbDecodeTarget decode_target = player_helper_->GetCurrentDecodeTarget();

    if (SbDecodeTargetIsValid(decode_target)) {
      return media_sandbox_.resource_provider()->CreateImageFromSbDecodeTarget(
          decode_target);
    }

    scoped_refptr<VideoFrame> frame = player_helper_->GetCurrentFrame();
    return frame ? reinterpret_cast<Image*>(frame->texture_id()) : NULL;
#else   // SB_HAS(GRAPHICS)
    return NULL;
#endif  // SB_HAS(GRAPHICS)
  }

  const std::string kAudioId = "audio";
  const std::string kVideoId = "video";

  bool is_adaptive_playback_;
  InitCobaltHelper init_cobalt_helper_;
  MediaSandbox media_sandbox_;
  scoped_ptr<WebMediaPlayerHelper> player_helper_;
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
