// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

/**
 * Cobalt Memory Breakdown DevTools Extension Panel Controller.
 * Connects to Cobalt's active CDP port via WebSocket and renders:
 * 1. Proportional Memory Footprint Distribution Bar (10 Canonical Allocators).
 * 2. Primary Session P50 Memory Breakdown table (12 continuous P50 allocators).
 * 3. Lifecycle Peak Memory scorecard (time-windowed PMF & Peak GPU).
 */

// 12 Continuous UMA memory breakdown metrics grouped by Process Totals and Allocator Breakdown
const METRIC_GROUPS = [
  {
    groupTitle: "Process Totals (OS / Kernel)",
    metrics: [
      { umaName: "Memory.Browser.ResidentSet", cdpKey: "Memory.Browser.ResidentSet.P50", label: "Physical Resident Set (RSS)" },
      { umaName: "Memory.Browser.PrivateMemoryFootprint", cdpKey: "Memory.Browser.PrivateMemoryFootprint.P50", label: "Private Memory Footprint" },
    ]
  },
  {
    groupTitle: "Subsystem & Allocator Breakdown",
    metrics: [
      { umaName: "Memory.Experimental.Browser2.PartitionAlloc", cdpKey: "Memory.Experimental.Browser2.PartitionAlloc.P50", label: "PartitionAlloc C++ Heap", cssClass: "segment-pa", color: "var(--color-pa)" },
      { umaName: "Memory.Experimental.Browser2.Malloc", cdpKey: "Memory.Experimental.Browser2.Malloc.P50", label: "System Malloc Heap", cssClass: "segment-malloc", color: "var(--color-malloc)" },
      { umaName: "Memory.Experimental.Browser2.V8", cdpKey: "Memory.Experimental.Browser2.V8.P50", label: "V8 JavaScript Engine Heap", cssClass: "segment-v8", color: "var(--color-v8)" },
      { umaName: "Memory.Experimental.Browser2.BlinkGC", cdpKey: "Memory.Experimental.Browser2.BlinkGC.P50", label: "Blink C++ GC (cppgc)", cssClass: "segment-blinkgc", color: "var(--color-blinkgc)" },
      { umaName: "Memory.Experimental.Browser2.Skia", cdpKey: "Memory.Experimental.Browser2.Skia.P50", label: "Skia 2D Graphics & Images", cssClass: "segment-skia", color: "var(--color-skia)" },
      { umaName: "Memory.Browser.LibChrobaltRss", cdpKey: "Memory.Browser.LibChrobaltRss.P50", label: "Binary Executable Pages (libchrobalt.so)", cssClass: "segment-binary", color: "var(--color-binary)" },
      { umaName: "Memory.Experimental.Browser2.CodeOther", cdpKey: "Memory.Experimental.Browser2.CodeOther.P50", label: "Other Dynamic Shared Libraries", cssClass: "segment-codeother", color: "var(--color-codeother)" },
      { umaName: "Memory.Experimental.Browser2.Fonts", cdpKey: "Memory.Experimental.Browser2.Fonts.P50", label: "Font Caches & Glyph Buffers", cssClass: "segment-fonts", color: "var(--color-fonts)" },
      { umaName: "Memory.Experimental.Browser2.Stacks", cdpKey: "Memory.Experimental.Browser2.Stacks.P50", label: "Thread Stacks", cssClass: "segment-stacks", color: "var(--color-stacks)" },
      { umaName: "Memory.Experimental.Browser2.JavaHeap", cdpKey: "Memory.Experimental.Browser2.JavaHeap.P50", label: "Android ART Java Heap", cssClass: "segment-java", color: "var(--color-java)" },
    ]
  }
];

// Flat list for counter computation
const ALL_CANONICAL_METRICS = METRIC_GROUPS.flatMap(g => g.metrics);

// 10 Canonical Allocators for Distribution Bar (dynamically retrieved from subsystem group)
const ALLOCATOR_SEGMENTS = METRIC_GROUPS.find(g => g.groupTitle.includes("Subsystem"))?.metrics || [];

