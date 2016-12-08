#  BEGIN LICENSE BLOCK
#  Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
#  The contents of this file are subject to the Mozilla Public License Version
#  1.1 (the "License"); you may not use this file except in compliance with
#  the License. You may obtain a copy of the License at
#  http://www.mozilla.org/MPL/
#
#  Software distributed under the License is distributed on an "AS IS" basis,
#  WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
#  for the specific language governing rights and limitations under the
#  License.
#
#  The Original Code is Places Test code.
#
#  The Initial Developer of the Original Code is Mozilla Corp.
#  Portions created by the Initial Developer are Copyright (C) 2009
#  the Initial Developer. All Rights Reserved.
#
#  Contributor(s):
#  David Dahl <ddahl@mozilla.com>
#
#  Alternatively, the contents of this file may be used under the terms of
#  either the GNU General Public License Version 2 or later (the "GPL"), or
#  the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
#  in which case the provisions of the GPL or the LGPL are applicable instead
#  of those above. If you wish to allow use of your version of this file only
#  under the terms of either the GPL or the LGPL, and not to allow others to
#  use your version of this file under the terms of the MPL, indicate your
#  decision by deleting the provisions above and replace them with the notice
#  and other provisions required by the GPL or the LGPL. If you do not delete
#  the provisions above, a recipient may use your version of this file under
#  the terms of any one of the MPL, the GPL or the LGPL.
#
#  END LICENSE BLOCK

"""
using the 'Places generator'
----------------------------

Requirements: django.db - you should have a full install of django 1.0.2 release. We are using django solely for it's ORM and 'manage.py inspectdb' utility which reverse-engineers a db schema into django Model classes.

You must have django and the 'places' python module in your PYTHONPATH

in my ~/.zshrc I have:

export PYTHONPATH=$PYTHONPATH:~/code/python:~/code/mozilla-central/mozilla/toolkit/components/places/tests/generator/places

inside ~/code/mozilla-central/mozilla/toolkit/components/places/tests/generator is the places/ Django 'project'

You must set 2 ENV VARS:

1) PLACES_DB_PATH
 e.g.: export PLACES_DB_PATH=~/code/python/places/places.sqlite

2) DJANGO_SETTINGS_MODULE
 e.g.: export DJANGO_SETTINGS_MODULE=places.settings

Generate Script Docs:

python places/builddb/generate.py --help

================================================
This will pump your places.sqlite with a bunch of
ficticious places, history and bookmarks based on the current 'max' aggregation at https://places-stats.mozilla.com/
"""

import os
from optparse import OptionParser
import random
import urllib2
import random
import time
from copy import deepcopy
from math import ceil, floor

from places import uuid
from places.builddb.models import *
from places.builddb.http import get_web_data

"""
Places data generator. Create as many places/history/bookmarks as needed for testing Places queries for performance.

Tunable parameters: absolute # visits, bookmarks, age:
create random dates up to today,

"""

DEBUG = True
PERCENT_PLACES_TYPED = 0.3
TWENTY_FOUR_HOURS = (60 * 60 * 24 * 1000 * 1000)

from django.db import connection, transaction


