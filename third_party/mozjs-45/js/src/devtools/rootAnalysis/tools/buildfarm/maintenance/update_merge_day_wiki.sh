#!/bin/bash -e

# Explicitly unset any pre-existing environment variables to avoid variable collision
unset DRY_RUN

INCREMENT_TITLES="Template:Version/Gecko/release/next \
    Template:Version/Gecko/central/current \
    Template:Version/Gecko/aurora/current \
    Template:Version/Gecko/beta/current \
    Template:Version/Gecko/release/current"

B2G_TITLES="Template:B2G_DEV_VERSION"
ESR_TITLES="Template:Version/Gecko/esr/current"
SIX_WEEK_DATE_TITLES="Template:NextReleaseDate"
WIKI_COMMENT="Merge day"

. "$(dirname "${0}")/wiki_functions.sh"

function usage {
    echo "Usage: $0 -h"
    echo "Usage: $0 -b B2G_VERSION [-e ESR_VERSION] [-d]"
    echo
    echo "    -h:                    Display help."
    echo "    -b B2G_VERSION:        REQUIRED! New B2G version for mozilla-central. e.g. 2.1"
    echo "    -e ESR_VERSION:        New ESR version (only set this when a new ESR version comes along!)"
    echo "    -r NEXT_RELEASE_DATE:  Next release date YYYY-MM-DD . By default we'll increment 6 weeks."
    echo "    -d:                    Dry run; will not make changes, only validates login."
    echo "You need to set WIKI_USERNAME and WIKI_PASSWORD in env before running."
}


function increment_wiki_integer {
    echo "  * Preparing wiki page to include new content..."
    old_content=$(cat ${current_content})
    expr $old_content + 1 > "${new_content}"
}

function add_six_weeks {
    echo "  * Preparing wiki page to include new content..."
    old_content=$(cat ${current_content})
    {
        echo '#!/usr/bin/env python'
        echo 'import datetime'
        echo 'import time'
        echo "s = '${old_content}'"
        echo 't = time.mktime(datetime.datetime.strptime(s, "%Y-%m-%d").timetuple()) + 3600 * 24 * 7 * 6'
        echo 'print datetime.datetime.fromtimestamp(t).strftime("%Y-%m-%d")'
    } > "${current_content}"
    python "${current_content}" > "${new_content}"
}

echo "  * Parsing parameters of $(basename "${0}")..."
# Parse parameters passed to this script
while getopts ":dhb:e:r:" opt; do
    case "${opt}" in
        d)  DRY_RUN=1
            ;;
        h)  usage
            exit 0
            ;;
        b)  B2G_VERSION="${OPTARG}"
            ;;
        e)  ESR_VERSION="${OPTARG}"
            ;;
        r)  NEXT_RELEASE_DATE="${OPTARG}"
            ;;
        ?) echo foo; usage >&2
            exit 1
            ;;
    esac
done

DRY_RUN="${DRY_RUN:-0}"
if [ ! -n "${B2G_VERSION}" ]; then
    echo "Missing b2g version!"
    usage
    exit 1
fi

check_wiki_login_env
wiki_login

# Bump b2g version.  This is more prone to error, so let's do it first.
for WIKI_TITLE in ${B2G_TITLES}; do
    # create some temporary files
    current_content="$(mktemp -t current-content.XXXXXXXXXX)"
    new_content="$(mktemp -t new-content.XXXXXXXXXX)"
    echo "  * Retrieving current wiki text of https://wiki.mozilla.org/${WIKI_TITLE}..."
    curl -s "https://wiki.mozilla.org/${WIKI_TITLE}?action=raw" >> "${current_content}"
    old_content=$(cat ${current_content})
    if [ "${old_content}" == "${B2G_VERSION}" ]; then
        echo "***** B2g version ${B2G_VERSION} hasn't changed!"
        rm "${new_content}"
        rm "${current_content}"
        continue
    fi
    echo ${B2G_VERSION} > "${new_content}"
    wiki_edit_login
    wiki_post
    echo "  * Deleting temporary files..."
    rm "${new_content}"
    rm "${current_content}"
done

# Bump esr version.
if [ -n "${ESR_VERSION}" ]; then
    for WIKI_TITLE in ${ESR_TITLES}; do
        # create some temporary files
        current_content="$(mktemp -t current-content.XXXXXXXXXX)"
        new_content="$(mktemp -t new-content.XXXXXXXXXX)"
        echo "  * Retrieving current wiki text of https://wiki.mozilla.org/${WIKI_TITLE}..."
        curl -s "https://wiki.mozilla.org/${WIKI_TITLE}?action=raw" >> "${current_content}"
        old_content=$(cat ${current_content})
        if [ "${old_content}" == "${ESR_VERSION}" ]; then
            echo "***** ESR version ${ESR_VERSION} hasn't changed!"
            rm "${new_content}"
            rm "${current_content}"
            continue
        fi
        echo ${ESR_VERSION} > "${new_content}"
        wiki_edit_login
        wiki_post
        echo "  * Deleting temporary files..."
        rm "${new_content}"
        rm "${current_content}"
    done
fi

# Bump next release date.
for WIKI_TITLE in ${SIX_WEEK_DATE_TITLES}; do
    # create some temporary files
    current_content="$(mktemp -t current-content.XXXXXXXXXX)"
    new_content="$(mktemp -t new-content.XXXXXXXXXX)"
    echo "  * Retrieving current wiki text of https://wiki.mozilla.org/${WIKI_TITLE}..."
    curl -s "https://wiki.mozilla.org/${WIKI_TITLE}?action=raw" >> "${current_content}"
    old_content=$(cat ${current_content})
    if [ -n "${NEXT_RELEASE_DATE}" ]; then
        if [ "${old_content}" == "${NEXT_RELEASE_DATE}" ]; then
            echo "***** Next release date ${NEXT_RELEASE_DATE} hasn't changed!"
            rm "${new_content}"
            rm "${current_content}"
            continue
        fi
        echo "huh?"
        echo ${NEXT_RELEASE_DATE} > "${new_content}"
    else
        add_six_weeks
    fi
    new_date=$(cat "${new_content}")
    echo "  * New date: ${new_date}"
    wiki_edit_login
    wiki_post
    echo "  * Deleting temporary files..."
    rm "${new_content}"
    rm "${current_content}"
done

# Increment these wiki template numbers!
for WIKI_TITLE in ${INCREMENT_TITLES}; do
    # create some temporary files
    current_content="$(mktemp -t current-content.XXXXXXXXXX)"
    new_content="$(mktemp -t new-content.XXXXXXXXXX)"
    echo "  * Retrieving current wiki text of https://wiki.mozilla.org/${WIKI_TITLE}..."
    curl -s "https://wiki.mozilla.org/${WIKI_TITLE}?action=raw" >> "${current_content}"
    increment_wiki_integer
    wiki_edit_login
    wiki_post
    echo "  * Deleting temporary files..."
    rm "${new_content}"
    rm "${current_content}"
done

wiki_logout
rm "${cookie_jar}"

exit "${EXIT_CODE}"
