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

VlogInfo::VmodulePattern::VmodulePattern(const std::string& pattern)
    : pattern(pattern),
      vlog_level(VlogInfo::kDefaultVlogLevel),
      match_target(MATCH_MODULE) {
  // If the pattern contains a {forward,back} slash, we assume that
  // it's meant to be tested against the entire __FILE__ string.
  std::string::size_type first_slash = pattern.find_first_of("\\/");
  if (first_slash != std::string::npos) {
    // The backslash is an escape character for patterns, so we need
    // to escape it.  This is okay, because it's highly unlikely that
    // the user would want to match literal *s or ?s.  However, we may
    // want to have our own simpler version of MatchPattern() to avoid
    // these sorts of hacks.
    ReplaceSubstringsAfterOffset(&this->pattern, first_slash, "\\", "\\\\");
    match_target = MATCH_FILE;
  }
}

VlogInfo::VmodulePattern::VmodulePattern()
    : vlog_level(VlogInfo::kDefaultVlogLevel),
      match_target(MATCH_MODULE) {}

VlogInfo::VlogInfo(const std::string& v_switch,
                   const std::string& vmodule_switch)
    : max_vlog_level_(kDefaultVlogLevel) {
  typedef std::pair<std::string, std::string> KVPair;
  if (!v_switch.empty() &&
      !base::StringToInt(v_switch, &max_vlog_level_)) {
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
    VmodulePattern pattern(it->first);
    if (!base::StringToInt(it->second, &pattern.vlog_level)) {
      LOG(WARNING) << "Parsed vlog level for \""
                   << it->first << "=" << it->second
                   << "\" as " << pattern.vlog_level;
    }
    vmodule_levels_.push_back(pattern);
  }
}

VlogInfo::~VlogInfo() {}

namespace {

// Given a path, returns the basename with the extension chopped off
// (and any -inl suffix).  We avoid using FilePath to minimize the
// number of dependencies the logging system has.
base::StringPiece GetModule(const base::StringPiece& file) {
  base::StringPiece module(file);
  base::StringPiece::size_type last_slash_pos =
      module.find_last_of("\\/");
  if (last_slash_pos != base::StringPiece::npos)
    module.remove_prefix(last_slash_pos + 1);
  base::StringPiece::size_type extension_start = module.rfind('.');
  module = module.substr(0, extension_start);
  static const char kInlSuffix[] = "-inl";
  static const int kInlSuffixLen = arraysize(kInlSuffix) - 1;
  if (module.ends_with(kInlSuffix))
    module.remove_suffix(kInlSuffixLen);
  return module;
}

}  // namespace

int VlogInfo::GetVlogLevel(const base::StringPiece& file) {
  if (!vmodule_levels_.empty()) {
    base::StringPiece module(GetModule(file));
    for (std::vector<VmodulePattern>::const_iterator it =
             vmodule_levels_.begin(); it != vmodule_levels_.end(); ++it) {
      base::StringPiece target(
          (it->match_target == VmodulePattern::MATCH_FILE) ? file : module);
      if (MatchPattern(target, it->pattern))
        return it->vlog_level;
    }
  }
  return max_vlog_level_;
}

}  // namespace
