"""

Data storage for the slave allocator

"""

import sqlalchemy as sa

metadata = sa.MetaData()

# basic definitions

distros = sa.Table('distros', metadata,
                   sa.Column('distroid', sa.Integer, primary_key=True),
                   sa.Column(
                   'name', sa.String(128), nullable=False, unique=True),
                   mysql_engine="InnoDB",
                   )

datacenters = sa.Table('datacenters', metadata,
                       sa.Column('dcid', sa.Integer, primary_key=True),
                       sa.Column('name', sa.String(
                                 128), nullable=False, unique=True),
                       mysql_engine="InnoDB",
                       )

bitlengths = sa.Table('bitlengths', metadata,
                      sa.Column('bitsid', sa.Integer, primary_key=True),
                      sa.Column(
                      'name', sa.String(128), nullable=False, unique=True),
                      mysql_engine="InnoDB",
                      )

speeds = sa.Table('speeds', metadata,
                  sa.Column('speedid', sa.Integer, primary_key=True),
                  sa.Column(
                  'name', sa.String(128), nullable=False, unique=True),
                  mysql_engine="InnoDB",
                  )

purposes = sa.Table('purposes', metadata,
                    sa.Column('purposeid', sa.Integer, primary_key=True),
                    sa.Column(
                    'name', sa.String(128), nullable=False, unique=True),
                    mysql_engine="InnoDB",
                    )

trustlevels = sa.Table('trustlevels', metadata,
                       sa.Column('trustid', sa.Integer, primary_key=True),
                       sa.Column('name', sa.String(
                                 128), nullable=False, unique=True),
                       mysql_engine="InnoDB",
                       )

environments = sa.Table('environments', metadata,
                        sa.Column('envid', sa.Integer, primary_key=True),
                        sa.Column('name', sa.String(
                                  128), nullable=False, unique=True),
                        mysql_engine="InnoDB",
                        )

tac_templates = sa.Table('tac_templates', metadata,
                         sa.Column('tplid', sa.Integer, primary_key=True),
                         sa.Column('name', sa.String(
                                   128), nullable=False, unique=True),
                         sa.Column('template', sa.Text, nullable=False),
                         mysql_engine="InnoDB",
                         )

# pools

pools = sa.Table('pools', metadata,
                 sa.Column('poolid', sa.Integer, primary_key=True),
                 sa.Column(
                     'name', sa.String(128), nullable=False, unique=True),
                 mysql_engine="InnoDB",
                 )

# slave passwords, based on pool and distro

slave_passwords = sa.Table('slave_passwords', metadata,
                           # for most pools, all slaves have the same password, but for some pools,
                           # different distros have different passwords.  Needless complexity FTW!
                           # If the distro column is NULL, that is considered a wildcard and will match
                           # all distros.
                           sa.Column(
                           'poolid', sa.Integer, sa.ForeignKey('pools.poolid'), nullable=False),
                           sa.Column('distroid', sa.Integer,
                                     sa.ForeignKey('distros.distroid')),
                           sa.Column('password', sa.Text, nullable=False),
                           mysql_engine="InnoDB",
                           )

# all slaves

slaves = sa.Table('slaves', metadata,
                  sa.Column('slaveid', sa.Integer, primary_key=True),
                  sa.Column(
                  'name', sa.String(128), nullable=False, unique=True),

                  # silo (c.f. corresponding index below)
                  sa.Column('distroid', sa.Integer, sa.ForeignKey(
                            'distros.distroid'), nullable=False),
                  sa.Column(
                  'bitsid', sa.Integer, sa.ForeignKey('bitlengths.bitsid'),
                  nullable=False),
                  sa.Column(
                  'speedid', sa.Integer, sa.ForeignKey('speeds.speedid'),
                  nullable=False, default=0),
                  sa.Column('purposeid', sa.Integer, sa.ForeignKey(
                            'purposes.purposeid'), nullable=False),
                  sa.Column(
                  'dcid', sa.Integer, sa.ForeignKey('datacenters.dcid'),
                  nullable=False),
                  sa.Column('trustid', sa.Integer, sa.ForeignKey(
                            'trustlevels.trustid'), nullable=False),
                  sa.Column(
                  'envid', sa.Integer, sa.ForeignKey('environments.envid'),
                  nullable=False),

                  # pool
                  sa.Column(
                  'poolid', sa.Integer, sa.ForeignKey('pools.poolid'), nullable=False),

                  # config
                  sa.Column('basedir', sa.Text, nullable=False),
                  sa.Column(
                  'locked_masterid', sa.Integer, sa.ForeignKey('masters.masterid')),
                  sa.Column(
                  'custom_tplid', sa.Integer, sa.ForeignKey('tac_templates.tplid')),
                  sa.Column(
                  'enabled', sa.Boolean, nullable=False, default=True),
                  sa.Column('notes', sa.Text),

                  # state
                  sa.Column(
                  'current_masterid', sa.Integer, sa.ForeignKey('masters.masterid')),
                  mysql_engine="InnoDB",
                  )

# masters

masters = sa.Table('masters', metadata,
                   sa.Column('masterid', sa.Integer, primary_key=True),
                   sa.Column('nickname', sa.String(128),
                             nullable=False, unique=True),
                   sa.Column('fqdn', sa.Text, nullable=False),
                   sa.Column('http_port', sa.Integer,
                             nullable=False),  # 0 for no HTTP port
                   sa.Column('pb_port', sa.Integer, nullable=False),
                   sa.Column(
                   'dcid', sa.Integer, sa.ForeignKey('datacenters.dcid'),
                   nullable=False),

                   # pool
                   sa.Column(
                   'poolid', sa.Integer, sa.ForeignKey('pools.poolid'), nullable=False),

                   # config
                   sa.Column(
                   'enabled', sa.Boolean, nullable=False, default=True),
                   sa.Column('notes', sa.Text),
                   mysql_engine="InnoDB",
                   )

# indices

sa.Index('slave_silo',
         slaves.c.distroid,
         slaves.c.bitsid,
         slaves.c.speedid,
         slaves.c.purposeid,
         slaves.c.dcid,
         slaves.c.trustid,
         slaves.c.envid,
         )

sa.Index('slave_poolid',
         slaves.c.poolid)
