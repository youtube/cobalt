// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/media/base/media_log.h"
#include "cobalt/media/decoder_buffer_allocator.h"
#include "cobalt/media/filters/chunk_demuxer.h"
#include "starboard/common/string.h"
#include "starboard/event.h"

namespace cobalt {
namespace media {
namespace sandbox {

namespace {

class DemuxerHostStub : public DemuxerHost {
  void OnBufferedTimeRangesChanged(
      const Ranges<base::TimeDelta>& ranges) override {}

  void SetDuration(base::TimeDelta duration) override {}

  void OnDemuxerError(PipelineStatus error) override {}

  void AddTextStream(DemuxerStream* text_stream,
                     const TextTrackConfig& config) override {}

  void RemoveTextStream(DemuxerStream* text_stream) override {}
};

void OnDemuxerOpen() {}

void OnEncryptedMediaInitData(EmeInitDataType type,
                              const std::vector<uint8_t>& init_data) {}

void OnInitSegmentReceived(std::unique_ptr<MediaTracks> tracks) {}

void OnDemuxerStatus(PipelineStatus status) { status = PIPELINE_OK; }

std::string LoadFile(const std::string& file_name) {
  base::FilePath file_path(file_name);
  if (!file_path.IsAbsolute()) {
    base::FilePath content_path;
    base::PathService::Get(base::DIR_TEST_DATA, &content_path);
    DCHECK(content_path.IsAbsolute());
    file_path = content_path.Append(file_path);
  }

  std::string content;
  if (!base::ReadFileToString(file_path, &content)) {
    LOG(ERROR) << "Failed to load file " << file_path.value();
    return "";
  }
  return content;
}

const char* GetDemuxerStreamType(DemuxerStream* demuxer_stream) {
  return demuxer_stream->type() == DemuxerStream::AUDIO ? "audio" : "video";
}

void ReadDemuxerStream(DemuxerStream* demuxer_stream);

void OnDemuxerStreamRead(DemuxerStream* demuxer_stream,
                         DemuxerStream::Status status,
                         const scoped_refptr<DecoderBuffer>& decoder_buffer) {
  if (!decoder_buffer->end_of_stream()) {
    LOG(INFO) << "Reading " << GetDemuxerStreamType(demuxer_stream)
              << " buffer at " << decoder_buffer->timestamp();
    ReadDemuxerStream(demuxer_stream);
  } else {
    LOG(INFO) << "Received " << GetDemuxerStreamType(demuxer_stream) << " EOS";
  }
}

void ReadDemuxerStream(DemuxerStream* demuxer_stream) {
  DCHECK(demuxer_stream);
  demuxer_stream->Read(
      base::Bind(OnDemuxerStreamRead, base::Unretained(demuxer_stream)));
}

}  // namespace

int SandboxMain(int argc, char** argv) {
  if (argc != 3) {
    // Path should be in the form of
    //     "cobalt/demos/media-element-demo/dash-video-240p.mp4".
    LOG(ERROR) << "Usage: " << argv[0] << " <audio_path> <video_path>";
    return 1;
  }

  DecoderBufferAllocator decoder_buffer_allocator;
  base::MessageLoop message_loop;
  // A one-per-process task scheduler is needed for usage of APIs in
  // base/post_task.h which will be used by some net APIs like
  // URLRequestContext;
  base::TaskScheduler::CreateAndStartWithDefaultParams("Cobalt TaskScheduler");
  DemuxerHostStub demuxer_host;
  std::unique_ptr<ChunkDemuxer> demuxer(new ChunkDemuxer(
      &decoder_buffer_allocator, base::Bind(OnDemuxerOpen),
      base::Bind(OnEncryptedMediaInitData), new MediaLog, false));
  demuxer->Initialize(&demuxer_host, base::Bind(OnDemuxerStatus), false);

  ChunkDemuxer::Status status =
      demuxer->AddId("audio", "audio/mp4; codecs=\"mp4a.40.2\"");
  DCHECK_EQ(status, ChunkDemuxer::kOk);

  int video_url_length = strlen(argv[2]);
  if (video_url_length > 5 &&
      strncmp(argv[2] + video_url_length - 5, ".webm", 5) == 0) {
    status = demuxer->AddId("video", "video/webm; codecs=\"vp9\"");
  } else {
    status = demuxer->AddId("video", "video/mp4; codecs=\"avc1.640028\"");
  }
  DCHECK_EQ(status, ChunkDemuxer::kOk);

  base::TimeDelta timestamp_offset;

  std::string audio_content = LoadFile(argv[1]);
  std::string video_content = LoadFile(argv[2]);
  DCHECK(!audio_content.empty());
  DCHECK(!video_content.empty());

  demuxer->SetTracksWatcher("audio", base::Bind(OnInitSegmentReceived));
  demuxer->SetTracksWatcher("video", base::Bind(OnInitSegmentReceived));
  demuxer->AppendData("audio", reinterpret_cast<uint8*>(&audio_content[0]),
                      audio_content.size(), base::TimeDelta(),
                      base::TimeDelta::Max(), &timestamp_offset);
  demuxer->AppendData("video", reinterpret_cast<uint8*>(&video_content[0]),
                      video_content.size(), base::TimeDelta(),
                      base::TimeDelta::Max(), &timestamp_offset);
  demuxer->MarkEndOfStream(PIPELINE_OK);

  DemuxerStream* audio_stream = demuxer->GetStream(DemuxerStream::AUDIO);
  DemuxerStream* video_stream = demuxer->GetStream(DemuxerStream::VIDEO);

  ReadDemuxerStream(audio_stream);
  ReadDemuxerStream(video_stream);

  base::RunLoop().RunUntilIdle();

  demuxer->Stop();

  return 0;
}

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt

COBALT_WRAP_SIMPLE_MAIN(cobalt::media::sandbox::SandboxMain);
