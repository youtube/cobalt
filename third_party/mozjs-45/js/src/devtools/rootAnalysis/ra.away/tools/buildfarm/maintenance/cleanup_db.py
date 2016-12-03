#!/usr/bin/env python
import sqlalchemy as sa
import time
import json

import logging
log = logging.getLogger(__name__)


# From
# http://stackoverflow.com/questions/5631078/sqlalchemy-print-the-actual-query
def query_to_str(statement, bind=None):
    """
    print a query, with values filled in
    for debugging purposes *only*
    for security, you should always separate queries from their values
    please also note that this function is quite slow
    """
    import sqlalchemy.orm
    if isinstance(statement, sqlalchemy.orm.Query):
        if bind is None:
            bind = statement.session.get_bind(
                statement._mapper_zero_or_none())
        statement = statement.statement
    elif bind is None:
        bind = statement.bind

    dialect = bind.dialect
    compiler = statement._compiler(dialect)

    class LiteralCompiler(compiler.__class__):
        def visit_bindparam(
                self, bindparam, within_columns_clause=False,
                literal_binds=False, **kwargs
        ):
            return super(LiteralCompiler, self).render_literal_bindparam(
                bindparam, within_columns_clause=within_columns_clause,
                literal_binds=literal_binds, **kwargs)

    compiler = LiteralCompiler(dialect, statement)
    return compiler.process(statement)


def cleaner_upper(select_query, delete_queries):
    """
    Cleans stuff up.

    Runs select query, which should return a list of ids to delete.
    Chunks of ids are passed to each query of delete_queries.

    delete queries should be functions that accept a list of ids to delete.

    Returns once select_query no longer returns any results

    Sleeps between rounds
    """
    while True:
        t = time.time()
        log.debug("finding rows: %s", query_to_str(select_query))
        rows = select_query.execute()
        log.info("found %i rows in %.2fs", rows.rowcount, time.time() - t)
        if rows.rowcount == 0:
            break

        # Delete 100 at a time
        ids = [row[0] for row in rows]
        chunk_size = 100
        for i in range(0, len(ids), chunk_size):
            for q in delete_queries:
                t = time.time()
                q(ids[i:i + chunk_size])
                sleep_time = max(0.1, (time.time() - t) * 4.0)
                log.debug("sleeping for %.2fs", sleep_time)
                time.sleep(sleep_time)


def deleter(column):
    """
    Returns a function that accepts a list of ids, and deletes
    rows that match.
    """
    def delete_func(ids):
        t = time.time()
        q = column.table.delete().where(column.in_(ids))
        log.debug(query_to_str(q))
        n = q.execute().rowcount
        log.info("deleted %i rows from %s in %.2fs", n, column.table.name,
                 time.time() - t)
    return delete_func


def cleanup_statusdb_builds(meta, cutoff):
    log.info("Cleaning up builds before %s", cutoff)
    t_builds = sa.Table('builds', meta, autoload=True)
    t_steps = sa.Table('steps', meta, autoload=True)
    t_properties = sa.Table('build_properties', meta, autoload=True)
    t_schedulerdb_requests = sa.Table('schedulerdb_requests', meta,
                                      autoload=True)

    builds_q = sa.select([t_builds.c.id]).\
        where(t_builds.c.starttime < cutoff).\
        order_by(t_builds.c.starttime.asc()).\
        limit(10000)

    cleaner_upper(builds_q, [
        deleter(t_steps.c.build_id),
        deleter(t_properties.c.build_id),
        deleter(t_schedulerdb_requests.c.status_build_id),
        deleter(t_builds.c.id),
    ])
    log.info("Finished cleaning up builds")


def cleanup_statusdb_orphaned_steps(meta):
    log.info("Cleaning up orphaned steps")
    t_builds = sa.Table('builds', meta, autoload=True)
    t_steps = sa.Table('steps', meta, autoload=True)

    q = sa.select(
        [t_steps.c.build_id],
        # nopep8 - we really do need to use == None
        t_builds.c.id == None,
        from_obj=[t_steps.outerjoin(t_builds, t_steps.c.build_id ==
                                    t_builds.c.id)],
        distinct=True,
    ).limit(10000)

    cleaner_upper(q, [
        deleter(t_steps.c.build_id),
    ])
    log.info("Finished cleaning up orphaned steps")


def cleanup_statusdb_orphaned_properties(meta):
    log.info("Cleaning up orphaned build properties")
    t_builds = sa.Table('builds', meta, autoload=True)
    t_properties = sa.Table('build_properties', meta, autoload=True)

    q = sa.select(
        [t_properties.c.build_id],
        # nopep8 - we really do need to use == None
        t_builds.c.id == None,
        from_obj=[t_properties.outerjoin(t_builds, t_properties.c.build_id ==
                                         t_builds.c.id)],
        distinct=True,
    ).limit(10000)

    cleaner_upper(q, [
        deleter(t_properties.c.build_id),
    ])
    log.info("Finished cleaning up orphaned build properties")


