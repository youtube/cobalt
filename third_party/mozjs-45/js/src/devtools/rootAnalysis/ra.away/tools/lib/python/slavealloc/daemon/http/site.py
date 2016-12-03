import textwrap
from twisted.web import resource, server, util
from slavealloc.daemon.http import ui, gettac, api


def redirectTo(path, request):
    if path[0] == '/':
        path = request.site.base_url + path[1:]
    return util.redirectTo(path, request)


class RootResource(resource.Resource):
    "root (/, or base_url) resource for the HTTP service"
    addSlash = True
    isLeaf = False

    def render_GET(self, request):
        # if we're running the UI, redirect there
        if request.site.run_ui:
            return redirectTo('/ui', request)

        # otherwise, return some simple text
        request.setResponseCode(404)
        request.setHeader('content-type', 'text/plain')
        return textwrap.dedent("""\
        This allocator instance is not running the web UI.

        Use the URI format /gettac/SLAVENAME to request a TAC file.
        """).strip()


class Site(server.Site):
    def __init__(self, allocator, base_url='/', run_allocator=False, run_ui=False):
        root = resource.Resource()
        server.Site.__init__(self, root)

        if base_url and base_url[-1] != '/':
            base_url += '/'
        self.base_url = base_url

        # let resources know what we're running
        self.run_allocator = run_allocator
        self.run_ui = run_ui

        # resources will use this to find the allocator, if it's enabled
        self.allocator = allocator

        # set up the top-level URI and the omnipresent API
        root.putChild('', RootResource())
        root.putChild('api', api.makeRootResource())

        # set up sub-URI's as necessary
        if run_allocator:
            root.putChild('gettac', gettac.makeRootResource())

        if run_ui:
            root.putChild('ui', ui.makeRootResource(self))
