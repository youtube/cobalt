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
This file was mostly generated from django's 'django-admin.py inspectdb' facility: http://docs.djangoproject.com/en/dev/ref/django-admin/?from=olddocs#inspectdb

Some of the models had to be manually tweaked to get the column type correct and/or column attrs
"""

from django.db import models


class MozKeywords(models.Model):
    id = models.IntegerField(null=True, primary_key=True, blank=True)
    keyword = models.TextField(null=True, blank=True)

    class Meta:
        db_table = u'moz_keywords'


class MozFavicons(models.Model):
    id = models.IntegerField(null=True, primary_key=True, blank=True)
    url = models.TextField(
        unique=True, blank=True)  # This field type is a guess.
    data = models.TextField(blank=True)  # This field type is a guess.
    mime_type = models.CharField(max_length=32, blank=True)
    expiration = models.TextField(blank=True)  # This field type is a guess.

    class Meta:
        db_table = u'moz_favicons'


class MozPlaces(models.Model):
    id = models.IntegerField(null=True, primary_key=True, blank=True)
    url = models.TextField(
        unique=True, blank=True)  # This field type is a guess.
    title = models.TextField(blank=True)  # This field type is a guess.
    rev_host = models.TextField(blank=True)  # This field type is a guess.
    visit_count = models.IntegerField(null=True, blank=True)
    hidden = models.IntegerField()
    typed = models.IntegerField()
    favicon = models.ForeignKey(MozFavicons, null=True)
    frecency = models.IntegerField()

    class Meta:
        db_table = u'moz_places'


class MozAnnoAttributes(models.Model):
    id = models.IntegerField(null=True, primary_key=True, blank=True)
    name = models.CharField(unique=True, max_length=32)

    class Meta:
        db_table = u'moz_anno_attributes'


class MozAnnos(models.Model):
    id = models.IntegerField(null=True, primary_key=True, blank=True)
    place = models.ForeignKey(MozPlaces)
    anno_attribute = models.ForeignKey(MozAnnoAttributes)
    mime_type = models.CharField(max_length=32, blank=True)
    content = models.TextField(blank=True)  # This field type is a guess.
    flags = models.IntegerField(null=True, blank=True)
    expiration = models.IntegerField(null=True, blank=True)
    _type = models.IntegerField(null=True, blank=True, db_column=u'type')
    dateadded = models.IntegerField(null=True, db_column=u'dateAdded',
                                    blank=True)  # Field name made lowercase.
    lastmodified = models.IntegerField(null=True, db_column=u'lastModified', blank=True)  # Field name made lowercase.

    class Meta:
        db_table = u'moz_annos'


class MozBookmarks(models.Model):
    id = models.IntegerField(null=True, primary_key=True, blank=True)
    _type = models.IntegerField(null=True, blank=True, db_column=u'type')
    fk = models.IntegerField()
    parent = models.IntegerField(null=True, blank=True)
    position = models.IntegerField(null=True, blank=True)
    title = models.TextField(blank=True)  # This field type is a guess.
    keyword = models.ForeignKey(MozKeywords, null=True, blank=True)
    folder_type = models.TextField(blank=True)
    dateadded = models.IntegerField(null=True, db_column=u'dateAdded',
                                    blank=True)  # Field name made lowercase.
    lastmodified = models.IntegerField(null=True, db_column=u'lastModified', blank=True)  # Field name made lowercase.

    class Meta:
        db_table = u'moz_bookmarks'


class MozBookmarksRoots(models.Model):
    root_name = models.CharField(unique=True, max_length=16, blank=True)
    folder_id = models.ForeignKey(MozBookmarks)

    class Meta:
        db_table = u'moz_bookmarks_roots'


class MozHistoryvisits(models.Model):
    id = models.IntegerField(null=True, primary_key=True, blank=True)
    from_visit = models.IntegerField(null=True, blank=True)
    place = models.ForeignKey(MozPlaces)
    visit_date = models.IntegerField(null=True, blank=True)
    visit_type = models.IntegerField(null=True, blank=True)
    session = models.IntegerField(null=True, blank=True)

    class Meta:
        db_table = u'moz_historyvisits'


class MozInputhistory(models.Model):
    place_id = models.IntegerField()
    input = models.TextField(primary_key=True)  # This field type is a guess.
    use_count = models.FloatField(null=True, blank=True)

    class Meta:
        db_table = u'moz_inputhistory'


class MozItemsAnnos(models.Model):
    id = models.IntegerField(null=True, primary_key=True, blank=True)
    item_id = models.ForeignKey(MozBookmarks)
    anno_attribute_id = models.ForeignKey(MozAnnoAttributes)
    mime_type = models.CharField(max_length=32, blank=True)
    content = models.TextField(blank=True)  # This field type is a guess.
    flags = models.IntegerField(null=True, blank=True)
    expiration = models.IntegerField(null=True, blank=True)
    _type = models.IntegerField(null=True, blank=True, db_column=u'type')
    dateadded = models.IntegerField(null=True, db_column=u'dateAdded',
                                    blank=True)  # Field name made lowercase.
    lastmodified = models.IntegerField(null=True, db_column=u'lastModified', blank=True)  # Field name made lowercase.

    class Meta:
        db_table = u'moz_items_annos'
