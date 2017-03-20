import os
import site
import sys

my_dir = os.path.dirname(__file__)
site.addsitedir(os.path.join(my_dir, '../../lib/python'))

from signing.client import buildValidatingOpener, get_token

if __name__ == '__main__':
    from optparse import OptionParser
    parser = OptionParser(__doc__)

    parser.add_option('-c', '--server-cert', dest='cert',
                      default=os.path.join(my_dir, 'host.cert'))
    parser.add_option('-u', '--username', dest='username', default='cltsign')
    parser.add_option('-p', '--password', dest='password')
    parser.add_option('-H', '--host', dest='host')
    parser.add_option('-d', '--duration', dest='duration')
    parser.add_option('-i', '--ip', dest='slave_ip')
    options, args = parser.parse_args()

    buildValidatingOpener(options.cert)
    baseurl = 'https://%s' % options.host
    sys.stdout.write(get_token(baseurl, options.username,
                     options.password, options.slave_ip, options.duration))