IGNORABLE_CLASSES = (
    "buildbotcustom.l10n.TriggerableL10n",
    "buildbotcustom.scheduler.PersistentScheduler",
    "buildbot.schedulers.basic.Dependent",
    "buildbot.schedulers.triggerable.Triggerable",
    "buildbotcustom.scheduler.TriggerBouncerCheck",
    "buildbotcustom.scheduler.Dependent-props",
    "buildbotcustom.buildbotcustom.l10n.TriggerableL10n",
    "buildbot.schedulers.timed.Nightly",
    "buildbotcustom.scheduler.SpecificNightly-props",
    "buildbotcustom.scheduler.Nightly-props",
    "buildbotcustom.scheduler.AggregatingScheduler",
)

PERBUILD_CLASSES = (
    "buildbot.schedulers.basic.Scheduler",
    "buildbotcustom.scheduler.MultiScheduler",
    "buildbotcustom.scheduler.BuilderChooserScheduler",
    "buildbotcustom.scheduler.Scheduler-props",
    "buildbotcustom.scheduler.BuilderChooserScheduler-props",
    "buildbotcustom.scheduler.EveryNthScheduler")

_change_cache = {}


def get_change_date(db, changeid):
    if changeid in _change_cache:
        return _change_cache[changeid]

    q = sa.text("SELECT when_timestamp FROM changes WHERE changeid=:changeid")
    results = db.execute(q, changeid=changeid)
    row = results.fetchone()
    if row:
        _change_cache[changeid] = row.when_timestamp
        return row.when_timestamp
    else:
        _change_cache[changeid] = None
        return None


def should_delete(db, s, cutoff):
    state = json.loads(s.state)
    if s.class_name in IGNORABLE_CLASSES:
        # Not sure if we have enough data here to make a decision...
        return False
    elif s.class_name in PERBUILD_CLASSES:
        assert "last_processed" in state
        change_time = get_change_date(db, state['last_processed'])
        if change_time < cutoff:
            return True
    else:
        log.warning("unhandled scheduler class for scheduler %s: %s %s",
                    s.schedulerid, s.class_name, s.name)
        return False


def cleanup_schedulerdb_schedulers(db):
    now = time.time()
    regular_cutoff = now - 7 * 86400  # 1 week
    release_cutoff = now - 60 * 86400  # 2 months

    # Get all the schedulers from the db
    q = sa.text("SELECT * FROM schedulers")
    schedulers = db.execute(q)
    to_delete = []
    for s in schedulers:
        if s.name.startswith("release-"):
            cutoff = release_cutoff
        else:
            cutoff = regular_cutoff

        try:
            if should_delete(db, s, cutoff):
                to_delete.append(s)
        except Exception:
            log.exception("couldn't process scheduler %s: %s", s.schedulerid,
                          s.name)

    for s in to_delete:
        log.info("deleting scheduler %s: %s %s %s", s.schedulerid,
                 s.class_name, s.name, s.state)
        q = sa.text("DELETE FROM schedulers WHERE schedulerid=:schedulerid")
        db.execute(q, schedulerid=s.schedulerid)


if __name__ == '__main__':
    from optparse import OptionParser
    from ConfigParser import RawConfigParser
    parser = OptionParser()
    parser.set_defaults(
        filename=None,
        loglevel=logging.INFO,
        logfile=None,
        skip_orphans=False,
    )
    parser.add_option("-l", "--logfile", dest="logfile")
    parser.add_option("--status-db", dest="status_db")
    parser.add_option("--scheduler-db", dest="scheduler_db")
    parser.add_option("--config", dest="config_file", help="config file")
    parser.add_option("--cutoff", dest="cutoff",
                      help="cutoff date, prior to which we'll delete data. "
                      "format is YYYY-MM-DD")
    parser.add_option("--skip-orphans", dest="skip_orphans",
                      action="store_true")
    parser.add_option("-v", "--verbose", dest="loglevel", action="store_const",
                      const=logging.DEBUG, help="run verbosely")
    parser.add_option("-q", "--quiet", dest="loglevel", action="store_const",
                      const=logging.WARNING, help="run quietly")
    options, args = parser.parse_args()

    # Configure logging
    root_log = logging.getLogger()
    log_formatter = logging.Formatter("%(asctime)s - %(message)s")

    if options.logfile:
        import logging.handlers
        # Week one log file per week for 4 weeks
        handler = logging.handlers.TimedRotatingFileHandler(options.logfile,
                                                            when='W6',
                                                            backupCount=4)
    else:
        handler = logging.StreamHandler()
    handler.setFormatter(log_formatter)
    root_log.setLevel(options.loglevel)
    root_log.addHandler(handler)

    if not options.cutoff:
        parser.error("cutoff date is required")

    # Load options from config if it's set
    if options.config_file:
        config_parser = RawConfigParser()
        config_parser.read([options.config_file])
        if config_parser.has_option("db", "status_db"):
            options.status_db = config_parser.get("db", "status_db")
        if config_parser.has_option("db", "scheduler_db"):
            options.scheduler_db = config_parser.get("db", "scheduler_db")

    # Clean up statusdb
    if options.status_db:
        status_db = sa.create_engine(options.status_db, pool_recycle=300)
        meta = sa.MetaData(bind=status_db)
        cleanup_statusdb_builds(meta, options.cutoff)
        if not options.skip_orphans:
            cleanup_statusdb_orphaned_steps(meta)
            cleanup_statusdb_orphaned_properties(meta)

    if options.scheduler_db:
        scheduler_db = sa.create_engine(options.scheduler_db, pool_recycle=300)

        cleanup_schedulerdb_schedulers(scheduler_db)
