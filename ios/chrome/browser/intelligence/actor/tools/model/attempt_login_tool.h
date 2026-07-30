// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ATTEMPT_LOGIN_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ATTEMPT_LOGIN_TOOL_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/types/expected.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"

class ProfileIOS;

namespace optimization_guide {
namespace proto {
class AttemptLoginAction;
}  // namespace proto
}  // namespace optimization_guide

namespace web {
class WebState;
}  // namespace web

namespace actor {

class ToolDelegate;
struct ToolExecutionResult;

// Tool to attempt login on a page.
class AttemptLoginTool : public ActorTool {
 public:
  static base::expected<std::unique_ptr<AttemptLoginTool>, ToolExecutionResult>
  Create(const optimization_guide::proto::AttemptLoginAction& action,
         ToolDelegate* tool_delegate,
         ProfileIOS* profile);

  ~AttemptLoginTool() override;

  // ActorTool:
  void Execute(ToolExecutionCallback callback) override;
  base::WeakPtr<web::WebState> GetTargetWebState() const override;
  ToolType GetToolType() const override;

 private:
  AttemptLoginTool(base::WeakPtr<web::WebState> web_state,
                   ToolDelegate* tool_delegate);

  base::WeakPtr<web::WebState> web_state_;
  raw_ptr<ToolDelegate> tool_delegate_;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ATTEMPT_LOGIN_TOOL_H_
