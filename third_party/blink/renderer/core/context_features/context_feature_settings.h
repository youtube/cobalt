// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CONTEXT_FEATURES_CONTEXT_FEATURE_SETTINGS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CONTEXT_FEATURES_CONTEXT_FEATURE_SETTINGS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExecutionContext;

// ContextFeatureSettings attaches to an ExecutionContext some basic flags
// pertaining to the enabled/disabled state of any platform API features which
// are gated behind a ContextEnabled extended attribute in IDL.
class CORE_EXPORT ContextFeatureSettings final
    : public GarbageCollected<ContextFeatureSettings>,
      public Supplement<ExecutionContext> {
 public:
  static const char kSupplementName[];

  enum class CreationMode { kCreateIfNotExists, kDontCreateIfNotExists };

  explicit ContextFeatureSettings(ExecutionContext&);

  // Returns the ContextFeatureSettings for an ExecutionContext. If one does not
  // already exist for the given context, one is created.
  static ContextFeatureSettings* From(ExecutionContext*, CreationMode);

  // ContextEnabled=MojoJS feature
  void EnableMojoJS(bool enable) { enable_mojo_js_ = enable; }
  bool isMojoJSEnabled() const { return enable_mojo_js_; }

  // ContextEnabled=MojoJSFileSystemAccessHelper
  void EnableMojoJSFileSystemAccessHelper(bool enable) {
    DCHECK(enable_mojo_js_);
    enable_mojo_js_file_system_access_helper_ = enable;
  }
  bool isMojoJSFileSystemAccessHelperEnabled() const {
    return enable_mojo_js_file_system_access_helper_;
  }

  // ContextEnabled=PrivateAggregationInSharedStorage
  void EnablePrivateAggregationInSharedStorage(bool enable) {
    enable_private_aggregation_in_shared_storage_ = enable;
  }
  bool isPrivateAggregationInSharedStorageEnabled() const {
    return enable_private_aggregation_in_shared_storage_;
  }

  void Trace(Visitor*) const override;

 private:
  bool enable_mojo_js_ = false;
  bool enable_mojo_js_file_system_access_helper_ = false;
  bool enable_private_aggregation_in_shared_storage_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CONTEXT_FEATURES_CONTEXT_FEATURE_SETTINGS_H_
