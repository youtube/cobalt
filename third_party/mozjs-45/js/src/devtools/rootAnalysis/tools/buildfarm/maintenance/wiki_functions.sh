#!/bin/bash

function check_wiki_login_env {
    t="${1:-}"
    if [ -z "${WIKI_USERNAME}" ]; then
        usage
        echo "ERROR: Environment variable WIKI_USERNAME must be set for publishing wiki page to https://wiki.mozilla.org/$t" >&2
        echo "Exiting..." >&2
        exit 73
    else
        echo "  * Environment variable WIKI_USERNAME defined ('${WIKI_USERNAME}')"
    fi

    if [ -z "${WIKI_PASSWORD}" ]; then
        usage
        echo "ERROR: Environment variable WIKI_PASSWORD must be set for publishing wiki page to https://wiki.mozilla.org/$t" >&2
        echo "Exiting..." >&2
        exit 74
    else
        echo "  * Environment variable WIKI_PASSWORD defined"
    fi
}

function wiki_login {
    # Requires WIKI_TITLE set; maybe we should pass this in as an arg
    cookie_jar="$(mktemp -t cookie-jar.XXXXXXXXXX)"
    # login, store cookies in "${cookie_jar}" temporary file, and get login token...
    echo "  * Logging in to wiki as user '${WIKI_USERNAME}' and getting login token and session cookie..."
    json="$(curl -s -c "${cookie_jar}" -d action=login -d lgname="${WIKI_USERNAME}" -d lgpassword="${WIKI_PASSWORD}" -d format=json 'https://wiki.mozilla.org/api.php')"
    login_token="$(echo "${json}" | sed -n 's/.*"token":"//p' | sed -n 's/".*//p')"
    if [ -z "${login_token}" ]; then
        {
            echo "ERROR: Could not retrieve login token"
            echo "    Ran: curl -s -c '${cookie_jar}' -d action=login -d lgname='${WIKI_USERNAME}' -d lgpassword=********* -d format=json 'https://wiki.mozilla.org/api.php'"
            echo "    Output retrieved:"
            echo "${json}" | sed 's/^/        /'
        } >&2
        exit 75
   fi
    echo "  * Login token retrieved: '${login_token}'"
}

function wiki_edit_login {
    # login again, using login token received (see https://www.mediawiki.org/wiki/API:Login)
    # split out from wiki_login so we can edit many pages with the same cookie.
    # Requires you to have run wiki_login already, and also set WIKI_TITLE.
    echo "  * Logging in again, and passing login token just received..."
    curl -s -b "${cookie_jar}" -d action=login -d lgname="${WIKI_USERNAME}" -d lgpassword="${WIKI_PASSWORD}" -d lgtoken="${login_token}" 'https://wiki.mozilla.org/api.php' 2>&1 > /dev/null
    # get an edit token, remembering to pass previous cookies (see https://www.mediawiki.org/wiki/API:Edit)
    echo "  * Retrieving an edit token for making wiki changes..."
    edit_token_page="$(curl -b "${cookie_jar}" -s -d action=query -d prop=info -d intoken=edit -d titles=${WIKI_TITLE} 'https://wiki.mozilla.org/api.php')"
    edit_token="$(echo "${edit_token_page}" | sed -n 's/.*edittoken=&quot;//p' | sed -n 's/&quot;.*//p')"
if [ -z "${edit_token}" ]; then
        error_received="$(echo "${edit_token_page}" | sed -n 's/.*&lt;info xml:space=&quot;preserve&quot;&gt;<\/span>\(.*\)<span style="color:blue;">&lt;\/info&gt;<\/span>.*/\1/p')"
        if [ "${error_received}" == "Action 'edit' is not allowed for the current user" ]; then
            {
                echo "ERROR: Either user '${WIKI_USERNAME}' is not authorized to publish changes to the wiki page https://wiki.mozilla.org/${WIKI_TITLE},"
                echo "    or the wrong password has been specified for this user (not dispaying password here for security reasons)."
            }
            exit 76
        elif [ -n "${error_received}" ]; then
            {
                echo "ERROR: Problem getting an edit token for updating wiki: ${error_received}"
            } >&2
            exit 77
        else
            {
                echo "ERROR: Could not retrieve edit token"
                echo "    Ran: curl -b '${cookie_jar}' -s -d action=query -d prop=info -d intoken=edit -d titles=${WIKI_TITLE} 'https://wiki.mozilla.org/api.php'"
                echo "    Output retrieved:"
                echo "${edit_token_page}" | sed 's/^/        /'
            } >&2
            exit 78
        fi
     fi
    echo "  * Edit token retrieved: '${edit_token}'"
}

function wiki_post {
    # now post new content...
    # Requires WIKI_TITLE, WIKI_COMMENT, new_content set; maybe we should pass these in as args
    EXIT_CODE=0
    if [ "${DRY_RUN}" == 0 ]; then
        publish_log="$(mktemp -t publish-log.XXXXXXXXXX)"
        echo "  * Publishing updated page to https://wiki.mozilla.org/${WIKI_TITLE}..."
        curl -s -b "${cookie_jar}" -H 'Content-Type:application/x-www-form-urlencoded' -d action=edit -d title="${WIKI_TITLE}" -d "summary=${WIKI_COMMENT}" --data-urlencode "text=$(cat "${new_content}")" --data-urlencode token="${edit_token}" 'https://wiki.mozilla.org/api.php' 2>&1 > "${publish_log}"
        echo "  * Checking whether publish was successful..."
        if grep -q Success "${publish_log}"; then
            echo "  * ${WIKI_TITLE} updated successfully."
        else
            echo "ERROR: Failed to update wiki page https://wiki.mozilla.org/${WIKI_TITLE}" >&2
            EXIT_CODE=68
        fi
        [ -n "${DEBUG_DIR}" ] && cp "${publish_log}" "${DEBUG_DIR}/publish.log"
        rm "${publish_log}"
    else
        echo "  * Not publishing changes to https://wiki.mozilla.org/${WIKI_TITLE} since this is a dry run..."
    fi
}

function wiki_logout {
    # Be kind and logout.
    echo "  * Logging out of wiki"
    curl -s -b "${cookie_jar}" -d action=logout 'https://wiki.mozilla.org/api.php' >/dev/null
}
