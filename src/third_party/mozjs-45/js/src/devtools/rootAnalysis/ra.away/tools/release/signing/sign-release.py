#!/usr/bin/python
import os
import site
# Modify our search path to find our modules
site.addsitedir(os.path.join(os.path.dirname(__file__), "../../lib/python"))
import tempfile
import shutil
import time
import sys
import logging

from util.file import copyfile, sha1sum
from util.archives import bunzip2, bzip2, packfile, unpackfile
from util.paths import convertPath, findfiles

from signing.utils import shouldSign, getChkFile, signfile, sortFiles, \
    filterFiles, fileInfo, checkTools

log = logging.getLogger()


def _signLocale(obj, dstdir, files, remember=None):
    # Helper function to sign one individual locale, and keep track of some
    # stats
    nFiles, cacheHits, nSigned = 0, 0, 0
    for f in files:
        ext = os.path.splitext(f)[1]
        if ext == '.mar':
            compressed = True
            # We normally sign the .mar file after the .exe, so there's no point
            # in caching the results
            if remember is None:
                remember = False
        else:
            compressed = False
            if remember is None:
                remember = True
        results = obj.signPackage(f, dstdir, remember, compressed)
        if not results:
            return results
        nFiles += results[0]
        cacheHits += results[1]
        nSigned += results[2]
    return nFiles, cacheHits, nSigned


