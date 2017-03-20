#!/bin/bash

VENV=$1
LOGFILE=$2
CONFIG=$3

if [ -z "$VENV" ]; then
    VENV=/home/cltbld/release-runner/venv
fi

if [ ! -e "$VENV" ]; then
    echo "Could not find Python virtual environment '$VENV'"
    exit 1
fi

if [ -z "$LOGFILE" ]; then
    LOGFILE=/var/log/supervisor/release-runner.log
fi

LOGFILE_DIR=$(dirname $LOGFILE)
if [ ! -e $LOGFILE_DIR  ]; then
    echo "Could not find directory '$LOGFILE_DIR' for the logs"
    exit 1
fi

if [ -z "$CONFIG" ]; then
    CONFIG=/home/cltbld/.release-runner.ini
fi

if [ ! -e "$CONFIG" ]; then
    echo "Could not find configuration file '$CONFIG'"
    exit 1
fi

# Mozilla hg is symlinked as /usr/local/bin/hg
export PATH=/usr/local/bin:$PATH

. $VENV/bin/activate

# Sleep time after a failure, in seconds.
SLEEP_TIME=60
NOTIFY_TO=$(grep "notify_to:" $CONFIG|perl -pe 's/.*?<(.*?)>/$1 /g')
if [ -z "$NOTIFY_TO" ]; then
    NOTIFY_TO="release@mozilla.com"
fi
NOTIFY_FROM=$(grep "notify_from:" $CONFIG|perl -pe 's/.*?<(.*?)>/$1 /g')
if [ -z "$NOTIFY_FROM" ]; then
    NOTIFY_FROM="release@mozilla.com"
fi
SUBJECT_TAG="[dev-release-runner]"
if [ -n "`grep release-automation-notifications@mozilla.com $CONFIG`" ]; then
    SUBJECT_TAG="[release-runner]"
fi


CURR_DIR=$(cd $(dirname $0); pwd)
HOSTNAME=`hostname -s`

cd $CURR_DIR

python release-runner.py -c $CONFIG
RETVAL=$?
# Exit code 5 is a failure during polling. We don't want to send mail about
# this, because it will just try again after sleeping.
if [[ $RETVAL == 5 ]]; then
    sleep $SLEEP_TIME;
# Any other non-zero exit code is some other issue, and we should send mail
# about it.
elif [[ $RETVAL != 0 ]]; then
    # Super crazy sed magic below to grab everything from the last run.
    # Explanation of it:
    # H = append each line to the hold space while iterating through the file.
    # If "Fetching release requests" appears in a line, replace the hold space
    # buffer with it. This happens every time we encounter this pattern, so
    # eventually we'll end up only the last instance of it (and what follows)
    # in the hold space.
    # ${...} = stuff do to do when we hit EOF
    # g = copy the hold space into the pattern space
    # p = print the pattern space (ie, the the last instance of
    # "Fetching release requests" and what follows).
    #
    # If for some reason we have a log file that doesn't have
    # "Fetching release requests" in it, the entire file will be printed.
    # It's doubtful this will happen, so we won't waste time dealing with yet.
    (
        echo "Release runner encountered a runtime error: "
        sed -n 'H;/Fetching release requests/h; ${;g;p;}' $LOGFILE
        echo
        echo "The full log is available on $HOSTNAME in $LOGFILE"
        echo "I'll sleep for $SLEEP_TIME seconds before retry"
        echo
        echo "- release runner"
    ) | mail -s "${SUBJECT_TAG} failed" -r $NOTIFY_FROM $NOTIFY_TO

    sleep $SLEEP_TIME
fi
