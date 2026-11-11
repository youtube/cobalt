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
"""Seeds Cobalt's splash cache on an Android device, the way the web app would.

Connects to Cobalt's DevTools over adb, finds the youtube.com page, and runs
the Cache API calls the web app would run:

  const cache = await caches.open('default');
  await cache.put('/splash_selector.wasm', new Response(...));
  await cache.put('/splash_1080.webm', new Response(...));

Cobalt must be running with the web app loaded. Each FILE is stored under its
basename; the content type comes from the suffix.

Usage:
  seed_splash_cache.py [--device SERIAL] [--cache NAME] FILE...
  seed_splash_cache.py --list
  seed_splash_cache.py --clear
"""

import argparse
import base64
import json
import mimetypes
import os
import subprocess
import sys
import urllib.request

SRC_ROOT = subprocess.check_output(
    ["git", "rev-parse", "--show-toplevel"],
    cwd=os.path.dirname(os.path.abspath(__file__)), text=True).strip()
NODE = os.path.join(SRC_ROOT, "third_party/node/linux/node-linux-x64/bin/node")
PORT = 9333
TYPES = {".wasm": "application/wasm", ".webm": "video/webm",
         ".png": "image/png", ".json": "application/json"}


def adb(device, *args):
  cmd = ["adb"] + (["-s", device] if device else []) + list(args)
  return subprocess.run(cmd, check=True, capture_output=True, text=True).stdout


def find_page(device):
  adb(device, "forward", f"tcp:{PORT}",
      "localabstract:content_shell_devtools_remote")
  targets = json.load(urllib.request.urlopen(f"http://localhost:{PORT}/json",
                                             timeout=5))
  for t in targets:
    if t.get("type") == "page" and "youtube.com" in t.get("url", ""):
      return t["webSocketDebuggerUrl"]
  sys.exit(f"No youtube.com page among DevTools targets: "
           f"{[t.get('url') for t in targets]}")


def evaluate(ws_url, expression):
  """Runs an async expression in the page and returns its JSON result."""
  script = r"""
    const [wsUrl, expression] = process.argv.slice(1);
    const ws = new WebSocket(wsUrl);
    ws.onopen = () => ws.send(JSON.stringify({id: 1, method: 'Runtime.evaluate',
        params: {expression, awaitPromise: true, returnByValue: true}}));
    ws.onmessage = (m) => {
      const msg = JSON.parse(m.data);
      if (msg.id !== 1) return;
      console.log(JSON.stringify(msg.result));
      ws.close();
    };
    ws.onerror = (e) => { console.error(e.message); process.exit(1); };
  """
  out = subprocess.run([NODE, "-e", script, "--", ws_url, expression],
                       check=True, capture_output=True, text=True).stdout
  result = json.loads(out)
  if "exceptionDetails" in result:
    sys.exit(f"JS exception: {json.dumps(result['exceptionDetails'])[:500]}")
  return result["result"].get("value")


def main():
  ap = argparse.ArgumentParser(description=__doc__,
                               formatter_class=argparse.RawDescriptionHelpFormatter)
  ap.add_argument("--device", default=os.environ.get("ANDROID_SERIAL"))
  ap.add_argument("--cache", default="default",
                  help="cache name: 'default', or a topic such as 'music'")
  ap.add_argument("--list", action="store_true", help="list the cache")
  ap.add_argument("--clear", action="store_true", help="delete the entries")
  ap.add_argument("files", nargs="*")
  args = ap.parse_args()
  ws_url = find_page(args.device)
  cache = args.cache

  if args.clear:
    print(evaluate(ws_url, f"""(async () => {{
      const cache = await caches.open('{cache}');
      const keys = await cache.keys();
      for (const k of keys) await cache.delete(k);
      return 'deleted ' + keys.length;
    }})()"""))

  for path in args.files:
    name = os.path.basename(path)
    data = base64.b64encode(open(path, "rb").read()).decode()
    ctype = TYPES.get(os.path.splitext(name)[1], "application/octet-stream")
    print(name, evaluate(ws_url, f"""(async () => {{
      const bytes = Uint8Array.from(atob('{data}'), c => c.charCodeAt(0));
      const cache = await caches.open('{cache}');
      await cache.put('/{name}', new Response(bytes,
          {{headers: {{'Content-Type': '{ctype}'}}}}));
      return 'put ' + bytes.length + ' bytes as ' + location.origin + '/{name}';
    }})()"""))

  print(f"cache '{cache}' now:", evaluate(ws_url, f"""(async () => {{
    const cache = await caches.open('{cache}');
    const out = [];
    for (const k of await cache.keys()) {{
      const r = await cache.match(k);
      out.push(k.url + ' (' + (await r.arrayBuffer()).byteLength + ' B)');
    }}
    return out;
  }})()"""))


if __name__ == "__main__":
  main()