class Signer:
    def __init__(self, keydir, concurrency=1, keepCache=False, fake=False,
                 unsignedInstallers=False):
        """
        `keydir`    - where MozAuthenticode.svc,.pvk can be found

        `concurrency` - How many processes to use for signing.  If > 1, then
            that many children processes will be spawned to sign locales in
            parallel, and the main process will remain mostly idle to manage the
            children.

        `keepCache` - If False, then our cache of signed files is deleted as
            soon as this object is created

        `fake`      - If True, then no signing is actually done.

        `unsignedInstallers` - If True, then don't sign the final installer .exe's
        """

        # What directory to use to store our cached files
        self.cacheDir = 'cache'
        self.keydir = keydir
        self.fake = fake
        self.unsignedInstallers = unsignedInstallers
        self.concurrency = concurrency

        # Blow away our cache if we're not keeping it
        if not keepCache:
            log.debug("Purging cache")
            if os.path.exists(self.cacheDir):
                shutil.rmtree(self.cacheDir)

    def rememberFile(self, hsh, filename):
        """Remember this file in our cache.

        `filename` will be copied into our cache identified by `hsh`

        Usually `hsh` is the sha1sum of the unsigned file, and `filename`
        points to the signed file.
        """
        dstfile = os.path.join(self.cacheDir, hsh, os.path.basename(filename))
        dstdir = os.path.dirname(dstfile)
        if not os.path.exists(dstdir):
            os.makedirs(dstdir, 0755)
        # Copying files isn't atomic, so copy to a temporary file first, and
        # then rename to the final destination when we're done copying
        copyfile(filename, dstfile + ".tmp")
        os.rename(dstfile + ".tmp", dstfile)

    def getFile(self, hsh, filename):
        """Returns the path to a file in our cache identified by `hsh`, and that is named `filename`

        Returns None if there is no such file in our cache
        """
        dstfile = os.path.join(self.cacheDir, hsh, os.path.basename(filename))
        if os.path.exists(dstfile):
            return dstfile
        return None

    def signPackage(self, pkgfile, dstdir, remember=False, compressed=False):
        """Sign `pkgfile`, putting the results into `dstdir`.

        If `remember` is True, then cache the newly signed files into our
        cache.

        If `compressed` is True, then the contents of pkgfile are bz2
        compressed (e.g. in a mar file), and should be decompressed before
        signing.
        """
        log.info("Processing %s", pkgfile)
        basename = os.path.basename(pkgfile)
        dstfile = convertPath(pkgfile, dstdir)

        # Keep track of our output in a list here, and we can output everything
        # when we're done This is to avoid interleaving the output from
        # multiple processes.
        logs = []
        logs.append("Repacking %s to %s" % (pkgfile, dstfile))
        parentdir = os.path.dirname(dstfile)
        if not os.path.exists(parentdir):
            os.makedirs(parentdir, 0755)

        nFiles = 0
        cacheHits = 0
        nSigned = 0
        tmpdir = tempfile.mkdtemp()
        try:
            # Unpack it
            logs.append("Unpacking %s to %s" % (pkgfile, tmpdir))
            unpackfile(pkgfile, tmpdir)
            # Swap in files we have already signed
            for f in findfiles(tmpdir):
                # We don't need to do anything to files we're not going to sign
                if not shouldSign(f):
                    continue

                h = sha1sum(f)
                basename = os.path.basename(f)
                nFiles += 1
                chk = getChkFile(f)

                # Look in the cache for another file with the same original
                # hash
                cachedFile = self.getFile(h, f)
                if cachedFile:
                    cacheHits += 1
                    assert os.path.basename(cachedFile) == basename
                    logs.append("Copying %s from %s" % (basename, cachedFile))
                    # Preserve the original file's mode; don't use the cached mode
                    # We usually process installer .exe's first, and 7z doesn't
                    # preserve the file mode, so the cached copies of the files
                    # are mode 0666.  In the mar files, executables have mode
                    # 0777, so we want to preserve that.
                    copyfile(cachedFile, f, copymode=False)
                    if chk:
                        # If there's a .chk file for this file, copy that out of cache
                        # It's an error if this file doesn't exist in cache
                        cachedChk = self.getFile(h, chk)
                        logs.append("Copying %s from %s" %
                                    (os.path.basename(cachedChk), cachedChk))
                        copyfile(cachedChk, chk, copymode=False)
                else:
                    # We need to sign this file
                    # If this file is compressed, check if we have a cached copy that
                    # is uncompressed
                    if compressed:
                        bunzip2(f)
                        h2 = sha1sum(f)
                        cachedFile = self.getFile(h2, f)
                        if cachedFile:
                            # We have a cached copy of this file that is uncompressed.
                            # So copy it into our dstdir, and recompress it, and
                            # save it for future use.
                            cacheHits += 1
                            assert os.path.basename(cachedFile) == basename
                            logs.append("Copying %s from uncompressed %s" %
                                        (basename, cachedFile))
                            # See note above about not copying the file's mode
                            copyfile(cachedFile, f, copymode=False)
                            bzip2(f)
                            if chk:
                                # If there's a .chk file for this file, copy that out of cache
                                # It's an error if this file doesn't exist in
                                # cache
                                cachedChk = self.getFile(h2, chk)
                                logs.append("Copying %s from %s" % (
                                    os.path.basename(cachedChk), cachedChk))
                                copyfile(cachedChk, chk, copymode=False)
                                bzip2(chk)
                            if remember:
                                logs.append(
                                    "Caching compressed %s as %s" % (f, h))
                                self.rememberFile(h, f)
                                # Remember any regenerated chk files
                                if chk:
                                    logs.append("Caching %s as %s" % (chk, h))
                                    self.rememberFile(h, chk)
                            continue

                    nSigned += 1
                    logs.append("Signing %s" % f)
                    signfile(f, self.keydir, self.fake)
                    if compressed:
                        bzip2(f)
                        # If we have a chk file, compress that too
                        if chk:
                            bzip2(chk)
                    if remember:
                        logs.append("Caching %s as %s" % (f, h))
                        self.rememberFile(h, f)
                        # Remember any regenerated chk files
                        if chk:
                            logs.append("Caching %s as %s" % (chk, h))
                            self.rememberFile(h, chk)

            # Repack it
            logs.append("Packing %s" % dstfile)
            packfile(dstfile, tmpdir)
            # Sign installer
            if dstfile.endswith('.exe') and not self.unsignedInstallers:
                logs.append("Signing %s" % dstfile)
                signfile(dstfile, self.keydir, self.fake)
            return nFiles, cacheHits, nSigned
        except:
            log.exception("Error signing %s", pkgfile)
            return False
        finally:
            # Clean up after ourselves, and output our logs
            shutil.rmtree(tmpdir)
            log.info("\n  ".join(logs))

    def signFiles(self, files, dstdir, product, firstLocale='en-US', firstLocaleSigned=False):
        """Sign `files`, putting the results into `dstdir`.  If `files` has a
        single entry, and refers to a directory, then all files under that
        directory are signed.

        `product` is the product name, e.g. 'firefox', and is used to filename
        pattern matching

        `firstLocale` is the first locale that should be signed, and will have
        the results cached.  Other locales will use the cached versions of
        these signed files where appropriate.

        If `firstLocaleSigned` is True, then all `firstLocale` files must
        already be signed.  The unsigned and signed copies of `firstLocale`
        will be unpacked, and the results cached so that other locales will use
        the signed copies of `firstLocale` files where appropriate.
        """
        start = time.time()

        if len(files) == 1 and os.path.isdir(files[0]):
            files = findfiles(files[0])

        files = sortFiles(filterFiles(files, product), product, firstLocale)
        nfiles = len(files)

        # Split the files up into locales
        locales = {}
        for f in files:
            info = fileInfo(f, product)
            locale = info['locale']
            if not locale in locales:
                locales[locale] = []
            locales[locale].append(f)

        # This is required to be global because localeFinished is called via a
        # callback when child processes finish signing locales, and the
        # callback doesn't have access to this local scope
        global doneLocales
        doneLocales = 0

        # pool_start records when the pool of children has started processing
        # files.  We initialize it to start here because we haven't started the
        # pool yet!
        pool_start = start

        # This is called when we finish signing a locale.  We update some
        # stats, and report on our progress
        def localeFinished(locale):
            global doneLocales
            doneLocales += 1
            now = int(time.time())
            n = len(locales)
            t_per_locale = (now - pool_start) / doneLocales
            remaining = (n - doneLocales) * t_per_locale
            eta_s = remaining % 60
            eta_m = (remaining / 60) % 60
            eta_h = (remaining / 3600)
            percent = 100.0 * doneLocales / float(n)
            eta_time = time.strftime(
                "%H:%M:%S", time.localtime(int(now + remaining)))
            log.info("%i/%i (%.2f%%) complete, ETA in %i:%02i:%02i at %s",
                     doneLocales, n, percent, eta_h, eta_m, eta_s, eta_time)

        # Sign the firstLocale first.  If we don't have any firstLocale files,
        # then something is wrong!
        if firstLocale not in locales:
            log.error("No files found with locale %s", firstLocale)
            sys.exit(1)

        if not firstLocaleSigned:
            if not _signLocale(self, dstdir, locales[firstLocale], remember=True):
                log.error("Error signing %s", firstLocale)
                sys.exit(1)
        else:
            # If the first locale is already signed, we need to unpack both the
            # signed and unsigned copies, and then make sure the signed
            # versions are cached
            for uf in locales[firstLocale]:
                sf = convertPath(uf, dstdir)
                if not os.path.exists(sf):
                    log.error("Signed version of %s doesn't exist as expected at %s", uf, sf)
                    sys.exit(1)

                unsigned_dir = tempfile.mkdtemp()
                signed_dir = tempfile.mkdtemp()
                try:
                    log.info("Unpacking %s into %s", uf, unsigned_dir)
                    unpackfile(uf, unsigned_dir)
                    log.info("Unpacking %s into %s", sf, signed_dir)
                    unpackfile(sf, signed_dir)
                    for f in findfiles(unsigned_dir):
                        # We don't need to cache things that aren't signed
                        if not shouldSign(f):
                            continue
                        # Calculate the hash of the original, unsigned file
                        h = sha1sum(f)
                        sf = signed_dir + f[len(unsigned_dir):]
                        # Cache the signed version
                        log.info("Caching %s as %s" % (f, h))
                        self.rememberFile(h, sf)
                        chk = getChkFile(sf)
                        if chk:
                            log.info("Caching %s as %s" % (chk, h))
                            self.rememberFile(h, chk)
                finally:
                    shutil.rmtree(unsigned_dir)
                    shutil.rmtree(signed_dir)
        localeFinished(firstLocale)

        results = [0, 0, 0]
        # We're going to start our pool of children for processing the other
        # locales, so reset our pool_start time to the current time.
        # We do this because the time to sign the firstLocale isn't
        # representative of the time to sign future locales, due to caching and
        # parallelization.  Using pool_start gives more accurate ETA
        # calculations.
        pool_start = time.time()

        # Setup before signing
        # We define different addLocale functions depending on if we're signing
        # things concurrently or serially
        if self.concurrency > 1:
            # If we're doing signing in parallel, start up our pool of children
            # using the processing module
            import processing.pool
            pool = processing.pool.Pool(self.concurrency)
            result_list = []

            def addLocale(locale):
                def cb(r):
                    if not r:
                        log.error("Signing locale %s failed", locale)
                        return

                    for i in range(3):
                        results[i] += r[i]
                    localeFinished(locale)

                files = locales[locale]
                r = pool.apply_async(
                    _signLocale, (self, dstdir, files), callback=cb)
                result_list.append(r)
        else:
            pool = None

            def addLocale(locale):
                files = locales[locale]
                r = _signLocale(self, dstdir, files)
                if not r:
                    log.error("Signing locale %s failed", locale)
                    sys.exit(1)
                localeFinished(locale)
                for i in range(3):
                    results[i] += r[i]

        # Now go through our locales and call addLocale to start processing
        # them
        for locale in sorted(locales.keys()):
            if locale == firstLocale:
                continue
            addLocale(locale)

        error = False
        if pool:
            # Clean up the pool of child processes afterwards
            for r in result_list:
                try:
                    if not r.get():
                        log.error("Signing locale %s failed", locale)
                        error = True
                        pool.terminate()
                        break
                except:
                    log.exception("Error")
                    pool.terminate()
                    sys.exit(1)
            try:
                pool.close()
                pool.join()
            except:
                log.exception("Error")
                pool.terminate()
                raise

        # Report on our stats
        end = time.time()
        nTotalFiles, cacheHits, nSigned = results
        if nTotalFiles > 0:
            hitrate = cacheHits / float(nTotalFiles) * 100.0
        else:
            hitrate = 0
        if nfiles > 0:
            time_per_file = (end - start) / float(nfiles)
        else:
            time_per_file = 0
        log.info("%.2f%% hit rate, %i files signed", hitrate, nSigned)
        log.info("%s files repacked in %.0f seconds (%.2f seconds per file)",
                 nfiles, end - start, time_per_file)

        if error:
            sys.exit(1)

