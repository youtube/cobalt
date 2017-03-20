#!/usr/bin/env python

import optparse
from optparse import Option, OptionParser, OptionError
import urllib2
from urllib import urlencode, quote
import base64
import sys
import os
import traceback
from release.platforms import buildbot2bouncer, buildbot2ftp, \
    getAllLocales, getPlatforms
from release.versions import getPrettyVersion


class TuxedoOption(Option):
    ATTRS = Option.ATTRS + ['required']

    def _check_required(self):
        if self.required and not self.takes_value():
            raise OptionError(
                "required flag set for option that doesn't take a value",
                self)

    # Make sure _check_required() is called from the constructor!
    CHECK_METHODS = Option.CHECK_METHODS + [_check_required]

    def process(self, opt, value, values, parser):
        Option.process(self, opt, value, values, parser)
        parser.option_seen[self] = 1


class TuxedoOptionParser(OptionParser):

    def _init_parsing_state(self):
        OptionParser._init_parsing_state(self)
        self.option_seen = {}

    def check_values(self, values, args):
        for option in self.option_list:
            if (isinstance(option, Option) and
                option.required and
                    option not in self.option_seen):
                self.print_help()
                self.set_usage(optparse.SUPPRESS_USAGE)
                self.error("%s not supplied" % option)
        return (values, args)


class TuxedoEntrySubmitter(object):

    full_product_template = {}
    complete_mar_template = {}
    partial_mar_template = {}

    def __init__(self, config, productName, version, tuxedoServerUrl,
                 brandName=None, bouncerProductName=None, shippedLocales=None,
                 partialVersions=None, username=None, password=None,
                 verbose=True, dryRun=False, platforms=None, milestone=None,
                 bouncerProductSuffix=None):
        self.config = config
        self.productName = productName
        self.version = version
        self.milestone = milestone
        self.tuxedoServerUrl = tuxedoServerUrl
        self.brandName = brandName or productName.capitalize()
        self.bouncerProductName = bouncerProductName or productName.capitalize(
        )
        self.shippedLocales = shippedLocales
        self.partialVersions = partialVersions
        self.username = username
        self.password = password
        self.verbose = verbose
        self.dryRun = dryRun
        self.platforms = platforms
        self.addMARs = False

        if not self.platforms:
            self.platforms = getPlatforms()

        if self.shippedLocales:
            self.locales = getAllLocales(self.shippedLocales)
        else:
            self.locales = None

        self.bouncer_product_name = '%s-%s' % (self.bouncerProductName,
                                               self.version)
        if bouncerProductSuffix:
            self.bouncer_product_name = '%s-%s' % (self.bouncer_product_name,
                                                   bouncerProductSuffix)
        self.complete_mar_bouncer_product_name = '%s-%s-Complete' % \
            (self.bouncerProductName, self.version)
        self.partial_mar_bouncer_product_names = {}
        for previousVersion in self.partialVersions:
            self.addMARs = True
            self.partial_mar_bouncer_product_names[previousVersion] = '%s-%s-Partial-%s' % \
                (self.bouncerProductName, self.version, previousVersion)
        self.read_config()

    def read_config(self):
        """Example config file


[DEFAULT]
complete_mar_template = /%(product)s/releases/%(version)s/update/%(ftp_platform)s/%(locale)s/%(product)s-%(version)s.complete.mar
partial_mar_template = /%(product)s/releases/%(version)s/update/%(ftp_platform)s/%(locale)s/%(product)s-%(previous_version)s-%(version)s.partial.mar

[win32]
full_product_template = /%(product)s/releases/%(version)s/%(ftp_platform)s/%(locale)s/%(brandName)s Setup %(prettyVersion)s.exe

[linux]
full_product_template = /%(product)s/releases/%(version)s/%(ftp_platform)s/%(locale)s/%(product)s-%(version)s.tar.bz2

[macosx]
full_product_template = /%(product)s/releases/%(version)s/%(ftp_platform)s/%(locale)s/%(brandName)s %(prettyVersion)s.dmg

"""
        try:
            import ConfigParser
        except ImportError:
            import configparser as ConfigParser

        cfg = ConfigParser.RawConfigParser()
        if not cfg.read(self.config):
            print >> sys.stderr, "Cannot read %s" % self.config
            sys.exit(2)

        for platform in self.platforms:
            self.full_product_template[platform] = cfg.get(platform,
                                                           'full_product_template')
            self.complete_mar_template[platform] = cfg.get(platform,
                                                           'complete_mar_template')
            self.partial_mar_template[platform] = cfg.get(platform,
                                                          'partial_mar_template')

    def tuxedoRequest(self, url, postdata=None):
        full_url = self.tuxedoServerUrl + url
        request = urllib2.Request(full_url)
        if self.username and self.password:
            basicAuth = base64.encodestring(
                '%s:%s' % (self.username, self.password))
            request.add_header("Authorization", "Basic %s" % basicAuth.strip())
        if postdata:
            if isinstance(postdata, dict):
                postdata = urlencode(postdata)
            request.add_data(postdata)
        if self.dryRun:
            print >> sys.stderr, "Tuxedo API URL: %s" % full_url
            if postdata:
                print >> sys.stderr, "POST data: %s" % postdata
            return
        try:
            return urllib2.urlopen(request).read()
        except urllib2.URLError:
            print >> sys.stderr, "FAILED: Tuxedo API error. URL: %s" % full_url
            if postdata:
                print >> sys.stderr, "POST data: %s" % postdata
            traceback.print_exc(file=sys.stdout)
            sys.exit(1)

    def product_add(self, product):
        if self.verbose:
            print "Adding product: %s" % product
            if self.locales:
                print "Locales: %s" % ", ".join(self.locales)
        post_data = "product=" + quote(product)
        if self.locales:
            locales_post_data = ["languages=%s" % l for l in self.locales]
            locales_post_data = "&".join(locales_post_data)
            post_data += "&" + locales_post_data
        response = self.tuxedoRequest("product_add/", post_data)
        if self.verbose:
            print "Server response:"
            print response

    def location_add(self, product, platform, path):
        path = path.replace(' ', '%20')
        if self.verbose:
            print "Adding location for %s, %s: %s" % \
                (product, buildbot2bouncer(platform), path)
        self.tuxedoRequest("location_add/",
                           {'product': product,
                            'os': buildbot2bouncer(platform),
                            'path': path})

    def add_products(self):
        self.product_add(self.bouncer_product_name)
        if self.addMARs:
            self.product_add(self.complete_mar_bouncer_product_name)
            if self.partialVersions:
                for name in self.partial_mar_bouncer_product_names.values():
                    self.product_add(name)

    def add_locations(self):
        for platform in self.platforms:
            template_dict = {'product': self.productName,
                             'brandName': self.brandName,
                             'bouncer_product': self.bouncerProductName,
                             'version': self.version,
                             'milestone': self.milestone,
                             'prettyVersion': getPrettyVersion(self.version),
                             'ftp_platform': buildbot2ftp(platform),
                             'locale': ':lang'}
            # Full product
            path = self.full_product_template[platform] % template_dict
            self.location_add(self.bouncer_product_name, platform, path)

            # Complete MAR product
            if self.addMARs:
                path = self.complete_mar_template[platform] % template_dict
                self.location_add(self.complete_mar_bouncer_product_name,
                                  platform, path)

                # Partial MAR product
                for previousVersion in self.partialVersions:
                    template_dict['previous_version'] = previousVersion
                    path = self.partial_mar_template[platform] % template_dict
                    name = self.partial_mar_bouncer_product_names[
                        previousVersion]
                    self.location_add(name, platform, path)

    def submit(self):
        self.add_products()
        self.add_locations()


