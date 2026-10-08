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
"""Captures Perfetto traces with memory-infra from a running Cobalt instance.

Connects to Cobalt's Chrome DevTools Protocol (CDP) endpoint on localhost:9222
(or specified host:port), activates the `disabled-by-default-memory-infra`
tracing category, collects periodic or explicit memory dumps, and streams the
resulting trace in protobuf format (compatible with https://ui.perfetto.dev).

Usage:
  # Collect a 10-second memory-infra trace on localhost:9222
  python3 cobalt/tools/performance/memory/cdp_meminfra_tracing.py

  # Collect a 30-second trace with periodic memory dumps every 500ms
  python3 cobalt/tools/performance/memory/cdp_meminfra_tracing.py \
      --duration 30 --periodic-interval 500

  # Interactive mode: run until Ctrl+C or Enter is pressed
  python3 cobalt/tools/performance/memory/cdp_meminfra_tracing.py --interactive

  # Specify a custom output path and gzip compression
  python3 cobalt/tools/performance/memory/cdp_meminfra_tracing.py \
      --output my_trace.pftrace.gz --compress

  # Add extra categories
  python3 cobalt/tools/performance/memory/cdp_meminfra_tracing.py \
      --categories blink.console,v8
"""

import argparse
import asyncio
import base64
import json
import os
import signal
import sys
import time
import urllib.error
import urllib.request
from typing import Any, Dict, List, Optional

try:
  import websockets
except ImportError:
  print(
      "Error: The 'websockets' package is required but not installed.\n"
      "Install it using: pip install websockets"
      " (or sudo apt install python3-websockets)",
      file=sys.stderr,
  )
  sys.exit(1)


class CDPClient:
  """Asynchronous Chrome DevTools Protocol client over WebSocket.

  This class manages sending commands and receiving events/responses over a
  WebSocket connection to a running Cobalt/Chromium instance.

  Lifetime and Ownership:
    Instances are created and owned by the tracing orchestrator (e.g.,
    capture_meminfra_trace) and their lifetime is tied to the active tracing
    session.

  Threading Model:
    This class is designed to be used within an asyncio event loop and is
    Thread-affine to the thread running the event loop.
  """

  def __init__(self, ws: Any):
    self.ws = ws
    self._next_id = 1
    self._pending_commands: Dict[int, asyncio.Future] = {}
    self._event_queues: Dict[str, asyncio.Queue] = {}
    self._read_task = asyncio.create_task(self._listen_loop())

  async def _listen_loop(self):
    """Continuously reads incoming WebSocket messages and routes them."""
    try:
      async for raw_msg in self.ws:
        try:
          msg = json.loads(raw_msg)
        except json.JSONDecodeError:
          continue

        msg_id = msg.get("id")
        if msg_id is not None and msg_id in self._pending_commands:
          fut = self._pending_commands.pop(msg_id)
          if not fut.done():
            fut.set_result(msg)
        elif "method" in msg:
          method = msg["method"]
          if method in self._event_queues:
            await self._event_queues[method].put(msg)
    except (asyncio.CancelledError, websockets.ConnectionClosed):
      pass
    finally:
      # Cancel any remaining pending futures on disconnect
      for fut in self._pending_commands.values():
        if not fut.done():
          fut.cancel()
      self._pending_commands.clear()

  async def send_command(
      self,
      method: str,
      params: Optional[Dict[str, Any]] = None,
      timeout: float = 60.0,
  ) -> Dict[str, Any]:
    """Sends a CDP command and awaits its result."""
    cmd_id = self._next_id
    self._next_id += 1
    future = asyncio.get_running_loop().create_future()
    self._pending_commands[cmd_id] = future

    msg: Dict[str, Any] = {"id": cmd_id, "method": method}
    if params is not None:
      msg["params"] = params

    await self.ws.send(json.dumps(msg))
    try:
      resp = await asyncio.wait_for(future, timeout=timeout)
    except asyncio.TimeoutError as e:
      self._pending_commands.pop(cmd_id, None)
      raise TimeoutError(
          f"Timed out waiting for response to CDP command: {method}") from e

    if "error" in resp:
      err = resp["error"]
      raise RuntimeError(f"CDP error in {method}: {err}")
    return resp.get("result", {})

  def listen_for_event(self, event_name: str) -> asyncio.Queue:
    """Registers a listener queue for a CDP event notification."""
    if event_name not in self._event_queues:
      self._event_queues[event_name] = asyncio.Queue()
    return self._event_queues[event_name]

  async def close(self):
    """Closes the client and background listener."""
    self._read_task.cancel()
    try:
      await self._read_task
    except asyncio.CancelledError:
      pass