class DateForward(object):
    """
    go through the places db and increment the dates by one day
    moz_annos.dateAdded
    moz_annos.lastModified
    moz_annos.expiration
    moz_bookmarks.dateAdded
    moz_bookmarks.lastModified
    moz_favicons.expiration
    moz_historyvisits.visit_date
    moz_items_annos.dateAdded
    moz_items_annos.expiration
    moz_items_annos.lastModified

    """
    def __init__(self):
        pass

    def table_column(self):
        """
        keeps track of the tables/columns that need to be updated
        """
        table_cols = {'moz_annos': ['dateadded', 'lastmodified', 'expiration', ],
                      'moz_bookmarks': ['dateadded', 'lastmodified', ],
                      'moz_favicons': ['expiration'],
                      'moz_historyvisits': ['visit_date'],
                      'moz_items_annos': ['dateadded', 'lastmodified', 'expiration', ]}
        return table_cols

    def place_id(self):
        sql = """select id from moz_places where title = 'Places Testing Placeholder'"""
        cursor = connection.cursor()
        rows = cursor.execute(sql, [])
        id = rows.fetchall()[0][0]
        return id

    def last_update(self):
        """
        get the last update date
        """
        id = self.place_id()
        sql = """SELECT visit_date FROM moz_historyvisits WHERE place_id = %s ORDER BY id DESC""" % id
        cursor = connection.cursor()
        rows = cursor.execute(sql)
        _rows = rows.fetchall()
        return _rows[0][0]

    def increment_dates(self):
        """
        update the dates
        """
        last_update_seconds = ((self.last_update() / 1000) / 1000)
        now = int(time.time())
        self.now_micro_secs = ((now * 1000) * 1000)

        if DEBUG:
            print "last update microseconds: %s" % self.last_update()
            print "now: %s" % self.now_micro_secs
        time_diff = (now - last_update_seconds)
        self.time_diff = ((time_diff * 1000) * 1000)

        if DEBUG:
            print "\nTIME DIFF (microseconds): %s\n\n" % str(self.time_diff)

        if DEBUG:
            print "Incrementing History"
        self.increment_history()
        if DEBUG:
            print "Incrementing Bookmarks"
        self.increment_bookmarks()
        if DEBUG:
            print "Incrementing Favicons"
        self.increment_favicons()
        if DEBUG:
            print "Incrementing Annos"
        self.increment_annos()
        if DEBUG:
            print "Incrementing Items Annos"
        self.increment_items_annos()
        if DEBUG:
            print "VACUUM/REINDEX"
        self.vacuum_and_reindex()

        last_time_run()

    def increment_history(self):

        self.increment_query(
            'moz_historyvisits', 'visit_date', restrict_update=True)

    def increment_bookmarks(self):

        self.increment_query('moz_bookmarks', 'dateadded')
        self.increment_query('moz_bookmarks', 'lastmodified')

    def date_missing(self, _date):
        """
        check to see if the date is None or 0, if so, return True
        """
        if _date == 0 or _date is None or _date == '':
            return True
        else:
            return False

    def increment_query(self, table_name, col_name, restrict_update=False):
        """
        shorter, faster updates of the date column
        """
        if restrict_update:
            # get the place_id
            place_id = self.place_id()

        try:
            sql = "UPDATE %s SET %s " % (table_name, col_name,)
            sql = sql + " = %s + %s"

            if restrict_update:
                where = " WHERE %s < %s AND place_id != %s AND %s != ''" % (
                    col_name, self.now_micro_secs, place_id, col_name,)
            else:
                where = " WHERE %s < %s AND %s != ''" % \
                    (col_name, self.now_micro_secs, col_name, )

            sql = sql + where

            debug_sql = sql % (col_name, self.time_diff,)
            if DEBUG:
                print debug_sql
            cursor = connection.cursor()
            cursor.execute(debug_sql)
            cursor.close()
        except Exception, e:
            print """************ ERROR *******************"""
            print """***%s""" % e
            raise

    def increment_favicons(self):
        """
        uses a simpler approach via a cursor
        """
        self.increment_query('moz_favicons', 'expiration')

    def increment_annos(self):

        self.increment_query('moz_annos', 'dateadded')
        self.increment_query('moz_annos', 'lastmodified')
        self.increment_query('moz_annos', 'expiration')

    def increment_items_annos(self):

        self.increment_query('moz_items_annos', 'dateadded')
        self.increment_query('moz_items_annos', 'lastmodified')
        self.increment_query('moz_items_annos', 'expiration')

    def vacuum_and_reindex(self):
        try:
            v_sql = "VACUUM;"
            r_sql = "REINDEX;"

            cursor = connection.cursor()
            cursor.execute(v_sql)
            cursor.execute(r_sql)
            cursor.close()
        except Exception, e:
            print """************ ERROR *******************"""
            print """***%s""" % e
            raise


def right_now():
    """
    right now! in microseconds
    """
    return time.time() * 1000 * 1000


def six_months_ago():
    """
    time in microseconds six months ago (roughly)
    """
    six_months = (60 * 60 * 24 * 180 * 1000 * 1000)
    n = right_now()
    smo = n - six_months
    return smo


def random_date():
    """
    a random date (in microseconds since epoch) up to six months ago
    """
    six_months = six_months_ago()
    now = right_now()

    s = random.randint(int(six_months), int(now))
    return s


def alpha():
    return map(chr, range(97, 123))

ALPHA = alpha()


