// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/vlog.h"

#include "base/basictypes.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"

namespace logging {

const int VlogInfo::kDefaultVlogLevel = 0;

VlogInfo::VlogInfo(const std::string& v_switch,
                   const std::string& vmodule_switch)
    : max_vlog_level_(kDefaultVlogLevel) {
  typedef std::pair<std::string, std::string> KVPair;
  if (!base::StringToInt(v_switch, &max_vlog_level_)) {
    LOG(WARNING) << "Parsed v switch \""
                 << v_switch << "\" as " << max_vlog_level_;
  }
  std::vector<KVPair> kv_pairs;
  if (!base::SplitStringIntoKeyValuePairs(
          vmodule_switch, '=', ',', &kv_pairs)) {
    LOG(WARNING) << "Could not fully parse vmodule switch \""
                 << vmodule_switch << "\"";
  }
  for (std::vector<KVPair>::const_iterator it = kv_pairs.begin();
       it != kv_pairs.end(); ++it) {
    int vlog_level = kDefaultVlogLevel;
    if (!base::StringToInt(it->second, &vlog_level)) {
      LOG(WARNING) << "Parsed vlog level for \""
                   << it->first << "=" << it->second
                   << "\" as " << vlog_level;
    }
    vmodule_levels_.push_back(std::make_pair(it->first, vlog_level));
  }
}

VlogInfo::~VlogInfo() {}

int VlogInfo::GetVlogLevel(const base::StringPiece& file) {
  if (!vmodule_levels_.empty()) {
    base::StringPiece module(file);
    base::StringPiece::size_type last_slash_pos =
        module.find_last_of("\\/");
    if (last_slash_pos != base::StringPiece::npos) {
      module.remove_prefix(last_slash_pos + 1);
    }
    base::StringPiece::size_type extension_start = module.find('.');
    module = module.substr(0, extension_start);
    static const char kInlSuffix[] = "-inl";
    static const int kInlSuffixLen = arraysize(kInlSuffix) - 1;
    if (module.ends_with(kInlSuffix)) {
      module.remove_suffix(kInlSuffixLen);
    }
    for (std::vector<VmodulePattern>::const_iterator it =
             vmodule_levels_.begin(); it != vmodule_levels_.end(); ++it) {
      // TODO(akalin): Use a less-heavyweight version of MatchPattern
      // (we can pretty much assume we're dealing with ASCII).
      if (MatchPattern(module, it->first)) {
        return it->second;
      }
    }
  }
  return max_vlog_level_;
}

}  // namespace
