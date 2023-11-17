// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/sandbox/format_guesstimator.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "cobalt/media/sandbox/web_media_player_helper.h"
#include "cobalt/render_tree/image.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_tracks.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/chunk_demuxer.h"
#include "net/base/filename_util.h"
#include "net/base/url_util.h"
#include "starboard/common/file.h"
#include "starboard/common/string.h"
#include "starboard/memory.h"
#include "starboard/types.h"
#include "third_party/chromium/media/base/timestamp_constants.h"
#include "third_party/chromium/media/cobalt/ui/gfx/geometry/size.h"

namespace cobalt {
namespace media {
namespace sandbox {

namespace {

using ::media::ChunkDemuxer;

// The possible mime type configurations that are supported by cobalt.
const std::vector<std::string> kSupportedMimeTypes = {
    "audio/mp4; codecs=\"ac-3\"",
    "audio/mp4; codecs=\"ec-3\"",
    "audio/mp4; codecs=\"mp4a.40.2\"",
    "audio/webm; codecs=\"opus\"",

    "video/mp4; codecs=\"av01.0.05M.08\"",
    "video/mp4; codecs=\"avc1.640028, mp4a.40.2\"",
    "video/mp4; codecs=\"avc1.640028\"",
    "video/mp4; codecs=\"hvc1.1.6.H150.90\"",
    "video/webm; codecs=\"vp9\"",
};

// Can be called as:
//   IsFormat("https://example.com/audio.mp4", ".mp4")
//   IsFormat("cobalt/demos/video.webm", ".webm")
bool IsFormat(const std::string& path_or_url, const std::string& format) {
  DCHECK(!format.empty() && format[0] == '.') << "Invalid format: " << format;
  return path_or_url.size() > format.size() &&
         path_or_url.substr(path_or_url.size() - format.size()) == format;
}

base::FilePath ResolvePath(const std::string& path) {
  base::FilePath result(path);
  if (!result.IsAbsolute()) {
    base::FilePath content_path;
    base::PathService::Get(base::DIR_TEST_DATA, &content_path);
    result = content_path.Append(result);
  }
  if (SbFileCanOpen(result.value().c_str(), kSbFileOpenOnly | kSbFileRead)) {
    return result;
  }
  LOG(WARNING) << "Failed to resolve path \"" << path << "\" as \""
               << result.value() << '"';
  return base::FilePath();
}

// Read the first 256kb of the local file specified by |path|.  If the size
// of the file is less than 256kb, the function reads all of its content.
std::vector<uint8_t> ReadHeader(const base::FilePath& path) {
  // Size of the input file to be read into memory for checking the validity
  // of ChunkDemuxer::AppendData() calls.
  const int64_t kHeaderSize = 256 * 1024;  // 256kb
  starboard::ScopedFile file(path.value().c_str(),
                             kSbFileOpenOnly | kSbFileRead);
  int64_t bytes_to_read = std::min(kHeaderSize, file.GetSize());
  DCHECK_GE(bytes_to_read, 0);
  std::vector<uint8_t> buffer(bytes_to_read);
  auto bytes_read =
      file.Read(reinterpret_cast<char*>(buffer.data()), bytes_to_read);
  DCHECK_EQ(bytes_to_read, bytes_read);

  return buffer;
}

void OnInitSegmentReceived(std::unique_ptr<::media::MediaTracks> tracks) {}

// Extract the value of "codecs" parameter from content type. It will return
// "avc1.42E01E" for "video/mp4; codecs="avc1.42E01E".
// Note that this function assumes that the input is always valid and does
// minimum validation..
std::string ExtractCodecs(const std::string& content_type) {
  static const char kCodecs[] = "codecs=";

  // SplitString will also trim the results.
  std::vector<std::string> tokens = ::base::SplitString(
      content_type, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (size_t i = 1; i < tokens.size(); ++i) {
    if (strncasecmp(tokens[i].c_str(), kCodecs, strlen(kCodecs))) {
      continue;
    }
    auto codec = tokens[i].substr(strlen("codecs="));
    base::TrimString(codec, " \"", &codec);
    return codec;
  }
  NOTREACHED();
  return "";
}

}  // namespace

FormatGuesstimator::FormatGuesstimator(const std::string& path_or_url,
                                       MediaModule* media_module) {
  GURL url(path_or_url);
  if (url.is_valid()) {
    // If it is a url, assume that it is a progressive video.
    InitializeAsProgressive(url);
    return;
  }
  base::FilePath path = ResolvePath(path_or_url);
  if (path.empty() || !SbFileCanOpen(path.value().c_str(), kSbFileRead)) {
    return;
  }
  InitializeAsAdaptive(path, media_module);

  if (!is_valid() && IsFormat(path_or_url, ".mp4")) {
    // It's an mp4 file but not in DASH, let's try progressive again.
    bool is_from_root = !path_or_url.empty() && path_or_url[0] == '/';
    auto path_from_root = (is_from_root ? "" : "/") + path_or_url;
    progressive_url_ = GURL("file://" + path_from_root);
    SB_LOG(INFO) << progressive_url_.spec();
    mime_type_ = "video/mp4; codecs=\"avc1.640028, mp4a.40.2\"";
  }
}

void FormatGuesstimator::InitializeAsProgressive(const GURL& url) {
  // Mp4 is the only supported progressive format.
  if (!IsFormat(url.spec(), ".mp4")) {
    return;
  }
  progressive_url_ = url;
  mime_type_ = "video/mp4; codecs=\"avc1.640028, mp4a.40.2\"";
}

void FormatGuesstimator::InitializeAsAdaptive(const base::FilePath& path,
                                              MediaModule* media_module) {
  std::vector<uint8_t> header = ReadHeader(path);

  for (const auto& expected_supported_mime_type : kSupportedMimeTypes) {
    DCHECK(mime_type_.empty());

    ChunkDemuxer* chunk_demuxer = NULL;
    WebMediaPlayerHelper::ChunkDemuxerOpenCB open_cb = base::Bind(
        [](ChunkDemuxer** handle, ChunkDemuxer* chunk_demuxer) -> void {
          *handle = chunk_demuxer;
        },
        &chunk_demuxer);
    // We create a new |web_media_player_helper| every iteration in order to
    // obtain a handle to a new |ChunkDemuxer| without any accumulated state as
    // a result of previous calls to |AddId| and |AppendData| methods.
    WebMediaPlayerHelper web_media_player_helper(media_module, open_cb,
                                                 gfx::Size(1920, 1080));

    // |chunk_demuxer| will be set when |open_cb| is called asynchronously
    // during initialization of |web_media_player_helper|. Wait until it is set
    // before proceeding.
    while (!chunk_demuxer) {
      base::RunLoop().RunUntilIdle();
    }

    const std::string id = "stream";
    if (chunk_demuxer->AddId(id, expected_supported_mime_type) !=
        ChunkDemuxer::kOk) {
      continue;
    }

    chunk_demuxer->SetTracksWatcher(id, base::Bind(OnInitSegmentReceived));
    chunk_demuxer->SetParseWarningCallback(
        id, base::BindRepeating([](::media::SourceBufferParseWarning warning) {
          LOG(WARNING) << "Encountered SourceBufferParseWarning "
                       << static_cast<int>(warning);
        }));

    base::TimeDelta unused_timestamp;
    if (!chunk_demuxer->AppendData(
            id, header.data(), header.size(), base::TimeDelta(),
            ::media::kInfiniteDuration, &unused_timestamp)) {
      // Failing to |AppendData()| means the chosen format is not the file's
      // true format.
      continue;
    }
    // Succeeding |AppendData()| may be a false positive (i.e. the expected
    // configuration does not match with the configuration determined by the
    // ChunkDemuxer). To confirm, we check the decoder configuration determined
    // by the ChunkDemuxer against the chosen format.
    auto streams = chunk_demuxer->GetAllStreams();
    DCHECK_EQ(streams.size(), 1);

    if (streams[0]->type() == ::media::DemuxerStream::AUDIO) {
      const auto& decoder_config = streams[0]->audio_decoder_config();
      if (::media::StringToAudioCodec(ExtractCodecs(
              expected_supported_mime_type)) == decoder_config.codec()) {
        adaptive_path_ = path;
        mime_type_ = expected_supported_mime_type;
        break;
      }
      continue;
    }
    DCHECK_EQ(streams[0]->type(), ::media::DemuxerStream::VIDEO);
    const auto& decoder_config = streams[0]->video_decoder_config();
    if (::media::StringToVideoCodec(ExtractCodecs(
            expected_supported_mime_type)) == decoder_config.codec()) {
      adaptive_path_ = path;
      mime_type_ = expected_supported_mime_type;
      break;
    }
  }
}

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt
