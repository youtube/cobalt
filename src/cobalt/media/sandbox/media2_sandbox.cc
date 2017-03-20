// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "base/bind.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/media/base/media_log.h"
#include "cobalt/media/filters/chunk_demuxer.h"
#include "starboard/event.h"

namespace cobalt {
namespace media {
namespace sandbox {

using ::media::ChunkDemuxer;
using ::media::DecoderBuffer;
using ::media::DemuxerStream;
using ::media::MediaTracks;

namespace {

class DemuxerHostStub : public ::media::DemuxerHost {
  void OnBufferedTimeRangesChanged(
      const ::media::Ranges<base::TimeDelta>& ranges) OVERRIDE {}

  void SetDuration(base::TimeDelta duration) OVERRIDE {}

  void OnDemuxerError(::media::PipelineStatus error) OVERRIDE {}

  void AddTextStream(::media::DemuxerStream* text_stream,
                     const ::media::TextTrackConfig& config) OVERRIDE {}

  void RemoveTextStream(::media::DemuxerStream* text_stream) OVERRIDE {}
};

void OnDemuxerOpen() {}

void OnEncryptedMediaInitData(::media::EmeInitDataType type,
                              const std::vector<uint8_t>& init_data) {}

void OnInitSegmentReceived(scoped_ptr<MediaTracks> tracks) {}

void OnDemuxerStatus(::media::PipelineStatus status) {
  status = ::media::PIPELINE_OK;
}

std::string LoadFile(const std::string& file_name) {
  FilePath file_path(file_name);
  if (!file_path.IsAbsolute()) {
    FilePath content_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &content_path);
    DCHECK(content_path.IsAbsolute());
    file_path = content_path.Append(file_path);
  }

  std::string content;
  if (!file_util::ReadFileToString(file_path, &content)) {
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
    //     "cobalt/browser/testdata/media-element-demo/dash-video-240p.mp4".
    LOG(ERROR) << "Usage: " << argv[0] << " <audio_path> <video_path>";
    return 1;
  }

  MessageLoop message_loop;

  DemuxerHostStub demuxer_host;
  scoped_ptr<ChunkDemuxer> demuxer(new ChunkDemuxer(
      base::Bind(OnDemuxerOpen), base::Bind(OnEncryptedMediaInitData),
      new ::media::MediaLog, false));
  demuxer->Initialize(&demuxer_host, base::Bind(OnDemuxerStatus), false);

  ChunkDemuxer::Status status =
      demuxer->AddId("audio", "audio/mp4", "mp4a.40.2");
  DCHECK_EQ(status, ChunkDemuxer::kOk);
  status = demuxer->AddId("video", "video/mp4", "avc1.640028");
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
  demuxer->MarkEndOfStream(::media::PIPELINE_OK);

  DemuxerStream* audio_stream = demuxer->GetStream(DemuxerStream::AUDIO);
  DemuxerStream* video_stream = demuxer->GetStream(DemuxerStream::VIDEO);

  ReadDemuxerStream(audio_stream);
  ReadDemuxerStream(video_stream);

  message_loop.RunUntilIdle();

  demuxer->Stop();

  return 0;
}

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt

COBALT_WRAP_SIMPLE_MAIN(cobalt::media::sandbox::SandboxMain);
