#!/bin/bash -e

# Explicitly unset any pre-existing environment variables to avoid variable collision
unset DRY_RUN DEBUG_DIR
WIKI_TEXT_ADDITIONS_FILE=""

. "$(dirname "${0}")/wiki_functions.sh"

WIKI_TITLE="ReleaseEngineering/Maintenance"

function usage {
    echo "Usage: $0 -h"
    echo "Usage: $0 [-r RECONFIG_DIR ] -d [-w WIKI_TEXT_ADDITIONS_FILE]"
    echo "Usage: $0 [-r RECONFIG_DIR ] -w WIKI_TEXT_ADDITIONS_FILE"
    echo
    echo "    -d:                           Dry run; will not make changes, only validates login."
    echo "    -h:                           Display help."
    echo "    -r DEBUG_DIR:                 Copy temporary generated files into directory DEBUG_DIR"
    echo "    -w WIKI_TEXT_ADDITIONS_FILE:  File containing wiki markdown to insert into wiki page."
    echo "You need to set WIKI_USERNAME and WIKI_PASSWORD in env before running."
}

# Simple function to output the name of this script and the options that were passed to it
function command_called {
    echo -n "Command called:"
    for ((INDEX=0; INDEX<=$#; INDEX+=1))
    do
        echo -n " '${!INDEX}'"
    done
    echo ''
    echo "From directory: '$(pwd)'"
}

command_called "${@}" | sed '1s/^/  * /;2s/^/    /'

echo "  * Parsing parameters of $(basename "${0}")..."
# Parse parameters passed to this script
while getopts ":dhr:w:" opt; do
    case "${opt}" in
        d)  DRY_RUN=1
            ;;
        h)  usage
            exit 0
            ;;
        r)  DEBUG_DIR="${OPTARG}"
            ;;
        w)  WIKI_TEXT_ADDITIONS_FILE="${OPTARG}"
            ;;
        ?)  usage >&2
            exit 1
            ;;
    esac
done

DRY_RUN="${DRY_RUN:-0}"
DEBUG_DIR="${DEBUG_DIR:-}"

if [ -n "${DEBUG_DIR}" ]; then
    if [ ! -d "${DEBUG_DIR}" ]; then
        echo "  * Creating directory '${DEBUG_DIR}'..."
        if ! mkdir -p "${DEBUG_DIR}"; then
            echo "ERROR: Directory '${DEBUG_DIR}' could not be created from directory '$(pwd)'." >&2
            exit 70
        fi
    else
        echo "  * Debug directory '${DEBUG_DIR}' exists - OK"
    fi
else
    echo "  * Not storing temporary files (DEBUG_DIR has not been specified)"
fi

# if doing a dry run, not *necessary* to specify a wiki text file, but optional, so if specified, use it
if [ "${DRY_RUN}" == 0 ] || [ -n "${WIKI_TEXT_ADDITIONS_FILE}" ]; then
    if [ -z "${WIKI_TEXT_ADDITIONS_FILE}" ]; then
        echo "ERROR: Must provide a file containing additional wiki text to embed, e.g. '${0}' -w 'reconfig-bugs.wikitext'" >&2
        echo "Exiting..." >&2
        exit 71
    fi
    if [ ! -f "${WIKI_TEXT_ADDITIONS_FILE}" ]; then
        echo "ERROR: File '${WIKI_TEXT_ADDITIONS_FILE}' not found. Working directory is '$(pwd)'." >&2
        echo "This file should contain additional wiki content to be inserted in https://wiki.mozilla.org/ReleaseEngineering/Maintenance." >&2
        echo "Exiting..." >&2
        exit 72
    fi
fi

check_wiki_login_env "${WIKI_TITLE}"

# create some temporary files
current_content="$(mktemp -t current-content.XXXXXXXXXX)"
new_content="$(mktemp -t new-content.XXXXXXXXXX)"

echo "  * Retrieving current wiki text of https://wiki.mozilla.org/${WIKI_TITLE}..."
curl -s "https://wiki.mozilla.org/${WIKI_TITLE}?action=raw" >> "${current_content}"

# find first "| in production" line in the current content, and grab line number
old_line="$(sed -n '/^| in production$/=' "${current_content}" | head -1)"

echo "  * Preparing wiki page to include new content..."
# create new version of whole page, and put in "${new_content}" file...
{
    # old content, up to 2 lines before the first "| in production" line
    sed -n 1,$((old_line-2))p "${current_content}"
    # the new content to add
    if [ -r "${WIKI_TEXT_ADDITIONS_FILE}" ]; then
        echo '|-'
        echo '| in production'
        echo "| `TZ=America/Los_Angeles date +"%Y-%m-%d %H:%M PT"`"
        echo '|'
        cat "${WIKI_TEXT_ADDITIONS_FILE}"
    fi
    # the rest of the page (starting from line before "| in production"
    sed -n $((old_line-1)),\$p "${current_content}"
} > "${new_content}"

WIKI_COMMENT="reconfig"
wiki_login
wiki_edit_login
wiki_post
wiki_logout

if [ -n "${DEBUG_DIR}" ]; then
    echo "  * Backing up temporary files generated during wiki publish before they get deleted..."
    cp "${cookie_jar}"      "${DEBUG_DIR}/cookie_jar.txt"
    cp "${new_content}"     "${DEBUG_DIR}/new_wiki_content.txt"
    cp "${current_content}" "${DEBUG_DIR}/old_wiki_content.log"
fi

# remove temporary files
echo "  * Deleting temporary files..."
rm "${cookie_jar}"
rm "${new_content}"
rm "${current_content}"

exit "${EXIT_CODE}"
