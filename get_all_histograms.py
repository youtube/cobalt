import json
import requests
import websocket
import time
import sys

def main():
    try:
        response = requests.get('http://127.0.0.1:9222/json', timeout=5)
        targets = response.json()
        target = next((t for t in targets if t['type'] == 'browser'), None)
        if not target and targets:
            target = next((t for t in targets if t['type'] == 'page'), targets[0])
        
        ws_url = target['webSocketDebuggerUrl']
        ws = websocket.create_connection(ws_url)
        
        # Get all histograms
        ws.send(json.dumps({'id': 1, 'method': 'Browser.getHistograms', 'params': {'query': ''}}))
        result = json.loads(ws.recv())
        
        histograms = result.get('result', {}).get('histograms', [])
        
        with open('all_histograms.txt', 'w') as f:
            for h in histograms:
                f.write(h['name'] + ' : ' + str(h.get('count', 0)) + '\n')
                
        print(f"Saved {len(histograms)} histograms to all_histograms.txt")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == '__main__':
    main()
