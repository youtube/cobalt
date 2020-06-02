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

#ifndef COBALT_MEDIA_SANDBOX_FORMAT_GUESSTIMATOR_H_
#define COBALT_MEDIA_SANDBOX_FORMAT_GUESSTIMATOR_H_

#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "cobalt/media/media_module.h"
#include "url/gurl.h"

namespace cobalt {
namespace media {
namespace sandbox {

// Try the best effort to guesstimate the format specified by |path_or_url|.
// Note that to avoid pulling data from the net, when |path_or_url| is a url, it
// will be identified as progressive mp4.
class FormatGuesstimator {
 public:
  FormatGuesstimator(const std::string& path_or_url, MediaModule* media_module);

  bool is_valid() const { return is_progressive() || is_adaptive(); }
  bool is_progressive() const { return progressive_url_.is_valid(); }
  bool is_adaptive() const { return !adaptive_path_.empty(); }
  bool is_audio() const {
    DCHECK(is_adaptive());
    return mime_type_.find("audio/") == 0;
  }

  const GURL& progressive_url() const {
    DCHECK(is_progressive());
    DCHECK(!is_adaptive());
    return progressive_url_;
  }

  const std::string& adaptive_path() const {
    DCHECK(is_adaptive());
    DCHECK(!is_progressive());
    return adaptive_path_.value();
  }

  const std::string& mime_type() const {
    DCHECK(is_valid());
    return mime_type_;
  }

 private:
  void InitializeAsProgressive(const GURL& url);
  void InitializeAsAdaptive(const base::FilePath& path,
                            MediaModule* media_module);

  GURL progressive_url_;
  base::FilePath adaptive_path_;
  std::string mime_type_;
};

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_SANDBOX_FORMAT_GUESSTIMATOR_H_