def get_websocket_debugger_url(
    host: str,
    port: int,
    target_type: str = "auto",
    timeout: float = 5.0,
) -> str:
  """Retrieves the WebSocket debugger URL from the CDP HTTP endpoint.

  Args:
    host: DevTools host name or IP address.
    port: DevTools HTTP port.
    target_type: 'browser', 'page', or 'auto'. Browser target is preferred for
      system-wide tracing; page target is used as fallback.
    timeout: HTTP request timeout in seconds.

  Returns:
    The websocket URL string.

  Raises:
    RuntimeError: If no suitable target or websocket URL could be found.
  """
  # 1. Try Browser target (/json/version)
  if target_type in ("auto", "browser"):
    try:
      url = f"http://{host}:{port}/json/version"
      req = urllib.request.Request(url)
      with urllib.request.urlopen(req, timeout=timeout) as resp:
        data = json.loads(resp.read().decode("utf-8"))
        ws_url = data.get("webSocketDebuggerUrl")
        if ws_url:
          return ws_url
    except Exception as exc:  # pylint: disable=broad-exception-caught
      if target_type == "browser":
        raise RuntimeError(f"Failed to retrieve browser target from "
                           f"http://{host}:{port}/json/version") from exc

  # 2. Try Targets list (/json or /json/list)
  try:
    url = f"http://{host}:{port}/json"
    req = urllib.request.Request(url)
    with urllib.request.urlopen(req, timeout=timeout) as resp:
      targets = json.loads(resp.read().decode("utf-8"))

    # If page requested or auto, prefer page target, then browser target
    chosen_target = None
    for t in targets:
      if t.get("type") == "page" and t.get("webSocketDebuggerUrl"):
        chosen_target = t
        break
    if not chosen_target:
      for t in targets:
        if t.get("webSocketDebuggerUrl"):
          chosen_target = t
          break

    if chosen_target and chosen_target.get("webSocketDebuggerUrl"):
      return chosen_target["webSocketDebuggerUrl"]
  except Exception as e:  # pylint: disable=broad-exception-caught
    raise RuntimeError(
        f"Failed to query CDP targets from http://{host}:{port}/json: {e}"
    ) from e

  raise RuntimeError(
      f"No active targets with webSocketDebuggerUrl found on {host}:{port}")


async def wait_for_cdp(
    host: str,
    port: int,
    poll_interval: float = 1.0,
    total_wait: float = 30.0,
) -> bool:
  """Waits for the CDP endpoint to become accessible."""
  start_time = time.monotonic()
  while time.monotonic() - start_time < total_wait:
    try:
      get_websocket_debugger_url(host, port, timeout=poll_interval)
      return True
    except Exception:  # pylint: disable=broad-exception-caught
      await asyncio.sleep(poll_interval)
  return False


PRESET_CATEGORIES: Dict[str, List[str]] = {
    "minimal": ["disabled-by-default-memory-infra",],
    "memory": [
        "disabled-by-default-memory-infra",
        "disabled-by-default-memory-infra.detailed",
    ],
    "rdk": [
        "disabled-by-default-memory-infra",
        "disabled-by-default-memory-infra.detailed",
        "disabled-by-default-cc.debug",
        "gpu",
        "cc",
        "skia",
        "blink",
        "v8",
    ],
}