if __name__ == "__main__":
    from optparse import OptionParser
    parser = OptionParser()
    parser.set_defaults(
        dest=None,
        first_locale="en-US",
        concurrency=1,
        keep_cache=False,
        keydir=None,
        fake=False,
        prev=False,
        product="firefox",
        unsigned_installers=False,
    )
    parser.add_option(
        "-o", "--dest", dest="dest", help="destination directory")
    parser.add_option("", "--first-locale", dest="first_locale",
                      help="first locale to sign")
    parser.add_option("-j", "--concurrency", dest="concurrency",
                      help="concurrency", type="int")
    parser.add_option("", "--keep-cache", dest="keep_cache", action="store_true", help="don't delete cache of signed files before starting")
    parser.add_option("", "--keydir", dest="keydir",
                      help="directory where signing keys are located")
    parser.add_option("", "--fake", dest="fake", action="store_true",
                      help="don't actually sign anything")
    parser.add_option("-p", "--previously-signed", dest="prev", action="store_true", help="use a previously signed first locale")
    parser.add_option("", "--product", dest="product", help="product name")
    parser.add_option("", "--unsigned-installers", dest="unsigned_installers", action="store_true", help="don't sign the installer .exes")

    options, args = parser.parse_args()
    if not options.dest:
        parser.error("Must specify a destination directory")
    if not options.keydir:
        parser.error("Must specify a key directory")

    try:
        checkTools()
    except OSError, e:
        parser.error(e.message)

    if not os.path.exists('checkouts/stubs'):
        parser.error("No checkouts/stubs directory found")

    if options.concurrency == 1:
        logging.basicConfig(level=logging.INFO, format="%(message)s")
    else:
        logging.basicConfig(
            level=logging.INFO, format="[%(process)d] %(message)s")

    s = Signer(options.keydir, options.concurrency, options.keep_cache,
               options.fake, options.unsigned_installers)
    s.signFiles(args, options.dest, options.product, firstLocale=options.first_locale, firstLocaleSigned=options.prev)
