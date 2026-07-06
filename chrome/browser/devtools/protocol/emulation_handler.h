// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_EMULATION_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_EMULATION_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/protocol/emulation.h"
#include "content/public/common/buildflags.h"
#if BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)
#include "components/infobars/content/content_infobar_manager.h"  // nogncheck
#include "components/infobars/core/infobar.h"  // nogncheck
#endif
#include "content/public/browser/devtools_agent_host.h"

#if BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)
class EmulationHandler : public protocol::Emulation::Backend,
                         public infobars::InfoBarManager::Observer {
 public:
  EmulationHandler(content::DevToolsAgentHost* agent_host,
                   protocol::UberDispatcher* dispatcher);
  ~EmulationHandler() override;

  EmulationHandler(const EmulationHandler&) = delete;
  EmulationHandler& operator=(const EmulationHandler&) = delete;

  // Emulation::Backend:
  protocol::Response Disable() override;
  protocol::Response SetAutomationOverride(bool enabled) override;

  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;

 private:
  infobars::ContentInfoBarManager* GetContentInfoBarManager();

  raw_ptr<content::DevToolsAgentHost> agent_host_;
  raw_ptr<infobars::InfoBar> automation_info_bar_ = nullptr;
};
#else
class EmulationHandler : public protocol::Emulation::Backend {
 public:
  EmulationHandler(content::DevToolsAgentHost* agent_host,
                   protocol::UberDispatcher* dispatcher);
  ~EmulationHandler() override;

  EmulationHandler(const EmulationHandler&) = delete;
  EmulationHandler& operator=(const EmulationHandler&) = delete;

  // Emulation::Backend:
  protocol::Response Disable() override;
  protocol::Response SetAutomationOverride(bool enabled) override;

 private:
  raw_ptr<content::DevToolsAgentHost> agent_host_;
};
#endif  // BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_EMULATION_HANDLER_H_
