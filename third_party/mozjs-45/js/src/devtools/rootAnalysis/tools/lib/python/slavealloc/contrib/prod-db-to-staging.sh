#! /bin/sh

# This script is designed to copy the 'buildslaves' database to the
# 'buildslaves_staging' database.  It should be run whenever you want to test
# behaviors in staging.
#
# It lives in build/tools under lib/python/slavealloc/contrib.
#
# Use as
#  vim ./buildslaves-db-to-staging.sh # fill in the values below
#  ./buildslaves-db-to-staging.sh

STAGING_PASSWORD=
STAGING_RW_HOST=
STAGING_USER=
STAGING_DB=
PROD_PASSWORD=
PROD_RO_HOST=
PROD_USER=
PROD_DB=

mysqldump -h "$PROD_RO_HOST" -u "$PROD_USER" -p"$PROD_PASSWORD" "$PROD_DB" | \
    mysql -h "$STAGING_RW_HOST" -u "$STAGING_USER" -p"$STAGING_PASSWORD" "$STAGING_DB"
