# server.py

from flask import Flask, jsonify, send_file
from hashlib import file_digest
from os import path

app = Flask(__name__)
file_location = '/usr/local/google/home/<path_to_crx>/cobalt.crx'
self_endpoint = 'http://127.0.0.1:5000'

@app.route('/omaha/stub/health', methods=['GET'])
def get_health():
  return jsonify({'status': 'healthy'})

@app.route('/omaha/stub/update', methods=['POST'])
def post_updates():
  # Get SHA256 hash of the file being served.
  with open(file_location, 'rb') as f:
    digest = file_digest(f, 'sha256').hexdigest()
    filename = path.basename(f.name)

  # Populate the JSON response with required fields.
  update = {'response':{'protocol':'3.1','app':[{'appid':'{B725A22D-553A-49DC-BD61-F042B07C6B22}','cohort':'1:0/1/2:','status':'ok','cohortname':'SB16.qa.25.lts.10','ping':{'status':'ok'},'updatecheck':{'status':'ok','urls':{'url':[{'codebase': self_endpoint + '/omaha/stub/file/'}]},'manifest':{'version':'99.99.1030','packages':{'package':[{'hash_sha256':digest,'name':filename}]}}}}]}}

  # Add a prefix to the response data.
  response = jsonify(update)
  data = response.get_data(as_text=True)
  data = ")]}'" + data
  response.set_data(data)

  return response

@app.route('/omaha/stub/file/cobalt.crx', methods=['GET'])
def get_file():
  return send_file(file_location)

if __name__ == '__main__':
  app.run(debug=True)
