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
#include "starboard/file.h"

namespace cobalt {
namespace media {
namespace sandbox {
namespace {

#if defined(COBALT_MEDIA_SOURCE_2016)
typedef media::WebMediaPlayer::AddIdStatus AddIdStatus;
#else   // defined(COBALT_MEDIA_SOURCE_2016)
typedef ::media::WebMediaPlayer::AddIdStatus AddIdStatus;
using ::media::Ranges;
using ::media::VideoFrame;
using ::media::WebMediaPlayer;
#endif  // defined(WebMediaPlayerDelegate)

using base::TimeDelta;
using render_tree::Image;
using starboard::ScopedFile;

FilePath ResolvePath(const char* path) {
  FilePath result(path);
  if (!result.IsAbsolute()) {
    FilePath content_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &content_path);
    DCHECK(content_path.IsAbsolute());
    result = content_path.Append(result);
  }
  return result;
}

std::vector<std::string> MakeStringVector(const char* string) {
  std::vector<std::string> result;
  result.push_back(string);
  return result;
}

void AddSourceBuffers(const std::string audio_id, const std::string video_id,
                      bool is_webm, WebMediaPlayer* player) {
  const char kAACMime[] = "audio/mp4";
  const char kAACCodecs[] = "mp4a.40.2";
  const char kAVCMime[] = "video/mp4";
  const char kAVCCodecs[] = "avc1.640028";
  const char kWebMMime[] = "video/webm";
  const char kVp9Codecs[] = "vp9";

  AddIdStatus status =
      player->SourceAddId(audio_id, kAACMime, MakeStringVector(kAACCodecs));
  CHECK_EQ(status, WebMediaPlayer::kAddIdStatusOk);
  status =
      player->SourceAddId(video_id, is_webm ? kWebMMime : kAVCMime,
                          MakeStringVector(is_webm ? kVp9Codecs : kAVCCodecs));
  CHECK_EQ(status, WebMediaPlayer::kAddIdStatusOk);
}

bool IsWebM(const FilePath& path) {
  std::string filename = path.value();
  return filename.size() >= 5 &&
         filename.substr(filename.size() - 5) == ".webm";
}

void AppendData(WebMediaPlayer* player, const std::string& id, ScopedFile* file,
                int64* offset) {
  const float kLowWaterMarkInSeconds = 5.f;
  const int64 kMaxBytesToAppend = 1024 * 1024;
  char buffer[kMaxBytesToAppend];

  while (*offset < file->GetSize()) {
    Ranges<TimeDelta> ranges = player->SourceBuffered(id);
    float end_of_buffer =
        ranges.size() == 0 ? 0.f : ranges.end(ranges.size() - 1).InSecondsF();
    float media_time = player->GetCurrentTime();
    if (end_of_buffer - media_time > kLowWaterMarkInSeconds) {
      break;
    }
    int64 bytes_to_append =
        std::min(kMaxBytesToAppend, file->GetSize() - *offset);
    file->Read(buffer, bytes_to_append);
    player->SourceAppend(id, reinterpret_cast<uint8*>(buffer), bytes_to_append);
    *offset += bytes_to_append;
  }
}

scoped_refptr<Image> FrameCB(WebMediaPlayerHelper* player_helper,
                             const base::TimeDelta& time) {
  UNREFERENCED_PARAMETER(time);

  scoped_refptr<VideoFrame> frame = player_helper->GetCurrentFrame();
  return frame ? reinterpret_cast<Image*>(frame->texture_id()) : NULL;
}

int SandboxMain(int argc, char** argv) {
  if (argc != 3 && argc != 4) {
    LOG(ERROR) << "Usage: " << argv[0]
               << " [--null_audio_streamer] <audio file path> "
               << "<video file path>";
    return 1;
  }
  MediaSandbox media_sandbox(
      argc, argv,
      FilePath(FILE_PATH_LITERAL("web_media_player_sandbox_trace.json")));
  WebMediaPlayerHelper player_helper(media_sandbox.GetMediaModule());

  // Note that we can't access PathService until MediaSandbox is initialized.
  FilePath audio_path = ResolvePath(argv[argc - 2]);
  FilePath video_path = ResolvePath(argv[argc - 1]);

  ScopedFile audio_file(audio_path.value().c_str(),
                        kSbFileOpenOnly | kSbFileRead);
  ScopedFile video_file(video_path.value().c_str(),
                        kSbFileOpenOnly | kSbFileRead);

  if (!audio_file.IsValid()) {
    LOG(ERROR) << "Failed to open audio file: " << audio_path.value();
    return 1;
  }

  if (!video_file.IsValid()) {
    LOG(ERROR) << "Failed to open video file: " << video_path.value();
    return 1;
  }

  LOG(INFO) << "Playing " << audio_path.value() << " and "
            << video_path.value();

  WebMediaPlayer* player = player_helper.player();

  const std::string kAudioId = "audio";
  const std::string kVideoId = "video";

  AddSourceBuffers(kAudioId, kVideoId, IsWebM(video_path), player);

  int64 audio_offset = 0;
  int64 video_offset = 0;
  bool eos_appended = false;

  scoped_refptr<VideoFrame> last_frame;

  media_sandbox.RegisterFrameCB(
      base::Bind(FrameCB, base::Unretained(&player_helper)));

  for (;;) {
    AppendData(player, kAudioId, &audio_file, &audio_offset);
    AppendData(player, kVideoId, &video_file, &video_offset);
    if (!eos_appended && video_offset == video_file.GetSize()) {
      player->SourceAbort(kAudioId);
      player->SourceEndOfStream(WebMediaPlayer::kEndOfStreamStatusNoError);
      eos_appended = true;
    }

    if (player_helper.IsPlaybackFinished()) {
      break;
    }
    scoped_refptr<VideoFrame> frame = player_helper.GetCurrentFrame();
    if (frame && frame != last_frame) {
      LOG(INFO) << "showing frame " << frame->GetTimestamp().InMicroseconds();
      last_frame = frame;
    }
    MessageLoop::current()->RunUntilIdle();
  }

  last_frame = NULL;

  media_sandbox.RegisterFrameCB(MediaSandbox::FrameCB());
  LOG(INFO) << "Playback finished.";

  return 0;
}

}  // namespace
}  // namespace sandbox
}  // namespace media
}  // namespace cobalt

COBALT_WRAP_SIMPLE_MAIN(cobalt::media::sandbox::SandboxMain);
