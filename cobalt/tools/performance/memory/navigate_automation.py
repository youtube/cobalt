import json
import sys
import time
import requests
import websocket

def main():
  port = 9222
  if len(sys.argv) > 1:
    port = int(sys.argv[1])

  # Wait 15 seconds to let the home page load and settle before searching for the target
  print("Waiting 15 seconds for initial page load to settle...")
  time.sleep(15)

  # Wait for DevTools page target to appear
  print("Finding page target...")
  page_ws = None
  for _ in range(20):
    try:
      resp = requests.get(f"http://localhost:{port}/json", timeout=2)
      if resp.status_code == 200:
        for target in resp.json():
          if target.get("type") == "page":
            page_ws = target.get("webSocketDebuggerUrl")
            break
      if page_ws:
        break
    except Exception:
      pass
    time.sleep(1)

  if not page_ws:
    print("Error: Could not find page target DevTools URL!", file=sys.stderr)
    sys.exit(1)

  print("Triggering navigation to automation routine...")
  ws = None
  try:
    ws = websocket.create_connection(page_ws, timeout=5)
    ws.send(json.dumps({
        "id": 1,
        "method": "Page.navigate",
        "params": {
            "url": "https://www.youtube.com/tv?automationRoutine=browseWatchRoutine"
        }
    }))
    # Wait for the server to close or acknowledge
    ws.recv()
  except Exception as e:
    # A connection closed/detached exception is expected because the page is reloading
    print(f"Connection detached as expected during navigation: {e}")
  finally:
    if ws:
      try:
        ws.close()
      except Exception:
        pass
  print("Navigation trigger sequence completed.")

if __name__ == "__main__":
  main()
