# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import logging
import urllib2


_log = logging.getLogger(__name__)
API_BASE = 'https://api.github.com'
EXPORT_LABEL = 'chromium-export'


class WPTGitHub(object):

    def __init__(self, host, user, token):
        self.host = host
        self.user = user
        self.token = token
        assert self.user and self.token

    def auth_token(self):
        return base64.b64encode('{}:{}'.format(self.user, self.token))

    def request(self, path, method, body=None):
        assert path.startswith('/')
        if body:
            body = json.dumps(body)
        opener = urllib2.build_opener(urllib2.HTTPHandler)
        request = urllib2.Request(url=API_BASE + path, data=body)
        request.add_header('Accept', 'application/vnd.github.v3+json')
        request.add_header('Authorization', 'Basic {}'.format(self.auth_token()))
        request.get_method = lambda: method
        response = opener.open(request)
        status_code = response.getcode()
        try:
            return json.load(response), status_code
        except ValueError:
            return None, status_code

    def create_pr(self, remote_branch_name, desc_title, body):
        """Creates a PR on GitHub.

        API doc: https://developer.github.com/v3/pulls/#create-a-pull-request

        Returns:
            A raw response object if successful, None if not.
        """
        assert remote_branch_name
        assert desc_title
        assert body

        path = '/repos/w3c/web-platform-tests/pulls'
        body = {
            'title': desc_title,
            'body': body,
            'head': remote_branch_name,
            'base': 'master',
        }
        data, status_code = self.request(path, method='POST', body=body)

        if status_code != 201:
            return None

        return data

    def add_label(self, number):
        path = '/repos/w3c/web-platform-tests/issues/%d/labels' % number
        body = [EXPORT_LABEL]
        return self.request(path, method='POST', body=body)

    def in_flight_pull_requests(self):
        path = '/search/issues?q=repo:w3c/web-platform-tests%20is:open%20type:pr%20label:{}'.format(EXPORT_LABEL)
        data, status_code = self.request(path, method='GET')
        if status_code == 200:
            return data['items']
        else:
            raise Exception('Non-200 status code (%s): %s' % (status_code, data))

    def merge_pull_request(self, pull_request_number):
        path = '/repos/w3c/web-platform-tests/pulls/%d/merge' % pull_request_number
        body = {
            'merge_method': 'rebase',
        }
        data, status_code = self.request(path, method='PUT', body=body)

        if status_code == 405:
            raise Exception('PR did not passed necessary checks to merge: %d' % pull_request_number)
        elif status_code == 200:
            return data
        else:
            raise Exception('PR could not be merged: %d' % pull_request_number)

    def delete_remote_branch(self, remote_branch_name):
        # TODO(jeffcarp): Unit test this method
        path = '/repos/w3c/web-platform-tests/git/refs/heads/%s' % remote_branch_name
        data, status_code = self.request(path, method='DELETE')

        if status_code != 204:
            # TODO(jeffcarp): Raise more specific exception (create MergeError class?)
            raise Exception('Received non-204 status code attempting to delete remote branch: {}'.format(status_code))

        return data
