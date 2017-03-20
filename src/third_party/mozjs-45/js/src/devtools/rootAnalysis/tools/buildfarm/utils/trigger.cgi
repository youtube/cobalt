#!/usr/bin/env python

import cgitb; cgitb.enable()

import sys
import cgi
import re
import urllib2
import urllib
import subprocess
import os
MASTER = 'localhost'
PORT = '9010'
MASTERPATH = "/builds/buildbot/production-addons"
ENV = os.environ.copy()

#if var is a valid number returns a value other than None
def checkNumber(var):
    if var is None:
        return None
    reNumber = re.compile('^[0-9.]*$')
    return reNumber.match(var)

#if var is a valid string returns a value other than None
def checkString(var):
    if var is None:
        return None
    reString = re.compile('^[0-9A-Za-z\._\-+/%]*$')
    return reString.match(var)

#return full url to a release build of the correct version for the given os
def getBuildUrl(firefoxVersion, osVersion):
    buildUrl = ''
    buildInfo = {}
    buildInfo['win32']    = ["win32/en-US/", '"Firefox[^"]*.exe"']
    buildInfo['win64']    = ["win32/en-US/", '"Firefox[^"]*.exe"']
    buildInfo['macosx']   = ["mac/en-US/", '"Firefox[^"]*.dmg"']
    buildInfo['macosx64'] = ["mac/en-US/", '"Firefox[^"]*.dmg"']
    buildInfo['linux']    = ["linux-i686/en-US/", '"firefox[^"]*.tar.bz2"']
    buildInfo['linux64']  = ["linux-x86_64/en-US/", '"firefox[^"]*.tar.bz2"']
    try:
        if osVersion in buildInfo:
            releaseUrl = "http://releases.mozilla.org/pub/mozilla.org/firefox/releases/latest-%s/%s" % (firefoxVersion, buildInfo[osVersion][0])
            f = urllib2.urlopen(releaseUrl)
            file = f.read()
            match = re.search(buildInfo[osVersion][1], file)
            if match:
                buildPath = match.group(0).strip('"')
                buildUrl = releaseUrl + buildPath
	else:
	    print "ERROR: getBuildUrl: no buildInfo for given osVersion %s" % (osVersion,)
    except Exception, e:
        print "ERROR: buildUrl construction: ", e.__class__,  e, firefoxVersion, osVersion
        buildUrl = ''

    return buildUrl

#attempt to open the given addonUrl, provides error messages if the url is bad or the server is unreachable
def pingUrl(myUrl):
    try:
        res = urllib2.urlopen(urllib.unquote_plus(myUrl).replace('%3A', ':'))
        return myUrl
    except urllib2.URLError, e:
        if hasattr(e, 'reason'):
            print 'ERROR: pingUrl: %s' % (e.reason,)
        elif hasattr(e, 'code'):
            print 'ERROR: pingUrl, server error: %s' % (e.code,)
    except Exception, e:
        print "ERROR: pingUrl: ", e.__class__, e, myUrl
    return ''

print "Content-type: text/plain\n\n"
form = cgi.FieldStorage()
# incoming trigger string has the following parameters:
# os, one of linux/linux64/macosx/macosx64/win32/win64
# url, link to addon to be downloaded and tested for the given os
# firefox, of the format firefoxX.Y (eg, firefox3.5, firefox4.0, etc)

fields = ["os", "url", "firefox"]
os = None
url = None
firefox = None
#get form values and ensure that they are valid
for field in fields:
    if form.has_key(field):
        form[field].value = urllib.quote_plus(form[field].value, '/')
        print "INFO: validating key %s %s" % (field, form[field].value)
        if checkString(form[field].value):
            globals()[field] = form[field].value
#check values for correctness
if (None in (os, url, firefox)):
    print "ERROR: didn't get all necessary fields"
    sys.exit(1)
#check os for correctness
if (not (os in ('linux', 'linux64', 'macosx', 'macosx64', 'win32', 'win64'))):
    print "ERROR: invalid os specified: %s" % (os,)
    sys.exit(1)
#check firefox version for correctness
match = re.search('firefox(\d\.\d)', firefox)
if match:
    firefox = match.group(1) #reduce down to the version number
else:
    print "ERROR: invalid firefox specified: %s" % (firefox,)
    sys.exit(1)
#check addon url for correctness
url = pingUrl(url)
if not url:
    print "ERROR: addon url no good"
    sys.exit(1)

print "INFO: collected fields:\n\tos: %s\n\turl: %s\n\tfirefox: %s\n\t" % (os, url, firefox)
buildUrl = getBuildUrl(firefox, os)
if not buildUrl:
    print "ERROR: buildUrl no good"
    sys.exit(1)
print "INFO: converted firefox to: %s" % (buildUrl,)

#run sendchange
try:
    cmdline = "%s/bin/buildbot sendchange --master=%s:%s --branch=addontester-%s-talos --username=addonWebDevice --property addonUrl:%s %s" % (MASTERPATH, MASTER, PORT, os, url, buildUrl)
    print "INFO: running command: %s" % (cmdline,)
    pipe = subprocess.Popen(cmdline, shell=True, env=ENV, stdout=-1).stdout
    data = pipe.read()
    print "SENDCHANGE: %s" % (data,)
    pipe.close()
except Exception, e:
    print "SENDCHANGE: ", e.__class__, e, cmdline

sys.exit()
