// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_process_memory_metrics_emitter.h"

#include "build/build_config.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/process_map.h"
#include "extensions/common/extension.h"
#endif

namespace {

// Returns true iff the given |process| is responsible for hosting the
// main-frame of the given |page|.
bool HostsMainFrame(const performance_manager::ProcessNode* process,
                    const performance_manager::PageNode* page) {
  const performance_manager::FrameNode* main_frame = page->GetMainFrameNode();
  if (main_frame == nullptr) {
    // |process| can't host a frame that doesn't exist.
    return false;
  }

  return main_frame->GetProcessNode() == process;
}

std::vector<ProcessMemoryMetricsEmitter::ProcessInfo> GetProcessToPageInfoMap(
    performance_manager::Graph* graph) {
  std::vector<ProcessMemoryMetricsEmitter::ProcessInfo> process_infos;
  // Assign page nodes unique IDs within this lookup only.
  base::flat_map<const performance_manager::PageNode*, uint64_t> page_id_map;
  for (const performance_manager::ProcessNode* process_node :
       graph->GetAllProcessNodes()) {
    if (process_node->GetProcessId() == base::kNullProcessId)
      continue;

    // First add all processes and their basic information.
    ProcessMemoryMetricsEmitter::ProcessInfo& process_info =
        process_infos.emplace_back();
    process_info.pid = process_node->GetProcessId();
    process_info.launch_time = process_node->GetLaunchTime();

    // Then add information about their associated page nodes. Only renderers
    // are associated with page nodes.
    if (process_node->GetProcessType() != content::PROCESS_TYPE_RENDERER) {
      continue;
    }

    base::flat_set<const performance_manager::PageNode*> page_nodes =
        performance_manager::GraphOperations::GetAssociatedPageNodes(
            process_node);
    const base::TimeTicks now = base::TimeTicks::Now();
    for (const performance_manager::PageNode* page_node : page_nodes) {
      if (page_node->GetUkmSourceID() == ukm::kInvalidSourceId)
        continue;

      // Get or generate the tab id.
      uint64_t& tab_id = page_id_map[page_node];
      if (tab_id == 0u) {
        // 0 is an invalid id, meaning `page_node` was just inserted in
        // `page_id_map` and its tab id must be generated.
        tab_id = page_id_map.size();
      }

      ProcessMemoryMetricsEmitter::PageInfo& page_info =
          process_info.page_infos.emplace_back();
      page_info.ukm_source_id = page_node->GetUkmSourceID();

      page_info.tab_id = tab_id;
      page_info.hosts_main_frame = HostsMainFrame(process_node, page_node);
      page_info.is_visible = page_node->IsVisible();
      page_info.time_since_last_visibility_change =
          now - page_node->GetLastVisibilityChangeTime();
      page_info.time_since_last_navigation =
          page_node->GetTimeSinceLastNavigation();
    }
  }
  return process_infos;
}

}  // namespace

ChromeProcessMemoryMetricsEmitter::ChromeProcessMemoryMetricsEmitter() = default;

ChromeProcessMemoryMetricsEmitter::ChromeProcessMemoryMetricsEmitter(
    base::ProcessId pid_scope)
    : ProcessMemoryMetricsEmitter(pid_scope) {}

ChromeProcessMemoryMetricsEmitter::~ChromeProcessMemoryMetricsEmitter() = default;

void ChromeProcessMemoryMetricsEmitter::FetchProcessInfos() {
  performance_manager::Graph* graph =
      performance_manager::PerformanceManager::GetGraph();
  auto process_infos = GetProcessToPageInfoMap(graph);
  ReceivedProcessInfos(std::move(process_infos));
}

int ChromeProcessMemoryMetricsEmitter::GetNumberOfExtensions(base::ProcessId pid) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Retrieve the renderer process host for the given pid.
  content::RenderProcessHost* rph = nullptr;
  for (auto iter = content::RenderProcessHost::AllHostsIterator();
       !iter.IsAtEnd(); iter.Advance()) {
    if (!iter.GetCurrentValue()->GetProcess().IsValid())
      continue;

    if (iter.GetCurrentValue()->GetProcess().Pid() == pid) {
      rph = iter.GetCurrentValue();
      break;
    }
  }
  if (!rph) {
    return 0;
  }

  // Count the number of extensions associated with this `rph`'s profile.
  extensions::ProcessMap* process_map =
      extensions::ProcessMap::Get(rph->GetBrowserContext());
  if (!process_map) {
    return 0;
  }

  const extensions::Extension* extension =
      process_map->GetEnabledExtensionByProcessID(rph->GetDeprecatedID());
  // Only include this extension if it's not a hosted app.
  return (extension && !extension->is_hosted_app()) ? 1 : 0;
#else
  return 0;
#endif
}
