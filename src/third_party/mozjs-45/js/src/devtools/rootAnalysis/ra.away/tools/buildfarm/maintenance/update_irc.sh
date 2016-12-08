#!/bin/bash -eu

# I use ii here as a quick-and-dirty command-line interface to irc.
# You could quite easily replace this with a call out to an existing
# bot or external script.
MSG=$1
if [ "${MSG}" == "" ]; then
    exit 1
fi

II=`which ii`
if [ "${II}" == "" ]; then
    echo "  * IRC client not found. Skipping IRC update."
    exit 2
fi

IRCSERVER='irc.mozilla.org'
IRCNICK='reconfig-bot'
IRCCHANNEL='#releng'
${II} -s ${IRCSERVER} -n ${IRCNICK} -f "Reconfig Bot" &
iipid="$!"
sleep 5
printf "/j %s\n" "${IRCCHANNEL}" > ~/irc/${IRCSERVER}/in
while [ ! -d ~/irc/${IRCSERVER}/${IRCCHANNEL} ]; do
    echo "  * ~/irc/${IRCSERVER}/${IRCCHANNEL} doesn't exist. Waiting."
    sleep 5
done
echo "  * Updating IRC with: ${MSG}"
echo "${MSG}" > ~/irc/${IRCSERVER}/${IRCCHANNEL}/in
sleep 5
kill ${iipid}
