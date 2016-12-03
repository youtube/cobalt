import os
from util.commands import run_cmd


def generateChecksums(checksums_dir, sums_info):
    """
    Generates {MD5,SHA1,etc}SUMS files using *.checksums files.

    @type  checksums_dir: string
    @param checksums_dir: Directory name wich *.checksums files

    @type  sums_info: dict
    @param sums_info: A dictionary which contains hash type and output file
                      pairs. Example: {'sha1': '/tmp/SHA1SUMS',
                                       'md5': '/tmp/MD5SUMS'}
    """
    sums = {}
    for hash_type in sums_info.keys():
        sums[hash_type] = []
    for top, dirs, files in os.walk(checksums_dir):
        files = [f for f in files if f.endswith('.checksums')]
        for f in files:
            fd = open(os.path.join(top, f))
            for line in fd:
                line = line.rstrip()
                try:
                    hash, hash_type, size, file_name = line.split(None, 3)
                except ValueError:
                    print "Failed to parse the following line:"
                    print line
                    raise
                entry = (hash, file_name)
                if hash_type in sums and entry not in sums[hash_type]:
                    sums[hash_type].append(entry)
    for hash_type in sums_info.keys():
        sums_file = open(sums_info[hash_type], 'w')
        # sort by file name
        for hash, file_name in sorted(sums[hash_type], key=lambda x: x[1]):
            sums_file.write('%s  %s\n' % (hash, file_name))
        sums_file.close()


def signFiles(files):
    for f in files:
        run_cmd(
            ['bash', '-c', os.environ['MOZ_SIGN_CMD'] + ' -f gpg "%s"' % f])