def getOptions():
    parser = TuxedoOptionParser(option_class=TuxedoOption)
    parser.add_option("--config", dest="config",
                      required=True,
                      help="Configuration file")
    parser.add_option("-p", "--product-name", dest="productName",
                      required=True,
                      help="Product name")
    parser.add_option("-v", "--version", dest="version",
                      required=True,
                      help="Product version")
    parser.add_option("--milestone", dest="milestone",
                      help="Product milestone")
    parser.add_option("-t", "--tuxedo-server-url", dest="tuxedoServerUrl",
                      required=True,
                      help="Bouncer/Tuxedo API URL")
    parser.add_option("-r", "--brand-name", dest="brandName",
                      help="Brand name")
    parser.add_option(
        "-b", "--bouncer-product-name", dest="bouncerProductName",
        help="Bouncer product name")
    parser.add_option("--bouncer-product-suffix", dest="bouncerProductSuffix",
                      help="Bouncer product suffix")
    parser.add_option("-l", "--shipped-locales", dest="shippedLocales",
                      help="shipped-locales file location")
    parser.add_option(
        "--partial-version", dest="partialVersions", action="append",
        default=[], help="Old product version")
    parser.add_option("--platform", action="append",
                      dest="platforms",
                      help="Platform(s) to be processed")
    parser.add_option("--dry-run", action="store_true", dest="dryRun",
                      default=False,
                      help="Print debug information and exit")
    parser.add_option("--username", dest="username",
                      help="Tuxedo user name")
    parser.add_option("--password", dest="password",
                      help="Tuxedo password")
    parser.add_option("--credentials-file", dest="credentials",
                      help="Get Tuxedo username/password from file")
    parser.add_option("-q", "--quiet", action="store_false", dest="verbose",
                      default=True,
                      help="Don't print status messages to stdout")
    return parser.parse_args()


def credentials_from_file(credentials_file):
    try:
        module, _ = os.path.splitext(credentials_file)
        credentials = __import__(module)
        username = credentials.tuxedoUsername
        password = credentials.tuxedoPassword
        return (username, password)
    except ImportError:
        print >> sys.stderr, \
            "Cannot open credentials file: %s" % credentials_file
        sys.exit(3)
    except AttributeError:
        print >> sys.stderr, \
            "Cannot retrieve credentials file: %s" % credentials_file
        sys.exit(4)


def main():
    (options, args) = getOptions()

    username = options.username
    password = options.password
    if options.credentials:
        (username, password) = credentials_from_file(options.credentials)

    tuxedo = TuxedoEntrySubmitter(config=options.config,
                                  productName=options.productName,
                                  version=options.version,
                                  milestone=options.milestone,
                                  tuxedoServerUrl=options.tuxedoServerUrl,
                                  brandName=options.brandName,
                                  bouncerProductName=options.bouncerProductName,
                                  shippedLocales=options.shippedLocales,
                                  partialVersions=options.partialVersions,
                                  username=username,
                                  password=password,
                                  verbose=options.verbose,
                                  dryRun=options.dryRun,
                                  platforms=options.platforms,
                                  bouncerProductSuffix=options.bouncerProductSuffix,
                                  )
    tuxedo.submit()


if __name__ == '__main__':
    main()
