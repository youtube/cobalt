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

#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/time.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/media/sandbox/media_sandbox.h"
#include "cobalt/media/sandbox/media_source_demuxer.h"
#include "cobalt/media/sandbox/zzuf_fuzzer.h"
#include "media/base/bind_to_loop.h"
#include "starboard/directory.h"

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
          ::media::ShellBufferFactory::Instance()->AllocateBufferNow(desc.size);
      memcpy(current_au_buffer_->GetWritableData(), &au_data_[0] + desc.offset,
             desc.size);
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
void DumpFuzzedData(const std::string& filename, std::vector<uint8> container,
                    const MediaSourceDemuxer& demuxer,
                    const ZzufFuzzer& fuzzer) {
  std::vector<uint8>::iterator last_found = container.begin();
  for (size_t i = 0; i < demuxer.GetFrameCount(); ++i) {
    MediaSourceDemuxer::AUDescriptor desc = demuxer.GetFrame(i);
    std::vector<uint8>::const_iterator begin =
        demuxer.au_data().begin() + desc.offset;
    std::vector<uint8>::const_iterator end = begin + desc.size;
    std::vector<uint8>::iterator offset =
        std::search(last_found, container.end(), begin, end);
    std::copy(fuzzer.GetFuzzedContent().begin() + desc.offset,
              fuzzer.GetFuzzedContent().begin() + desc.offset + desc.size,
              offset);
    last_found = offset + desc.size + 1;
  }
  file_util::WriteFile(FilePath(filename),
                       reinterpret_cast<const char*>(&container[0]),
                       container.size());
}

bool IsVideoFile(const std::string& name) {
  if (name.size() < 5) {
    return false;
  }
  if (SbStringCompareNoCase(name.c_str() + name.size() - 5, ".webm") == 0) {
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
                    ScopedVector<MediaSourceDemuxer>* demuxers,
                    ScopedVector<ZzufFuzzer>* fuzzers) {
  const float kMinFuzzRatio = 0.01f;
  const float kMaxFuzzRatio = 0.05f;

  std::vector<std::string>::iterator iter = filenames->begin();
  while (iter != filenames->end()) {
    LOG(INFO) << "Loading " << *iter;
    std::string content;
    if (file_util::ReadFileToString(FilePath(*iter), &content) &&
        !content.empty()) {
      scoped_ptr<MediaSourceDemuxer> demuxer(new MediaSourceDemuxer(
          std::vector<uint8>(content.begin(), content.end())));
      if (demuxer->valid() && demuxer->GetFrameCount() > 0) {
        fuzzers->push_back(
            new ZzufFuzzer(demuxer->au_data(), kMinFuzzRatio, kMaxFuzzRatio));
        demuxers->push_back(demuxer.release());
        ++iter;
      } else {
        LOG(ERROR) << "Failed to demux video: " << *iter;
        iter = filenames->erase(iter);
      }
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
      argc, argv, FilePath(FILE_PATH_LITERAL("raw_video_decoder_fuzzer.json")));

  // Note that we can't access PathService until MediaSandbox is initialized.
  std::vector<std::string> filenames = CollectVideoFiles(argv[2]);

  ScopedVector<MediaSourceDemuxer> demuxers;
  ScopedVector<ZzufFuzzer> data_fuzzers;

  GatherTestData(&filenames, &demuxers, &data_fuzzers);

  DCHECK_EQ(filenames.size(), demuxers.size());
  DCHECK_EQ(demuxers.size(), data_fuzzers.size());

  if (demuxers.empty()) {
    LOG(ERROR) << "No files to fuzz";
    return 1;
  }

  for (long i = 0; i < iterations; ++i) {
    for (size_t file_index = 0; file_index < demuxers.size(); ++file_index) {
      LOG(INFO) << "Fuzzing \"" << filenames[file_index] << "\" with seed "
                << data_fuzzers[file_index]->seed();
      scoped_ptr<ShellRawVideoDecoder> decoder =
          media_sandbox.GetMediaModule()->GetRawVideoDecoderFactory()->Create(
              demuxers[file_index]->config(), NULL, false);
      DCHECK(decoder);
      VideoDecoderFuzzer decoder_fuzzer(
          data_fuzzers[file_index]->GetFuzzedContent(), demuxers[file_index],
          decoder.get());
      decoder_fuzzer.Fuzz();
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
