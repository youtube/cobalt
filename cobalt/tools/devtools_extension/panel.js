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
 * 1. Real-Time Live Memory Time Series Chart (1 Hz polling over 60s window).
 * 2. Integrated Proportional Memory Distribution Bar (10 Canonical Allocators).
 * 3. Primary Session P50 Memory Breakdown table (12 continuous P50 allocators).
 * 4. Lifecycle Peak Memory scorecard (time-windowed PMF & Peak GPU).
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

// Live Time-Series State (Rolling 60s Window at 1 Hz)
const MAX_HISTORY_SAMPLES = 60;
const liveHistory = [];

let ws = null;
let pollInterval = null;
let nextRequestId = 1;
let latestMetrics = {};

// DOM Elements
const cdpUrlInput = document.getElementById("cdp-url");
const btnConnect = document.getElementById("btn-connect");
const btnRefresh = document.getElementById("btn-refresh");
const statusBadge = document.getElementById("connection-status");
const streamingBadge = document.getElementById("streaming-badge");
const tableBody = document.getElementById("metrics-table-body");
const samplingProgress = document.getElementById("sampling-progress");
const samplingHint = document.getElementById("sampling-hint");
const distributionBar = document.getElementById("distribution-bar");
const guardrailsGrid = document.getElementById("guardrails-grid");

// Chart DOM Elements
const chartCanvas = document.getElementById("live-memory-chart");
const chartTooltip = document.getElementById("chart-tooltip");
const liveValRss = document.getElementById("live-val-rss");
const liveValPmf = document.getElementById("live-val-pmf");
const liveValV8 = document.getElementById("live-val-v8");

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
  setupChartInteractivity();
  drawTimeSeriesChart();

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
  if ((baseHostUrl.startsWith("ws://") || baseHostUrl.startsWith("wss://")) && baseHostUrl.includes("/devtools/page/")) {
    return baseHostUrl;
  }

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
      streamingBadge.textContent = "● 1 Hz Active";
      streamingBadge.className = "streaming-badge";
      btnConnect.textContent = "Disconnect";
      btnConnect.className = "btn btn-secondary";
      btnRefresh.disabled = false;

      if (typeof chrome !== "undefined" && chrome.storage && chrome.storage.local) {
        chrome.storage.local.set({ cdpUrl: targetUrl });
      }

      sendCdpCommand("Performance.enable");
      requestMetrics();

      // Static 1 Hz live telemetry streaming polling
      clearPolling();
      pollInterval = setInterval(requestMetrics, 1000);
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
      streamingBadge.textContent = "● 1 Hz Inactive";
      streamingBadge.className = "streaming-badge inactive";
      btnConnect.textContent = "Connect";
      btnConnect.className = "btn btn-primary";
      btnRefresh.disabled = true;
    };

    ws.onerror = (err) => {
      console.error("WebSocket error:", err);
      clearPolling();
      statusBadge.textContent = "Error";
      statusBadge.className = "status-badge disconnected";
      streamingBadge.textContent = "● 1 Hz Inactive";
      streamingBadge.className = "streaming-badge inactive";
      btnRefresh.disabled = true;
    };
  } catch (error) {
    console.error("Connection failed:", error);
    clearPolling();
    statusBadge.textContent = "Failed";
    statusBadge.className = "status-badge disconnected";
    streamingBadge.textContent = "● 1 Hz Inactive";
    streamingBadge.className = "streaming-badge inactive";
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

  // Record live time series snapshot
  recordLiveSample();

  updateUi();
}

