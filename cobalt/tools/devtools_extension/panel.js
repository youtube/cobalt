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
 * Communicates with Cobalt's active CDP port via WebSocket and renders
 * live memory allocations, session P50 medians, and peak guardrails.
 */

let ws = null;
let pollTimer = null;
let baselineMetrics = {};
let latestMetrics = {};

// DOM Elements
const cdpUrlInput = document.getElementById("cdp-url");
const btnConnect = document.getElementById("btn-connect");
const btnRefresh = document.getElementById("btn-refresh");
const btnBaseline = document.getElementById("btn-baseline");
const btnExportJson = document.getElementById("btn-export-json");
const toggleLive = document.getElementById("toggle-live");
const statusBadge = document.getElementById("connection-status");
const tableBody = document.getElementById("metrics-table-body");

// KPI Value Elements
const valRss = document.getElementById("val-rss");
const valPmf = document.getElementById("val-pmf");
const valV8 = document.getElementById("val-v8");
const valGpuPeak = document.getElementById("val-gpu-peak");
const deltaRss = document.getElementById("delta-rss");
const deltaPmf = document.getElementById("delta-pmf");
const deltaV8 = document.getElementById("delta-v8");

// Guardrails
const gr0to2 = document.getElementById("gr-0to2");
const gr2to4 = document.getElementById("gr-2to4");
const gr4to8 = document.getElementById("gr-4to8");
const gr8to16 = document.getElementById("gr-8to16");

// Stacked Bar Segments
const barStacked = document.getElementById("allocator-stacked-bar");

function formatBytesToMB(bytes) {
  if (bytes === undefined || bytes === null || isNaN(bytes)) return "-- MB";
  return (bytes / (1024 * 1024)).toFixed(2) + " MB";
}

function formatDelta(current, baseline) {
  if (baseline === undefined || baseline === null) return "Baseline: --";
  const diff = current - baseline;
  const diffMb = (diff / (1024 * 1024)).toFixed(2);
  if (diff > 0) return `Baseline: +${diffMb} MB 🔺`;
  if (diff < 0) return `Baseline: ${diffMb} MB 🔻`;
  return "Baseline: ±0.00 MB";
}

// Initialize and Auto-Connect on Panel Load
document.addEventListener("DOMContentLoaded", () => {
  // Automatically attempt local connection immediately on open
  connectCDP();
});

async function resolveWebSocketUrl(baseHostUrl) {
  // If user already specified a direct ws:// url with target id, use it.
  if (baseHostUrl.startsWith("ws://") && baseHostUrl.includes("/devtools/page/")) {
    return baseHostUrl;
  }

  // Otherwise, discover page target from http://<host>:<port>/json
  let httpBase = baseHostUrl.replace("ws://", "http://");
  if (!httpBase.startsWith("http://")) {
    httpBase = "http://" + httpBase;
  }

  const response = await fetch(`${httpBase}/json`);
  if (!response.ok) {
    throw new Error(`Target discovery failed with status ${response.status}`);
  }
  const targets = await response.json();
  const pageTarget = targets.find(t => t.type === "page" && t.webSocketDebuggerUrl) || targets[0];
  if (!pageTarget || !pageTarget.webSocketDebuggerUrl) {
    throw new Error("No active Cobalt page target found.");
  }
  return pageTarget.webSocketDebuggerUrl;
}

async function connectCDP() {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.close();
    return;
  }

  try {
    statusBadge.textContent = "Connecting...";
    statusBadge.className = "status-badge";

    const wsUrl = await resolveWebSocketUrl(cdpUrlInput.value.trim());
    ws = new WebSocket(wsUrl);

    ws.onopen = () => {
      statusBadge.textContent = "Connected ●";
      statusBadge.className = "status-badge connected";
      btnConnect.textContent = "Disconnect";
      btnConnect.className = "btn btn-secondary";

      // Enable Performance Domain & query metrics
      ws.send(JSON.stringify({ id: 1, method: "Performance.enable" }));
      requestMetrics();

      if (toggleLive.checked) {
        startLivePolling();
      }
    };

    ws.onmessage = (event) => {
      try {
        const msg = JSON.parse(event.data);
        if (msg.result && msg.result.metrics) {
          handlePerformanceMetrics(msg.result.metrics);
        }
      } catch (err) {
        console.error("Failed to parse CDP message", err);
      }
    };

    ws.onclose = () => {
      statusBadge.textContent = "Disconnected";
      statusBadge.className = "status-badge disconnected";
      btnConnect.textContent = "Connect";
      btnConnect.className = "btn btn-primary";
      stopLivePolling();
    };

    ws.onerror = (err) => {
      console.error("WebSocket error", err);
      statusBadge.textContent = "Error";
      statusBadge.className = "status-badge disconnected";
    };
  } catch (error) {
    console.error("Connection failed:", error);
    statusBadge.textContent = "Failed";
    statusBadge.className = "status-badge disconnected";
  }
}

function requestMetrics() {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ id: 2, method: "Performance.getMetrics" }));
  }
}

function startLivePolling() {
  stopLivePolling();
  pollTimer = setInterval(requestMetrics, 1000);
}

function stopLivePolling() {
  if (pollTimer) {
    clearInterval(pollTimer);
    pollTimer = null;
  }
}

function handlePerformanceMetrics(metrics) {
  latestMetrics = {};
  metrics.forEach(m => {
    latestMetrics[m.name] = m.value;
  });

  updateUi();
}

