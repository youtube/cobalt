// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEADLESS_COMMAND_HANDLER_HEADLESS_COMMAND_HANDLER_H_
#define COMPONENTS_HEADLESS_COMMAND_HANDLER_HEADLESS_COMMAND_HANDLER_H_

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/devtools/simple_devtools_protocol_client/simple_devtools_protocol_client.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace headless {

class HeadlessCommandHandler : public content::WebContentsObserver {
 public:
  typedef base::OnceCallback<void()> DoneCallback;

  HeadlessCommandHandler(const HeadlessCommandHandler&) = delete;
  HeadlessCommandHandler& operator=(const HeadlessCommandHandler&) = delete;

  static GURL GetHandlerUrl();

  static bool HasHeadlessCommandSwitches(const base::CommandLine& command_line);

  // The caller may override the TaskRunner used for file I/O by providing
  // a value for |io_task_runner|.
  static void ProcessCommands(
      content::WebContents* web_contents,
      GURL target_url,
      DoneCallback done_callback,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner = {});

 private:
  using SimpleDevToolsProtocolClient =
      simple_devtools_protocol_client::SimpleDevToolsProtocolClient;

  HeadlessCommandHandler(
      content::WebContents* web_contents,
      GURL target_url,
      DoneCallback done_callback,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  ~HeadlessCommandHandler() override;

  void ExecuteCommands();

  // content::WebContentsObserver implementation:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void WebContentsDestroyed() override;

  void OnTargetCrashed(const base::Value::Dict&);

  void OnCommandsResult(base::Value::Dict result);

  void Done();

  SimpleDevToolsProtocolClient devtools_client_;
  SimpleDevToolsProtocolClient browser_devtools_client_;
  GURL target_url_;
  DoneCallback done_callback_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  base::FilePath pdf_file_path_;
  base::FilePath screenshot_file_path_;
};

}  // namespace headless

#endif  // COMPONENTS_HEADLESS_COMMAND_HANDLER_HEADLESS_COMMAND_HANDLER_H_
