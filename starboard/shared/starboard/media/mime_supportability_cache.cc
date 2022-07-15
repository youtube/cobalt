// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/media/mime_supportability_cache.h"

#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>

#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/common/mutex.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/once.h"
#include "starboard/shared/starboard/media/mime_type.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

namespace {

const size_t kDefaultCacheMaxSize = 2000;

class MimeSupportabilityContainer {
 public:
  struct Entry {
    ParsedMimeInfo mime_info;
    Supportability supportability = kSupportabilityUnknown;

    explicit Entry(const std::string& mime_string) : mime_info(mime_string) {}
  };

  // GetParsedMimeAndSupportability() will first try to find a cached Entry for
  // the mime string. If no cached entry, a new Entry will be created with
  // parsed mime information and supportability kSupportabilityUnknown.
  // Ideally, we should decouple mime parsing and cache functionality, but
  // considering that the cache is only for internal use, to avoid repeated
  // lookups, we do parsing in this function.
  const Entry& GetParsedMimeAndSupportability(const std::string& mime_string) {
    ScopedLock scoped_lock(mutex_);
    auto entry_iter = entries_.find(mime_string);
    if (entry_iter != entries_.end()) {
      return entry_iter->second;
    }

    // We can't find anything from the cache. Parse mime string and cache
    // parsed MimeType and ParsedMimeInfo.
    auto insert_result = entries_.insert({mime_string, Entry(mime_string)});

    fifo_queue_.push(insert_result.first);
    while (fifo_queue_.size() > max_size_) {
      entries_.erase(fifo_queue_.front());
      fifo_queue_.pop();
    }
    SB_DCHECK(entries_.size() == fifo_queue_.size());

    return insert_result.first->second;
  }

  // CacheSupportability() will find the target entry and update the
  // supportability. If there's no existing entry, it will parse the mime
  // string and create one.
  void CacheSupportability(const std::string& mime_string,
                           Supportability supportability) {
    SB_DCHECK(!mime_string.empty());
    SB_DCHECK(supportability != kSupportabilityUnknown);

    {
      ScopedLock scoped_lock(mutex_);
      auto entry_iter = entries_.find(mime_string);
      if (entry_iter != entries_.end()) {
        entry_iter->second.supportability = supportability;
        return;
      }
    }

    // Parse the mime string and create an entry.
    GetParsedMimeAndSupportability(mime_string);
    // Update the supportability again.
    CacheSupportability(mime_string, supportability);
  }

  // ClearCachedSupportabilities() will reset all cached |supportability|, but
  // will not remove parsed mime infos.
  void ClearCachedSupportabilities() {
    ScopedLock scoped_lock(mutex_);
    for (auto& iter : entries_) {
      iter.second.supportability = kSupportabilityUnknown;
    }
  }

  void SetCacheMaxSize(int size) { max_size_ = size; }

  void DumpCache() {
    ScopedLock scoped_lock(mutex_);
    std::stringstream ss;
    ss << "\n========Dumping MimeInfoCache========";
    for (const auto& entry_iter : entries_) {
      const ParsedMimeInfo& mime_info = entry_iter.second.mime_info;
      ss << "\nMime: " << entry_iter.first;
      ss << "\n  ParsedMimeInfo:";
      ss << "\n    MimeType : " << mime_info.mime_type().ToString();
      if (mime_info.is_valid()) {
        if (mime_info.has_audio_info()) {
          const ParsedMimeInfo::AudioCodecInfo& audio_info =
              mime_info.audio_info();
          ss << "\n    Audio Codec : "
             << GetMediaAudioCodecName(audio_info.codec);
          ss << "\n    Channels : " << audio_info.channels;
        }
        if (mime_info.has_video_info()) {
          const ParsedMimeInfo::VideoCodecInfo& video_info =
              mime_info.video_info();
          ss << "\n    Video Codec : "
             << GetMediaVideoCodecName(video_info.codec);
          ss << "\n    Profile : " << video_info.profile;
          ss << "\n    Level : " << video_info.level;
          ss << "\n    BitDepth : " << video_info.bit_depth;
          ss << "\n    PrimaryId : "
             << GetMediaPrimaryIdName(video_info.primary_id);
          ss << "\n    TransferId : "
             << GetMediaTransferIdName(video_info.transfer_id);
          ss << "\n    MatrixId : "
             << GetMediaMatrixIdName(video_info.matrix_id);
          ss << "\n    Width : " << video_info.frame_width;
          ss << "\n    Height : " << video_info.frame_height;
          ss << "\n    Fps : " << video_info.fps;
          ss << "\n    DecodeToTexture : "
             << (video_info.decode_to_texture_required ? "true" : "false");
        }
      } else {
        ss << "\n    Mime info is not valid";
      }

      ss << "\n  Supportability: ";
      switch (entry_iter.second.supportability) {
        case kSupportabilityUnknown:
          ss << "Unknown";
          break;
        case kSupportabilitySupported:
          ss << "Supported";
          break;
        case kSupportabilityNotSupported:
          ss << "NotSupported";
          break;
      }
    }
    ss << "\n========End of Dumping========";

    SB_DLOG(INFO) << ss.str();
  }

 private:
  typedef std::unordered_map<std::string, Entry> Entries;

  Mutex mutex_;
  Entries entries_;
  std::queue<Entries::iterator> fifo_queue_;
  std::atomic_int max_size_{kDefaultCacheMaxSize};
};

SB_ONCE_INITIALIZE_FUNCTION(MimeSupportabilityContainer, GetContainer);

}  // namespace

// static
SB_ONCE_INITIALIZE_FUNCTION(MimeSupportabilityCache,
                            MimeSupportabilityCache::GetInstance);

void MimeSupportabilityCache::SetCacheMaxSize(size_t size) {
  GetContainer()->SetCacheMaxSize(size);
}

Supportability MimeSupportabilityCache::GetMimeSupportability(
    const std::string& mime,
    ParsedMimeInfo* mime_info) {
  // Get cached parsed mime infos and supportability. If no cache is found,
  // MimeSupportabilityContainer will parse the mime string, and return a parsed
  // MimeType and its parsed audio/video information.
  const MimeSupportabilityContainer::Entry& entry =
      GetContainer()->GetParsedMimeAndSupportability(mime);

  if (mime_info) {
    // Return cached ParsedMimeInfo.
    *mime_info = entry.mime_info;
  }

  return is_enabled_ ? entry.supportability : kSupportabilityUnknown;
}

void MimeSupportabilityCache::CacheMimeSupportability(
    const std::string& mime,
    Supportability supportability) {
  if (!is_enabled_) {
    return;
  }
  if (supportability == kSupportabilityUnknown) {
    SB_LOG(WARNING) << "Rejected unknown supportability.";
    return;
  }

  GetContainer()->CacheSupportability(mime, supportability);
}

void MimeSupportabilityCache::ClearCachedMimeSupportabilities() {
  GetContainer()->ClearCachedSupportabilities();
}

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