# pylint: disable=too-many-positional-arguments,too-many-arguments
async def capture_meminfra_trace(
    host: str = "localhost",
    port: int = 9222,
    duration: float = 10.0,
    output_path: str = "meminfra_trace.perfetto-trace",
    periodic_interval_ms: int = 1000,
    level_of_detail: str = "detailed",
    deterministic: bool = True,
    request_dump: bool = True,
    preset: str = "memory",
    extra_categories: Optional[List[str]] = None,
    buffer_size_kb: int = 200000,
    record_mode: str = "recordUntilFull",
    compress: bool = False,
    interactive: bool = False,
) -> str:
  """Connects to Cobalt via CDP and records a Perfetto memory-infra trace.

  Args:
    host: DevTools host.
    port: DevTools port.
    duration: Recording duration in seconds. 0 means record until interrupted.
    output_path: Destination file path for trace data.
    periodic_interval_ms: Interval between periodic memory dumps in ms. 0
      disables periodic triggers.
    level_of_detail: 'detailed', 'light', or 'background'.
    deterministic: Force GC before explicit memory dumps for consistency.
    request_dump: Explicitly request global memory dump(s).
    preset: Category preset ('memory', 'rdk', 'minimal').
    extra_categories: Additional category strings to record.
    buffer_size_kb: Tracing buffer size in KB.
    record_mode: CDP recordMode ('recordUntilFull', 'recordContinuously', etc.).
    compress: Compress trace stream using gzip.
    interactive: If True, waits for user input (Enter or Ctrl+C) to stop.

  Returns:
    The output file path.
  """
  ws_url = get_websocket_debugger_url(host, port)
  print(f"[CDP] Connecting to: {ws_url}")

  # Build categories list from preset + extras
  base_cats = PRESET_CATEGORIES.get(preset, PRESET_CATEGORIES["memory"])
  categories = list(base_cats)
  if extra_categories:
    for cat in extra_categories:
      cat = cat.strip()
      if cat and cat not in categories:
        categories.append(cat)

  # Build traceConfig
  trace_config: Dict[str, Any] = {
      "recordMode": record_mode,
      "traceBufferSizeInKb": buffer_size_kb,
      "includedCategories": categories,
  }

  if periodic_interval_ms > 0:
    trace_config["memoryDumpConfig"] = {
        "triggers": [{
            "mode": level_of_detail,
            "periodic_interval_ms": periodic_interval_ms,
        }]
    }

  stream_compression = "gzip" if compress else "none"
  start_params: Dict[str, Any] = {
      "transferMode": "ReturnAsStream",
      "streamFormat": "proto",
      "streamCompression": stream_compression,
      "traceConfig": trace_config,
  }

  async with websockets.connect(
      ws_url, max_size=None, ping_interval=None) as ws:
    client = CDPClient(ws)
    tracing_complete_queue = client.listen_for_event("Tracing.tracingComplete")

    stop_event = asyncio.Event()

    # Setup signal handler for graceful interruption
    loop = asyncio.get_running_loop()

    def handle_sigint():
      print(
          "\n[CDP] Interruption signal received. Stopping trace gracefully...")
      stop_event.set()

    for sig in (signal.SIGINT, signal.SIGTERM):
      try:
        loop.add_signal_handler(sig, handle_sigint)
      except (NotImplementedError, RuntimeError):
        pass

    try:
      cats_str = ", ".join(categories)
      print(f"[CDP] Starting Perfetto trace (categories: {cats_str})...")
      if periodic_interval_ms > 0:
        print(f"[CDP] Periodic memory dumps configured: every"
              f" {periodic_interval_ms}ms ({level_of_detail})")
      else:
        print("[CDP] Periodic memory dumps disabled")

      try:
        await client.send_command("Tracing.start", start_params)
      except RuntimeError as e:
        if "already been started" in str(e).lower():
          print(
              "[CDP] Stale tracing session detected; stopping previous session"
              " and retrying...")
          try:
            await client.send_command("Tracing.end")
            await asyncio.wait_for(tracing_complete_queue.get(), timeout=10.0)
          except Exception:  # pylint: disable=broad-exception-caught
            pass
          await asyncio.sleep(1.0)
          await client.send_command("Tracing.start", start_params)
        else:
          raise
      print("[CDP] Tracing active.")

      if request_dump:
        try:
          print(
              f"[CDP] Requesting initial memory dump (level: {level_of_detail},"
              f" deterministic: {deterministic})...")
          dump_res = await client.send_command(
              "Tracing.requestMemoryDump",
              {
                  "deterministic": deterministic,
                  "levelOfDetail": level_of_detail,
              },
          )
          dump_guid = dump_res.get("dumpGuid")
          dump_success = dump_res.get("success")
          print(f"[CDP] Initial memory dump completed: GUID={dump_guid},"
                f" success={dump_success}")
        except Exception as e:  # pylint: disable=broad-exception-caught
          print(f"[CDP] Warning: Initial memory dump failed: {e}")

      # Wait for duration or interactive trigger
      if interactive or duration <= 0:
        print("[CDP] Recording indefinitely. Press Enter or Ctrl+C to stop"
              " tracing...")
        # Run input in executor to avoid blocking asyncio event loop
        input_task = asyncio.create_task(asyncio.to_thread(sys.stdin.readline))
        wait_task = asyncio.create_task(stop_event.wait())
        _, pending = await asyncio.wait([input_task, wait_task],
                                        return_when=asyncio.FIRST_COMPLETED)
        for t in pending:
          t.cancel()
      else:
        print(f"[CDP] Recording trace for {duration:.1f} seconds...")
        start_ts = time.monotonic()
        while not stop_event.is_set():
          elapsed = time.monotonic() - start_ts
          remaining = duration - elapsed
          if remaining <= 0:
            break
          await asyncio.sleep(min(0.5, remaining))

      if request_dump and not stop_event.is_set():
        try:
          print("[CDP] Requesting final memory dump before stopping...")
          dump_res = await client.send_command(
              "Tracing.requestMemoryDump",
              {
                  "deterministic": deterministic,
                  "levelOfDetail": level_of_detail,
              },
          )
          dump_guid = dump_res.get("dumpGuid")
          dump_success = dump_res.get("success")
          print(f"[CDP] Final memory dump completed: GUID={dump_guid},"
                f" success={dump_success}")
        except Exception as e:  # pylint: disable=broad-exception-caught
          print(f"[CDP] Warning: Final memory dump request failed: {e}")

      # Stop tracing
      print("[CDP] Stopping tracing and flushing trace buffers...")
      await client.send_command("Tracing.end")

      # Wait for tracingComplete
      complete_msg = await asyncio.wait_for(
          tracing_complete_queue.get(), timeout=60.0)
      params = complete_msg.get("params", {})
      stream_handle = params.get("stream")
      data_loss = params.get("dataLossOccurred", False)
      trace_format = params.get("traceFormat", "proto")

      if data_loss:
        print("[CDP] Warning: Trace buffer wrapped around, data loss occurred.")

      if not stream_handle:
        raise RuntimeError(
            "No stream handle returned by Tracing.tracingComplete")

      print(f"[CDP] Trace stream ready (handle: {stream_handle},"
            f" format: {trace_format}).")
      print(f"[CDP] Reading trace stream and saving to {output_path}...")

      output_dir = os.path.dirname(os.path.abspath(output_path))
      if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir, exist_ok=True)

      total_bytes = 0
      with open(output_path, "wb") as f:
        while True:
          read_res = await client.send_command(
              "IO.read",
              {
                  "handle": stream_handle,
                  "size": 1048576,  # 1 MB chunk
              })
          data_str = read_res.get("data", "")
          if read_res.get("base64Encoded", False):
            chunk = base64.b64decode(data_str)
          else:
            chunk = data_str.encode("utf-8")

          if chunk:
            f.write(chunk)
            total_bytes += len(chunk)

          if read_res.get("eof", False):
            break

      # Close stream
      try:
        await client.send_command("IO.close", {"handle": stream_handle})
      except Exception:  # pylint: disable=broad-exception-caught
        pass

      size_kb = total_bytes / 1024.0
      size_mb = size_kb / 1024.0
      if size_mb >= 1.0:
        size_str = f"{size_mb:.2f} MB ({total_bytes:,} bytes)"
      else:
        size_str = f"{size_kb:.2f} KB ({total_bytes:,} bytes)"

      print(f"[CDP] Trace successfully captured: {output_path} [{size_str}]")
      print(
          "[CDP] Open this file in the Perfetto UI at: https://ui.perfetto.dev")
      return output_path

    finally:
      # Restore signal handlers
      for sig in (signal.SIGINT, signal.SIGTERM):
        try:
          loop.remove_signal_handler(sig)
        except (NotImplementedError, RuntimeError):
          pass
      await client.close()


