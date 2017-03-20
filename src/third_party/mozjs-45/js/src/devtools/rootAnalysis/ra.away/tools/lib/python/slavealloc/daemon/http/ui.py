import os
from twisted.web import static


def makeRootResource(site):
    # root corresponds to slavealloc/www
    wwwdir = os.path.abspath(
        os.path.join(os.path.dirname(__file__), "..", "..", "www"))
    root = static.File(wwwdir)

    # serve 'index.html' at the root, with the base_url encoed.
    index_html = open(os.path.join(wwwdir, 'index.html')).read()
    index_html = index_html.replace('%%BASE_URL%%', site.base_url)
    root.putChild('', static.Data(index_html, 'text/html'))

    return root
