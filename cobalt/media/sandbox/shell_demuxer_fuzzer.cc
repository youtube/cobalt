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

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/media/sandbox/in_memory_data_source.h"
#include "cobalt/media/sandbox/media_sandbox.h"
#include "cobalt/media/sandbox/zzuf_fuzzer.h"
#include "media/base/bind_to_loop.h"
#include "media/base/pipeline_status.h"
#include "media/filters/shell_demuxer.h"
#include "starboard/directory.h"

namespace cobalt {
namespace media {
namespace sandbox {
namespace {

using base::Bind;
using file_util::ReadFileToString;
using ::media::BindToCurrentLoop;
using ::media::DecoderBuffer;
using ::media::Demuxer;
using ::media::DemuxerHost;
using ::media::DemuxerStream;
using ::media::PipelineStatus;
using ::media::ShellDemuxer;

class ShellDemuxerFuzzer : DemuxerHost {
 public:
  explicit ShellDemuxerFuzzer(const std::vector<uint8>& content)
      : error_occurred_(false), eos_count_(0), stopped_(false) {
    demuxer_ = new ShellDemuxer(base::MessageLoopProxy::current(),
                                new InMemoryDataSource(content));
  }

  void Fuzz() {
    demuxer_->Initialize(
        this, Bind(&ShellDemuxerFuzzer::InitializeCB, base::Unretained(this)));

    // Check if there is any error or if both of the audio and video streams
    // have reached eos.
    while (!error_occurred_ && eos_count_ != 2) {
      MessageLoop::current()->RunUntilIdle();
    }

    demuxer_->Stop(Bind(&ShellDemuxerFuzzer::StopCB, base::Unretained(this)));

    while (!stopped_) {
      MessageLoop::current()->RunUntilIdle();
    }
  }

 private:
  // DataSourceHost methods (parent class of DemuxerHost)
  void SetTotalBytes(int64 total_bytes) OVERRIDE {
    UNREFERENCED_PARAMETER(total_bytes);
  }

  void AddBufferedByteRange(int64 start, int64 end) OVERRIDE {
    UNREFERENCED_PARAMETER(start);
    UNREFERENCED_PARAMETER(end);
  }

  void AddBufferedTimeRange(base::TimeDelta start,
                            base::TimeDelta end) OVERRIDE {
    UNREFERENCED_PARAMETER(start);
    UNREFERENCED_PARAMETER(end);
  }

  // DemuxerHost methods
  void SetDuration(base::TimeDelta duration) OVERRIDE {
    UNREFERENCED_PARAMETER(duration);
  }
  void OnDemuxerError(PipelineStatus error) OVERRIDE {
    UNREFERENCED_PARAMETER(error);
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
    audio_stream->Read(BindToCurrentLoop(Bind(
        &ShellDemuxerFuzzer::ReadCB, base::Unretained(this), audio_stream)));
    video_stream->Read(BindToCurrentLoop(Bind(
        &ShellDemuxerFuzzer::ReadCB, base::Unretained(this), video_stream)));
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
        Bind(&ShellDemuxerFuzzer::ReadCB, base::Unretained(this), stream)));
  }

  bool error_occurred_;
  int eos_count_;
  bool stopped_;
  scoped_refptr<ShellDemuxer> demuxer_;
};

bool IsVideoFile(const std::string& name) {
  if (name.size() < 4) {
    return false;
  }
  if (SbStringCompareNoCase(name.c_str() + name.size() - 4, ".flv") == 0) {
    return true;
  }
  if (SbStringCompareNoCase(name.c_str() + name.size() - 4, ".mp4") == 0) {
    return true;
  }
  return false;
}

// This function returns a vector of video files each with its full path name.
std::vector<std::string> CollectVideoFiles(const std::string& pathname) {
  std::vector<std::string> result;

  SbDirectory directory = SbDirectoryOpen(pathname.c_str(), NULL);
  if (!SbDirectoryIsValid(directory)) {
    // Assuming it is a file.
    if (IsVideoFile(pathname)) {
      result.push_back(pathname);
    }
    return result;
  }

  SbDirectoryEntry entry;
  while (SbDirectoryGetNext(directory, &entry)) {
    if (IsVideoFile(entry.name)) {
      result.push_back(pathname + SB_FILE_SEP_STRING + entry.name);
    }
  }
  SbDirectoryClose(directory);
  return result;
}

// Create MediaSourceDemuxer and ZzufFuzzer for every file in |filenames| and
// put them into |demuxers| and |fuzzers|.
// If failed to create demuxer and fuzzer for a particular filename, it will be
// removed from |filenames| so the size of |filenames|, |demuxers| and |fuzzers|
// will be the same after the function returns.
void GatherTestData(std::vector<std::string>* filenames,
                    ScopedVector<ZzufFuzzer>* fuzzers) {
  const float kMinFuzzRatio = 0.001f;
  const float kMaxFuzzRatio = 0.05f;

  std::vector<std::string>::iterator iter = filenames->begin();
  while (iter != filenames->end()) {
    LOG(INFO) << "Loading " << *iter;
    std::string content;
    if (ReadFileToString(FilePath(*iter), &content) && !content.empty()) {
      std::vector<uint8> vector_content(content.begin(), content.end());
      fuzzers->push_back(
          new ZzufFuzzer(vector_content, kMinFuzzRatio, kMaxFuzzRatio));
      ++iter;
    } else {
      LOG(ERROR) << "Failed to load video: " << *iter;
      iter = filenames->erase(iter);
    }
  }
}

int SandboxMain(int argc, char** argv) {
  if (argc != 3) {
    LOG(ERROR) << "Usage: " << argv[0] << " <number of iterations> "
               << "<video file name|directory name contains video files>";
    LOG(ERROR) << "For example: " << argv[0] << " 200000 /data/video-files";
    return 1;
  }

  long iterations = SbStringParseSignedInteger(argv[1], NULL, 10);

  if (iterations <= 0) {
    LOG(ERROR) << "Invalid 'number of iterations' " << argv[1];
  }

  MediaSandbox media_sandbox(
      argc, argv, FilePath(FILE_PATH_LITERAL("shell_demuxer_fuzzer.json")));

  // Note that we can't access PathService until MediaSandbox is initialized.
  std::vector<std::string> filenames = CollectVideoFiles(argv[2]);

  ScopedVector<ZzufFuzzer> data_fuzzers;

  GatherTestData(&filenames, &data_fuzzers);

  DCHECK_EQ(filenames.size(), data_fuzzers.size());

  if (data_fuzzers.empty()) {
    LOG(ERROR) << "No files to fuzz";
    return 1;
  }

  for (long i = 0; i < iterations; ++i) {
    for (size_t file_index = 0; file_index < data_fuzzers.size();
         ++file_index) {
      LOG(INFO) << "Fuzzing \"" << filenames[file_index] << "\" with seed "
                << data_fuzzers[file_index]->seed();
      ShellDemuxerFuzzer demuxer_fuzzer(
          data_fuzzers[file_index]->GetFuzzedContent());
      demuxer_fuzzer.Fuzz();
      data_fuzzers[file_index]->AdvanceSeed();
    }
  }

  return 0;
}

}  // namespace
}  // namespace sandbox
}  // namespace media
}  // namespace cobalt

COBALT_WRAP_SIMPLE_MAIN(cobalt::media::sandbox::SandboxMain);