// Peak Guardrail Scorecard Metrics
const GUARDRAIL_METRICS = [
  {
    title: "0 to 2 min Window",
    umaName: "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.0to2min",
    description: "Boot & initial browse phase"
  },
  {
    title: "2 to 4 min Window",
    umaName: "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.2to4min",
    description: "Early navigation & initial playback"
  },
  {
    title: "4 to 8 min Window",
    umaName: "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.4to8min",
    description: "Continuous browsing"
  },
  {
    title: "8 to 16 min Window",
    umaName: "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.8to16min",
    description: "Extended session stability"
  },
  {
    title: "Peak GPU (Page Load)",
    umaName: "Memory.GPU.PeakMemoryUsage2.PageLoad",
    cdpKey: "Memory.GPU.PeakMemoryUsage2.PageLoad.P50",
    fallbackKey: "Memory.GPU.PeakMemoryUsage2.PageLoad",
    description: "Initial page navigation & render"
  }
];

let ws = null;
let pollInterval = null;
let nextRequestId = 1;
let latestMetrics = {};

// DOM Elements
const cdpUrlInput = document.getElementById("cdp-url");
const btnConnect = document.getElementById("btn-connect");
const btnRefresh = document.getElementById("btn-refresh");
const statusBadge = document.getElementById("connection-status");
const tableBody = document.getElementById("metrics-table-body");
const samplingProgress = document.getElementById("sampling-progress");
const samplingHint = document.getElementById("sampling-hint");
const distributionBar = document.getElementById("distribution-bar");
const guardrailsGrid = document.getElementById("guardrails-grid");

function formatBytesToMB(bytes) {
  if (bytes === undefined || bytes === null || isNaN(bytes)) return "-- MB";
  if (typeof bytes === "number" && bytes > 0 && bytes < 10240) {
    return "< 0.01 MB";
  }
  return (bytes / (1024 * 1024)).toFixed(2) + " MB";
}

function clearPolling() {
  if (pollInterval) {
    clearInterval(pollInterval);
    pollInterval = null;
  }
}

// Auto-Connect on Panel Load and Restore Persisted URL
document.addEventListener("DOMContentLoaded", () => {
  renderGuardrails();

  if (typeof chrome !== "undefined" && chrome.storage && chrome.storage.local) {
    chrome.storage.local.get(["cdpUrl"], (result) => {
      if (result && result.cdpUrl) {
        cdpUrlInput.value = result.cdpUrl;
      }
      connectCDP();
    });
  } else {
    connectCDP();
  }
});

// Clean up socket and timers on window unload
window.addEventListener("beforeunload", () => {
  clearPolling();
  if (ws) {
    ws.close();
    ws = null;
  }
});

async function resolveWebSocketUrl(baseHostUrl) {
  // If already a direct ws:// or wss:// URL with target ID, return directly.
  if ((baseHostUrl.startsWith("ws://") || baseHostUrl.startsWith("wss://")) && baseHostUrl.includes("/devtools/page/")) {
    return baseHostUrl;
  }

  // Convert WebSocket scheme to HTTP scheme for target discovery
  let httpBase = baseHostUrl.replace(/^ws:\/\//, "http://").replace(/^wss:\/\//, "https://");
  if (!httpBase.startsWith("http://") && !httpBase.startsWith("https://")) {
    httpBase = "http://" + httpBase;
  }

  const response = await fetch(`${httpBase}/json`);
  if (!response.ok) {
    throw new Error(`Target discovery failed with status ${response.status}`);
  }
  const targets = await response.json();
  if (!Array.isArray(targets)) {
    throw new Error("Invalid target list format from discovery endpoint.");
  }
  const pageTarget = targets.find(t => t.type === "page" && t.webSocketDebuggerUrl) || targets[0];
  if (!pageTarget || !pageTarget.webSocketDebuggerUrl) {
    throw new Error("No active Cobalt page target found.");
  }
  return pageTarget.webSocketDebuggerUrl;
}

function sendCdpCommand(method, params = {}) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    const id = nextRequestId++;
    ws.send(JSON.stringify({ id, method, params }));
  }
}

