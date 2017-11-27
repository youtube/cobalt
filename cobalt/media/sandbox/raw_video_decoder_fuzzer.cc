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

#include <map>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/media/sandbox/fuzzer_app.h"
#include "cobalt/media/sandbox/media_sandbox.h"
#include "cobalt/media/sandbox/media_source_demuxer.h"
#include "cobalt/media/sandbox/zzuf_fuzzer.h"
#include "media/base/bind_to_loop.h"
#include "starboard/memory.h"

namespace cobalt {
namespace media {
namespace sandbox {
namespace {

using base::Time;
using ::media::BindToCurrentLoop;
using ::media::DecoderBuffer;
using ::media::ShellRawVideoDecoder;
using ::media::VideoFrame;

class VideoDecoderFuzzer {
 public:
  VideoDecoderFuzzer(const std::vector<uint8_t>& au_data,
                     MediaSourceDemuxer* demuxer, ShellRawVideoDecoder* decoder)
      : au_data_(au_data),
        demuxer_(demuxer),
        decoder_(decoder),
        au_index_(0),
        error_occured_(false),
        eos_decoded_(false) {}

  void Fuzz() {
    UpdateCurrentAUBuffer();
    decoder_->Decode(current_au_buffer_, BindToCurrentLoop(base::Bind(
                                             &VideoDecoderFuzzer::FrameDecoded,
                                             base::Unretained(this))));
    MessageLoop::current()->RunUntilIdle();
    DCHECK(IsEnded());
  }

 private:
  void UpdateCurrentAUBuffer() {
    if (au_index_ < demuxer_->GetFrameCount()) {
      MediaSourceDemuxer::AUDescriptor desc = demuxer_->GetFrame(au_index_);
      current_au_buffer_ =
          ::media::ShellBufferFactory::Instance()->AllocateBufferNow(
              desc.size, desc.is_keyframe);
      SbMemoryCopy(current_au_buffer_->GetWritableData(),
                   &au_data_[0] + desc.offset, desc.size);
      ++au_index_;
    } else if (!current_au_buffer_->IsEndOfStream()) {
      current_au_buffer_ =
          DecoderBuffer::CreateEOSBuffer(::media::kNoTimestamp());
    }
  }
  void FrameDecoded(ShellRawVideoDecoder::DecodeStatus status,
                    const scoped_refptr<VideoFrame>& frame) {
    if (frame) {
      last_frame_decoded_time_ = Time::Now();
      if (frame->IsEndOfStream()) {
        eos_decoded_ = true;
      }
    }
    switch (status) {
      case ShellRawVideoDecoder::FRAME_DECODED:
      case ShellRawVideoDecoder::NEED_MORE_DATA:
        UpdateCurrentAUBuffer();
        break;
      case ShellRawVideoDecoder::FATAL_ERROR:
        error_occured_ = true;
        // Even if there is a fatal error, we still want to keep sending the
        // rest buffers to decoder.
        UpdateCurrentAUBuffer();
        break;
      case ShellRawVideoDecoder::RETRY_WITH_SAME_BUFFER:
        if (current_au_buffer_->IsEndOfStream() &&
            (Time::Now() - last_frame_decoded_time_).InMilliseconds() > 500) {
          error_occured_ = true;
        }
        break;
    }
    if (!IsEnded()) {
      decoder_->Decode(
          current_au_buffer_,
          BindToCurrentLoop(base::Bind(&VideoDecoderFuzzer::FrameDecoded,
                                       base::Unretained(this))));
    }
  }
  bool IsEnded() const {
    if (error_occured_) return true;
    return eos_decoded_ ||
           (error_occured_ && current_au_buffer_->IsEndOfStream());
  }

