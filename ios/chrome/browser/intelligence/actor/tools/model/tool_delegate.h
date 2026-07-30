// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TOOL_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TOOL_DELEGATE_H_

#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

namespace actor {

class ActorToolFactory;
class AggregatedJournal;

// Provides the tools layer with access to objects owned by the orchestration
// layer. The lifetime of this is tied to the ActorTask, which outlives any
// tools that use it.
//
// This is based on chrome/browser/actor/tools/tool_delegate.h.
class ToolDelegate {
 public:
  virtual ~ToolDelegate() = default;

  // Returns the unique identifier of the active task.
  virtual ActorTaskId GetTaskId() const = 0;

  // Returns the journal used for logging.
  virtual AggregatedJournal& GetJournal() const = 0;

  // Returns the tool factory associated with this task.
  virtual ActorToolFactory& GetToolFactory() const = 0;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TOOL_DELEGATE_H_