function updateUi() {
  // 1. Live Subsystems
  const rssLive = latestMetrics["Memory.Browser.ResidentSet.Live"] || 0;
  const pmfLive = latestMetrics["Memory.Browser.PrivateMemoryFootprint.Live"] || 0;
  const v8Live = latestMetrics["Memory.Experimental.Browser2.V8.Live"] ||
                 latestMetrics["JSHeapUsedSize"] || 0;
  const stacksLive = latestMetrics["Memory.Experimental.Browser2.Stacks.Live"] || 0;
  const gpuPeakP50 = latestMetrics["Memory.GPU.PeakMemoryUsage2.PageLoad.P50"] || 0;

  valRss.textContent = formatBytesToMB(rssLive);
  valPmf.textContent = formatBytesToMB(pmfLive);
  valV8.textContent = formatBytesToMB(v8Live);
  valGpuPeak.textContent = formatBytesToMB(gpuPeakP50);

  deltaRss.textContent = formatDelta(rssLive, baselineMetrics["Memory.Browser.ResidentSet.Live"]);
  deltaPmf.textContent = formatDelta(pmfLive, baselineMetrics["Memory.Browser.PrivateMemoryFootprint.Live"]);
  deltaV8.textContent = formatDelta(v8Live, baselineMetrics["Memory.Experimental.Browser2.V8.Live"]);

  // 2. Guardrails
  gr0to2.textContent = formatBytesToMB(latestMetrics["Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.0to2min"]);
  gr2to4.textContent = formatBytesToMB(latestMetrics["Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.2to4min"]);
  gr4to8.textContent = formatBytesToMB(latestMetrics["Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.4to8min"]);
  gr8to16.textContent = formatBytesToMB(latestMetrics["Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.8to16min"]);

  // 3. Stacked Bar Distribution
  if (rssLive > 0) {
    const v8Pct = Math.min(100, (v8Live / rssLive) * 100);
    const stacksPct = Math.min(100 - v8Pct, (stacksLive / rssLive) * 100);
    const otherPct = Math.max(0, 100 - v8Pct - stacksPct);

    barStacked.children[0].style.width = `${v8Pct.toFixed(1)}%`;
    barStacked.children[0].title = `V8 Heap: ${formatBytesToMB(v8Live)} (${v8Pct.toFixed(1)}%)`;

    barStacked.children[1].style.width = `${stacksPct.toFixed(1)}%`;
    barStacked.children[1].title = `Thread Stacks: ${formatBytesToMB(stacksLive)} (${stacksPct.toFixed(1)}%)`;

    barStacked.children[2].style.width = `${otherPct.toFixed(1)}%`;
    barStacked.children[2].title = `Other: ${formatBytesToMB(rssLive - v8Live - stacksLive)} (${otherPct.toFixed(1)}%)`;
  }

  // 4. Detailed Table Render
  renderTable(rssLive);
}

function renderTable(totalRss) {
  const memoryEntries = Object.keys(latestMetrics)
    .filter(k => k.startsWith("Memory.") || k.startsWith("JSHeap"))
    .sort();

  if (memoryEntries.length === 0) {
    tableBody.innerHTML = `<tr><td colspan="6" class="placeholder-row">No memory metrics recorded yet.</td></tr>`;
    return;
  }

  let html = "";
  memoryEntries.forEach(name => {
    const val = latestMetrics[name];
    const valMb = formatBytesToMB(val);
    const baseline = baselineMetrics[name];
    const deltaStr = formatDelta(val, baseline);

    let typeTag = `<span style="color: #60a5fa">Metric</span>`;
    if (name.endsWith(".Live")) {
      typeTag = `<span style="color: #34d399; font-weight: 600;">⚡ Live</span>`;
    } else if (name.endsWith(".P50")) {
      typeTag = `<span style="color: #fbbf24; font-weight: 600;">📊 P50</span>`;
    } else if (name.includes("HighestPrivateMemoryFootprint")) {
      typeTag = `<span style="color: #f87171; font-weight: 600;">⏱ Guardrail</span>`;
    }

    const pctRss = (totalRss > 0 && typeof val === "number")
      ? ((val / totalRss) * 100).toFixed(1) + "%"
      : "--";

    const domain = name.startsWith("Memory.Browser") ? "Kernel/OS"
                 : name.startsWith("Memory.Experimental.Browser2") ? "Subsystem"
                 : name.startsWith("Memory.GPU") ? "GPU/Raster"
                 : "V8/Engine";

    html += `
      <tr>
        <td style="font-weight: 500; color: var(--text-bright);">${name}</td>
        <td>${typeTag}</td>
        <td>${valMb}</td>
        <td style="font-size: 10px;">${deltaStr}</td>
        <td>${pctRss}</td>
        <td style="color: var(--text-dim);">${domain}</td>
      </tr>
    `;
  });

  tableBody.innerHTML = html;
}

// Event Listeners
btnConnect.addEventListener("click", connectCDP);
btnRefresh.addEventListener("click", requestMetrics);

btnBaseline.addEventListener("click", () => {
  baselineMetrics = { ...latestMetrics };
  updateUi();
});

btnExportJson.addEventListener("click", () => {
  const exportData = {
    timestamp: new Date().toISOString(),
    target: cdpUrlInput.value,
    metrics: latestMetrics,
    baseline: baselineMetrics
  };
  const blob = new Blob([JSON.stringify(exportData, null, 2)], { type: "application/json" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = `cobalt_memory_snapshot_${Date.now()}.json`;
  a.click();
  URL.revokeObjectURL(url);
});

toggleLive.addEventListener("change", () => {
  if (toggleLive.checked) {
    startLivePolling();
  } else {
    stopLivePolling();
  }
});
