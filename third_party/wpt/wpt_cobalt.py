#!/usr/bin/env python
import logging
import os
from tools.wpt import wpt # This inserts sys.paths for next import
from tools.serve.serve import inject_script
import wptserve.response
from six.moves.urllib.parse import unquote, urlsplit
import wptserve.handlers

class PatchedResponseWriter(wptserve.response.ResponseWriter):
    def write(self, data):
        request_path = self._response.request.request_path

        # We don't need to modify non-html files
        # Includes both '.htm' and '.html' formats
        if (".htm" not in request_path):
            return super(PatchedResponseWriter, self).write(data)

        # Inject URLSearchParams polyfill
        data = inject_script(data, b"<script src='/resources/url-search-params-polyfill.js'></script>")

        return super(PatchedResponseWriter, self).write(data)

wptserve.response.ResponseWriter = PatchedResponseWriter

class PatchedFileHandler(wptserve.handlers.FileHandler):
    def __call__(self, request, response):
        path = unquote(request.url_parts.path)
        
        if path == '/resources/testharness.js':
            request.url = request.url.replace('/testharness.js','/testharness_cobalt.js')
        
        request.url_parts = urlsplit(request.url)
        return super(PatchedFileHandler, self).__call__(request, response)

if __name__ == "__main__":
    wptserve.handlers.FileHandler = PatchedFileHandler
    wptserve.handlers.file_handler = PatchedFileHandler()

    import wptserve.routes
    patch_routes = wptserve.routes.routes
    patch_routes[2] = ("GET", "*", wptserve.handlers.file_handler)
    wpt.main()