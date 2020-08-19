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

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/media/sandbox/fuzzer_app.h"
#include "cobalt/media/sandbox/in_memory_data_source.h"
#include "cobalt/media/sandbox/media_sandbox.h"
#include "media/base/bind_to_loop.h"
#include "media/base/pipeline_status.h"
#include "media/progressive/progressive_demuxer.h"

namespace cobalt {
namespace media {
namespace sandbox {
namespace {

using base::Bind;
using ::media::BindToCurrentLoop;
using ::media::DecoderBuffer;
using ::media::Demuxer;
using ::media::DemuxerHost;
using ::media::DemuxerStream;
using ::media::PipelineStatus;
using ::media::ProgressiveDemuxer;

class DemuxerFuzzer : DemuxerHost {
 public:
  explicit DemuxerFuzzer(const std::vector<uint8>& content)
      : error_occurred_(false), eos_count_(0), stopped_(false) {
    demuxer_ =
        new ProgressiveDemuxer(base::MessageLoop::current()->task_runner(),
                               new InMemoryDataSource(content));
  }

  void Fuzz() {
    demuxer_->Initialize(
        this, Bind(&DemuxerFuzzer::InitializeCB, base::Unretained(this)));

    // Check if there is any error or if both of the audio and video streams
    // have reached eos.
    while (!error_occurred_ && eos_count_ != 2) {
      base::RunLoop().RunUntilIdle();
    }

    demuxer_->Stop(Bind(&DemuxerFuzzer::StopCB, base::Unretained(this)));

    while (!stopped_) {
      base::RunLoop().RunUntilIdle();
    }
  }

 private:
  // DataSourceHost methods (parent class of DemuxerHost)
  void SetTotalBytes(int64 total_bytes) override {
  }

  void AddBufferedByteRange(int64 start, int64 end) override {
  }

  void AddBufferedTimeRange(base::TimeDelta start,
                            base::TimeDelta end) override {
  }

  // DemuxerHost methods
  void SetDuration(base::TimeDelta duration) override {
  }
  void OnDemuxerError(PipelineStatus error) override {
    error_occurred_ = true;
  }

  void InitializeCB(PipelineStatus status) {
    DCHECK(!error_occurred_);
    if (status != ::media::PIPELINE_OK) {
      error_occurred_ = true;
      return;
    }
    scoped_refptr<DemuxerStream> audio_stream =
        demuxer_->GetStream(DemuxerStream::AUDIO);
    scoped_refptr<DemuxerStream> video_stream =
        demuxer_->GetStream(DemuxerStream::VIDEO);
    if (!audio_stream || !video_stream ||
        !audio_stream->audio_decoder_config().IsValidConfig() ||
        !video_stream->video_decoder_config().IsValidConfig()) {
      error_occurred_ = true;
      return;
    }
    audio_stream->Read(BindToCurrentLoop(
        Bind(&DemuxerFuzzer::ReadCB, base::Unretained(this), audio_stream)));
    video_stream->Read(BindToCurrentLoop(
        Bind(&DemuxerFuzzer::ReadCB, base::Unretained(this), video_stream)));
  }

  void StopCB() { stopped_ = true; }

  void ReadCB(const scoped_refptr<DemuxerStream>& stream,
              DemuxerStream::Status status,
              const scoped_refptr<DecoderBuffer>& buffer) {
    DCHECK_NE(status, DemuxerStream::kAborted);
    if (status == DemuxerStream::kOk) {
      DCHECK(buffer);
      if (buffer->IsEndOfStream()) {
        ++eos_count_;
        return;
      }
    }
    DCHECK(!error_occurred_);
    stream->Read(BindToCurrentLoop(
        Bind(&DemuxerFuzzer::ReadCB, base::Unretained(this), stream)));
  }

  bool error_occurred_;
  int eos_count_;
  bool stopped_;
  scoped_refptr<ProgressiveDemuxer> demuxer_;
};

class DemuxerFuzzerApp : public FuzzerApp {
 public:
  explicit DemuxerFuzzerApp(MediaSandbox* media_sandbox)
      : media_sandbox_(media_sandbox) {}

  std::vector<uint8> ParseFileContent(
      const std::string& file_name,
      const std::vector<uint8>& file_content) override {
    std::string ext = base::FilePath(file_name).Extension();
    if (ext != ".flv" && ext != ".mp4") {
      LOG(ERROR) << "Skip unsupported file " << file_name;
      return std::vector<uint8>();
    }
    return file_content;
  }

  void Fuzz(const std::string& file_name,
            const std::vector<uint8>& fuzzing_content) override {
    DemuxerFuzzer demuxer_fuzzer(fuzzing_content);
    demuxer_fuzzer.Fuzz();
  }

 private:
  MediaSandbox* media_sandbox_;
};

int SandboxMain(int argc, char** argv) {
  MediaSandbox media_sandbox(
      argc, argv, base::FilePath(FILE_PATH_LITERAL("demuxer_fuzzer.json")));
  DemuxerFuzzerApp fuzzer_app(&media_sandbox);

  if (fuzzer_app.Init(argc, argv)) {
    fuzzer_app.RunFuzzingLoop();
  }

  return 0;
}

}  // namespace
}  // namespace sandbox
}  // namespace media
}  // namespace cobalt

COBALT_WRAP_SIMPLE_MAIN(cobalt::media::sandbox::SandboxMain);
