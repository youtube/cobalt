import os
from os import path


def getLatestDir(product, branch, platform, nightlyDir="nightly",
                 protocol=None, server=None):
    if protocol:
        assert server is not None, "server is required with protocol"

    dirpath = "/pub/mozilla.org/" + product + "/" + nightlyDir + "/" + \
              "latest-" + branch + "-" + platform

    if protocol:
        return protocol + "://" + server + dirpath
    else:
        return dirpath


def getSnippetDir(brandName, version, buildNumber):
    return '%s-%s-build%s' % (brandName, version, buildNumber)


def getMUSnippetDir(brandName, oldVersion, oldBuildNumber, version,
                    buildNumber):
    return '%s-%s-build%s-%s-build%s-MU' % (brandName, oldVersion,
                                            oldBuildNumber, version,
                                            buildNumber)


def getRealpath(file_name, depth=1, cwd=None):
    """Returns real path of the file, resolving symlinks.

    @type  file_name: string
    @param file_name: File name to be resolved.

    @type  depth: int
    @param depth: How many directories to add to the path.
                  /buids/buildbot/builder_master1/localconfig.py
                  which points to
                  /buids/buildbot/builder_master1/configs/mozilla/production_config.py
                  will return mozilla/production_config.py with depth set to 1,
                  and configs/mozilla/production_config.py with depth set to 2.

    @type  cwd: string
    @param cwd: Current working direcotry. Default is os.getcwd().
    """

    if not cwd:
        cwd = os.getcwd()

    if not path.isabs(file_name):
        file_name = path.join(cwd, file_name)
    real_path = path.realpath(file_name)
    # Get N leading directories + file name
    depth += 1
    path_members = real_path.split(os.sep)[-depth:]
    return path.join(*path_members)


def get_repo_dirname(repo_path):
    """Returns repo directory name based on repo path"""
    return repo_path.rstrip("/").split("/")[-1]
