# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from flask import Flask


app = Flask(__name__)


if __name__ == '__main__':
    # Instantiate when run locally (not used when deploying to GAE).
    app.run(host='127.0.0.1', port=8080, debug=True)