async function connectCDP() {
  if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) {
    clearPolling();
    ws.close();
    ws = null;
    return;
  }

  const targetUrl = cdpUrlInput.value.trim();
  if (!targetUrl) {
    statusBadge.textContent = "URL Required";
    statusBadge.className = "status-badge disconnected";
    return;
  }

  try {
    statusBadge.textContent = "Connecting...";
    statusBadge.className = "status-badge";

    const wsUrl = await resolveWebSocketUrl(targetUrl);
    ws = new WebSocket(wsUrl);

    ws.onopen = () => {
      statusBadge.textContent = "Connected ●";
      statusBadge.className = "status-badge connected";
      btnConnect.textContent = "Disconnect";
      btnConnect.className = "btn btn-secondary";
      btnRefresh.disabled = false;

      // Save the successfully connected URL
      if (typeof chrome !== "undefined" && chrome.storage && chrome.storage.local) {
        chrome.storage.local.set({ cdpUrl: targetUrl });
      }

      // Enable Performance Domain & query metrics
      sendCdpCommand("Performance.enable");
      requestMetrics();

      // Start periodic polling every 3 seconds for real-time telemetry updates
      clearPolling();
      pollInterval = setInterval(requestMetrics, 3000);
    };

    ws.onmessage = (event) => {
      try {
        const msg = JSON.parse(event.data);
        if (!msg || typeof msg !== "object") return;
        if (msg.error) {
          console.error("CDP Protocol Error:", msg.error);
          return;
        }
        if (msg.result && msg.result.metrics) {
          handlePerformanceMetrics(msg.result.metrics);
        }
      } catch (err) {
        console.error("Failed to parse CDP message:", err);
      }
    };

    ws.onclose = () => {
      clearPolling();
      statusBadge.textContent = "Disconnected";
      statusBadge.className = "status-badge disconnected";
      btnConnect.textContent = "Connect";
      btnConnect.className = "btn btn-primary";
      btnRefresh.disabled = true;
    };

    ws.onerror = (err) => {
      console.error("WebSocket error:", err);
      clearPolling();
      statusBadge.textContent = "Error";
      statusBadge.className = "status-badge disconnected";
      btnRefresh.disabled = true;
    };
  } catch (error) {
    console.error("Connection failed:", error);
    clearPolling();
    statusBadge.textContent = "Failed";
    statusBadge.className = "status-badge disconnected";
    btnRefresh.disabled = true;
  }
}

function requestMetrics() {
  sendCdpCommand("Performance.getMetrics");
}

function handlePerformanceMetrics(metrics) {
  if (!Array.isArray(metrics)) return;
  latestMetrics = {};
  metrics.forEach(m => {
    if (m && m.name !== undefined) {
      latestMetrics[m.name] = m.value;
    }
  });

  updateUi();
}

function updateUi() {
  const rssP50 = latestMetrics?.["Memory.Browser.ResidentSet.P50"] || null;

  // 1. Count Reported vs Pending Canonical Subsystems
  let reportedCount = 0;
  ALL_CANONICAL_METRICS.forEach(item => {
    if (latestMetrics?.[item.cdpKey] !== undefined && latestMetrics?.[item.cdpKey] !== null) {
      reportedCount++;
    }
  });

  const totalCount = ALL_CANONICAL_METRICS.length;
  samplingProgress.textContent = `${reportedCount} / ${totalCount} Reported`;
  if (reportedCount === totalCount) {
    samplingProgress.className = "sampling-badge complete";
    samplingHint.style.display = "none";
  } else {
    samplingProgress.className = "sampling-badge";
    samplingHint.style.display = "block";
  }

  // 2. Render Integrated Visual Distribution Bar (10 Canonical Allocators)
  renderDistributionBar(rssP50);

  // 3. Render Grouped Breakdown Table Rows
  renderTable(rssP50);

  // 4. Render Peak Memory & Lifecycle Guardrail Scorecard
  renderGuardrails();
}

