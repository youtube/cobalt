import difflib
import logging
import re
from release.info import readConfig
import urllib2
from util.commands import run_cmd, get_output
from util.hg import get_repo_name, make_hg_url
from subprocess import CalledProcessError

log = logging.getLogger(__name__)


def check_buildbot():
    """check if buildbot command works"""
    try:
        run_cmd(['buildbot', '--version'])
    except CalledProcessError:
        log.error("FAIL: buildbot command doesn't work", exc_info=True)
        raise


def find_version(contents, versionNumber):
    """Given an open readable file-handle look for the occurrence
       of the version # in the file"""
    ret = re.search(re.compile(re.escape(versionNumber), re.DOTALL), contents)
    return ret


def locale_diff(locales1, locales2):
    """ accepts two lists and diffs them both ways, returns any differences
    found """
    diff_list = [locale for locale in locales1 if not locale in locales2]
    diff_list.extend(locale for locale in locales2 if not locale in locales1)
    return diff_list


def get_buildbot_username_param():
    cmd = ['buildbot', 'sendchange', '--help']
    output = get_output(cmd)
    if "-W, --who=" in output:
        return "--who"
    else:
        return "--username"


def sendchange(branch, revision, username, master, products):
    """Send the change to buildbot to kick off the release automation"""
    if isinstance(products, basestring):
        products = [products]
    cmd = [
        'buildbot',
        'sendchange',
        get_buildbot_username_param(),
        username,
        '--master',
        master,
        '--branch',
        branch,
        '-p',
        'products:%s' % ','.join(products),
        '-p',
        'script_repo_revision:%s' % revision,
        'release_build'
    ]
    log.info("Executing: %s" % cmd)
    run_cmd(cmd)


def verify_mozconfigs(branch, revision, hghost, product, mozconfigs,
                      nightly_mozconfigs, whitelist=None):
    """Compare nightly mozconfigs for branch to release mozconfigs and
    compare to whitelist of known differences"""
    branch_name = get_repo_name(branch)
    if whitelist:
        mozconfigWhitelist = readConfig(whitelist, ['whitelist'])
    else:
        mozconfigWhitelist = {}
    log.info("Comparing %s mozconfigs to nightly mozconfigs..." % product)
    success = True
    for platform, mozconfig in mozconfigs.items():
        urls = []
        mozconfigs = []
        nightly_mozconfig = nightly_mozconfigs[platform]
        mozconfig_paths = [mozconfig, nightly_mozconfig]
        # Create links to the two mozconfigs.
        releaseConfig = make_hg_url(hghost, branch, 'http', revision,
                                    mozconfig)
        for c in mozconfig, nightly_mozconfig:
            urls.append(make_hg_url(hghost, branch, 'http', revision, c))
        for url in urls:
            try:
                mozconfigs.append(urllib2.urlopen(url).readlines())
            except urllib2.HTTPError as e:
                log.error("MISSING: %s - ERROR: %s" % (url, e.msg))
                # Nothing to compare against
                return False
        diffInstance = difflib.Differ()
        if len(mozconfigs) == 2:
            diffList = list(diffInstance.compare(mozconfigs[0], mozconfigs[1]))
            for line in diffList:
                clean_line = line[1:].strip()
                if (line[0] == '-' or line[0] == '+') and len(clean_line) > 1:
                    # skip comment lines
                    if clean_line.startswith('#'):
                        continue
                    # compare to whitelist
                    message = ""
                    if line[0] == '-':
                        if platform in mozconfigWhitelist.get(branch_name, {}):
                            if clean_line in \
                                    mozconfigWhitelist[branch_name][platform]:
                                continue
                    elif line[0] == '+':
                        if platform in mozconfigWhitelist.get('nightly', {}):
                            if clean_line in \
                                    mozconfigWhitelist['nightly'][platform]:
                                continue
                            else:
                                log.warning("%s not in %s %s!" % (
                                    clean_line, platform,
                                    mozconfigWhitelist['nightly'][platform]))
                    else:
                        log.error("Skipping line %s!" % line)
                        continue
                    message = "found in %s but not in %s: %s"
                    if line[0] == '-':
                        log.error(message % (mozconfig_paths[0],
                                             mozconfig_paths[1], clean_line))
                    else:
                        log.error(message % (mozconfig_paths[1],
                                             mozconfig_paths[0], clean_line))
                    success = False
        else:
            log.info("Missing mozconfigs to compare for %s" % platform)
            return False
    return success