def numbers():
    return [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]


def new_favicon():
    """
    create a new, unique favicon
    """
    guid = uuid.generate()
    url = 'http://tld.tld/%s.ico' % guid
    favicon = MozFavicons.objects.create(
        url=url,
        data='wootuyiuyeryuwyerywueyurywueyruuewryueyuryuewyurkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjsdhjshjdhjshdjhjsdhjshjkdhjshjdhjshkjdhsjkhdkjhsjdhjshjhdd;sklfnlk;dnfjnklwmnkldjfklhadlfgjkhrnfjkgb;jlshgn;ruignifng;irgn;srhg;rnv;irn;',
        mime_type='image/png',
        expiration=random_date())
    return favicon


def reverse_host(url):
    """
    reverse the host
    """
    parts = urllib2.urlparse.urlparse(url)
    ru = "".join(reversed(parts[1])) + '.'
    return ru


def url_parts():
    """
    return a dictionary like: {'proto':'http'
                               'host':'www',
                               'domain':'foo',
                               'tld':'com'}
    """
    protocol = ['https', 'http', 'ftp']
    host_len = random.randint(4, 26)
    host = "".join(random.sample(ALPHA, host_len))
    domain_len = random.randint(2, 26)
    domain = "".join(random.sample(ALPHA, domain_len))
    tld_len = random.randint(2, 3)
    tld = "".join(random.sample(ALPHA, tld_len))
    proto_idx = random.randint(0, 2)
    proto = protocol[proto_idx]
    return {'proto': proto, 'host': host, 'domain': domain, 'tld': tld}


def new_title():
    """
    return a random title
    """
    title_len = random.randint(3, 26)
    title = "".join(random.sample(ALPHA, title_len))
    return title


def keyword_gen(n):
    kw_list, kw_count = make_keywords(n)

    def gen():
        for obj in kw_list:
            yield obj
    return gen()


def make_keywords(n):
    """
    make num_keywords
    """
    kw_list = []
    i = 0
    while n > 0:
        keyword_len = random.randint(3, 18)
        kw = "".join(random.sample(ALPHA, keyword_len))
        keyword = MozKeywords(keyword=kw)
        keyword.save()
        k = MozKeywords.objects.get(keyword=kw)
        kw_list.append(k)
        n = n - 1
        i = i + 1

    return (kw_list, i)


def expo(MEAN, round_ceil='ceil'):
    """
    returns a value for a exponential dustribution
    """
    lambd = 1.0 / MEAN  # the lambda value for the random generator
    val = random.expovariate(lambd)
    if round_ceil == 'ceil':
        val = int(ceil(val))
    val = int(round(val))
    return val


def test_expo(num, MEAN):
    """
    test the curvyness
    """
    f = []
    while num > 0:
        f.append(expo(MEAN))
        num = num - 1
    return f


def frecency():
    """
    calculate an exponential frecency value
    """
    MEAN = 146  # 146 is our mean value, because ddahl's is 146
    lambd = 1.0 / MEAN  # the lambda value for the random generator
    val = random.expovariate(lambd)
    if val < 1:
        # flip a coin:
        if random.randint(0, 1) == 0:
            return 0
        else:
            return -1
    f_ceil = ceil(val)
    return f_ceil


def test_frecency(num):
    frec_list = []
    while num > 0:
        frec_list.append(frecency())
        num = num - 1
    frec_list.sort()
    return frec_list


def flip_coin():
    return random.randint(0, 1)


def slant(rows, heads):
    """
    slant the coin toss to a percentage
    """
    results = []
    num_heads = round(int(rows * heads))
    num_tails = rows - num_heads
    while num_heads > 0:
        results.append(1)
        num_heads = num_heads - 1
    while num_tails > 0:
        results.append(0)
        num_tails = num_tails - 1
    return results


def flip_coin_slant(rows, heads):
    results = slant(rows, heads)

    def fcs():
        for r in results:
            yield r
    return fcs