  const std::vector<uint8_t>& au_data_;
  MediaSourceDemuxer* demuxer_;
  ShellRawVideoDecoder* decoder_;
  size_t au_index_;
  scoped_refptr<DecoderBuffer> current_au_buffer_;
  bool error_occured_;
  bool eos_decoded_;
  Time last_frame_decoded_time_;
};

int CalculateCheckSum(const std::vector<uint8>& data) {
  int checksum = 0;
  for (size_t i = 0; i < data.size(); ++i) {
    checksum += data[i];
  }
  return checksum;
}

// This function replace the original data inside the original file with the
// fuzzed data to created a valid container with fuzzed AUs.  |filename| should
// contain a file that inside a path readable by the host.
// The following statement can be used inside RawVideoDecoderFuzzerApp::Fuzz()
// to save the fuzzing content back into its original container format.
//   DumpFuzzedData(filename, GetFileContent(file_name), *demuxers_[file_name],
//                  fuzzing_content);
void DumpFuzzedData(const std::string& filename, std::vector<uint8> container,
                    const MediaSourceDemuxer& demuxer,
                    const std::vector<uint8>& fuzzing_content) {
  std::vector<uint8>::iterator last_found = container.begin();
  for (size_t i = 0; i < demuxer.GetFrameCount(); ++i) {
    MediaSourceDemuxer::AUDescriptor desc = demuxer.GetFrame(i);
    std::vector<uint8>::const_iterator begin =
        demuxer.au_data().begin() + desc.offset;
    std::vector<uint8>::const_iterator end = begin + desc.size;
    std::vector<uint8>::iterator offset =
        std::search(last_found, container.end(), begin, end);
    std::copy(fuzzing_content.begin() + desc.offset,
              fuzzing_content.begin() + desc.offset + desc.size, offset);
    last_found = offset + desc.size + 1;
  }
  file_util::WriteFile(FilePath(filename),
                       reinterpret_cast<const char*>(&container[0]),
                       container.size());
}

class RawVideoDecoderFuzzerApp : public FuzzerApp {
 public:
  explicit RawVideoDecoderFuzzerApp(MediaSandbox* media_sandbox)
      : media_sandbox_(media_sandbox) {}
  ~RawVideoDecoderFuzzerApp() {
    while (!demuxers_.empty()) {
      delete demuxers_.begin()->second;
      demuxers_.erase(demuxers_.begin());
    }
  }

  std::vector<uint8> ParseFileContent(
      const std::string& file_name,
      const std::vector<uint8>& file_content) override {
    std::string ext = FilePath(file_name).Extension();
    if (ext != ".webm" && ext != ".mp4" && ext != ".ivf") {
      LOG(ERROR) << "Skip unsupported file " << file_name;
      return std::vector<uint8>();
    }

    scoped_ptr<MediaSourceDemuxer> demuxer(new MediaSourceDemuxer(
        std::vector<uint8>(file_content.begin(), file_content.end())));
    if (demuxer->valid() && demuxer->GetFrameCount() > 0) {
      demuxers_[file_name] = demuxer.release();
      return demuxers_[file_name]->au_data();
    }
    LOG(ERROR) << "Failed to demux video: " << file_name;
    return std::vector<uint8>();
  }

  void Fuzz(const std::string& file_name,
            const std::vector<uint8>& fuzzing_content) override {
    DCHECK(demuxers_.find(file_name) != demuxers_.end());
    MediaSourceDemuxer* demuxer = demuxers_[file_name];
    scoped_ptr<ShellRawVideoDecoder> decoder =
        media_sandbox_->GetMediaModule()->GetRawVideoDecoderFactory()->Create(
            demuxer->config(), NULL, false);
    if (decoder) {
      VideoDecoderFuzzer decoder_fuzzer(fuzzing_content, demuxer,
                                        decoder.get());
      decoder_fuzzer.Fuzz();
    }
  }

 private:
  MediaSandbox* media_sandbox_;
  std::map<std::string, MediaSourceDemuxer*> demuxers_;
};

int SandboxMain(int argc, char** argv) {
  MediaSandbox media_sandbox(
      argc, argv, FilePath(FILE_PATH_LITERAL("raw_video_decoder_fuzzer.json")));
  RawVideoDecoderFuzzerApp fuzzer_app(&media_sandbox);

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
