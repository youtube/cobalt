import os
import sys

python26OrHigher = (sys.version_info >= (2, 6))

DEBUG = False

try:
    import simplejson
except:
    print "Error: simplejson is required to run generate.py where you fetch input from places-stats.mozilla.com"
try:
    import httplib2
except:
    print "Error: httplib2 is required to run generate.py where you fetch input from places-stats.mozilla.com"


def get_web_data():
    h = httplib2.Http(os.tmpnam())
    resp, content = h.request("https://places-stats.mozilla.com/stats",
                              "GET",
                              headers={'accept': 'application/json'})
    obj = simplejson.loads(content)
    if DEBUG:
        print content
    return obj
