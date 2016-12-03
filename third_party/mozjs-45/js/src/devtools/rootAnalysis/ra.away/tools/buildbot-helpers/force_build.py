#!/usr/bin/python

from urllib import urlencode
from urllib2 import urlopen
from urlparse import urljoin


class Forcer:
    maxProperties = 3
    """A Forcer knows how to force a build through the Buildbot web interface
       given the base URL to the master (masterUrl) and the builder name.
       You can pass name, comments, branch, revision, or properties to
       forceBuild() and they will be included in the POST
    """
    def __init__(self, masterUrl, builder, loud=True):
        self.masterUrl = masterUrl
        self.builder = builder
        self.forceUrl = urljoin(masterUrl, 'builders/%s/force' % builder)
        self.loud = loud

    def getArgs(self, name, comments, branch, revision, properties):
        if len(properties) > self.maxProperties:
            raise Exception("*** ERROR: Cannot pass more than 3 properties")

        args = {'username': name, 'comments': comments}
        if branch:
            args['branch'] = branch
        if revision:
            args['revision'] = revision
        i = 1
        for key, value in properties.iteritems():
            p = "property%d" % i
            args['%sname' % p] = key
            args['%svalue' % p] = value
            i += 1
        return args

    def forceBuild(self, name="Unknown", comments="Unknown", branch=None,
                   revision=None, properties={}):
        args = self.getArgs(name, comments, branch, revision, properties)
        params = urlencode(args)
        request = None
        try:
            if self.loud:
                print "Forcing %s with params: %s" % (self.forceUrl, str(args))
            request = urlopen(self.forceUrl, params)
        except:
            if self.loud:
                print "*** ERROR ***"
            raise
        # If we get past the try block, we assume success
