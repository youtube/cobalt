import sqlalchemy as sa
from slavealloc.data import model


def denormalized_slaves(environments=None, purposes=None, pools=None, enabled=None):
    locked_masters = model.masters.alias('locked_masters')
    current_masters = model.masters.alias('current_masters')
    q = sa.select([
        model.slaves,
        model.distros.c.name.label('distro'),
        model.bitlengths.c.name.label('bitlength'),
        model.speeds.c.name.label('speed'),
        model.purposes.c.name.label('purpose'),
        model.datacenters.c.name.label('datacenter'),
        model.trustlevels.c.name.label('trustlevel'),
        model.environments.c.name.label('environment'),
        model.pools.c.name.label('pool'),
        locked_masters.c.nickname.label('locked_master'),
        current_masters.c.nickname.label('current_master'),
    ], whereclause=(
        (model.distros.c.distroid == model.slaves.c.distroid) &
        (model.bitlengths.c.bitsid == model.slaves.c.bitsid) &
        (model.speeds.c.speedid == model.slaves.c.speedid) &
        (model.purposes.c.purposeid == model.slaves.c.purposeid) &
        (model.datacenters.c.dcid == model.slaves.c.dcid) &
        (model.trustlevels.c.trustid == model.slaves.c.trustid) &
        (model.environments.c.envid == model.slaves.c.envid) &
        (model.pools.c.poolid == model.slaves.c.poolid) &
        (model.slaves.c.envid != 5)
    ), from_obj=[
        # note that the other tables will be joined automatically
        model.slaves.outerjoin(
        locked_masters, onclause=
        locked_masters.c.masterid == model.slaves.c.locked_masterid
        ).outerjoin(
            current_masters, onclause=
            current_masters.c.masterid == model.slaves.c.current_masterid
        )
    ])
    if environments:
        q = q.where(model.environments.c.name.in_(environments))
    if purposes:
        q = q.where(model.purposes.c.name.in_(purposes))
    if pools:
        q = q.where(model.pools.c.name.in_(pools))
    if enabled:
        q = q.where(model.slaves.c.enabled == enabled)
    return q

denormalized_masters = sa.select([
    model.masters,
    model.datacenters.c.name.label('datacenter'),
    model.pools.c.name.label('pool'),
], whereclause=(
    (model.datacenters.c.dcid == model.masters.c.dcid) &
    (model.pools.c.poolid == model.masters.c.poolid)
))

# this is one query, but it's easier to build it in pieces.  Use bind parameter
# 'slaveid' to specify the new slave.  There will be exactly one result row,
# unless there are no masters for this slave.  That result row is from the
# masters table


def best_master():
    # we need two copies of slaves: one to find this slave's info, and one
    # enumerating its peers
    me = model.slaves.alias('me')
    peers = model.slaves.alias('peers')

    # all of the peers of this slave, *not* including the slave itself
    me_and_peers = me.join(peers, onclause=(
        # include only slaves matching our pool
        (me.c.poolid == peers.c.poolid) &
        # .. and matching our silo
        (me.c.distroid == peers.c.distroid) &
        (me.c.bitsid == peers.c.bitsid) &
        (me.c.purposeid == peers.c.purposeid) &
        (me.c.dcid == peers.c.dcid) &
        (me.c.trustid == peers.c.trustid) &
        (me.c.envid == peers.c.envid) &
        # .. but do not include the slave itself
        (me.c.slaveid != peers.c.slaveid)
    ))

    # find the peers' masters
    peers_masters = me_and_peers.join(model.masters, onclause=(
        (peers.c.current_masterid == model.masters.c.masterid)
    ))

    # query all masterids with nonzero counts
    nonzero_mastercounts = sa.select(
        [model.masters.c.masterid, sa.func.count().label('slavecount')],
        from_obj=peers_masters,
        whereclause=(me.c.slaveid == sa.bindparam('slaveid')),
        group_by=[model.masters.c.masterid],
    ).alias('nonzero_mastercounts')

    # and join that as a subquery to get matching masters with no slaves
    pool_masters = model.slaves.join(model.masters,
                                     onclause=(model.slaves.c.poolid == model.masters.c.poolid))
    joined_masters = pool_masters.outerjoin(nonzero_mastercounts,
                                            onclause=(model.masters.c.masterid == nonzero_mastercounts.c.masterid))

    # the slave counts in joined_masters are None where we'd like 0, so fix
    # that
    numeric_slavecount = sa.case(
        [(nonzero_mastercounts.c.slavecount == None, 0)],
        else_=nonzero_mastercounts.c.slavecount)
    numeric_slavecount = numeric_slavecount.label('numeric_slavecount')

    best_master = sa.select([model.masters],
                            from_obj=joined_masters,
                            whereclause=(
                            (model.slaves.c.slaveid == sa.bindparam('slaveid')) &
                           (model.masters.c.enabled)
                            ),
                            order_by=[
                            numeric_slavecount, model.masters.c.masterid],
                            limit=1)
    return best_master
best_master = best_master()

slave_password = sa.select([model.slave_passwords.c.password],
                           from_obj=[model.slaves, model.slave_passwords],
                           whereclause=(
                           (model.slaves.c.slaveid == sa.bindparam('slaveid')) &
                          (model.slave_passwords.c.poolid == model.slaves.c.poolid) &
                          ((model.slave_passwords.c.distroid == None) |
                          (model.slave_passwords.c.distroid == model.slaves.c.distroid))))
