# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import datetime
from optparse import OptionParser
import os
import re
import stat


def organize(basedir):
    """ This function organizes a directory of files by moving files that contain
        a date string in the format YYYY-MM-DD(-HH) into child directories of the format
        YYYY/MM/YYYY-MM-DD(-HH)/file, and creating top-level symlinks to the most recent
        30 days of directories.  It also creates a 'latest' symlink to point to the most
        recent date directory.
    """

    # Generate a list of top-level symlink dirs, and files that need to be
    # moved into date-specific directories.
    os.chdir(basedir)
    files_to_move = []
    symlink_dirs = []
    date_regex = '(\d{4}-\d{2}-\d{2})(-\d{2})?'
    date_re = re.compile(date_regex)
    for item in os.listdir(basedir):
        m = date_re.search(item)
        if m:
            if os.path.isfile(item) and not os.path.islink(item):
                date = m.group(0)
                short_filename = re.sub('[_-]?' + date_regex, '', item)
                files_to_move.append((item, date, short_filename))
            elif os.path.isdir(item) and os.path.islink(item):
                symlink_dirs.append(item)

    # Create new directories for dates that don't have them, and
    # corresponding top-level symlinks; move files into date-specific
    # directories.
    perms = stat.S_IRWXU | stat.S_IRGRP | stat.S_IXGRP | stat.S_IROTH | stat.S_IXOTH
    for (filename, date, short_filename) in files_to_move:
        date_parts = date.split('-')
        directory = os.path.join(date_parts[0], date_parts[1], date)
        if date not in symlink_dirs:
            if not os.access(directory, os.F_OK):
                os.makedirs(directory)
                # make sure all our directories have correct perms
                os.chmod(date_parts[0], perms)
                os.chmod(os.path.join(date_parts[0], date_parts[1]), perms)
                os.chmod(directory, perms)
            if not os.path.islink(date):
                os.symlink(directory, date)
                symlink_dirs.append(date)
        os.rename(filename, os.path.join(directory, filename))
        short_symlink = os.path.join(directory, short_filename)
        if os.path.exists(short_symlink) and os.path.islink(short_symlink):
            os.remove(short_symlink)
        if not os.path.exists(short_symlink):
            os.symlink(filename, short_symlink)

    # Delete any top-level symlinks that are > 30 days old
    for symlink in symlink_dirs:
        m = re.search('(\d{4})-(\d{2})-(\d{2})', symlink)
        if m:
            link_date = datetime.date(int(m.group(1)),
                                      int(m.group(2)),
                                      int(m.group(3)))
            datediff = datetime.date.today() - link_date
            if datediff.days > 30:
                os.remove(symlink)

    # Find the newest symlink and create a 'latest' symlink to the directory
    # it points to.
    if os.access('latest', os.F_OK):
        os.remove('latest')
    newest_symlink = None
    for symlink in symlink_dirs:
        if not newest_symlink or symlink > newest_symlink:
            newest_symlink = symlink
    if newest_symlink:
        date_parts = newest_symlink.split('-')
        directory = os.path.join(date_parts[0], date_parts[1], newest_symlink)
        os.symlink(directory, 'latest')

if __name__ == "__main__":
    parser = OptionParser()
    parser.add_option('--directory', dest='basedir', action='store',
                      default='.',
                      help='base directory to organize')
    options, args = parser.parse_args()

    organize(options.basedir)