def main():
  parser = argparse.ArgumentParser(
      description=(
          "Capture Perfetto traces with memory-infra from a running Cobalt"
          " instance via Chrome DevTools Protocol."))
  parser.add_argument(
      "--host",
      type=str,
      default="localhost",
      help="DevTools HTTP host (default: localhost)",
  )
  parser.add_argument(
      "--port",
      type=int,
      default=9222,
      help="DevTools HTTP port (default: 9222)",
  )
  parser.add_argument(
      "--duration",
      type=float,
      default=10.0,
      help=("Duration of trace in seconds (default: 10.0). Set to 0 to record"
            " until interrupted."),
  )
  parser.add_argument(
      "-i",
      "--interactive",
      action="store_true",
      help="Record interactively until Enter or Ctrl+C is pressed.",
  )
  parser.add_argument(
      "-o",
      "--output",
      type=str,
      default="meminfra_trace.perfetto-trace",
      help="Output file path (default: meminfra_trace.perfetto-trace)",
  )
  parser.add_argument(
      "--periodic-interval",
      type=int,
      default=1000,
      dest="periodic_interval_ms",
      help=("Periodic memory dump interval in milliseconds (default: 1000). Set"
            " to 0 to disable periodic dumps."),
  )
  parser.add_argument(
      "--level-of-detail",
      type=str,
      choices=["detailed", "light", "background"],
      default="detailed",
      help="Memory dump detail level (default: detailed)",
  )
  parser.add_argument(
      "--deterministic",
      action=argparse.BooleanOptionalAction,
      default=True,
      help=("Force garbage collection before explicit memory dump for"
            " determinism."),
  )
  parser.add_argument(
      "--request-dump",
      action=argparse.BooleanOptionalAction,
      default=True,
      help="Explicitly request memory dump(s) during tracing (default: True).",
  )
  parser.add_argument(
      "--preset",
      type=str,
      choices=["memory", "rdk", "minimal"],
      default="memory",
      help=(
          "Tracing category preset: 'memory' (default: memory-infra detailed),"
          " 'rdk' (memory-infra + cc + gpu + skia + v8), or 'minimal'"),
  )
  parser.add_argument(
      "--categories",
      type=str,
      default="",
      help=("Comma-separated list of additional categories to include (e.g."
            " blink.console,v8)"),
  )
  parser.add_argument(
      "--buffer-size",
      type=int,
      default=200000,
      dest="buffer_size_kb",
      help="Trace buffer size in KB (default: 200000 = ~200MB)",
  )
  parser.add_argument(
      "--record-mode",
      type=str,
      choices=[
          "recordUntilFull",
          "recordContinuously",
          "recordAsMuchAsPossible",
      ],
      default="recordUntilFull",
      help="Trace buffer record mode (default: recordUntilFull)",
  )
  parser.add_argument(
      "--compress",
      "--gzip",
      action="store_true",
      dest="compress",
      help="Compress the trace stream with gzip",
  )
  parser.add_argument(
      "--wait",
      action="store_true",
      help="Wait for CDP endpoint to become ready before tracing",
  )
  parser.add_argument(
      "--wait-timeout",
      type=float,
      default=30.0,
      help="Maximum time in seconds to wait for CDP (default: 30.0)",
  )

  args = parser.parse_args()

  # Check / wait for CDP connection
  if args.wait:
    print(f"[CDP] Waiting up to {args.wait_timeout}s for CDP on"
          f" {args.host}:{args.port}...")
    ready = asyncio.run(
        wait_for_cdp(args.host, args.port, total_wait=args.wait_timeout))
    if not ready:
      print(
          f"Error: DevTools endpoint on {args.host}:{args.port} did not become"
          " available.",
          file=sys.stderr,
      )
      sys.exit(1)

  # Check if DevTools is reachable
  try:
    get_websocket_debugger_url(args.host, args.port, timeout=3.0)
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(
        f"Error: Could not connect to DevTools on {args.host}:{args.port}: {e}",
        file=sys.stderr,
    )
    print(
        "Make sure Cobalt is running with --remote-debugging-port="
        f"{args.port}",
        file=sys.stderr,
    )
    sys.exit(1)

  extra_categories = [c for c in args.categories.split(",") if c.strip()]

  output_path = args.output
  if args.compress and not (output_path.endswith(".gz") or
                            output_path.endswith(".gzip")):
    output_path += ".gz"

  try:
    asyncio.run(
        capture_meminfra_trace(
            host=args.host,
            port=args.port,
            duration=args.duration,
            output_path=output_path,
            periodic_interval_ms=args.periodic_interval_ms,
            level_of_detail=args.level_of_detail,
            deterministic=args.deterministic,
            request_dump=args.request_dump,
            preset=args.preset,
            extra_categories=extra_categories,
            buffer_size_kb=args.buffer_size_kb,
            record_mode=args.record_mode,
            compress=args.compress,
            interactive=args.interactive,
        ))
  except KeyboardInterrupt:
    print("\n[CDP] Tracing interrupted by user.")
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f"\n[CDP] Fatal error: {e}", file=sys.stderr)
    sys.exit(1)


if __name__ == "__main__":
  main()
