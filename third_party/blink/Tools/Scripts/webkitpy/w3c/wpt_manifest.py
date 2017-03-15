# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""WPTManifest is responsible for handling MANIFEST.json.

The MANIFEST.json file contains metadata about files in web-platform-tests,
such as what tests exist, and extra information about each test, including
test type, options, URLs to use, and reference file paths if applicable.
"""

import json
import logging

from webkitpy.common.webkit_finder import WebKitFinder

_log = logging.getLogger(__file__)


class WPTManifest(object):

    def __init__(self, json_content):
        # TODO(tkent): Create a Manifest object by Manifest.from_json().
        # See ../thirdparty/wpt/wpt/tools/manifest/manifest.py.
        self.raw_dict = json.loads(json_content)

    def _items_for_path(self, path_in_wpt):
        """Returns manifest items for the given WPT path, or None if not found.

        The format of a manifest item depends on
        https://github.com/w3c/wpt-tools/blob/master/manifest/item.py
        and is assumed to be a list of the format [url, extras],
        or [url, references, extras] for reftests, or None if not found.

        For most testharness tests, the returned items is expected
        to look like this:: [["/some/test/path.html", {}]]
        """
        items = self.raw_dict['items']
        if path_in_wpt in items['manual']:
            return items['manual'][path_in_wpt]
        elif path_in_wpt in items['reftest']:
            return items['reftest'][path_in_wpt]
        elif path_in_wpt in items['testharness']:
            return items['testharness'][path_in_wpt]
        return None

    def is_test_file(self, path_in_wpt):
        return self._items_for_path(path_in_wpt) is not None

    def file_path_to_url_paths(self, path_in_wpt):
        manifest_items = self._items_for_path(path_in_wpt)
        assert manifest_items is not None
        if len(manifest_items) != 1:
            return []
        url = manifest_items[0][0]
        if url[1:] != path_in_wpt:
            # TODO(tkent): foo.any.js and bar.worker.js should be accessed
            # as foo.any.html, foo.any.worker, and bar.worker with WPTServe.
            return []
        return [path_in_wpt]

    @staticmethod
    def _get_extras_from_item(item):
        return item[-1]

    def is_slow_test(self, test_name):
        items = self._items_for_path(test_name)
        if not items:
            return False
        extras = WPTManifest._get_extras_from_item(items[0])
        return 'timeout' in extras and extras['timeout'] == 'long'

    def extract_reference_list(self, path_in_wpt):
        """Extracts reference information of the specified reference test.

        The return value is a list of (match/not-match, reference path in wpt)
        like:
           [("==", "foo/bar/baz-match.html"),
            ("!=", "foo/bar/baz-mismatch.html")]
        """
        all_items = self.raw_dict['items']
        if path_in_wpt not in all_items['reftest']:
            return []
        reftest_list = []
        for item in all_items['reftest'][path_in_wpt]:
            for ref_path_in_wpt, expectation in item[1]:
                reftest_list.append((expectation, ref_path_in_wpt))
        return reftest_list

    @staticmethod
    def ensure_manifest(host):
        """Checks whether the manifest exists, and then generates it if necessary."""
        finder = WebKitFinder(host.filesystem)
        manifest_path = finder.path_from_webkit_base('LayoutTests', 'external', 'wpt', 'MANIFEST.json')
        base_manifest_path = finder.path_from_webkit_base('LayoutTests', 'external', 'WPT_BASE_MANIFEST.json')

        if not host.filesystem.exists(base_manifest_path):
            _log.error('Manifest base not found at "%s".', base_manifest_path)
            host.filesystem.write_text_file(base_manifest_path, '{}')

        if not host.filesystem.exists(manifest_path):
            _log.debug('Manifest not found, copying from base "%s".', base_manifest_path)
            host.filesystem.copyfile(base_manifest_path, manifest_path)

        wpt_path = manifest_path = finder.path_from_webkit_base('LayoutTests', 'external', 'wpt')
        WPTManifest.generate_manifest(host, wpt_path)

    @staticmethod
    def generate_manifest(host, dest_path):
        """Generates MANIFEST.json on the specified directory."""
        executive = host.executive
        finder = WebKitFinder(host.filesystem)
        manifest_exec_path = finder.path_from_webkit_base('Tools', 'Scripts', 'webkitpy', 'thirdparty', 'wpt', 'wpt', 'manifest')

        cmd = ['python', manifest_exec_path, '--work', '--tests-root', dest_path]
        _log.debug('Running command: %s', ' '.join(cmd))
        proc = executive.popen(cmd, stdout=executive.PIPE, stderr=executive.PIPE, stdin=executive.PIPE, cwd=finder.webkit_base())
        out, err = proc.communicate('')
        if proc.returncode:
            _log.info('# ret> %d', proc.returncode)
            if out:
                _log.info(out)
            if err:
                _log.info(err)
            host.exit(proc.returncode)
        return proc.returncode, out
