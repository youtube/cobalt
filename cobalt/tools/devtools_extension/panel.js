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
 * Cobalt Memory Breakdown DevTools Extension Panel Controller (PR 3.1).
 * Connects to Cobalt's active CDP port via WebSocket and renders the
 * primary Session Median Telemetry table (P50 breakdown).
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
      { umaName: "Memory.Experimental.Browser2.PartitionAlloc", cdpKey: "Memory.Experimental.Browser2.PartitionAlloc.P50", label: "PartitionAlloc C++ Heap" },
      { umaName: "Memory.Experimental.Browser2.Malloc", cdpKey: "Memory.Experimental.Browser2.Malloc.P50", label: "System Malloc Heap" },
      { umaName: "Memory.Experimental.Browser2.V8", cdpKey: "Memory.Experimental.Browser2.V8.P50", label: "V8 JavaScript Engine Heap" },
      { umaName: "Memory.Experimental.Browser2.BlinkGC", cdpKey: "Memory.Experimental.Browser2.BlinkGC.P50", label: "Blink C++ GC (cppgc)" },
      { umaName: "Memory.Experimental.Browser2.Skia", cdpKey: "Memory.Experimental.Browser2.Skia.P50", label: "Skia 2D Graphics & Images" },
      { umaName: "Memory.Browser.LibChrobaltRss", cdpKey: "Memory.Browser.LibChrobaltRss.P50", label: "Binary Executable Pages (libchrobalt.so)" },
      { umaName: "Memory.Experimental.Browser2.CodeOther", cdpKey: "Memory.Experimental.Browser2.CodeOther.P50", label: "Other Dynamic Shared Libraries" },
      { umaName: "Memory.Experimental.Browser2.Fonts", cdpKey: "Memory.Experimental.Browser2.Fonts.P50", label: "Font Caches & Glyph Buffers" },
      { umaName: "Memory.Experimental.Browser2.Stacks", cdpKey: "Memory.Experimental.Browser2.Stacks.P50", label: "Thread Stacks" },
      { umaName: "Memory.Experimental.Browser2.JavaHeap", cdpKey: "Memory.Experimental.Browser2.JavaHeap.P50", label: "Android ART Java Heap" },
    ]
  }
];

// Flat list for counter computation
const ALL_CANONICAL_METRICS = METRIC_GROUPS.flatMap(g => g.metrics);

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
  const rssP50 = latestMetrics["Memory.Browser.ResidentSet.P50"] || null;

  // 1. Count Reported vs Pending Canonical Subsystems
  let reportedCount = 0;
  ALL_CANONICAL_METRICS.forEach(item => {
    if (latestMetrics[item.cdpKey] !== undefined && latestMetrics[item.cdpKey] !== null) {
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

  // 2. Render Grouped Breakdown Rows
  renderTable(rssP50);
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
      const rawVal = latestMetrics[item.cdpKey];
      const isRecorded = rawVal !== undefined && rawVal !== null;

      let valDisplay = `<span class="pending-text">⏳ Pending sample...</span>`;
      let pctDisplay = `<span style="color: var(--text-dim)">--</span>`;

      if (isRecorded) {
        valDisplay = formatBytesToMB(rawVal);
        if (typeof rawVal === "number" && typeof totalRss === "number" && totalRss > 0) {
          pctDisplay = ((rawVal / totalRss) * 100).toFixed(1) + "%";
        }
      }

      html += `
        <tr>
          <td>
            <div class="metric-name-cell">
              <span class="metric-label">${item.label}</span>
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

// Event Listeners
btnConnect.addEventListener("click", connectCDP);
btnRefresh.addEventListener("click", requestMetrics);