def last_time_run():
    """
    Use get_or_create to make/get a url that keeps track of when this operation was last run on the database
    returns date in microseconds of when this place was last updated
    """
    my_url = "http://mozilla.com/places_test_placeholder"
    my_title = "Places Testing Placeholder"

    p, created = place = MozPlaces.objects.get_or_create(url=my_url, title=my_title, rev_host=reverse_host(my_url), visit_count=1, hidden=0, typed=1, frecency=-1)
    sql = """select id from moz_places where title = 'Places Testing Placeholder'"""
    cursor = connection.cursor()
    rows = cursor.execute(sql)
    id = rows.fetchall()[0][0]
    # insert the latest record into historyvisits
    sql = """INSERT INTO moz_historyvisits (from_visit, place_id, visit_date, visit_type, session ) VALUES (0, %s, %s, 1, 0 )""" % (int(id), right_now(),)
    cursor.execute(sql)
    transaction.commit_unless_managed()

    sql = """SELECT visit_date FROM moz_historyvisits WHERE place_id = %s ORDER BY id DESC""" % id
    rows = cursor.execute(sql)
    _rows = rows.fetchall()
    visit_date = _rows[0][0]
    return visit_date


def new_place(places_typed_gen):
    """
    create a new place
    """
    guid = uuid.generate()
    parts = url_parts()
    my_url = '%s://%s.%s.%s/%s' % (parts['proto'],
                                   parts['host'],
                                   parts['domain'],
                                   parts['tld'],
                                   guid)
    if DEBUG:
        print my_url
    my_title = new_title()
    frec = frecency()
    try:
        place = MozPlaces(
            url=my_url,
            title=my_title,
            rev_host=reverse_host(my_url),
            visit_count=expo(8),  # using 8 for MEAN for the fun of it for now
            hidden=0,
            typed=places_typed_gen.next(),
            favicon=new_favicon(),
            frecency=frec)
        place.save()
        p = MozPlaces.objects.filter(rev_host=place.rev_host)[0]
        return p

    except Exception, e:
        print e
        raise


def new_inputhistory(place):
    """
    create an inputhistory record for the place object passed in
    """
    input_len = random.randint(3, 20)
    ih = "".join(random.sample(ALPHA, input_len))
    ih_multi = random.randint(2, 9)
    uc = ih_multi * 0.9
    inp_hist = MozInputhistory(place_id=place.id, input=ih, use_count=uc)
    inp_hist.save()
    return inp_hist


def new_history(place, qty=5):
    """
    create a history record
    """
    rndm_date = random_date()

    try:
        while qty > 0:
            history = MozHistoryvisits.objects.create(
                from_visit=0,  # other visit this is related to/from/etc
                place=place,
                visit_date=rndm_date,
                visit_type=1,
                session=0
            )
            qty = qty - 1
    except:
        print "ERROR: cannot create historyvisit for %s" % str(place.id)
        raise


def new_bookmark(place, kw_gen):
    """
    make a bookmark item
    """
    try:
        k_obj = kw_gen.next()
    except Exception, e:
        print str(e)
        k_obj = None

    try:
        bookmark = MozBookmarks.objects.create(
            _type=1,  # XXX: what should this be?
            fk=place.id,
            parent=5,  # XXX: what is unfiled bookmarks id?
            position=-1,  # XXX: what should this be?
            title=place.title,
            folder_type=1,  # XXX: what should this be?
            dateadded=random_date(),
            lastmodified=random_date(),
            keyword=k_obj
        )
        return bookmark
    except Exception, e:
        print str(e)
        print "Err: Could not create bookmark from place: %s" % place.id


def parse_places_stats_obj(obj, source_idx):
    """
    Parse and massage the input for the places generator
    """
    class Options(object):
        def __init__(self, stats_obj, source_idx):
            # XXX: check names in json obj
            self.source = 'places_stats_site'
            self.path = None
            self.source_idx = source_idx
            src_obj = stats_obj[source_idx]
            self.n_places = int(float(stats_obj[source_idx]['moz_places_cnt']))
            self.h_mult = int(float(stats_obj[source_idx]['moz_historyvisits_cnt']) / float(stats_obj[source_idx]['moz_places_cnt']))
            self.b_mod = int(float(stats_obj[source_idx]['moz_places_cnt']) / float(stats_obj[source_idx]['moz_bookmarks_cnt']))
            kw_count = int(float(stats_obj[source_idx]['moz_keywords_cnt']))
            bm_count = int(float(stats_obj[source_idx]['moz_bookmarks_cnt']))
            self.n_keywords = kw_count
            self.n_inputhistory = \
                int(float(stats_obj[source_idx]['moz_inputhistory_cnt']))
            if DEBUG:
                print "########################################################"
                print "Creating %s Places" % self.n_places
                print "Creating about %s History Visits" % stats_obj[source_idx]['moz_historyvisits_cnt']
                print "Creating about %s Bookmarks" % stats_obj[source_idx]['moz_bookmarks_cnt']
                print "Creating %s Keywords" % self.n_keywords
                print "Creating %s Input History Records" % self.n_inputhistory
                print "########################################################"
            else:
                print "Starting Places DB Generation"
            time.sleep(3)

    options = Options(obj, source_idx)
    return options


