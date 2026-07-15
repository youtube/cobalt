#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Converts Cobalt heaps_v2 memory dumps into an interactive HTML flamegraph.

Generates a standalone, feature-rich HTML report with interactive SVG
flamegraphs, module color hashing, differential snapshot comparison (delta diff
mode), allocator tab filtering, minimum size thresholding, and symbol search.
Open the resulting .html file directly in any modern browser.
"""

import argparse
import json
import os
import sys
from typing import Any, Dict, List, Tuple


def parse_heaps_v2_snapshots(
    trace_path: str,) -> Tuple[List[Tuple[int, Dict[str, Any]]], bool]:
  """Extracts heaps_v2 snapshots from a JSON trace file."""
  if not os.path.exists(trace_path):
    print(f"❌ Error: Trace file not found at {trace_path}")
    return [], False

  try:
    with open(trace_path, "r", encoding="utf-8", errors="replace") as f:
      data = json.load(f)
  except (json.JSONDecodeError, OSError) as err:
    print(f"❌ Error reading trace file: {err}")
    return [], False

  if isinstance(data, list):
    events = data
  elif isinstance(data, dict):
    events = data.get("traceEvents") or []
  else:
    events = []

  snapshots = []

  for e in events:
    if not isinstance(e, dict):
      continue
    if e.get("name") not in ["periodic_interval", "memory_dump"]:
      continue
    args = e.get("args") or {}
    dumps = args.get("dumps") or {}
    if isinstance(dumps, str):
      try:
        dumps = json.loads(dumps)
      except json.JSONDecodeError:
        continue
    if isinstance(dumps, dict) and "heaps_v2" in dumps:
      snapshots.append((e.get("ts", 0), dumps["heaps_v2"]))

  if not snapshots:
    print("❌ Error: No heaps_v2 memory snapshots found in trace.")
    return [], False

  return snapshots, True


def build_tree_for_allocator(heaps: Dict[str, Any],
                             alloc_name: str) -> Dict[str, Any]:
  """Constructs hierarchical flamegraph tree for a specific allocator."""
  maps = heaps.get("maps") or {}
  nodes = maps.get("nodes") or []
  raw_strings = maps.get("strings") or []
  sid_to_text = {
      e.get("id"): e.get("string", "")
      for e in raw_strings
      if isinstance(e, dict) and "id" in e
  }
  node_dict = {
      n.get("id"): n for n in nodes if isinstance(n, dict) and "id" in n
  }

  allocators = heaps.get("allocators") or {}
  alloc_obj = allocators.get(alloc_name) or {}
  if not isinstance(alloc_obj, dict):
    return {"name": alloc_name, "value": 0, "count": 0, "children": []}

  alloc_nodes = alloc_obj.get("nodes") or []
  col_sizes = alloc_obj.get("sizes", alloc_obj.get("bytes", []))
  col_counts = alloc_obj.get("counts", alloc_obj.get("objects", []))

  root = {"name": alloc_name, "value": 0, "count": 0, "children": {}}

  def add_stack_to_root(stack: List[str], size: int, count: int):
    root["value"] += size
    root["count"] += count
    curr = root
    for frame in reversed(stack):
      if frame not in curr["children"]:
        curr["children"][frame] = {
            "name": frame,
            "value": 0,
            "count": 0,
            "children": {},
        }
      curr = curr["children"][frame]
      curr["value"] += size
      curr["count"] += count

  if (isinstance(alloc_nodes, list) and alloc_nodes and
      not isinstance(alloc_nodes[0], dict)):
    for idx, nid in enumerate(alloc_nodes):
      try:
        sz = (
            int(col_sizes[idx])
            if isinstance(col_sizes, list) and idx < len(col_sizes) else 0)
        cnt = (
            int(col_counts[idx])
            if isinstance(col_counts, list) and idx < len(col_counts) else 1)
      except (ValueError, TypeError):
        continue
      if sz <= 0:
        continue

      stack = []
      curr_id = nid
      visited = set()
      while curr_id in node_dict and curr_id not in visited:
        visited.add(curr_id)
        n = node_dict[curr_id]
        name_sid = n.get("name_sid")
        nm = sid_to_text.get(name_sid, f"Node_{curr_id}")
        stack.append(nm)
        curr_id = n.get("parent") or n.get("parent_id", 0)

      add_stack_to_root(stack, sz, cnt)
  elif isinstance(alloc_nodes, list):
    for an in alloc_nodes:
      if not isinstance(an, dict):
        continue
      nid = an.get("node_id", 0)
      try:
        sz = int(an.get("size", 0))
        cnt = int(an.get("count", 1))
      except (ValueError, TypeError):
        continue
      if sz <= 0:
        continue

      stack = []
      curr_id = nid
      visited = set()
      while curr_id in node_dict and curr_id not in visited:
        visited.add(curr_id)
        n = node_dict[curr_id]
        name_sid = n.get("name_sid")
        nm = sid_to_text.get(name_sid, f"Node_{curr_id}")
        stack.append(nm)
        curr_id = n.get("parent") or n.get("parent_id", 0)

      add_stack_to_root(stack, sz, cnt)

  def format_node(node: Dict[str, Any]) -> Dict[str, Any]:
    children_list = [format_node(child) for child in node["children"].values()]
    children_list.sort(key=lambda x: x["value"], reverse=True)
    return {
        "name": node["name"],
        "value": node["value"],
        "count": node["count"],
        "children": children_list,
    }

  return format_node(root)


def generate_html_report(snapshots: List[Tuple[int, Dict[str, Any]]]) -> str:
  """Constructs the full HTML report with embedded visualization logic."""
  processed_snapshots = []
  min_ts = snapshots[0][0] if snapshots else 0

  for idx, (ts_us, heaps) in enumerate(snapshots):
    rel_ts_sec = (ts_us - min_ts) / 1000000.0
    allocators = list(heaps.get("allocators", {}).keys())

    alloc_trees = {}
    for alloc_name in allocators:
      alloc_trees[alloc_name] = build_tree_for_allocator(heaps, alloc_name)

    total_value = sum(t["value"] for t in alloc_trees.values())
    total_count = sum(t["count"] for t in alloc_trees.values())
    combined_tree = {
        "name": "Total Heap Memory",
        "value": total_value,
        "count": total_count,
        "children": list(alloc_trees.values()),
    }
    alloc_trees["all"] = combined_tree

    processed_snapshots.append({
        "id": idx,
        "timestamp_sec": round(rel_ts_sec, 3),
        "total_bytes": total_value,
        "allocators": alloc_trees,
    })

  snapshots_json = json.dumps(processed_snapshots)

  html_template = (
      "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
      "  <meta charset=\"UTF-8\">\n"
      "  <title>Cobalt Heap Memory Flamegraph</title>\n"
      "  <style>\n"
      "    :root {\n"
      "      --bg: #11111b; --card: #1e1e2e; --border: #313244;\n"
      "      --text: #cdd6f4; --subtext: #a6adc8; --accent: #89b4fa;\n"
      "      --highlight: #f9e2af; --green: #a6e3a1; --red: #f38ba8;\n"
      "    }\n"
      "    body {\n"
      "      font-family: -apple-system, Roboto, sans-serif;\n"
      "      background-color: var(--bg); color: var(--text);\n"
      "      margin: 0; padding: 20px;\n"
      "    }\n"
      "    .header {\n"
      "      display: flex; justify-content: space-between;\n"
      "      align-items: center; margin-bottom: 15px;\n"
      "    }\n"
      "    h1 { margin: 0; font-size: 22px; color: var(--accent); }\n"
      "    .controls {\n"
      "      display: flex; gap: 12px; align-items: center;\n"
      "      flex-wrap: wrap; margin-bottom: 15px;\n"
      "    }\n"
      "    select, input {\n"
      "      background: var(--card); color: var(--text);\n"
      "      border: 1px solid var(--border);\n"
      "      padding: 7px 11px; border-radius: 6px; font-size: 13px;\n"
      "    }\n"
      "    .stat-badge {\n"
      "      background: var(--card); border: 1px solid var(--border);\n"
      "      padding: 7px 12px; border-radius: 6px; font-weight: bold;\n"
      "      color: var(--accent);\n"
      "    }\n"
      "    #flamegraph-container {\n"
      "      background: var(--card); border: 1px solid var(--border);\n"
      "      border-radius: 8px; padding: 15px; overflow-x: auto;\n"
      "      min-height: 520px; position: relative;\n"
      "    }\n"
      "    .flame-row {\n"
      "      display: flex; margin-bottom: 2px; height: 22px;\n"
      "      width: 100%;\n"
      "    }\n"
      "    .flame-bar {\n"
      "      box-sizing: border-box; border: 1px solid rgba(0,0,0,0.3);\n"
      "      border-radius: 3px; font-size: 11px; line-height: 20px;\n"
      "      padding: 0 4px; overflow: hidden; text-overflow: ellipsis;\n"
      "      white-space: nowrap; cursor: pointer; transition: all 0.1s;\n"
      "    }\n"
      "    .flame-bar:hover {\n"
      "      filter: brightness(1.25); outline: 2px solid var(--accent);\n"
      "    }\n"
      "    .match {\n"
      "      outline: 2px solid var(--highlight) !important;\n"
      "      filter: brightness(1.35);\n"
      "    }\n"
      "    #tooltip {\n"
      "      position: absolute; display: none; background: #181825;\n"
      "      border: 1px solid var(--accent); color: #cdd6f4;\n"
      "      padding: 10px; border-radius: 6px; font-size: 12px;\n"
      "      pointer-events: none; z-index: 1000;\n"
      "      box-shadow: 0 4px 14px rgba(0,0,0,0.6);\n"
      "    }\n"
      "    .btn {\n"
      "      background: var(--card); color: var(--text);\n"
      "      border: 1px solid var(--border); padding: 7px 11px;\n"
      "      border-radius: 6px; cursor: pointer;\n"
      "    }\n"
      "    .btn:hover { background: var(--border); }\n"
      "  </style>\n</head>\n<body>\n"
      "  <div class=\"header\">\n"
      "    <h1>🔥 Cobalt Heap Memory Flamegraph Visualizer</h1>\n"
      "    <div id=\"summary-badge\" class=\"stat-badge\">Total: 0 B</div>\n"
      "  </div>\n"
      "  <div class=\"controls\">\n"
      "    <label>Snapshot:\n"
      "      <select id=\"snapshot-select\" onchange=\"render()\"></select>\n"
      "    </label>\n"
      "    <label>Compare Diff With:\n"
      "      <select id=\"diff-select\" onchange=\"render()\"></select>\n"
      "    </label>\n"
      "    <label>Allocator:\n"
      "      <select id=\"allocator-select\" onchange=\"render()\"></select>\n"
      "    </label>\n"
      "    <label>Min Size:\n"
      "      <select id=\"min-size-select\" onchange=\"render()\">\n"
      "        <option value=\"0\">Show All (0 KB+)</option>\n"
      "        <option value=\"65536\">> 64 KB</option>\n"
      "        <option value=\"262144\" selected>> 256 KB</option>\n"
      "        <option value=\"1048576\">> 1 MB</option>\n"
      "      </select>\n"
      "    </label>\n"
      "    <input type=\"text\" id=\"search-box\"\n"
      "           placeholder=\"Search symbol...\"\n"
      "           oninput=\"highlightMatches()\">\n"
      "    <button class=\"btn\" onclick=\"resetZoom()\">Reset Zoom</button>\n"
      "  </div>\n"
      "  <div id=\"flamegraph-container\"></div>\n"
      "  <div id=\"tooltip\"></div>\n"
      "  <script>\n"
      f"    const snapshots = {snapshots_json};\n"
      "    let currentZoomNode = null;\n"
      "    function formatBytes(bytes) {\n"
      "      if (bytes === 0) return '0 B';\n"
      "      const sign = bytes < 0 ? '-' : '';\n"
      "      const absVal = Math.abs(bytes);\n"
      "      const k = 1024;\n"
      "      const sizes = ['B', 'KB', 'MB', 'GB'];\n"
      "      const idx =\n"
      "          Math.floor(Math.log(absVal) / Math.log(k));\n"
      "      const i = Math.min(idx, sizes.length - 1);\n"
      "      return sign + (absVal / Math.pow(k, i)).toFixed(2) + ' ' +"
      " sizes[i];\n"
      "    }\n"
      "    function init() {\n"
      "      const snapSelect = document.getElementById('snapshot-select');\n"
      "      const diffSelect = document.getElementById('diff-select');\n"
      "      diffSelect.innerHTML = '<option value=\"none\">None (Single' +"
      " ' Snapshot)</option>';\n"
      "      snapshots.forEach((s, idx) => {\n"
      "        const opt = document.createElement('option');\n"
      "        opt.value = idx;\n"
      "        opt.textContent = `Snapshot ${idx + 1} (t=${s.timestamp_sec}s -`"
      " + ` ${formatBytes(s.total_bytes)})`;\n"
      "        snapSelect.appendChild(opt);\n"
      "        const dOpt = document.createElement('option');\n"
      "        dOpt.value = idx;\n"
      "        dOpt.textContent =\n"
      "            `Snapshot ${idx + 1} (t=${s.timestamp_sec}s)`;\n"
      "        diffSelect.appendChild(dOpt);\n"
      "      });\n"
      "      updateAllocators();\n"
      "      render();\n"
      "    }\n"
      "    function updateAllocators() {\n"
      "      const sel = document.getElementById('snapshot-select');\n"
      "      const snapIdx = sel.value;\n"
      "      const allocSelect = document.getElementById('allocator-select');\n"
      "      allocSelect.innerHTML = '';\n"
      "      const allocs = snapshots[snapIdx].allocators;\n"
      "      Object.keys(allocs).forEach(name => {\n"
      "        const opt = document.createElement('option');\n"
      "        opt.value = name;\n"
      "        opt.textContent = name === 'all' ? 'All Allocators' : name;\n"
      "        allocSelect.appendChild(opt);\n"
      "      });\n"
      "    }\n"
      "    function hashColor(name) {\n"
      "      const s = name.toLowerCase();\n"
      "      if (s.includes('starboard') || s.includes('cobalt')) {\n"
      "        return 'hsl(215, 75%, 45%)';\n"
      "      }\n"
      "      if (s.includes('blink') || s.includes('wtf')) {\n"
      "        return 'hsl(145, 60%, 38%)';\n"
      "      }\n"
      "      if (s.includes('v8') || s.includes('js')) {\n"
      "        return 'hsl(280, 65%, 42%)';\n"
      "      }\n"
      "      if (s.includes('skia') || s.includes('gpu')) {\n"
      "        return 'hsl(25, 75%, 45%)';\n"
      "      }\n"
      "      let hash = 0;\n"
      "      for (let i = 0; i < name.length; i++) {\n"
      "        hash = name.charCodeAt(i) + ((hash << 5) - hash);\n"
      "      }\n"
      "      const h = Math.abs(hash) % 360;\n"
      "      return `hsl(${h}, 50%, 40%)`;\n"
      "    }\n"
      "    function computeDiffTree(target, base) {\n"
      "      const baseChildrenMap = {};\n"
      "      if (base && base.children) {\n"
      "        base.children.forEach(c => { baseChildrenMap[c.name] = c; });\n"
      "      }\n"
      "      const targetValue = target ? target.value : 0;\n"
      "      const baseValue = base ? base.value : 0;\n"
      "      const diffValue = targetValue - baseValue;\n"
      "      const targetCount = target ? target.count : 0;\n"
      "      const baseCount = base ? base.count : 0;\n"
      "      const name = target ? target.name : (base ? base.name : 'root');\n"
      "      const children = [];\n"
      "      const targetChildrenMap = {};\n"
      "      if (target && target.children) {\n"
      "        target.children.forEach(c => {\n"
      "          targetChildrenMap[c.name] = c;\n"
      "          children.push(computeDiffTree(c, baseChildrenMap[c.name]));\n"
      "        });\n"
      "      }\n"
      "      if (base && base.children) {\n"
      "        base.children.forEach(c => {\n"
      "          if (!targetChildrenMap[c.name]) {\n"
      "            children.push(computeDiffTree(null, c));\n"
      "          }\n"
      "        });\n"
      "      }\n"
      "      children.sort((a, b) =>\n"
      "          Math.abs(b.diffValue) - Math.abs(a.diffValue));\n"
      "      return {\n"
      "        name,\n"
      "        value: Math.max(targetValue, baseValue),\n"
      "        diffValue,\n"
      "        targetValue,\n"
      "        baseValue,\n"
      "        count: targetCount - baseCount,\n"
      "        children\n"
      "      };\n"
      "    }\n"
      "    function render() {\n"
      "      const sEl = document.getElementById('snapshot-select');\n"
      "      const dEl = document.getElementById('diff-select');\n"
      "      const aEl = document.getElementById('allocator-select');\n"
      "      const mEl = document.getElementById('min-size-select');\n"
      "      const snapIdx = sEl.value;\n"
      "      const diffIdx = dEl.value;\n"
      "      const allocName = aEl.value || 'all';\n"
      "      const minSize = parseInt(mEl.value, 10);\n"
      "      const snap = snapshots[snapIdx];\n"
      "      let rootNode =\n"
      "          snap.allocators[allocName] || snap.allocators.all;\n"
      "      const isDiffMode = diffIdx !== 'none';\n"
      "      if (isDiffMode) {\n"
      "        const baseSnap = snapshots[diffIdx];\n"
      "        const baseRoot =\n"
      "            baseSnap.allocators[allocName] || baseSnap.allocators.all;\n"
      "        rootNode = computeDiffTree(rootNode, baseRoot);\n"
      "      }\n"
      "      const badge = document.getElementById('summary-badge');\n"
      "      if (isDiffMode) {\n"
      "        const sign = rootNode.diffValue >= 0 ? '+' : '';\n"
      "        const dFrom = parseInt(diffIdx) + 1;\n"
      "        const dTo = parseInt(snapIdx) + 1;\n"
      "        const diffLbl = `Diff (S${dFrom} -> S${dTo}): `;\n"
      "        badge.textContent =\n"
      "            `${diffLbl}${sign}${formatBytes(rootNode.diffValue)}`;\n"
      "        badge.style.color =\n"
      "            rootNode.diffValue > 0 ? 'var(--red)' : 'var(--green)';\n"
      "      } else {\n"
      "        badge.textContent = `Total: ${formatBytes(rootNode.value)}` +\n"
      "                            ` (${rootNode.count} allocs)`;\n"
      "        badge.style.color = 'var(--accent)';\n"
      "      }\n"
      "      const focusNode = currentZoomNode || rootNode;\n"
      "      const container =\n"
      "          document.getElementById('flamegraph-container');\n"
      "      container.innerHTML = '';\n"
      "      const levels = [];\n"
      "      function traverse(node, depth, leftPct, widthPct) {\n"
      "        if (widthPct < 0.05) return;\n"
      "        const belowMin = node.value < minSize;\n"
      "        const belowDiff =\n"
      "            !isDiffMode || Math.abs(node.diffValue) < minSize;\n"
      "        if (belowMin && belowDiff) return;\n"
      "        if (!levels[depth]) levels[depth] = [];\n"
      "        levels[depth].push({ node, leftPct, widthPct });\n"
      "        if (node.children) {\n"
      "          let currentLeft = leftPct;\n"
      "          const totalVal = node.value || 1;\n"
      "          node.children.forEach(child => {\n"
      "            const childWidth = (child.value / totalVal) * widthPct;\n"
      "            traverse(child, depth + 1, currentLeft, childWidth);\n"
      "            currentLeft += childWidth;\n"
      "          });\n"
      "        }\n"
      "      }\n"
      "      traverse(focusNode, 0, 0, 100);\n"
      "      levels.forEach(level => {\n"
      "        const row = document.createElement('div');\n"
      "        row.className = 'flame-row';\n"
      "        level.forEach(item => {\n"
      "          const bar = document.createElement('div');\n"
      "          bar.className = 'flame-bar';\n"
      "          bar.style.width = item.widthPct + '%';\n"
      "          bar.style.marginLeft =\n"
      "              (item.leftPct - (row.lastLeft || 0)) + '%';\n"
      "          row.lastLeft = item.leftPct + item.widthPct;\n"
      "          if (isDiffMode) {\n"
      "            const dv = item.node.diffValue || 0;\n"
      "            if (dv > 0) {\n"
      "              bar.style.backgroundColor = 'hsl(343, 70%, 40%)';\n"
      "            } else if (dv < 0) {\n"
      "              bar.style.backgroundColor = 'hsl(140, 60%, 35%)';\n"
      "            } else {\n"
      "              bar.style.backgroundColor = 'hsl(230, 20%, 30%)';\n"
      "            }\n"
      "            const sign = dv >= 0 ? '+' : '';\n"
      "            bar.textContent =\n"
      "                `${item.node.name} (${sign}${formatBytes(dv)})`;\n"
      "          } else {\n"
      "            bar.style.backgroundColor = hashColor(item.node.name);\n"
      "            bar.textContent =\n"
      "                `${item.node.name} (${formatBytes(item.node.value)})`;\n"
      "          }\n"
      "          bar.onmouseover = (e) =>\n"
      "              showTooltip(e, item.node, focusNode.value, isDiffMode);\n"
      "          bar.onmousemove = moveTooltip;\n"
      "          bar.onmouseout = hideTooltip;\n"
      "          bar.onclick =\n"
      "              () => { currentZoomNode = item.node; render(); };\n"
      "          row.appendChild(bar);\n"
      "        });\n"
      "        container.appendChild(row);\n"
      "      });\n"
      "      highlightMatches();\n"
      "    }\n"
      "    function escapeHtml(str) {\n"
      "      if (!str) return '';\n"
      "      return String(str)\n"
      "          .replace(/&/g, '&amp;')\n"
      "          .replace(/</g, '&lt;')\n"
      "          .replace(/>/g, '&gt;')\n"
      "          .replace(/\"/g, '&quot;')\n"
      "          .replace(/'/g, '&#039;');\n"
      "    }\n"
      "    function showTooltip(e, node, totalVal, isDiffMode) {\n"
      "      const tt = document.getElementById('tooltip');\n"
      "      const pct = ((node.value / (totalVal || 1)) * 100).toFixed(2);\n"
      "      const safeName = escapeHtml(node.name);\n"
      "      if (isDiffMode) {\n"
      "        const sign = node.diffValue >= 0 ? '+' : '';\n"
      "        const clr = node.diffValue >= 0 ?\n"
      "            'var(--red)' : 'var(--green)';\n"
      "        const tgtStr = formatBytes(node.targetValue || 0);\n"
      "        const baseStr = formatBytes(node.baseValue || 0);\n"
      "        tt.innerHTML = `<b>${safeName}</b><br>` +\n"
      "            `Delta Diff: <b style=\"color:${clr}\">${sign}` +\n"
      "            `${formatBytes(node.diffValue)}</b><br>` +\n"
      "            `Target Snapshot: ${tgtStr}<br>` +\n"
      "            `Base Snapshot: ${baseStr}<br>` +\n"
      "            `Net Count Delta: ${node.count}`;\n"
      "      } else {\n"
      "        tt.innerHTML = `<b>${safeName}</b><br>` +\n"
      "            `Size: ${formatBytes(node.value)} (${pct}%)<br>` +\n"
      "            `Allocations: ${node.count}`;\n"
      "      }\n"
      "      tt.style.display = 'block';\n"
      "      moveTooltip(e);\n"
      "    }\n"
      "    function moveTooltip(e) {\n"
      "      const tt = document.getElementById('tooltip');\n"
      "      tt.style.left = (e.pageX + 15) + 'px';\n"
      "      tt.style.top = (e.pageY + 15) + 'px';\n"
      "    }\n"
      "    function hideTooltip() {\n"
      "      document.getElementById('tooltip').style.display = 'none';\n"
      "    }\n"
      "    function resetZoom() {\n"
      "      currentZoomNode = null;\n"
      "      render();\n"
      "    }\n"
      "    function highlightMatches() {\n"
      "      const box = document.getElementById('search-box');\n"
      "      const query = box.value.toLowerCase();\n"
      "      document.querySelectorAll('.flame-bar').forEach(bar => {\n"
      "        if (query && bar.textContent.toLowerCase().includes(query)) {\n"
      "          bar.classList.add('match');\n"
      "        } else {\n"
      "          bar.classList.remove('match');\n"
      "        }\n"
      "      });\n"
      "    }\n"
      "    window.onload = init;\n"
      "  </script>\n</body>\n</html>")
  return html_template


def convert_trace_to_html(trace_path: str, output_path: str) -> bool:
  """Converts a Cobalt JSON trace file into an interactive HTML report."""
  snapshots, success = parse_heaps_v2_snapshots(trace_path)
  if not success:
    return False

  html_content = generate_html_report(snapshots)

  try:
    with open(output_path, "w", encoding="utf-8") as f:
      f.write(html_content)
    print(f"🎉 Interactive HTML Flamegraph generated: {output_path}")
    print(
        f"   (Converted {len(snapshots)} snapshots, {len(html_content)} bytes)")
    print(f"   💡 Open file://{output_path} in your browser to view!")
    return True
  except OSError as err:
    print(f"❌ Error writing output file {output_path}: {err}")
    return False


def main():
  parser = argparse.ArgumentParser(
      description=(
          "Convert Cobalt heaps_v2 memory dumps into an interactive HTML\n"
          "flamegraph report for browser visualization."))
  parser.add_argument(
      "trace_path", help="Path to the JSON trace file (symbolized recommended)")
  parser.add_argument(
      "-o",
      "--output_path",
      help="Output .html file path (defaults to <name>.html)",
  )
  args = parser.parse_args()

  trace_path = os.path.abspath(args.trace_path)
  if args.output_path:
    output_path = os.path.abspath(args.output_path)
  else:
    base_name = os.path.splitext(trace_path)[0]
    output_path = base_name + ".html"

  success = convert_trace_to_html(trace_path, output_path)
  sys.exit(0 if success else 1)


if __name__ == "__main__":
  main()
