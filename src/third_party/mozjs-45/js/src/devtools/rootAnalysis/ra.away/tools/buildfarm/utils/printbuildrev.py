# print build ID to stdout
# and print revision(s) for tinderbox scraping
#
# example usage:
# python printbuildrev /path/to/appdir

import ConfigParser
import sys

app_path = sys.argv[1]

# XXX remove once bug 698843 is closed
if app_path == "thunderbird-test":
    app_path = "thunderbird"

appini = ConfigParser.ConfigParser()
appini.read('%s/application.ini' % app_path)

buildid = appini.get('App', 'BuildID')
app_repo = appini.get('App', 'SourceRepository')
app_revision = appini.get('App', 'SourceStamp')

platini = ConfigParser.ConfigParser()
platini.read('%s/platform.ini' % app_path)

plat_repo = platini.get('Build', 'SourceRepository')
plat_revision = platini.get('Build', 'SourceStamp')

print 'buildid: %s' % buildid

print 'TinderboxPrint:<a href=%s/rev/%s title="Built from revision %s">rev:%s</a>' % \
    (app_repo, app_revision, app_revision, app_revision)

if app_repo != plat_repo:
    print 'TinderboxPrint:<a href=%s/rev/%s title="Built from Mozilla revision %s">moz:%s</a>' % \
        (plat_repo, plat_revision, plat_revision, plat_revision)