def main(source,
         source_idx,
         num_places,
         bookmarks_mod,
         history_mult,
         n_keywords,
         n_inputhistory,
         path,
         debug):
    """
    configure and run the whole operation of creating data in places.sqlite
    """
    DEBUG = False
    if debug:
        DEBUG = True
    if path:
        os.environ['PLACES_DB_PATH'] = path
    counter = 1
    if DEBUG:
        print "Creating %s Places" % num_places

    PERCENT_PLACES_TYPED = 0.3

    kw_gen = keyword_gen(n_keywords)
    places_typed_gen = flip_coin_slant(num_places, PERCENT_PLACES_TYPED)()
    while num_places > 0:
        if DEBUG:
            print 'Place #%s created' % counter
        place = new_place(places_typed_gen)  # create a place
        if n_inputhistory > 0:
            inp_hist = new_inputhistory(place)
            n_inputhistory = n_inputhistory - 1
        history = new_history(place)  # create 5 history records
        if num_places % bookmarks_mod == 0:
            nb = new_bookmark(place, kw_gen)  # create a bookmark
        num_places = num_places - 1
        counter = counter + 1

    # update the time this was last run. The value is stored in a
    # moz_historyvisits row as 'visit_date' for the place with the
    # uri of "http://mozilla.com/places_test_placeholder'

    if DEBUG:
        print "Updating last time run timestamp"
    try:
        last_time_run()
    except Exception, e:
        print e
        raise

if __name__ == '__main__':

    parser = OptionParser()
    parser.add_option("-p", "--path", dest="path", default=None,
                      help="Path to SQLite db", metavar="PATH")
    parser.add_option("-s", "--source",
                      dest="source",
                      default='places_stats_site',
                      help="'made_up' data or 'places_stats_site' data?",
                      metavar="SOURCE")
    parser.add_option("-i", "--source_idx", dest="source_idx",
                      default='max',
                      help="Source object index: avg, min, max",
                      metavar="SOURCE_IDX")
    parser.add_option("-n", "--num-places", dest="n_places", default=300,
                      help="Number of Places to create",
                      metavar="NUM_PLACES")
    parser.add_option("-b", "--bookmarks-modulus", dest="b_mod", default=4,
                      help="Bookmarks Modulus, the number of places for which a single bookmark is created",
                      metavar="BOOKMARKS_MODULUS")
    parser.add_option("-k", "--num-keywords", dest="n_keywords", default=15,
                      help="Number of total Keywords",
                      metavar="KEYWORDS")
    parser.add_option(
        "-t", "--input-history", dest="n_inputhistory", default=50,
        help="Number of Input History rows",
        metavar="INPUT_HISTORY")
    parser.add_option("-m", "--history-mult", dest="h_mult", default=5,
                      help="The number of history items to create for each place", metavar="HISTORY_MULT")
    parser.add_option("-v", action="store_true", dest="debug",
                      default=True, help="Turn DEBUG on")
    parser.add_option("-q", action="store_false", dest="debug",
                      default=False, help="Turn DEBUG off")

    (options, args) = parser.parse_args()

    if options.source == 'places_stats_site':
        cmd_line_options = options
        stats_obj = get_web_data()
        options = parse_places_stats_obj(stats_obj,
                                         cmd_line_options.source_idx)
        options.path = cmd_line_options.path
        options.debug = cmd_line_options.debug

    KEYWORD_COUNT = 0
    KEYWORD_IDS = []

    main(options.source,
         options.source_idx,
         options.n_places,
         int(options.b_mod),
         int(options.h_mult),
         options.n_keywords,
         options.n_inputhistory,
         options.path,
         options.debug)