function recordLiveSample() {
  const rssBytes = latestMetrics["Memory.Browser.ResidentSet.Live"] ??
                   latestMetrics["Memory.Browser.ResidentSet.P50"] ?? 0;
  const pmfBytes = latestMetrics["Memory.Browser.PrivateMemoryFootprint.Live"] ??
                   latestMetrics["Memory.Browser.PrivateMemoryFootprint.P50"] ?? 0;
  const v8Bytes = latestMetrics["Memory.Experimental.Browser2.V8.Live"] ??
                  latestMetrics["Memory.Experimental.Browser2.V8.P50"] ?? 0;

  liveHistory.push({
    timestamp: Date.now(),
    rss: rssBytes,
    pmf: pmfBytes,
    v8: v8Bytes,
  });

  if (liveHistory.length > MAX_HISTORY_SAMPLES) {
    liveHistory.shift();
  }

  // Update live legend values
  liveValRss.textContent = formatBytesToMB(rssBytes);
  liveValPmf.textContent = formatBytesToMB(pmfBytes);
  liveValV8.textContent = formatBytesToMB(v8Bytes);

  drawTimeSeriesChart();
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

// ============================================================================
// HTML5 Canvas Time Series Chart Rendering
// ============================================================================

function drawTimeSeriesChart() {
  if (!chartCanvas) return;

  const dpr = window.devicePixelRatio || 1;
  const rect = chartCanvas.getBoundingClientRect();
  const width = rect.width;
  const height = rect.height;

  chartCanvas.width = width * dpr;
  chartCanvas.height = height * dpr;

  const ctx = chartCanvas.getContext("2d");
  ctx.scale(dpr, dpr);
  ctx.clearRect(0, 0, width, height);

  const padding = { top: 12, right: 16, bottom: 24, left: 48 };
  const chartW = width - padding.left - padding.right;
  const chartH = height - padding.top - padding.bottom;

  // Compute Max Y value in MB across all series (min ceiling: 50 MB)
  let maxBytes = 50 * 1024 * 1024;
  liveHistory.forEach(s => {
    if (s.rss > maxBytes) maxBytes = s.rss;
  });
  const maxYMB = Math.ceil((maxBytes / (1024 * 1024)) * 1.15); // 15% headroom

  // 1. Draw Gridlines & Y-Axis Scale
  ctx.strokeStyle = "rgba(63, 63, 70, 0.4)";
  ctx.lineWidth = 1;
  ctx.fillStyle = "#a1a1aa";
  ctx.font = "10px ui-monospace, SFMono-Regular, monospace";
  ctx.textAlign = "right";
  ctx.textBaseline = "middle";

  const numGridLines = 4;
  for (let i = 0; i <= numGridLines; i++) {
    const yVal = padding.top + (chartH / numGridLines) * i;
    const mbVal = Math.round(maxYMB - (maxYMB / numGridLines) * i);

    ctx.beginPath();
    ctx.moveTo(padding.left, yVal);
    ctx.lineTo(width - padding.right, yVal);
    ctx.stroke();

    ctx.fillText(`${mbVal} MB`, padding.left - 6, yVal);
  }

  // 2. Draw X-Axis Time Labels
  ctx.textAlign = "center";
  ctx.textBaseline = "top";
  const timeLabels = ["-60s", "-45s", "-30s", "-15s", "Now"];
  timeLabels.forEach((lbl, idx) => {
    const xVal = padding.left + (chartW / (timeLabels.length - 1)) * idx;
    ctx.fillText(lbl, xVal, height - padding.bottom + 6);
  });

  if (liveHistory.length < 2) {
    ctx.fillStyle = "#71717a";
    ctx.font = "italic 12px sans-serif";
    ctx.textAlign = "center";
    ctx.fillText("Waiting for real-time memory stream...", padding.left + chartW / 2, padding.top + chartH / 2);
    return;
  }

  // Helper coordinate mapper
  function getPointCoords(index, valBytes) {
    const x = padding.left + (index / (MAX_HISTORY_SAMPLES - 1)) * chartW;
    const mb = valBytes / (1024 * 1024);
    const y = padding.top + chartH - (mb / maxYMB) * chartH;
    return { x, y };
  }

  // Draw Line Series with Fill
  function drawSeries(key, strokeColor, fillColor) {
    const startIdx = MAX_HISTORY_SAMPLES - liveHistory.length;

    // Line Path
    ctx.beginPath();
    liveHistory.forEach((sample, i) => {
      const { x, y } = getPointCoords(startIdx + i, sample[key]);
      if (i === 0) {
        ctx.moveTo(x, y);
      } else {
        ctx.lineTo(x, y);
      }
    });

    ctx.strokeStyle = strokeColor;
    ctx.lineWidth = 2;
    ctx.stroke();

    // Area Fill
    if (fillColor) {
      const lastPoint = getPointCoords(startIdx + liveHistory.length - 1, liveHistory[liveHistory.length - 1][key]);
      const firstPoint = getPointCoords(startIdx, liveHistory[0][key]);

      ctx.lineTo(lastPoint.x, padding.top + chartH);
      ctx.lineTo(firstPoint.x, padding.top + chartH);
      ctx.closePath();
      ctx.fillStyle = fillColor;
      ctx.fill();
    }

    // Draw endpoint circle at current "Now" sample
    const latest = liveHistory[liveHistory.length - 1];
    const endCoord = getPointCoords(MAX_HISTORY_SAMPLES - 1, latest[key]);
    ctx.beginPath();
    ctx.arc(endCoord.x, endCoord.y, 3.5, 0, Math.PI * 2);
    ctx.fillStyle = strokeColor;
    ctx.fill();
  }

  // Draw layers: V8 Heap -> Private Footprint -> Resident Set
  drawSeries("v8", "#fbbf24", "rgba(251, 191, 36, 0.1)");
  drawSeries("pmf", "#38bdf8", "rgba(56, 189, 248, 0.08)");
  drawSeries("rss", "#a855f7", "rgba(168, 85, 247, 0.05)");
}

function setupChartInteractivity() {
  if (!chartCanvas || !chartTooltip) return;

  chartCanvas.addEventListener("mousemove", (e) => {
    if (liveHistory.length === 0) return;

    const rect = chartCanvas.getBoundingClientRect();
    const mouseX = e.clientX - rect.left;
    const padding = { left: 48, right: 16 };
    const chartW = rect.width - padding.left - padding.right;

    if (mouseX < padding.left || mouseX > rect.width - padding.right) {
      chartTooltip.style.display = "none";
      return;
    }

    const relX = (mouseX - padding.left) / chartW;
    const sampleSlot = Math.round(relX * (MAX_HISTORY_SAMPLES - 1));
    const startIdx = MAX_HISTORY_SAMPLES - liveHistory.length;
    const historyIdx = sampleSlot - startIdx;

    if (historyIdx >= 0 && historyIdx < liveHistory.length) {
      const sample = liveHistory[historyIdx];
      const secondsAgo = Math.round((Date.now() - sample.timestamp) / 1000);
      const timeStr = secondsAgo <= 0 ? "Now" : `-${secondsAgo}s ago`;

      chartTooltip.innerHTML = `
        <div style="font-weight: 700; color: #fff; margin-bottom: 3px;">⏱ ${timeStr}</div>
        <div style="color: #d8b4fe;">● RSS: ${formatBytesToMB(sample.rss)}</div>
        <div style="color: #7dd3fc;">● PMF: ${formatBytesToMB(sample.pmf)}</div>
        <div style="color: #fde047;">● V8: ${formatBytesToMB(sample.v8)}</div>
      `;
      chartTooltip.style.display = "block";
    } else {
      chartTooltip.style.display = "none";
    }
  });

  chartCanvas.addEventListener("mouseleave", () => {
    chartTooltip.style.display = "none";
  });

  window.addEventListener("resize", () => {
    drawTimeSeriesChart();
  });
}

// Event Listeners
btnConnect.addEventListener("click", connectCDP);
btnRefresh.addEventListener("click", requestMetrics);
