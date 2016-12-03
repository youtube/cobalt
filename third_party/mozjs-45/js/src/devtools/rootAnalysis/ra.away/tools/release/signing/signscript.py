#!/usr/bin/python
"""%prog [options] format inputfile outputfile inputfilename"""
import os
import os.path
import site
# Modify our search path to find our modules
site.addsitedir(os.path.join(os.path.dirname(__file__), "../../lib/python"))

import logging
import sys

from util.file import copyfile, safe_unlink
from signing.utils import shouldSign, signfile, osslsigncode_signfile
from signing.utils import gpg_signfile, mar_signfile, dmg_signpackage
from signing.utils import jar_signfile, emevoucher_signfile

if __name__ == '__main__':
    from optparse import OptionParser
    from ConfigParser import RawConfigParser

    parser = OptionParser(__doc__)
    parser.set_defaults(
        fake=False,
        signcode_keydir=None,
        gpg_homedir=None,
        loglevel=logging.INFO,
        configfile=None,
        mar_cmd=None,
        b2gmar_cmd=None,
        signcode_timestamp=None,
        jar_keystore=None,
        jar_keyname=None,
        emevoucher_key=None,
        emevoucher_chain=None,
    )
    parser.add_option("--keydir", dest="signcode_keydir",
                      help="where MozAuthenticode.spc, MozAuthenticode.spk can be found")
    parser.add_option("--gpgdir", dest="gpg_homedir",
                      help="where the gpg keyrings are")
    parser.add_option("--mac_id", dest="mac_id",
                      help="the mac signing identity")
    parser.add_option("--dmgkeychain", dest="dmg_keychain",
                      help="the mac signing keydir")
    parser.add_option("--fake", dest="fake", action="store_true",
                      help="do fake signing")
    parser.add_option("-c", "--config", dest="configfile",
                      help="config file to use")
    parser.add_option("--signcode_disable_timestamp",
                      dest="signcode_timestamp", action="store_false")
    parser.add_option("--jar_keystore", dest="jar_keystore",
                      help="keystore for signing jar_")
    parser.add_option("--jar_keyname", dest="jar_keyname",
                      help="which key to use from jar_keystore")
    parser.add_option("--emevoucher_key", dest="emevoucher_key",
                      help="The certificate to use for signing the eme voucher")
    parser.add_option("--emevoucher_chain", dest="emevoucher_chain",
                      help="Certificate chain to include in EME voucher signatures")
    parser.add_option(
        "-v", action="store_const", dest="loglevel", const=logging.DEBUG)

    options, args = parser.parse_args()

    if options.configfile:
        config = RawConfigParser()
        config.read(options.configfile)
        for option, value in config.items('signscript'):
            if option == "signcode_timestamp":
                value = config.getboolean('signscript', option)
            options.ensure_value(option, value)

    # Reset to default if this wasn't set in the config file
    if options.signcode_timestamp is None:
        options.signcode_timestamp = True

    logging.basicConfig(
        level=options.loglevel, format="%(asctime)s - %(message)s")

    if len(args) != 4:
        parser.error("Incorrect number of arguments")

    format_, inputfile, destfile, filename = args

    tmpfile = destfile + ".tmp"

    passphrase = sys.stdin.read().strip()
    if passphrase == '':
        passphrase = None

    if format_ == "signcode":
        if not options.signcode_keydir:
            parser.error("keydir required when format is signcode")
        copyfile(inputfile, tmpfile)
        if shouldSign(filename):
            signfile(tmpfile, options.signcode_keydir, options.fake,
                     passphrase, timestamp=options.signcode_timestamp)
        else:
            parser.error("Invalid file for signing: %s" % filename)
            sys.exit(1)
    elif format_ == "osslsigncode":
        safe_unlink(tmpfile)
        if not options.signcode_keydir:
            parser.error("keydir required when format is osslsigncode")
        if shouldSign(filename):
            osslsigncode_signfile(inputfile, tmpfile, options.signcode_keydir, options.fake,
                     passphrase, timestamp=options.signcode_timestamp)
        else:
            parser.error("Invalid file for signing: %s" % filename)
            sys.exit(1)
    elif format_ == "sha2signcode":
        safe_unlink(tmpfile)
        if not options.sha2signcode_keydir:
            parser.error("sha2signcode_keydir required when format is sha2signcode")
        if shouldSign(filename):
            # osslsigncode_signfile is used here because "sha2 signing" isn't
            # a different signing process, it's just signing using the same
            # tools/process with a different cert.
            osslsigncode_signfile(inputfile, tmpfile, options.sha2signcode_keydir, options.fake,
                     passphrase, timestamp=options.signcode_timestamp)
        else:
            parser.error("Invalid file for signing: %s" % filename)
            sys.exit(1)
    elif format_ == "gpg":
        if not options.gpg_homedir:
            parser.error("gpgdir required when format is gpg")
        safe_unlink(tmpfile)
        gpg_signfile(
            inputfile, tmpfile, options.gpg_homedir, options.fake, passphrase)
    elif format_ == "emevoucher":
        safe_unlink(tmpfile)
        emevoucher_signfile(
            inputfile, tmpfile, options.emevoucher_key,
            options.emevoucher_chain, options.fake, passphrase)
    elif format_ == "mar":
        if not options.mar_cmd:
            parser.error("mar_cmd is required when format is mar")
        safe_unlink(tmpfile)
        mar_signfile(
            inputfile, tmpfile, options.mar_cmd, options.fake, passphrase)
    elif format_ == "b2gmar":
        if not options.b2gmar_cmd:
            parser.error("b2gmar_cmd is required when format is b2gmar")
        safe_unlink(tmpfile)
        mar_signfile(
            inputfile, tmpfile, options.b2gmar_cmd, options.fake, passphrase)
    elif format_ == "dmg":
        if not options.dmg_keychain:
            parser.error("dmg_keychain required when format is dmg")
        if not options.mac_id:
            parser.error("mac_id required when format is dmg")
        safe_unlink(tmpfile)
        dmg_signpackage(inputfile, tmpfile, options.dmg_keychain, options.mac_id, options.mac_cert_subject_ou, options.fake, passphrase)
    elif format_ == "jar":
        if not options.jar_keystore:
            parser.error("jar_keystore required when format is jar")
        if not options.jar_keyname:
            parser.error("jar_keyname required when format is jar")
        copyfile(inputfile, tmpfile)
        jar_signfile(tmpfile, options.jar_keystore,
                     options.jar_keyname, options.fake, passphrase)

    os.rename(tmpfile, destfile)
