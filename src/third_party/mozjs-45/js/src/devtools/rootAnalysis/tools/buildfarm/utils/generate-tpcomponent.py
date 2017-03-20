# Generates the proper chrome/ and component/ directories that can be dropped
# into a profile to enable the pageloader
# Original Author: Alice Nodelman (anodelman@mozilla.com)
# Modified by:     Ben Hearsum    (bhearsum@mozilla.com)

import os
import sys
import zipfile
import shutil
import tempfile

# create the temp directory
tmp_dir = tempfile.mkdtemp()
pageloader_dir = os.path.join(tmp_dir, "pageloader")
# where the chrome/ and component/ directories will be put
working_dir = "."
chrome_dir = os.path.join(working_dir, 'chrome')
components_dir = os.path.join(working_dir, 'components')
# where the pageloader will be checked out from
cvsroot = ":pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot"
module = "mozilla/layout/tools/pageloader"


def removedir(rdir):
    if os.path.isdir(rdir):
        for root, dirs, files in os.walk(rdir, topdown=False):
            for name in files:
                os.remove(os.path.join(root, name))
            for name in dirs:
                os.rmdir(os.path.join(root, name))
        os.rmdir(rdir)


def zipdir(zip, zdir):
    if os.path.isdir(zdir):
        for root, dirs, files in os.walk(zdir, topdown=False):
            for name in files:
                zip.write(os.path.join(
                    root, name), os.path.join(os.path.basename(zdir), name))
    else:
        zip.write(zdir)


# bail if the directories already exist
if os.path.exists(chrome_dir):
    print "chrome/ directory exists, bailing out"
    sys.exit(1)
if os.path.exists(components_dir):
    print "components/ directory exists, bailing out"
    sys.exit(1)

oldcwd = os.getcwd()
os.chdir(tmp_dir)
# exit if cvs throws an error
if os.system("cvs -d%s co -d pageloader %s" % (cvsroot, module)):
    print "could not retrieve pageloader, bailing out"
    sys.exit(1)
os.chdir(oldcwd)

# create the directory structure in the working_dir
os.mkdir(chrome_dir)
os.mkdir(os.path.join(chrome_dir, 'content'))
os.mkdir(components_dir)

# create the pageloader.manifest file
f = open(os.path.join(chrome_dir, 'pageloader.manifest'), 'w')
f.write('content pageloader jar:pageloader.jar!/content/\n')
f.close()

shutil.copy(os.path.join(pageloader_dir, 'pageloader.xul'), os.path.join(
    chrome_dir, 'content', 'pageloader.xul'))
shutil.copy(os.path.join(pageloader_dir, 'quit.js'), os.path.join(
    chrome_dir, 'content', 'quit.js'))
shutil.copy(os.path.join(pageloader_dir, 'pageloader.js'), os.path.join(
    chrome_dir, 'content', 'pageloader.js'))
shutil.copy(os.path.join(pageloader_dir, 'report.js'), os.path.join(
    chrome_dir, 'content', 'report.js'))

# create pageloader.jar
jar = zipfile.ZipFile(os.path.join(chrome_dir, 'pageloader.jar'), 'w')
zipdir(jar, os.path.join(chrome_dir, 'content'))
jar.close()

removedir(os.path.join(chrome_dir, 'content'))

shutil.copy(os.path.join(pageloader_dir, 'tp-cmdline.js'), os.path.join(
    components_dir, 'tp-cmdline.js'))
shutil.copy(os.path.join(pageloader_dir, 'tp-cmdline.manifest'),
            os.path.join(components_dir, 'tp-cmdline.manifest'))

# get rid of the temporary directory
removedir(tmp_dir)
