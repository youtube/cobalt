from util.retry import retry
from util.commands import run_cmd, get_output
import os
import logging
log = logging.getLogger(__name__)


def checkoutSVN(targetSVNDirectory, svnURL):
    """
    Checkout the product details SVN
    """
    if not os.path.isdir(targetSVNDirectory):
        cmd = ["svn", "co", svnURL, targetSVNDirectory]
        run_cmd(cmd)


def exportJSON(targetSVNDirectory):
    """
    Export the PHP product details to json
    """
    retval = os.getcwd()
    try:
        os.chdir(targetSVNDirectory)
        run_cmd(["php", "export_json.php"])
    finally:
        os.chdir(retval)


def doCommitSVN(commitMSG):
    """
    Actually do the commit (called with retry)
    """
    log.info("svn commit -m " + commitMSG)
    run_cmd(["svn", "commit", "-m", commitMSG])


def getSVNrev(targetSVNDirectory):
    """
    Return the svn revision
    """
    retval = os.getcwd()
    try:
        os.chdir(targetSVNDirectory)
        return get_output(["svnversion"])
    finally:
        os.chdir(retval)


def commitSVN(targetSVNDirectory, product, version, fakeCommit=False):
    """
    Commit the change in the svn
    """
    retval = os.getcwd()
    os.chdir(targetSVNDirectory)
    try:
        if not os.path.isdir(".svn"):
            raise Exception("Could not find the svn directory")
        commitMSG = "'" + product + " version " + version + "'"
        if not fakeCommit:
            retry(doCommitSVN, args=(commitMSG), attempts=3, sleeptime=0)
    finally:
        os.chdir(retval)


def updateRev(targetSVNDirectory, newRev):
    run_cmd(["svn", "propset", "svn:externals", "'product-details -r" + newRev, "http://svn.mozilla.org/libs/product-details'", "tags/stage/includes"])
    run_cmd(["svn", "propset", "svn:externals", "'product-details -r" + newRev, "http://svn.mozilla.org/libs/product-details'", "tags/productions/includes"])
    run_cmd(["svn", "up"])
