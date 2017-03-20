import simplejson
import csv
import sqlalchemy as sa
from slavealloc.data import model


def setup_argparse(subparsers):

    subparser = subparsers.add_parser('dbimport', help="""Import objects into the
        database from CSV files.  This will take care of creating any additional
        rows in the ancillary tables, making it much easier than hand-crafted
        INSERT statements.  Note that all CSV files are assumed to begin with
        a header row.""")

    subparser.add_argument('-D', '--db', dest='dburl',
                           default='sqlite:///slavealloc.db',
                           help="""SQLAlchemy database URL; defaults to slavealloc.db in the
            current dir""")

    subparser.add_argument('--slave-data', dest='slave_data',
                           help="""csv of slave data to import (columns: name, basedir,
            distro, bitlength, purpose, datacenter, trustlevel,
            speed, environment, pool)""")

    subparser.add_argument('--master-data', dest='master_data',
                           help="""csv of master data to import (columns: nickname, fqdn,
            http_port, pb_port, datacenter, and pool)""")

    subparser.add_argument('--master-json', dest='master_json',
                           help="""JSON of master data to import, e.g., production-masters.json""")

    subparser.add_argument('--password-data', dest='password_data',
                           help="""csv of password data to import (columns: pool, distro,
            password); a distro of '*' is converted to NULL""")

    subparser.add_argument('--enable', dest='enable', action='store_true',
                           default=False,
                           help="""Enable newly added masters or slaves (default is to disable)""")

    return subparser


def process_args(subparser, args):
    pass


def main(args):
    eng = sa.create_engine(args.dburl)
    model.metadata.bind = eng

    slaves = []
    if args.slave_data:
        rdr = csv.DictReader(open(args.slave_data))
        slaves = list(rdr)

    masters = []
    if args.master_data:
        rdr = csv.DictReader(open(args.master_data))
        masters = list(rdr)
    elif args.master_json:
        masters = json2list(args.master_json)

    passwords = []
    if args.password_data:
        rdr = csv.DictReader(open(args.password_data))
        passwords = list(rdr)

    # get id's for all of the names, inserting into the db if necessary

    def normalize(table, idcolumn, values):
        # see what's in there already
        res = table.select().execute()
        mapping = dict([(row.name, row[idcolumn]) for row in res])

        # insert anything missing
        to_insert = set([value
                         for value in set(values)
                         if value not in mapping])
        if to_insert:
            table.insert().execute([{'name': n}
                                    for n in to_insert])

            # and re-select if we've changed anything
            res = table.select().execute()
            mapping = dict([(row.name, row[idcolumn]) for row in res])

        return mapping

    distros = normalize(model.distros, 'distroid',
                        [r['distro'] for r in slaves])
    bitlengths = normalize(model.bitlengths, 'bitsid',
                           [r['bitlength'] for r in slaves])
    purposes = normalize(model.purposes, 'purposeid',
                         [r['purpose'] for r in slaves])
    datacenters = normalize(model.datacenters, 'dcid',
                            [r['datacenter'] for r in slaves] +
                            [r['datacenter'] for r in masters])
    trustlevels = normalize(model.trustlevels, 'trustid',
                            [r['trustlevel'] for r in slaves])
    speeds = normalize(model.speeds, 'speedid',
                       [r['speed'] for r in slaves])
    environments = normalize(model.environments, 'envid',
                             [r['environment'] for r in slaves])
    pools = normalize(model.pools, 'poolid',
                      [r['pool'] for r in passwords] +
                      [r['pool'] for r in slaves] +
                      [r['pool'] for r in masters])

    # insert into the data tables

    if masters:
        model.masters.insert().execute([
            dict(nickname=row['nickname'],
                 fqdn=row['fqdn'],
                 http_port=int(
                 row['http_port']) if row['http_port'] else 0,
                 pb_port=int(row['pb_port']),
                 dcid=datacenters[row['datacenter']],
                 poolid=pools[row['pool']],
                 enabled=args.enable)
            for row in masters])

    if slaves:
        model.slaves.insert().execute([
            dict(name=row['name'],
                 distroid=distros[row['distro']],
                 bitsid=bitlengths[row['bitlength']],
                 purposeid=purposes[row['purpose']],
                 dcid=datacenters[row['datacenter']],
                 trustid=trustlevels[row['trustlevel']],
                 envid=environments[row['environment']],
                 speedid=speeds[row['speed']],
                 poolid=pools[row['pool']],
                 basedir=row['basedir'],
                 current_masterid=None,
                 enabled=args.enable)
            for row in slaves])

    if passwords:
        # convert a distro of '*' to NULL
        distros_or_null = distros.copy()
        distros_or_null['*'] = None

        model.slave_passwords.insert().execute([
            dict(poolid=pools[row['pool']],
                 distroid=distros_or_null[row['distro']],
                 password=row['password'])
            for row in passwords])


def json2list(json_file):
    # note that this embodies some releng-specific smarts at the moment

    list_json = simplejson.load(open(json_file))

    datacentre2datacenter = dict(
        mv='mtv1',
    )

    rv = []
    for master_json in list_json:
        if not master_json['enabled']:
            continue

        # smarts #1: releng -> IT names for datacenters
        datacenter = datacentre2datacenter[master_json['datacentre']]

        # smarts #2: role/dc -> pool
        pool = '%s-%s' % (master_json['role'], datacenter)

        # smarts #3: use unqualified hostname
        fqdn = master_json['hostname']

        master = dict(
            nickname=master_json['name'],
            fqdn=fqdn,
            http_port=master_json.get('http_port', None),
            pb_port=master_json['pb_port'],
            datacenter=datacenter,
            pool=pool)
        rv.append(master)

    return rv
