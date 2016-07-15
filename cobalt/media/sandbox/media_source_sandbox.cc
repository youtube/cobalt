/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/threading/platform_thread.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/media/sandbox/media_sandbox.h"
#include "cobalt/media/sandbox/web_media_player_helper.h"
#include "media/base/video_frame.h"
#include "net/base/net_util.h"

namespace cobalt {
namespace media {
namespace sandbox {
namespace {

typedef ::media::WebMediaPlayer::AddIdStatus AddIdStatus;
using base::TimeDelta;
using ::media::VideoFrame;
using ::media::WebMediaPlayer;

GURL ResolveUrl(const char* arg) {
  GURL video_url(arg);
  if (!video_url.is_valid()) {
    // Assume the input is a path.
    // Try to figure out the path to this file and convert it to a URL.
    FilePath result(arg);
    if (!result.IsAbsolute()) {
      FilePath content_path;
      PathService::Get(base::DIR_EXE, &content_path);
      DCHECK(content_path.IsAbsolute());
      // TODO: Get the "real" exe path. b/27257060
      result = content_path.DirName().DirName().Append(result);
    }
    video_url = net::FilePathToFileURL(result);
  }
  return video_url;
}

// Note that this class loads the content of the url into memory, so it won't
// work with large media files.
class Loader : loader::Fetcher::Handler {
 public:
  Loader(const GURL& url, loader::FetcherFactory* fetcher_factory)
      : done_(false), error_(false) {
    fetcher_ = fetcher_factory->CreateFetcher(url, this);
  }

  bool done() const { return done_; }
  bool error() const { return error_; }
  const std::vector<uint8_t>& buffer() const { return buffer_; }

 private:
  void OnReceived(loader::Fetcher* fetcher, const char* data,
                  size_t size) OVERRIDE {
    buffer_.insert(buffer_.end(), reinterpret_cast<const uint8_t*>(data),
                   reinterpret_cast<const uint8_t*>(data) + size);
  }
  void OnDone(loader::Fetcher* fetcher) OVERRIDE { done_ = true; }
  void OnError(loader::Fetcher* fetcher, const std::string& error) OVERRIDE {
    buffer_.clear();
    error_ = true;
  }

  scoped_ptr<loader::Fetcher> fetcher_;
  std::vector<uint8_t> buffer_;
  bool done_;
  bool error_;
};

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

bool IsWebMURL(const GURL& url) {
  std::string filename = url.ExtractFileName();
  return filename.size() >= 5 &&
         filename.substr(filename.size() - 5) == ".webm";
}

void AppendData(const std::string& id, const std::vector<uint8_t>& data,
                size_t* offset, WebMediaPlayer* player) {
  const float kLowWaterMarkInSeconds = 5.f;
  const size_t kMaxBytesToAppend = 1024 * 1024;

  ::media::Ranges<TimeDelta> ranges = player->SourceBuffered(id);
  while (*offset < data.size()) {
    float end_of_buffer =
        ranges.size() == 0 ? 0.f : ranges.end(ranges.size() - 1).InSecondsF();
    float media_time = player->GetCurrentTime();
    if (end_of_buffer - media_time > kLowWaterMarkInSeconds) {
      break;
    }
    size_t bytes_to_append = std::min(kMaxBytesToAppend, data.size() - *offset);
    player->SourceAppend(id, &data[0] + *offset, bytes_to_append);
    *offset += bytes_to_append;
  }
}

int SandboxMain(int argc, char** argv) {
  if (argc != 3 && argc != 4) {
    LOG(ERROR) << "Usage: " << argv[0]
               << " [--null_audio_streamer] <audio url|path> <video url|path>";
    return 1;
  }
  MediaSandbox media_sandbox(
      argc, argv,
      FilePath(FILE_PATH_LITERAL("web_media_player_sandbox_trace.json")));
  WebMediaPlayerHelper player_helper(media_sandbox.GetMediaModule());

  // Note that we can't access PathService until MediaSandbox is initialized.
  GURL audio_url = ResolveUrl(argv[argc - 2]);
  GURL video_url = ResolveUrl(argv[argc - 1]);

  if (!audio_url.is_valid()) {
    LOG(ERROR) << "Invalid Audio URL: " << audio_url;
    return 1;
  }

  if (!video_url.is_valid()) {
    LOG(ERROR) << "Invalid Video URL: " << video_url;
    return 1;
  }

  LOG(INFO) << "Loading " << audio_url << " and " << video_url;

  Loader audio_loader(audio_url, media_sandbox.GetFetcherFactory());
  Loader video_loader(video_url, media_sandbox.GetFetcherFactory());

  bool audio_finished = false;
  bool video_finished = false;
  while (!audio_finished || !video_finished) {
    MessageLoop::current()->RunUntilIdle();
    audio_finished = audio_loader.done() || audio_loader.error();
    video_finished = video_loader.done() || video_loader.error();
  }

  if (audio_loader.error()) {
    LOG(ERROR) << "Failed to load audio: " << audio_url;
    return 1;
  }

  if (video_loader.error()) {
    LOG(ERROR) << "Failed to load video: " << video_url;
    return 1;
  }

  WebMediaPlayer* player = player_helper.player();

  const std::string kAudioId = "audio";
  const std::string kVideoId = "video";

  AddSourceBuffers(kAudioId, kVideoId, IsWebMURL(video_url), player);

  size_t audio_offset = 0;
  size_t video_offset = 0;
  bool eos_appended = false;

  scoped_refptr<VideoFrame> last_frame;

  for (;;) {
    AppendData(kAudioId, audio_loader.buffer(), &audio_offset, player);
    AppendData(kVideoId, video_loader.buffer(), &video_offset, player);
    if (!eos_appended && audio_offset == audio_loader.buffer().size() &&
        video_offset == video_loader.buffer().size()) {
      player->SourceEndOfStream(WebMediaPlayer::kEndOfStreamStatusNoError);
      eos_appended = true;
    }

    if (player_helper.IsPlaybackFinished()) {
      break;
    }
    scoped_refptr<VideoFrame> frame = player_helper.GetCurrentFrame();
    if (frame != last_frame) {
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