function renderDistributionBar(totalRss) {
  if (!totalRss || totalRss <= 0) {
    distributionBar.innerHTML = `<div class="distribution-bar-empty">Waiting for memory telemetry...</div>`;
    return;
  }

  let totalAccountedBytes = 0;
  let barHtml = "";

  ALLOCATOR_SEGMENTS.forEach(item => {
    const rawVal = latestMetrics?.[item.cdpKey];
    if (typeof rawVal === "number" && rawVal > 0) {
      totalAccountedBytes += rawVal;
      const pct = ((rawVal / totalRss) * 100).toFixed(1);
      const mbStr = formatBytesToMB(rawVal);

      barHtml += `
        <div class="distribution-segment ${item.cssClass}" style="width: ${pct}%" title="${item.label}: ${mbStr} (${pct}%)"></div>
      `;
    }
  });

  // Calculate remaining unallocated / other native resident footprint
  const otherBytes = Math.max(0, totalRss - totalAccountedBytes);
  if (otherBytes > 0) {
    const pctOther = ((otherBytes / totalRss) * 100).toFixed(1);
    const mbOtherStr = formatBytesToMB(otherBytes);

    barHtml += `
      <div class="distribution-segment segment-other" style="width: ${pctOther}%" title="Other Resident Pages: ${mbOtherStr} (${pctOther}%)"></div>
    `;
  }

  distributionBar.innerHTML = barHtml || `<div class="distribution-bar-empty">Waiting for memory telemetry...</div>`;
}

function renderTable(totalRss) {
  let html = "";

  METRIC_GROUPS.forEach(group => {
    html += `
      <tr class="table-group-header">
        <td colspan="3">${group.groupTitle}</td>
      </tr>
    `;

    group.metrics.forEach(item => {
      const rawVal = latestMetrics?.[item.cdpKey];
      const isRecorded = rawVal !== undefined && rawVal !== null;

      let valDisplay = `<span class="pending-text">⏳ Pending sample...</span>`;
      let pctDisplay = `<span style="color: var(--text-dim)">--</span>`;

      if (isRecorded) {
        valDisplay = formatBytesToMB(rawVal);
        if (typeof rawVal === "number" && typeof totalRss === "number" && totalRss > 0) {
          pctDisplay = ((rawVal / totalRss) * 100).toFixed(1) + "%";
        }
      }

      // If the metric has an assigned color dot (subsystems), render it in the label
      let labelHtml = `<span class="metric-label">${item.label}</span>`;
      if (item.color) {
        labelHtml = `
          <div class="metric-label-row">
            <span class="metric-dot" style="background-color: ${item.color}"></span>
            <span class="metric-label">${item.label}</span>
          </div>
        `;
      }

      html += `
        <tr>
          <td>
            <div class="metric-name-cell">
              ${labelHtml}
              <span class="metric-key">${item.umaName}</span>
            </div>
          </td>
          <td>${valDisplay}</td>
          <td>${pctDisplay}</td>
        </tr>
      `;
    });
  });

  tableBody.innerHTML = html;
}

function renderGuardrails() {
  let html = "";

  GUARDRAIL_METRICS.forEach(g => {
    const lookupKey = g.cdpKey || g.umaName;
    let rawVal = latestMetrics?.[lookupKey];
    if (rawVal === undefined && g.fallbackKey) {
      rawVal = latestMetrics?.[g.fallbackKey];
    }

    const isRecorded = rawVal !== undefined && rawVal !== null;
    let valHtml = `<span class="guardrail-value pending">⏳ Waiting for window...</span>`;
    let statusBadgeHtml = `<span class="badge-guardrail-pending">Pending</span>`;

    if (isRecorded && typeof rawVal === "number") {
      valHtml = `<span class="guardrail-value">${formatBytesToMB(rawVal)}</span>`;
      statusBadgeHtml = `<span class="badge-guardrail-recorded">Recorded</span>`;
    }

    html += `
      <div class="guardrail-card">
        <div class="guardrail-header">
          <div>
            <div class="guardrail-title">${g.title}</div>
            <div class="guardrail-key">${g.umaName}</div>
          </div>
          ${statusBadgeHtml}
        </div>
        <div class="guardrail-value-row">
          ${valHtml}
        </div>
        <div class="guardrail-footer">
          <span class="guardrail-desc">${g.description}</span>
        </div>
      </div>
    `;
  });

  guardrailsGrid.innerHTML = html;
}

// Event Listeners
btnConnect.addEventListener("click", connectCDP);
btnRefresh.addEventListener("click", requestMetrics);
